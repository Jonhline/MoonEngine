#include "vk_internal.hpp"
#include <algorithm>
#include <cstdio>

namespace moon {
void Context::open_submission()
{
    VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    info.queueFamilyIndex = family;
    check(vkCreateCommandPool(device, &info, nullptr, &pool), "frame command pool");
    VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commands.commandPool = pool;
    commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commands.commandBufferCount = 1;
    check(vkAllocateCommandBuffers(device, &commands, &command), "frame command buffer");
    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    check(vkCreateFence(device, &fence_info, nullptr, &fence), "frame fence");
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    check(vkCreateSemaphore(device, &semaphore, nullptr, &acquired), "frame acquire semaphore");
    VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 8192},
                                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 65536}};
    VkDescriptorPoolCreateInfo descriptors_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptors_info.maxSets = 8192;
    descriptors_info.poolSizeCount = 2;
    descriptors_info.pPoolSizes = sizes;
    check(vkCreateDescriptorPool(device, &descriptors_info, nullptr, &descriptor_pool),
          "frame descriptor pool");
    arena = buffer(128ull * 1024 * 1024,
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (timestamp_bits) {
        VkQueryPoolCreateInfo query{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        query.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query.queryCount = 2;
        check(vkCreateQueryPool(device, &query, nullptr, &queries), "frame timestamps");
    }
}
void Context::swap_submission()
{
    std::swap(pool, spare.pool);
    std::swap(command, spare.command);
    std::swap(fence, spare.fence);
    std::swap(acquired, spare.acquired);
    std::swap(descriptor_pool, spare.descriptor_pool);
    std::swap(queries, spare.queries);
    std::swap(arena, spare.arena);
    std::swap(transient_targets, spare.transient_targets);
    std::swap(retired_images, spare.retired_images);
    std::swap(retired_buffers, spare.retired_buffers);
    std::swap(draw_sets, spare.draw_sets);
    last_set = VK_NULL_HANDLE;
    std::swap(pending, spare.pending);
    std::swap(timing_written, spare.timing_written);
}
void Context::write_timestamp_end()
{
    pass_mark("end");
    if (queries) {
        vkCmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queries, 1);
        timing_written = true;
    }
}
void Context::drain()
{
    flush();
    if (pending)
        reset_submission();
    if (spare.pending) {
        swap_submission();
        reset_submission();
        swap_submission();
    }
}
void Context::close_submission()
{
    for (auto framebuffer : transient_targets)
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    transient_targets.clear();
    retired_images.clear();
    for (auto &item : retired_buffers)
        destroy(item);
    retired_buffers.clear();
    draw_sets.clear();
    last_set = VK_NULL_HANDLE;
    destroy(arena);
    if (queries)
        vkDestroyQueryPool(device, queries, nullptr);
    if (descriptor_pool)
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
    if (fence)
        vkDestroyFence(device, fence, nullptr);
    if (pool)
        vkDestroyCommandPool(device, pool, nullptr);
    if (acquired)
        vkDestroySemaphore(device, acquired, nullptr);
}
} // namespace moon
extern "C" void moon_vk_render_drain(void)
{
    if (moon_vk_render_active())
        moon::context().drain();
}
extern "C" void moon_vk_render_pass_report(void)
{
    if (!moon_vk_render_active())
        return;
    auto &c = moon::context();
    if (!c.pass_frames)
        return;
    std::vector<std::pair<double, std::string>> rows;
    double total = 0;
    for (auto &e : c.pass_totals) {
        rows.push_back({e.second.first, e.first});
        total += e.second.first;
    }
    std::sort(rows.rbegin(), rows.rend());
    fprintf(stderr, "pass profile: %llu frames, %.3f ms/frame\n", (unsigned long long)c.pass_frames,
            total / 1e6 / c.pass_frames);
    for (auto &r : rows)
        fprintf(stderr, "  %8.3f ms %6.1f draws  %s\n", r.first / 1e6 / c.pass_frames,
                (double)c.pass_totals[r.second].second / c.pass_frames, r.second.c_str());
}
extern "C" void moon_vk_render_pass_reset(void)
{
    if (moon_vk_render_active()) {
        moon::context().pass_totals.clear();
        moon::context().pass_frames = 0;
    }
}
extern "C" void moon_vk_render_low_latency(int enabled)
{
    if (moon_vk_render_active()) {
        auto &context = moon::context();
        context.drain();
        context.low_latency = enabled != 0;
    }
}

extern "C" int moon_vk_render_prepare_geometry(void)
{
    if (!moon_vk_render_active() || getenv("MOON_VK_SKIP_PREPARE"))
        return 1;
    try {
        auto &context = moon::context();
        size_t before = context.vertex_buffers.size();
        for (auto &buffer : context.static_buffers)
            if (buffer.second && !context.buffers[buffer.first].empty())
                context.vertex_buffer(buffer.first);
        context.flush();
        fprintf(stderr, "Vulkan geometry preparation: %zu buffers\n",
                context.vertex_buffers.size() - before);
        return 1;
    } catch (const std::exception &error) {
        SDL_SetError("Vulkan geometry preparation: %s", error.what());
        return 0;
    }
}
