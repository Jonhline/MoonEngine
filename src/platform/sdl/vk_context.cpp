#include "vk_blit_clip.h"
#include "vk_internal.hpp"
#include "vk_render.h"
#include "vk_surface.h"
#include <SDL_vulkan.h>
#include <cstdio>
#include <cmath>
#include <limits>

namespace moon {
static std::unique_ptr<Context> active;
Context::~Context()
{
    /* Preview/command-line paths can return without explicit video shutdown.
       Image deleters consult target_cache: release GPU owners while every
       Context member is still alive, before reverse-order member destruction. */
    if (instance)
        close();
}
Context &context()
{
    return *active;
}
void check(VkResult result, const char *operation)
{
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}
uint32_t Context::memory_type(uint32_t mask, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(physical, &memory);
    for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
        if ((mask & (1u << i)) && (memory.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    throw std::runtime_error("No suitable Vulkan memory type");
}
Buffer Context::buffer(VkDeviceSize size, VkBufferUsageFlags usage, bool host_visible, bool cached)
{
    Buffer result;
    result.size = size;
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    check(vkCreateBuffer(device, &info, nullptr, &result.handle), "vkCreateBuffer");
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, result.handle, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type(requirements.memoryTypeBits,
                                             host_visible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                          : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    /* Uploads favor sequential writes; readbacks prefer CPU-cached memory.
       Keep the coherent fallback for devices without that combination. */
    if (host_visible && cached) {
        VkPhysicalDeviceMemoryProperties memory;
        vkGetPhysicalDeviceMemoryProperties(physical, &memory);
        const VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                               VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
            if ((requirements.memoryTypeBits & (1u << i)) &&
                (memory.memoryTypes[i].propertyFlags & required) == required) {
                allocation.memoryTypeIndex = i;
                break;
            }
    }
    check(vkAllocateMemory(device, &allocation, nullptr, &result.memory), "vkAllocateMemory");
    check(vkBindBufferMemory(device, result.handle, result.memory, 0), "vkBindBufferMemory");
    if (host_visible)
        check(vkMapMemory(device, result.memory, 0, size, 0, &result.mapped), "vkMapMemory");
    return result;
}
void Context::retire_vertex_buffer(unsigned id)
{
    auto found = vertex_buffers.find(id);
    if (found == vertex_buffers.end())
        return;
    /* Submitted vertex bindings may still refer to the old data. */
    retired_buffers.push_back(found->second);
    vertex_buffers.erase(found);
}
const Buffer *Context::vertex_buffer(unsigned id)
{
    if (stream_geometry || !static_buffers[id] || buffers[id].empty())
        return nullptr;
    auto found = vertex_buffers.find(id);
    if (found != vertex_buffers.end())
        return &found->second;
    const auto &source = buffers[id];
    size_t bytes = (source.size() + 3) & ~size_t(3);
    Buffer resident =
        buffer(bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
    for (size_t start = 0; start < bytes;) {
        size_t count = std::min(bytes - start, (size_t)arena.size);
        size_t offset = reserve(count, 4);
        statistics.update_buffer++;
        auto *destination = (uint8_t *)arena.mapped + offset;
        size_t used = std::min(count, source.size() - start);
        memcpy(destination, source.data() + start, used);
        memset(destination + used, 0, count - used);
        barrier();
        VkBufferCopy copy{offset, start, count};
        vkCmdCopyBuffer(command, arena.handle, resident.handle, 1, &copy);
        start += count;
    }
    statistics.vertex_upload_bytes += source.size();
    return &vertex_buffers.emplace(id, resident).first->second;
}
void Context::destroy(Buffer &b)
{
    if (b.mapped)
        vkUnmapMemory(device, b.memory);
    if (b.handle)
        vkDestroyBuffer(device, b.handle, nullptr);
    if (b.memory)
        vkFreeMemory(device, b.memory, nullptr);
    b = {};
}
void Context::destroy(Image &im)
{
    /* Image owners defer destruction until GPU use has completed. Drop its
       cached framebuffer before destroying the attachment view. */
    for (auto entry = target_cache.begin(); entry != target_cache.end();) {
        if (im.attachment &&
            (entry->first[0] == im.attachment || entry->first[1] == im.attachment)) {
            vkDestroyFramebuffer(device, entry->second, nullptr);
            entry = target_cache.erase(entry);
        } else
            ++entry;
    }
    if (im.view)
        vkDestroyImageView(device, im.view, nullptr);
    if (im.attachment)
        vkDestroyImageView(device, im.attachment, nullptr);
    if (im.handle)
        vkDestroyImage(device, im.handle, nullptr);
    if (im.memory)
        vkFreeMemory(device, im.memory, nullptr);
    im.view = VK_NULL_HANDLE;
    im.handle = VK_NULL_HANDLE;
    im.memory = VK_NULL_HANDLE;
}
std::shared_ptr<Image> Context::image(int w, int h, VkFormat format, int levels)
{
    if (w < 1 || h < 1 || (unsigned)w > properties.limits.maxImageDimension2D ||
        (unsigned)h > properties.limits.maxImageDimension2D)
        throw std::runtime_error("Invalid Vulkan image dimensions");
    auto im = std::shared_ptr<Image>(new Image, [this](Image *p) {
        destroy(*p);
        delete p;
    });
    im->width = w;
    im->height = h;
    im->format = format;
    im->levels = levels;
    bool depth = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    if (depth)
        im->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {(uint32_t)w, (uint32_t)h, 1};
    info.mipLevels = levels;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 (depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                        : VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    check(vkCreateImage(device, &info, nullptr, &im->handle), "vkCreateImage");
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(device, im->handle, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex =
        memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device, &allocation, nullptr, &im->memory), "vkAllocateMemory image");
    check(vkBindImageMemory(device, im->handle, im->memory, 0), "vkBindImageMemory");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = im->handle;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = format;
    view.subresourceRange = {depth ? VK_IMAGE_ASPECT_DEPTH_BIT : im->aspect, 0, (uint32_t)levels, 0, 1};
    check(vkCreateImageView(device, &view, nullptr, &im->view), "vkCreateImageView");
    view.subresourceRange.aspectMask=im->aspect;
    view.subresourceRange.levelCount = 1;
    check(vkCreateImageView(device, &view, nullptr, &im->attachment),
          "vkCreateImageView attachment");
    view.subresourceRange.levelCount = levels;
    end_pass();
    begin();
    im->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    return im;
}
void Context::begin()
{
    if (recording)
        return;
    VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command, &info), "vkBeginCommandBuffer");
    recording = true;
    emitted = {};
    if (queries) {
        vkCmdResetQueryPool(command, queries, 0, 2);
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queries, 0);
    }
    if (pass_queries) {
        vkCmdResetQueryPool(command, pass_queries, 0, 4096);
        pass_labels.clear();
        pass_draws.clear();
        pass_mark("start");
    }
}
void Context::pass_mark(const std::string &label)
{
    if (!pass_queries || !recording || pass_labels.size() >= 4095)
        return;
    vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pass_queries,
                        (uint32_t)pass_labels.size());
    pass_labels.push_back(label);
    pass_draws.push_back(0);
}
void Context::flush()
{
    if (!recording)
        return;
    end_pass();
    write_timestamp_end();
    check(vkEndCommandBuffer(command), "vkEndCommandBuffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    check(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit");
    statistics.submissions++;
    pending = true;
    reset_submission();
}
void Context::reset_submission()
{
    if (pending) {
        uint64_t start = SDL_GetPerformanceCounter();
        check(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "frame fence");
        statistics.fence_wait_ns += (SDL_GetPerformanceCounter() - start) *
                                   1000000000ull / SDL_GetPerformanceFrequency();
    }
    if (pending && timing_written && queries) {
        uint64_t values[2];
        if (vkGetQueryPoolResults(device, queries, 0, 2, sizeof values, values,
                                  sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            uint64_t mask = timestamp_bits == 64 ? UINT64_MAX : ((1ull << timestamp_bits) - 1);
            statistics.gpu_ns += (uint64_t)(((values[1] - values[0]) & mask) *
                                            (double)properties.limits.timestampPeriod);
            statistics.gpu_samples++;
        }
    }
    if (pending && pass_queries && pass_labels.size() >= 2) {
        std::vector<uint64_t> values(pass_labels.size());
        if (vkGetQueryPoolResults(device, pass_queries, 0, (uint32_t)values.size(),
                                  values.size() * 8, values.data(), 8,
                                  VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            uint64_t mask = timestamp_bits == 64 ? UINT64_MAX : ((1ull << timestamp_bits) - 1);
            for (size_t i = 0; i + 1 < values.size(); i++) {
                auto &t = pass_totals[pass_labels[i]];
                t.first += ((values[i + 1] - values[i]) & mask) *
                           (double)properties.limits.timestampPeriod;
                t.second += pass_draws[i];
                if (pass_frames == 100 && getenv("MOON_VK_PROFILE_ORDER"))
                    fprintf(stderr, "pass order: %8.3f ms %5u draws  %s\n",
                            ((values[i + 1] - values[i]) & mask) *
                                (double)properties.limits.timestampPeriod / 1e6,
                            pass_draws[i], pass_labels[i].c_str());
            }
            pass_frames++;
        }
        pass_labels.clear();
        pass_draws.clear();
    }
    pending = timing_written = false;
    check(vkResetFences(device, 1, &fence), "vkResetFences");
    check(vkResetCommandPool(device, pool, 0), "vkResetCommandPool");
    for (auto framebuffer : transient_targets)
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    transient_targets.clear();
    /* Deleted/replaced frontend images remain alive until submitted commands
       and their framebuffer objects have finished using them. */
    retired_images.clear();
    for (auto &buffer : retired_buffers)
        destroy(buffer);
    retired_buffers.clear();
    check(vkResetDescriptorPool(device, descriptor_pool, 0), "vkResetDescriptorPool");
    arena_used = 0;
    default_vertex_offset = SIZE_MAX;
    descriptor_count = 0;
    draw_sets.clear();
    last_set = VK_NULL_HANDLE;
    recording = false;
}
static VkPipelineStageFlags layout_stage(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    default:
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}
static VkAccessFlags layout_access(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return 0;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    default:
        return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
}
void Context::barrier()
{
    end_pass();
    begin();
    VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    memory.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memory.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memory, 0, nullptr, 0, nullptr);
    statistics.barriers++;
}
void Context::transition(Image &im, VkImageLayout layout)
{
    if (im.layout == layout)
        return;
    begin();
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = im.layout;
    barrier.newLayout = layout;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = im.handle;
    barrier.subresourceRange = {im.aspect, 0, (uint32_t)im.levels, 0, 1};
    barrier.srcAccessMask = layout_access(im.layout);
    barrier.dstAccessMask = layout_access(layout);
    vkCmdPipelineBarrier(command, layout_stage(im.layout), layout_stage(layout), 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);
    statistics.barriers++;
    im.layout = layout;
}
void Context::end_pass()
{
    if (!pass_color)
        return;
    vkCmdEndRenderPass(command);
    pass_color = pass_depth = VK_NULL_HANDLE;
}
size_t Context::reserve(size_t bytes, size_t alignment)
{
    size_t offset = (arena_used + alignment - 1) / alignment * alignment;
    if (offset + bytes > arena.size || descriptor_count >= 8000) {
        flush();
        offset = 0;
    }
    if (bytes > arena.size)
        throw std::runtime_error("Vulkan staging arena exhausted");
    arena_used = offset + bytes;
    begin();
    return offset;
}
Image *Context::color(unsigned fb)
{
    if (!fb) {
        int w, h;
        SDL_Vulkan_GetDrawableSize(window, &w, &h);
        if (w < 1 || h < 1)
            return nullptr;
        if (!screen || screen->width != w || screen->height != h) {
            drain();
            screen = image(w, h, VK_FORMAT_R8G8B8A8_UNORM);
        }
        return screen.get();
    }
    auto &target = framebuffers[fb];
    auto &images = target.color_texture ? textures : renderbuffers;
    auto found = images.find(target.color);
    return found == images.end() ? nullptr : found->second.get();
}
Image *Context::depth(unsigned fb)
{
    auto &images=framebuffers[fb].depth_texture?textures:renderbuffers;
    auto found = images.find(framebuffers[fb].depth);
    return found == images.end() ? nullptr : found->second.get();
}
void Context::upload(Image &im, int level, int x, int y, int w, int h, GLenum format,
                     const void *pixels)
{
    if (!pixels || w < 1 || h < 1)
        return;
    int channels = format == GL_RGB ? 3 : format == GL_RED ? 1 : 4;
    size_t offset = reserve((size_t)w * h * 4, 4);
    statistics.update_buffer++;
    auto *out = (uint8_t *)arena.mapped + offset;
    size_t stride = ((size_t)w * channels + state.unpack_alignment - 1) / state.unpack_alignment *
                    state.unpack_alignment;
    auto *src = (const uint8_t *)pixels;
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++) {
            auto *p = src + row * stride + col * channels;
            auto *q = out + ((size_t)row * w + col) * 4;
            q[0] = p[0];
            q[1] = channels > 1 ? p[1] : 0;
            q[2] = channels > 1 ? p[2] : 0;
            q[3] = channels == 4 ? p[3] : 255;
        }
    barrier();
    transition(im, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.bufferOffset = offset;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, (uint32_t)level, 0, 1};
    copy.imageOffset = {x, y, 0};
    copy.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyBufferToImage(command, arena.handle, im.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy);
    transition(im, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
void Context::read(Image &im, int x, int y, int w, int h, GLenum format, void *pixels)
{
    if (!pixels || w < 1 || h < 1)
        return;
    statistics.readbacks++;
    const size_t bytes = (size_t)w * h * 4;
    if (readback.size < bytes) {
        destroy(readback); /* A previous read already waited for its fence. */
        readback = buffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, true);
    }
    barrier();
    transition(im, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.imageSubresource = {(VkImageAspectFlags)(format == GL_DEPTH_COMPONENT
                                                      ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                      : VK_IMAGE_ASPECT_COLOR_BIT),
                             0, 0, 1};
    copy.imageOffset = {x, y, 0};
    copy.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyImageToBuffer(command, im.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.handle,
                           1, &copy);
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    statistics.barriers++;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
                         &host, 0, nullptr, 0, nullptr);
    flush();
    auto *src = (uint8_t *)readback.mapped;
    int channels = format == GL_RGB ? 3 : 4;
    size_t stride = ((size_t)w * channels + state.pack_alignment - 1) / state.pack_alignment *
                    state.pack_alignment;
    if (format == GL_DEPTH_COMPONENT && im.format == VK_FORMAT_D24_UNORM_S8_UINT) {
        for (int i = 0; i < w * h; i++)
            ((float *)pixels)[i] = (((uint32_t *)src)[i] & 0xffffffu) / 16777215.f;
    } else if (channels == 4) {
        /* Copy whole rows, including on devices without a cached coherent
           memory type. Tiny reads from an uncached mapping are expensive. */
        for (int row = 0; row < h; row++)
            memcpy((uint8_t *)pixels + row * stride, src + (size_t)row * w * 4, (size_t)w * 4);
    } else
        for (int row = 0; row < h; row++)
            for (int col = 0; col < w; col++)
                memcpy((uint8_t *)pixels + row * stride + col * channels,
                       src + ((size_t)row * w + col) * 4, channels);
}
void Context::blit(Image &src, Image &dst, int sx0, int sy0, int sx1, int sy1, int dx0, int dy0,
                   int dx1, int dy1, bool linear, bool is_depth)
{
    if (!moon_vk_clip_axis(sx0,sx1,src.width,dx0,dx1,dst.width) ||
        !moon_vk_clip_axis(sy0,sy1,src.height,dy0,dy1,dst.height)) return;
    barrier();
    if (pass_queries)
        pass_mark("blit " + std::to_string(src.width) + "x" + std::to_string(src.height) + " f" +
                  std::to_string(src.format) + " -> " + std::to_string(dst.width) + "x" +
                  std::to_string(dst.height));
    VkImageBlit region{};
    VkImageAspectFlags aspect = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource = {aspect, 0, 0, 1};
    region.dstSubresource = {aspect, 0, 0, 1};
    region.srcOffsets[0] = {sx0, sy0, 0};
    region.srcOffsets[1] = {sx1, sy1, 1};
    region.dstOffsets[0] = {dx0, dy0, 0};
    region.dstOffsets[1] = {dx1, dy1, 1};
    VkFilter filter = linear && !is_depth ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    if (src.handle == dst.handle) {
        transition(src, VK_IMAGE_LAYOUT_GENERAL);
        vkCmdBlitImage(command, src.handle, VK_IMAGE_LAYOUT_GENERAL, dst.handle,
                       VK_IMAGE_LAYOUT_GENERAL, 1, &region, filter);
        return;
    }
    transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdBlitImage(command, src.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.handle,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, filter);
}
/* The surface is gone (Android backgrounded us, the document picker took the
   foreground, the window was recreated). Drop the swapchain and the surface,
   make both again from the window we still own, and carry on. */
bool Context::recreate_surface()
{
    if (!window || !instance)
        return false;
    if (device) {
        drain();
        statistics.queue_waits++;
        VkResult idle = vkDeviceWaitIdle(device);
        if (idle != VK_SUCCESS) {
            if (!surface_error_reported)
                fprintf(stderr, "vulkan: cannot rebuild the surface: device wait %d\n", (int)idle);
            surface_error_reported = true;
            return false;
        }
    }
    if (device) {
        if (swapchain)
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        for (auto semaphore : present_ready)
            vkDestroySemaphore(device, semaphore, nullptr);
    }
    swapchain = VK_NULL_HANDLE;
    swap_images.clear();
    present_ready.clear();
    if (surface)
        vkDestroySurfaceKHR(instance, surface, nullptr);
    surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        if (!surface_error_reported)
            fprintf(stderr, "vulkan: could not recreate the surface: %s\n", SDL_GetError());
        surface_error_reported = true;
        surface = VK_NULL_HANDLE;
        return false;
    }
    int width = 0, height = 0;
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
    rebuild_swapchain = true;
    surface_lost = false;
    surface_error_reported = false;
    fprintf(stderr, "vulkan: surface lost, recreated (w=%d h=%d)\n", width, height);
    return true;
}

static const char *present_mode_name(VkPresentModeKHR mode)
{
    switch (mode) {
    case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
    case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
    case VK_PRESENT_MODE_FIFO_KHR: return "FIFO";
    default: return "UNKNOWN";
    }
}

bool Context::resize_swapchain()
{
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        return false;
    int w, h;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    if (w < 1 || h < 1)
        return false;
    if (!rebuild_swapchain && !surface_lost && extent.width == (unsigned)w &&
        extent.height == (unsigned)h)
        return true;
    /* Never touch a surface that is known to be gone. */
    if (surface_lost && !recreate_surface())
        return false;
    drain();
    statistics.queue_waits++;
    VkResult result = vkDeviceWaitIdle(device);
    if (result != VK_SUCCESS) {
        if (!surface_error_reported)
            fprintf(stderr, "vulkan: swapchain rebuild skipped: device wait %d\n", (int)result);
        surface_error_reported = true;
        return false;
    }
    VkSurfaceCapabilitiesKHR caps;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        if (!recreate_surface())
            return false;
        result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
    }
    if (result != VK_SUCCESS) {
        if (!surface_error_reported)
            fprintf(stderr, "vulkan: surface capabilities: %d\n", (int)result);
        surface_error_reported = true;
        surface_lost = result == VK_ERROR_SURFACE_LOST_KHR;
        return false;
    }
    surface_error_reported = false;
    if (!caps.currentExtent.width || !caps.currentExtent.height)
        return false;
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        !(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
        fprintf(stderr, "vulkan: the surface cannot present transfer images\n");
        return false;
    }
    uint32_t count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, nullptr);
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        if (recreate_surface())
            surface_lost = false;
        return false;
    }
    check(result, "surface format count");
    std::vector<VkSurfaceFormatKHR> formats(count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats.data());
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        if (recreate_surface())
            surface_lost = false;
        return false;
    }
    check(result, "surface formats");
    if (formats.empty())
        return false;
    auto chosen = formats.front();
    for (auto candidate : formats)
        if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
            candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            chosen = candidate;
    surface_format = chosen.format;
    extent = caps.currentExtent;
    if (extent.width == UINT32_MAX)
        extent = {(uint32_t)w, (uint32_t)h};
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height =
        std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, nullptr);
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        if (recreate_surface())
            surface_lost = false;
        return false;
    }
    check(result, "present mode count");
    std::vector<VkPresentModeKHR> modes(count);
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes.data());
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        if (recreate_surface())
            surface_lost = false;
        return false;
    }
    check(result, "present modes");
    if (!vsync)
        for (auto candidate : modes) {
            if (candidate == VK_PRESENT_MODE_MAILBOX_KHR ||
                candidate == VK_PRESENT_MODE_IMMEDIATE_KHR)
                mode = candidate;
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                break;
        }
    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface;
    info.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && info.minImageCount > caps.maxImageCount)
        info.minImageCount = caps.maxImageCount;
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = moon_vk_surface_transform(caps.supportedTransforms, caps.currentTransform);
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & info.compositeAlpha))
        info.compositeAlpha = (VkCompositeAlphaFlagBitsKHR)(caps.supportedCompositeAlpha &
                                                            -caps.supportedCompositeAlpha);
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = swapchain;
    VkSwapchainKHR replacement;
    result = vkCreateSwapchainKHR(device, &info, nullptr, &replacement);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        rebuild_swapchain = true;
        return false;
    }
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        /* The old handle is unusable, and SDL may have replaced the window's
           native surface under us. Build both again, then skip this frame. */
        surface_lost = true;
        recreate_surface();
        return false;
    }
    check(result, "vkCreateSwapchainKHR");
    fprintf(stderr, "render: swapchain present mode=%s vsync=%d\n", present_mode_name(mode),
            vsync ? 1 : 0);
    if (swapchain)
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = replacement;
    check(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr), "swapchain image count");
    for (auto semaphore : present_ready)
        vkDestroySemaphore(device, semaphore, nullptr);
    present_ready.clear();
    swap_images.resize(count);
    check(vkGetSwapchainImagesKHR(device, swapchain, &count, swap_images.data()),
          "swapchain images");
    present_ready.resize(count, VK_NULL_HANDLE);
    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (auto &semaphore : present_ready)
        check(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore), "present semaphore");
    rebuild_swapchain = false;
    return true;
}
void Context::present()
{
    if (!resize_swapchain()) {
        flush();
        return;
    }
    Image *source = color(0);
    if (!source)
        return;
    if (split_present)
        flush();
    uint32_t index;
    VkResult result =
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index);
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        surface_lost = true;
        recreate_surface();
        return;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        rebuild_swapchain = true;
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR)
        check(result, "vkAcquireNextImageKHR");
    else
        rebuild_swapchain = true;
    /* Retain render commands until presentation is ready. Order the screen
       writes before the blit without a render-to-present fence round trip. */
    barrier();
    VkImageMemoryBarrier image_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.srcQueueFamilyIndex = image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_barrier.image = swap_images[index];
    image_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    image_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    statistics.barriers++;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &image_barrier);
    transition(*source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkImageBlit blit{};
    blit.srcSubresource = blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    /* Internal rows retain the original bottom-up framebuffer convention. */
    blit.srcOffsets[0] = {0, source->height, 0};
    blit.srcOffsets[1] = {source->width, 0, 1};
    blit.dstOffsets[1] = {(int)extent.width, (int)extent.height, 1};
    vkCmdBlitImage(command, source->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swap_images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_NEAREST);
    image_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    image_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_barrier.dstAccessMask = 0;
    statistics.barriers++;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &image_barrier);
    write_timestamp_end();
    check(vkEndCommandBuffer(command), "vkEndCommandBuffer present");
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &present_ready[index];
    check(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit present");
    statistics.submissions++;
    pending = true;
    recording = false;
    VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &present_ready[index];
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain;
    info.pImageIndices = &index;
    result = vkQueuePresentKHR(queue, &info);
    if (result == VK_ERROR_SURFACE_LOST_KHR) {
        /* The frame reached a surface Android already took away. Rebuild it
           now so the next frame has somewhere to go; never throw here. */
        surface_lost = true;
        recreate_surface();
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        rebuild_swapchain = true;
    else
        check(result, "vkQueuePresentKHR");
    if (low_latency)
        reset_submission();
    else {
        statistics.frame_rotations++;
        swap_submission();
        reset_submission();
    }
}
void Context::close()
{
    if (device) {
        statistics.queue_waits++;
        vkDeviceWaitIdle(device);
        save_pipeline_cache();
        for (auto fb : transient_targets)
            vkDestroyFramebuffer(device, fb, nullptr);
        transient_targets.clear();
        for (auto &target : target_cache)
            vkDestroyFramebuffer(device, target.second, nullptr);
        target_cache.clear();
        retired_images.clear();
        for (auto &buffer : retired_buffers)
            destroy(buffer);
        retired_buffers.clear();
        for (auto &entry : vertex_buffers)
            destroy(entry.second);
        vertex_buffers.clear();
        for (auto &entry : programs) {
            auto &p = entry.second;
            for (auto &pipe : p.pipelines)
                vkDestroyPipeline(device, pipe.second, nullptr);
            if (p.vs)
                vkDestroyShaderModule(device, p.vs, nullptr);
            if (p.fs)
                vkDestroyShaderModule(device, p.fs, nullptr);
            if (p.layout)
                vkDestroyPipelineLayout(device, p.layout, nullptr);
        }
        programs.clear();
        if (pipeline_cache)
            vkDestroyPipelineCache(device, pipeline_cache, nullptr);
        textures.clear();
        texture_version++;
        renderbuffers.clear();
        white.reset();
        screen.reset();
        for (auto &sampler : samplers)
            vkDestroySampler(device, sampler.second, nullptr);
        for (auto &pass : passes)
            vkDestroyRenderPass(device, pass.second, nullptr);
        destroy(readback);
        close_submission();
        swap_submission();
        close_submission();
        if (pass_queries)
            vkDestroyQueryPool(device, pass_queries, nullptr);
        if (descriptors)
            vkDestroyDescriptorSetLayout(device, descriptors, nullptr);
        for (auto semaphore : present_ready)
            vkDestroySemaphore(device, semaphore, nullptr);
        present_ready.clear();
        if (swapchain)
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (surface)
        vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance)
        vkDestroyInstance(instance, nullptr);
    instance = VK_NULL_HANDLE;
}
} // namespace moon

extern "C" int moon_vk_render_active(void)
{
    return moon::active != nullptr;
}
extern "C" moon_vk_render_stats moon_vk_render_statistics(void)
{
    return moon::active ? moon::active->statistics : moon_vk_render_stats{};
}
extern "C" void moon_vk_scene_begin(int first)
{
    if (moon::active)
        moon::active->scene_first_pass = first != 0;
}
extern "C" int moon_vk_render_open(SDL_Window *window, int vsync)
{
    using namespace moon;
    active = std::make_unique<Context>();
    auto &c = *active;
    c.window = window;
    c.transient_framebuffers = getenv("MOON_VK_TRANSIENT_TARGETS") != nullptr;
    c.split_present = getenv("MOON_VK_SPLIT_PRESENT") != nullptr;
    c.uncached_descriptors = getenv("MOON_VK_UNCACHED_DESCRIPTORS") != nullptr;
    c.low_latency = getenv("MOON_VK_LOW_LATENCY") != nullptr;
    c.repeat_commands = getenv("MOON_VK_REPEAT_COMMANDS") != nullptr;
    c.profile_draws = getenv("MOON_VK_PROFILE_DRAWS") != nullptr;
    if (getenv("MOON_VK_PROFILE_PASSES"))
        c.low_latency = true;
    c.stream_geometry = getenv("MOON_VK_STREAM_GEOMETRY") != nullptr;
    c.vsync = vsync != 0;
    try {
        check(volkInitialize(), "Vulkan loader");
        unsigned count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
            throw std::runtime_error(SDL_GetError());
        std::vector<const char *> extensions(count);
        if (!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data()))
            throw std::runtime_error(SDL_GetError());
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Moon Engine";
        app.pEngineName = "Moon Engine";
        app.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance.pApplicationInfo = &app;
        instance.enabledExtensionCount = count;
        instance.ppEnabledExtensionNames = extensions.data();
        check(vkCreateInstance(&instance, nullptr, &c.instance), "vkCreateInstance");
        volkLoadInstance(c.instance);
        if (!SDL_Vulkan_CreateSurface(window, c.instance, &c.surface))
            throw std::runtime_error(SDL_GetError());
        check(vkEnumeratePhysicalDevices(c.instance, &count, nullptr),
              "vkEnumeratePhysicalDevices");
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(c.instance, &count, devices.data()),
              "vkEnumeratePhysicalDevices list");
        for (auto physical : devices) {
            uint32_t families = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, nullptr);
            std::vector<VkQueueFamilyProperties> queues(families);
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, queues.data());
            for (uint32_t i = 0; i < families; i++) {
                VkBool32 present = false;
                check(vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, c.surface, &present),
                      "surface support");
                if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                    c.physical = physical;
                    c.family = i;
                    c.timestamp_bits = queues[i].timestampValidBits;
                    break;
                }
            }
            if (c.physical)
                break;
        }
        if (!c.physical)
            throw std::runtime_error("No Vulkan graphics/present queue");
        vkGetPhysicalDeviceProperties(c.physical, &c.properties);
        VkPhysicalDeviceFeatures available;
        vkGetPhysicalDeviceFeatures(c.physical, &available);
        if (!available.independentBlend)
            throw std::runtime_error("Vulkan device lacks independentBlend");
        float priority = 1;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = c.family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        VkPhysicalDeviceFeatures features{};
        features.independentBlend = VK_TRUE;
        features.samplerAnisotropy = available.samplerAnisotropy;
        c.sampler_anisotropy = available.samplerAnisotropy != VK_FALSE;
        const char *swapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device.queueCreateInfoCount = 1;
        device.pQueueCreateInfos = &queue;
        device.pEnabledFeatures = &features;
        device.enabledExtensionCount = 1;
        device.ppEnabledExtensionNames = &swapchain;
        check(vkCreateDevice(c.physical, &device, nullptr, &c.device), "vkCreateDevice");
        volkLoadDevice(c.device);
        vkGetDeviceQueue(c.device, c.family, 0, &c.queue);
        std::array<VkDescriptorSetLayoutBinding, 9> bindings{};
        for (unsigned i = 0; i < bindings.size(); i++) {
            bindings[i].binding = i;
            bindings[i].descriptorCount = 1;
            bindings[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                           : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout.bindingCount = (uint32_t)bindings.size();
        layout.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(c.device, &layout, nullptr, &c.descriptors),
              "vkCreateDescriptorSetLayout");
        c.open_pipeline_cache();
        if (getenv("MOON_VK_PROFILE_PASSES") && c.timestamp_bits) {
            VkQueryPoolCreateInfo query{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            query.queryType = VK_QUERY_TYPE_TIMESTAMP;
            query.queryCount = 4096;
            check(vkCreateQueryPool(c.device, &query, nullptr, &c.pass_queries), "pass timestamps");
        }
        c.open_submission();
        c.swap_submission();
        c.open_submission();
        c.white = c.image(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
        uint8_t white[4] = {255, 255, 255, 255};
        c.upload(*c.white, 0, 0, 0, 1, 1, GL_RGBA, white);
        c.flush();
        fprintf(stderr, "Vulkan renderer: %s\n", c.properties.deviceName);
        return 1;
    } catch (const std::exception &error) {
        SDL_SetError("Vulkan: %s", error.what());
        fprintf(stderr, "Vulkan: %s\n", error.what());
        c.close();
        active.reset();
        return 0;
    }
}
extern "C" void moon_vk_render_close(void)
{
    if (moon::active) {
        moon::active->close();
        moon::active.reset();
    }
}
extern "C" void moon_vk_render_size(SDL_Window *window, int *width, int *height)
{
    SDL_Vulkan_GetDrawableSize(window, width, height);
}
extern "C" void moon_vk_render_present(void)
{
    if (!moon::active)
        return;
    try {
        moon::context().present();
        moon::context().error.clear();
    } catch (const std::exception &e) {
        /* A frame that cannot be presented is skipped, not fatal. The log line
           says why; the app keeps running so the next frame can recover. */
        auto &c = moon::context();
        if (c.error != e.what()) {
            c.error = e.what();
            SDL_SetError("Vulkan presentation: %s", e.what());
            fprintf(stderr, "vulkan: presentation skipped: %s\n", e.what());
        }
    }
}
extern "C" void moon_vk_render_surface_dirty(void)
{
    if (moon::active)
        moon::active->rebuild_swapchain = true;
}
extern "C" void moon_vk_render_surface_lost(void)
{
    if (moon::active) {
        moon::active->surface_lost = true;
        moon::active->rebuild_swapchain = true;
    }
}
extern "C" int moon_vk_render_vsync(int enabled)
{
    auto &c = moon::context();
    c.vsync = enabled != 0;
    c.rebuild_swapchain = true;
    return 1;
}
