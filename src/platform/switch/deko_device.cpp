/* Native Switch command/resource ownership. No OpenGL context or translation layer. */
#include "deko_device.h"
#include "deko_shader_blob.h"
#include "deko_transfer.h"
#include "deko_clear.h"
#include "deko_blend_commands.h"
#include "deko_shaders.h"
#include "deko_draw_range.h"
#include "deko_submission.h"
#include "deko_pipeline_cache.h"
#include <switch.h>
extern "C" {
#include "platform/switch_profile.h"
}
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
constexpr unsigned Frames = 3, Images = 8192, StreamBytes = 16 * 1024 * 1024;
struct Frame {
    DkCmdBuf commands{};
    nd_buffer command_memory{};
    std::vector<nd_buffer> extra_commands, streams;
    unsigned stream_index{}, used{};
    DkFence fence{};
    bool fenced{};
    unsigned draws{}, uploads{}, last_program{}, last_first{}, last_count{}, last_bytes{};
    unsigned target_width{}, target_height{};
    DkGpuAddr last_vertices{};
    size_t upload_bytes{};
    nd_submission pending{};
    unsigned submissions{};
};
struct Retired {
    nd_buffer memory;
    unsigned slot;
    uint64_t after;
};
struct Shader {
    DkShader object;
    nd_buffer memory;
};
struct Context {
    nd_pipeline_cache pipeline;
    DkDevice device{};
    DkQueue queue{};
    DkSwapchain swapchain{};
    nd_image *screens[Frames]{}, *color{}, *depth{};
    nd_buffer descriptors{};
    DkImageDescriptor *image_descriptors{};
    Shader ui_vert{}, ui_frag{}, mesh_vert{}, mesh_frag{}, mix_frag{}, env_frag{}, snapshot_frag{},
        mask_frag{};
    Shader probe_first_vert{}, probe_first_frag{}, probe_broken_vert{};
    Frame frames[Frames];
    std::vector<Retired> retired;
    std::vector<unsigned> free_images;
    uint64_t serial{};
    nd_image *images[Images]{};
    unsigned width{}, height{}, frame{}, next_image{1};
    int acquired{-1};
#ifdef MOON_DEKO_DIAGNOSTICS
    unsigned allocation_calls{}, allocation_size{}, allocation_flags{};
    uint64_t allocation_bytes{};
#endif
    bool active{};
} gpu;
uint32_t align_up(uint32_t n, uint32_t align)
{
    return (n + align - 1) & ~(align - 1);
}
void fault_context()
{
    std::fprintf(stderr, "Switch GPU fault context: serial=%llu slot=%u acquired=%d active=%d\n",
                 (unsigned long long)gpu.serial, gpu.frame, gpu.acquired, gpu.active);
#ifdef MOON_DEKO_DIAGNOSTICS
    uint64_t used = 0, available = 0;
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    std::fprintf(
        stderr,
        "Switch GPU allocation: calls=%u last_size=%u flags=%x requested_bytes=%llu process_used=%llu process_total=%llu\n",
        gpu.allocation_calls, gpu.allocation_size, gpu.allocation_flags,
        (unsigned long long)gpu.allocation_bytes, (unsigned long long)used,
        (unsigned long long)available);
#endif
    for (unsigned i = 0; i < Frames; i++) {
        const auto &f = gpu.frames[i];
        std::fprintf(stderr,
                     "Switch GPU frame[%u]: draws=%u uploads=%u upload_bytes=%llu streams=%zu "
                     "extra_commands=%zu last_program=%u first=%u count=%u vertex_bytes=%u "
                     "vertex_address=%llx target=%ux%u chunks=%u pending_transfers=%u\n",
                     i, f.draws, f.uploads, (unsigned long long)f.upload_bytes, f.streams.size(),
                     f.extra_commands.size(), f.last_program, f.last_first, f.last_count,
                     f.last_bytes, (unsigned long long)f.last_vertices, f.target_width,
                     f.target_height, f.submissions, f.pending.transfers);
    }
    std::fflush(stderr);
}
void fail(const char *what)
{
    std::fprintf(stderr, "deko3d fatal: %s\n", what);
    fault_context();
    std::fflush(stderr);
    std::abort();
}
void debug_message(void *, const char *context, DkResult result, const char *message)
{
    std::fprintf(stderr, "deko3d: %s result=%u: %s\n", context ? context : "unknown",
                 (unsigned)result, message ? message : "");
    std::fflush(stderr);
    /* The SDK debug archive reports the underlying NvError before the generic
       flush failure. Do not return into its default interactive error dialog. */
    if (message && std::strstr(message, "entered error state"))
        fault_context();
    if (result != DkResult_Success) {
        fault_context();
        std::abort();
    }
}
void boot_step(const char *step)
{
    std::fprintf(stderr, "Switch deko3d init: %s\n", step);
    std::fflush(stderr);
}
void collect(bool all = false)
{
    auto &list = gpu.retired;
    for (auto i = list.begin(); i != list.end();) {
        if (all || i->after <= gpu.serial) {
            if (i->memory.mem)
                dkMemBlockDestroy(i->memory.mem);
            if (i->slot)
                gpu.free_images.push_back(i->slot);
            i = list.erase(i);
        } else
            ++i;
    }
}
void command_memory(void *, DkCmdBuf cmd, size_t minimum)
{
    auto memory = nd_alloc(align_up(std::max<size_t>(minimum, 1024 * 1024), DK_MEMBLOCK_ALIGNMENT),
                           DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
    gpu.frames[gpu.frame].extra_commands.push_back(memory);
    dkCmdBufAddMemory(cmd, memory.mem, 0, memory.size);
}
Shader shader(const unsigned char *bytes, unsigned length)
{
    nd_shader_blob blob;
    if (!nd_shader_blob_copy(&blob, bytes, length))
        fail("invalid compiled shader or control allocation failed");
    Shader result{};
    /* deko3d dk_shader.cpp: code may not occupy the last 0x400 bytes of its block. */
    result.memory =
        nd_alloc(blob.code_size + DK_SHADER_CODE_UNUSABLE_SIZE,
                 DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code);
    std::memcpy(nd_cpu(result.memory), blob.code, blob.code_size);
    DkShaderMaker maker;
    dkShaderMakerDefaults(&maker, result.memory.mem, 0);
    /* dkShaderInitialize patches entrypoint/constbuf1_off in its input before
       copying the program header. Never hand it our embedded read-only array.
       devkitPro's SampleFramework/CShader.cpp also frees the control afterward. */
    maker.control = blob.control;
    dkShaderInitialize(&result.object, &maker);
    std::free(blob.control);
    if (!dkShaderIsValid(&result.object))
        fail("shader initialization failed");
    return result;
}
} // namespace
/* Preserve the actual libnx Result which the SDK otherwise replaces with
   DkResult_Fail. This wrapper observes failure only; it never retries or edits
   the channel, entries or fence increments. GNU ld --wrap binds the SDK call. */
extern "C" Result __real_nvGpuChannelKickoff(NvGpuChannel *channel);
extern "C" Result __wrap_nvGpuChannelKickoff(NvGpuChannel *channel)
{
    unsigned entries = channel->num_entries, increments = channel->fence_incr;
    Result result = __real_nvGpuChannelKickoff(channel);
    if (R_FAILED(result)) {
        std::fprintf(stderr,
                     "Switch GPU kickoff rejected: result=%08x module=%u description=%u "
                     "entries=%u fence_increments=%u\n",
                     result, R_MODULE(result), R_DESCRIPTION(result), entries, increments);
        fault_context();
    }
    return result;
}
nd_buffer nd_alloc(uint32_t size, uint32_t flags)
{
#ifdef MOON_DEKO_DIAGNOSTICS
    gpu.allocation_calls++;
    gpu.allocation_size = size;
    gpu.allocation_flags = flags;
    gpu.allocation_bytes += align_up(size, DK_MEMBLOCK_ALIGNMENT);
#endif
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker, gpu.device, align_up(size, DK_MEMBLOCK_ALIGNMENT));
    maker.flags = flags;
    nd_buffer result{dkMemBlockCreate(&maker), align_up(size, DK_MEMBLOCK_ALIGNMENT)};
    if (!result.mem)
        fail("GPU memory allocation failed");
    return result;
}
void *nd_cpu(nd_buffer m)
{
    return dkMemBlockGetCpuAddr(m.mem);
}
DkGpuAddr nd_gpu(nd_buffer m)
{
    return dkMemBlockGetGpuAddr(m.mem);
}
void nd_release(nd_buffer m)
{
    if (m.mem)
        gpu.retired.push_back({m, 0, gpu.serial + Frames});
}
nd_image *nd_image_create(unsigned w, unsigned h, bool depth, bool present, bool alpha_mask)
{
    auto *image = new nd_image{};
    image->width = w;
    image->height = h;
    image->depth = depth;
    image->alpha_mask = alpha_mask;
    DkImageLayoutMaker maker;
    dkImageLayoutMakerDefaults(&maker, gpu.device);
    maker.dimensions[0] = w;
    maker.dimensions[1] = h;
    maker.format = depth        ? DkImageFormat_ZF32
                   : alpha_mask ? DkImageFormat_R8_Unorm
                                : DkImageFormat_RGBA8_Unorm;
    /* SDK 0.5 ImageInfo::Transfer2D requires this for the GPU blits used by
       menu captures, layers and presentation. Depth uses the copy engine. */
    maker.flags = DkImageFlags_UsageRender |
                  (depth || alpha_mask ? 0 : DkImageFlags_Usage2DEngine) |
                  (present ? DkImageFlags_UsagePresent : 0);
    DkImageLayout layout;
    dkImageLayoutInitialize(&layout, &maker);
    image->storage =
        nd_alloc(dkImageLayoutGetSize(&layout), DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image);
    dkImageInitialize(&image->image, &layout, image->storage.mem, 0);
    dkImageViewDefaults(&image->view, &image->image);
    if (gpu.free_images.empty())
        image->slot = gpu.next_image++;
    else {
        image->slot = gpu.free_images.back();
        gpu.free_images.pop_back();
    }
    if (image->slot >= Images)
        fail("texture descriptor capacity exceeded");
    gpu.images[image->slot] = image;
    dkImageDescriptorInitialize(gpu.image_descriptors + image->slot, &image->view, false, false);
    if (gpu.active)
        dkCmdBufBarrier(nd_commands(), DkBarrier_None, DkInvalidateFlags_Descriptors);
    return image;
}
void nd_image_destroy(nd_image *image)
{
    if (!image)
        return;
    gpu.images[image->slot] = nullptr;
    gpu.retired.push_back({image->storage, image->slot, gpu.serial + Frames});
    delete image;
}
DkCmdBuf nd_commands()
{
    nd_begin();
    return gpu.frames[gpu.frame].commands;
}
DkGpuAddr nd_stream(const void *bytes, size_t size, unsigned alignment)
{
    nd_begin();
    auto &frame = gpu.frames[gpu.frame];
    unsigned offset = align_up(frame.used, alignment);
    if (frame.stream_index >= frame.streams.size())
        frame.streams.push_back(nd_alloc(std::max<size_t>(StreamBytes, size + alignment),
                                         DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached));
    if (offset + size > frame.streams[frame.stream_index].size) {
        frame.stream_index++;
        frame.used = 0;
        return nd_stream(bytes, size, alignment);
    }
    nd_buffer memory = frame.streams[frame.stream_index];
    if (bytes)
        std::memcpy(static_cast<unsigned char *>(nd_cpu(memory)) + offset, bytes, size);
    frame.used = offset + size;
    return nd_gpu(memory) + offset;
}
void nd_image_upload(nd_image *image, const uint8_t *rgba)
{
    if (!image || !rgba)
        return;
    if (image->alpha_mask)
        fail("RGBA upload to scalar snapshot image");
    DkCopyBuf from{nd_stream(rgba, (size_t)image->width * image->height * 4), 0, 0};
    auto &frame = gpu.frames[gpu.frame];
    frame.uploads++;
    frame.upload_bytes += (size_t)image->width * image->height * 4;
    DkImageRect rect{0, 0, 0, image->width, image->height, 1};
    nd_upload_commands(nd_commands(), &from, &image->view, &rect);
    if (nd_submit_transfer(frame.pending, gpu.queue, frame.commands,
                           (size_t)image->width * image->height * 4))
        frame.submissions++;
}
void nd_sync()
{
    if (!gpu.active)
        return;
    auto &frame = gpu.frames[gpu.frame];
    nd_submit_pending(frame.pending, gpu.queue, nd_commands());
    dkQueueWaitIdle(gpu.queue);
}
bool nd_image_read(nd_image *image, void *bytes)
{
    if (!image || !bytes)
        return false;
    size_t bytes_count = (size_t)image->width * image->height * (image->alpha_mask ? 1 : 4);
    auto staging = nd_alloc(bytes_count, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
    DkCopyBuf to{nd_gpu(staging), 0, 0};
    DkImageRect rect{0, 0, 0, image->width, image->height, 1};
    nd_transfer_begin(nd_commands());
    dkCmdBufCopyImageToBuffer(nd_commands(), &image->view, &rect, &to, 0);
    nd_sync();
    std::memcpy(bytes, nd_cpu(staging), bytes_count);
    dkMemBlockDestroy(staging.mem);
    return true;
}
void nd_copy(nd_image *src, nd_image *dst, unsigned x, unsigned y, unsigned w, unsigned h)
{
    if (!src || !dst || !w || !h)
        return;
    SWP_PUSH(SWP_GPU_COPY);
    DkImageRect a{x, y, 0, w, h, 1}, b{0, 0, 0, dst->width, dst->height, 1};
    nd_blit_commands(nd_commands(), &src->view, &a, &dst->view, &b);
    auto &frame = gpu.frames[gpu.frame];
    if (nd_submit_transfer(frame.pending, gpu.queue, frame.commands, 0))
        frame.submissions++;
    SWP_POP();
}
void nd_snapshot(nd_image *src, nd_image *dst, unsigned x, unsigned y, unsigned width,
                 unsigned height)
{
    if (!src || !dst || !width || src->width != dst->width || src->height != dst->height ||
        x > src->width || width > src->width - x || !height || y > src->height ||
        height > src->height - y)
        fail("invalid framebuffer snapshot span");
    SWP_PUSH(SWP_GPU_COPY);
    DkImageRect rect{x, y, 0, width, height, 1};
    /* Same framebuffer coordinates on both sides: the fragment shader still
       addresses DATE with gl_FragCoord, without scaling or a UV adjustment. */
    nd_snapshot_commands(nd_commands(), &src->view, &dst->view, &rect);
    auto &frame = gpu.frames[gpu.frame];
    if (nd_submit_transfer(frame.pending, gpu.queue, frame.commands, 0))
        frame.submissions++;
    SWP_POP();
}
void nd_bind(nd_image *color, nd_image *depth)
{
    nd_begin();
    if (gpu.color != color)
        dkCmdBufBarrier(nd_commands(), DkBarrier_Fragments, DkInvalidateFlags_Image);
    gpu.color = color ? color : nd_screen();
    gpu.depth = depth;
    dkCmdBufBindRenderTarget(nd_commands(), &gpu.color->view, depth ? &depth->view : nullptr);
}
void nd_viewport(int x, int y, int w, int h)
{
    DkViewport viewport{(float)x, (float)y, (float)w, (float)h, 0, 1};
    dkCmdBufSetViewports(nd_commands(), 0, &viewport, 1);
}
void nd_scissor(int x, int y, int w, int h)
{
    int right = std::min<int>(gpu.color->width, x + w),
        bottom = std::min<int>(gpu.color->height, y + h);
    x = std::max(0, x);
    y = std::max(0, y);
    DkScissor clip{(unsigned)x, (unsigned)y, (unsigned)std::max(0, right - x),
                   (unsigned)std::max(0, bottom - y)};
    dkCmdBufSetScissors(nd_commands(), 0, &clip, 1);
}
void nd_clear(float r, float g, float b, float a, bool color, bool depth)
{
    if (color) {
        float values[4]{r, g, b, a};
        nd_clear_color_commands(nd_commands(), DkColorMask_RGBA, values);
    }
    if (depth && gpu.depth)
        nd_clear_depth_commands(nd_commands());
}
void nd_begin()
{
    if (gpu.active)
        return;
    auto &frame = gpu.frames[gpu.frame];
    SWP_PUSH(SWP_GPU_FENCE);
    if (frame.fenced && dkFenceWait(&frame.fence, UINT64_MAX) != DkResult_Success)
        fail("frame fence failed; retaining in-flight buffers");
    SWP_POP();
    dkCmdBufClear(frame.commands);
    for (auto memory : frame.extra_commands)
        dkMemBlockDestroy(memory.mem);
    frame.extra_commands.clear();
    dkCmdBufAddMemory(frame.commands, frame.command_memory.mem, 0, frame.command_memory.size);
    /* Loading can consume many staging blocks. Retire extras after this
       frame slot's fence; retain only its normal streaming block. */
    for (size_t i = 1; i < frame.streams.size(); i++)
        dkMemBlockDestroy(frame.streams[i].mem);
    if (frame.streams.size() > 1)
        frame.streams.resize(1);
    frame.used = frame.stream_index = 0;
    frame.draws = frame.uploads = 0;
    frame.upload_bytes = 0;
    frame.pending = {};
    frame.submissions = 0;
    collect();
    gpu.pipeline = {};
    gpu.active = true;
    SWP_PUSH(SWP_GPU_ACQUIRE);
    gpu.acquired = dkQueueAcquireImage(gpu.queue, gpu.swapchain);
    SWP_POP();
    if (gpu.acquired < 0)
        fail("swapchain acquire failed");
    dkCmdBufBindImageDescriptorSet(frame.commands, nd_gpu(gpu.descriptors), Images);
    dkCmdBufBindSamplerDescriptorSet(
        frame.commands, nd_gpu(gpu.descriptors) + Images * sizeof(DkImageDescriptor), 4);
    gpu.color = gpu.screens[gpu.acquired];
    gpu.depth = nullptr;
    dkCmdBufBindRenderTarget(frame.commands, &gpu.color->view, nullptr);
}
nd_image *nd_screen()
{
    return gpu.screens[gpu.acquired >= 0 ? gpu.acquired : 0];
}
void nd_present()
{
    if (!gpu.active)
        return;
    auto &frame = gpu.frames[gpu.frame];
    dkCmdBufSignalFence(frame.commands, &frame.fence, true);
    dkQueueSubmitCommands(gpu.queue, dkCmdBufFinishList(frame.commands));
    frame.pending = {};
    dkQueuePresentImage(gpu.queue, gpu.swapchain, gpu.acquired);
    frame.fenced = true;
    gpu.active = false;
    gpu.serial++;
    gpu.frame = gpu.serial % Frames;
}
nd_image *nd_image_slot(unsigned slot)
{
    return slot < Images ? gpu.images[slot] : nullptr;
}
unsigned nd_program_count()
{
    return gpu.device ? 6 : 0;
}
void nd_pipeline(nd_program program, const nd_uniform &uniforms, nd_buffer vertices, unsigned first,
                 unsigned count, const unsigned images[4], const DkRasterizerState &raster,
                 const DkDepthStencilState &depth, const DkColorState &color,
                 const DkColorWriteState &mask, const DkBlendState &blend, float fixed_alpha,
                 unsigned sampler, DkGpuAddr vertex_address, unsigned probe_flags,
                 DkGpuAddr index_address, unsigned index_count)
{
    SWP_PUSH(SWP_GL);
    DkCmdBuf cmd = nd_commands();
    auto &frame = gpu.frames[gpu.frame];
    frame.draws++;
    frame.last_program = program;
    frame.last_first = first;
    frame.last_count = count;
    frame.last_bytes = vertices.size;
    frame.last_vertices = vertex_address ? vertex_address : nd_gpu(vertices);
    frame.target_width = gpu.color->width;
    frame.target_height = gpu.color->height;
    if (index_address) {
        if (program != ND_MESH || first > index_count || count > index_count - first)
            fail("draw outside index buffer");
    } else if (!nd_draw_range_valid(vertices.size, program == ND_MESH ? 108 : 32, first, count))
        fail("draw outside vertex buffer");
    DkShader const *shaders[2]{program == ND_MESH ? &gpu.mesh_vert.object : &gpu.ui_vert.object,
                               program == ND_MESH       ? &gpu.mesh_frag.object
                               : program == ND_MASK     ? &gpu.mask_frag.object
                               : program == ND_SNAPSHOT ? &gpu.snapshot_frag.object
                               : program == ND_ENV      ? &gpu.env_frag.object
                               : program == ND_MIX      ? &gpu.mix_frag.object
                                                        : &gpu.ui_frag.object};
    /* Only the standalone regression probe supplies flags. Production always
       uses zero. Load the exact 7715ae8 shader sources lazily for a same-GPU
       comparison, without replacing the player executable or its saves. */
    if (program == ND_MESH && (probe_flags & 19)) {
        if (!dkShaderIsValid(&gpu.probe_first_vert.object)) {
            gpu.probe_first_vert =
                shader(nd_probe_first_mesh_vert, sizeof nd_probe_first_mesh_vert);
            gpu.probe_first_frag =
                shader(nd_probe_first_mesh_frag, sizeof nd_probe_first_mesh_frag);
        }
        if (probe_flags & 1)
            shaders[0] = &gpu.probe_first_vert.object;
        if (probe_flags & 2)
            shaders[1] = &gpu.probe_first_frag.object;
        if (probe_flags & 16) {
            if (!dkShaderIsValid(&gpu.probe_broken_vert.object))
                gpu.probe_broken_vert =
                    shader(nd_probe_broken_mesh_vert, sizeof nd_probe_broken_mesh_vert);
            shaders[0] = &gpu.probe_broken_vert.object;
        }
    }
    nd_bind_pipeline_commands(gpu.pipeline, cmd, shaders, program == ND_MESH);
    dkCmdBufBindRasterizerState(cmd, &raster);
    dkCmdBufBindDepthStencilState(cmd, &depth);
    dkCmdBufBindColorState(cmd, &color);
    dkCmdBufBindColorWriteState(cmd, &mask);
    if (probe_flags & 4)
        dkCmdBufBindBlendState(cmd, 0, &blend);
    else
        nd_bind_blend_commands(cmd, &blend);
    if (probe_flags & 8) {
        /* Diagnostic override of the SDK MME colour-enable/write-mask path.
           NVIDIA clb197.h: LOGIC_OP=19C4, BLEND_ENABLE(0)=1360,
           ALPHA_TEST=12EC, COLOR_MASK(0)=1A00 (R/G/B/A at bits 0/4/8/12).
           No claim that the MME path is faulty; compare its actual pixels. */
        unsigned channels = mask.masks & 15;
        uint32_t words[]{0x20010671u,
                         0,
                         0x200104d8u,
                         color.blendEnableMask & 1u,
                         0x200104bbu,
                         0,
                         0x20010680u,
                         (channels & 1u) | ((channels & 2u) << 3) | ((channels & 4u) << 6) |
                             ((channels & 8u) << 9)};
        dkCmdBufReplayCmds(cmd, words, sizeof words / sizeof words[0]);
    }
    dkCmdBufSetBlendConst(cmd, 0, 0, 0, fixed_alpha);
    DkGpuAddr params = nd_stream(&uniforms, sizeof uniforms);
    unsigned param_size = align_up(sizeof uniforms, DK_UNIFORM_BUF_ALIGNMENT);
    dkCmdBufBindUniformBuffer(cmd, DkStage_Vertex, 0, params, param_size);
    dkCmdBufBindUniformBuffer(cmd, DkStage_Fragment, 0, params, param_size);
    dkCmdBufBindVtxBuffer(cmd, 0, vertex_address ? vertex_address : nd_gpu(vertices),
                          vertices.size);
    DkResHandle handles[4];
    for (unsigned i = 0; i < 4; i++)
        /* Only the surface uses TEX1 wrapping. Environment maps and saved
           framebuffer masks keep clamp-to-edge independently of that surface. */
        handles[i] = dkMakeTextureHandle(images[i], i == 0 ? sampler : 0);
    dkCmdBufBindTextures(cmd, DkStage_Fragment, 0, handles, 4);
    if (index_address) {
        dkCmdBufBindIdxBuffer(cmd, DkIdxFormat_Uint32, index_address);
        dkCmdBufDrawIndexed(cmd, DkPrimitive_Triangles, count, 1, first, 0, 0);
    } else
        dkCmdBufDraw(cmd, DkPrimitive_Triangles, count, 1, first, 0);
    SWP_POP();
}
#ifdef MOON_DEKO_DIAGNOSTICS
static bool upload_burst_probe()
{
    /* Synthetic loading burst based on log14's 592 uploads / about 60 MB.
       Validate real SDK partial lists before loading any disc or save data. */
    std::vector<nd_image *> images;
    std::vector<uint8_t> pixels(160 * 160 * 4), actual(pixels.size());
    for (unsigned i = 0; i < 592; i++) {
        for (size_t at = 0; at < pixels.size(); at += 4) {
            pixels[at] = i & 255;
            pixels[at + 1] = i >> 8;
            pixels[at + 2] = at / 4;
            pixels[at + 3] = 255 - pixels[at + 2];
        }
        auto *image = nd_image_create(160, 160);
        images.push_back(image);
        nd_image_upload(image, pixels.data());
    }
    bool ok = true;
    for (unsigned i : {0u, 31u, 32u, 255u, 591u}) {
        bool read = nd_image_read(images[i], actual.data());
        for (size_t at = 0; read && at < actual.size(); at += 4)
            read = actual[at] == (i & 255) && actual[at + 1] == (i >> 8) &&
                   actual[at + 2] == uint8_t(at / 4) &&
                   actual[at + 3] == uint8_t(255 - uint8_t(at / 4));
        ok = read && ok;
    }
    for (auto *image : images)
        nd_image_destroy(image);
    std::fprintf(stderr, "Switch deko3d selftest: upload-burst=%s images=592 bytes=60620800\n",
                 ok ? "PASS" : "FAIL");
    std::fflush(stderr);
    return ok;
}
#endif
int nd_init(unsigned width, unsigned height)
{
    boot_step("device");
    DkDeviceMaker device;
    dkDeviceMakerDefaults(&device);
    device.cbDebug = debug_message;
    gpu.device = dkDeviceCreate(&device);
    if (!gpu.device)
        return 0;
    DkQueueMaker queue;
    dkQueueMakerDefaults(&queue, gpu.device);
    queue.flags = DkQueueFlags_Graphics;
    boot_step("queue");
    gpu.queue = dkQueueCreate(&queue);
    if (!gpu.queue)
        fail("queue creation failed");
    gpu.width = width;
    gpu.height = height;
    gpu.descriptors = nd_alloc(Images * sizeof(DkImageDescriptor) + 4 * sizeof(DkSamplerDescriptor),
                               DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
    gpu.image_descriptors = static_cast<DkImageDescriptor *>(nd_cpu(gpu.descriptors));
    auto samplers = reinterpret_cast<DkSamplerDescriptor *>(gpu.image_descriptors + Images);
    for (unsigned i = 0; i < 4; i++) {
        DkSampler sampler;
        dkSamplerDefaults(&sampler);
        sampler.minFilter = sampler.magFilter = DkFilter_Linear;
        sampler.wrapMode[0] = (i & 1) ? DkWrapMode_Repeat : DkWrapMode_ClampToEdge;
        sampler.wrapMode[1] = (i & 2) ? DkWrapMode_Repeat : DkWrapMode_ClampToEdge;
        dkSamplerDescriptorInitialize(samplers + i, &sampler);
    }
    DkImage const *images[Frames];
    boot_step("framebuffers");
    for (unsigned i = 0; i < Frames; i++) {
        gpu.screens[i] = nd_image_create(width, height, false, true);
        images[i] = &gpu.screens[i]->image;
        auto &frame = gpu.frames[i];
        frame.command_memory =
            nd_alloc(2 * 1024 * 1024, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
        DkCmdBufMaker maker;
        dkCmdBufMakerDefaults(&maker, gpu.device);
        maker.cbAddMem = command_memory;
        frame.commands = dkCmdBufCreate(&maker);
        if (!frame.commands)
            fail("command buffer creation failed");
    }
    DkSwapchainMaker swap;
    NWindow *native = nwindowGetDefault();
    std::fprintf(stderr, "Switch deko3d init: NWindow valid=%d buffers=%llx\n",
                 nwindowIsValid(native), native ? (unsigned long long)native->slots_configured : 0);
    std::fflush(stderr);
    /* libnx nwindowSetDimensions/ConfigureBuffer reject an existing producer's
       configured buffers. Do not attempt to reuse an SDL/EGL-owned window. */
    if (!nwindowIsValid(native) || native->slots_configured)
        fail("native window already owned or invalid");
    dkSwapchainMakerDefaults(&swap, gpu.device, native, images, Frames);
    boot_step("swapchain");
    gpu.swapchain = dkSwapchainCreate(&swap);
    if (!gpu.swapchain)
        fail("swapchain creation failed");
    boot_step("ui vertex shader");
    gpu.ui_vert = shader(nd_ui_vert, sizeof nd_ui_vert);
    boot_step("ui fragment shader");
    gpu.ui_frag = shader(nd_ui_frag, sizeof nd_ui_frag);
    boot_step("mesh vertex shader");
    gpu.mesh_vert = shader(nd_mesh_vert, sizeof nd_mesh_vert);
    boot_step("mesh fragment shader");
    gpu.mesh_frag = shader(nd_mesh_frag, sizeof nd_mesh_frag);
    boot_step("mix fragment shader");
    gpu.mix_frag = shader(nd_mix_frag, sizeof nd_mix_frag);
    gpu.env_frag = shader(nd_env_frag, sizeof nd_env_frag);
    gpu.snapshot_frag = shader(nd_snapshot_frag, sizeof nd_snapshot_frag);
    gpu.mask_frag = shader(nd_mask_frag, sizeof nd_mask_frag);
    std::fprintf(stderr, "Switch renderer: deko3d native shaders=8 gl_context=0\n");
    std::fprintf(stderr, "Switch GPU diagnostics: draw-ranges=checked fault-context=3-frames\n");
    std::fprintf(stderr,
                 "Switch GPU submission: transfers=32 upload_bytes=4194304 raw-kickoff=1\n");
    std::fprintf(
        stderr,
        "Switch deko3d blend: per-target-separate-alpha=1 independent-dst-alpha=1 vertex-alpha=arithmetic-merge\n");
    std::fflush(stderr);
    boot_step("first acquire");
    nd_begin();
#ifdef MOON_DEKO_DIAGNOSTICS
    if (!upload_burst_probe())
        fail("partial submission upload burst did not preserve pixels");
#endif
    return 1;
}
void nd_shutdown()
{
    if (!gpu.device)
        return;
    nd_sync();
    dkQueueWaitIdle(gpu.queue);
    collect(true);
    dkSwapchainDestroy(gpu.swapchain);
    for (auto image : gpu.screens) {
        dkMemBlockDestroy(image->storage.mem);
        delete image;
    }
    for (auto &frame : gpu.frames) {
        dkCmdBufDestroy(frame.commands);
        dkMemBlockDestroy(frame.command_memory.mem);
        for (auto m : frame.extra_commands)
            dkMemBlockDestroy(m.mem);
        for (auto m : frame.streams)
            dkMemBlockDestroy(m.mem);
    }
    for (auto sh : {gpu.ui_vert, gpu.ui_frag, gpu.mesh_vert, gpu.mesh_frag, gpu.mix_frag,
                    gpu.env_frag, gpu.snapshot_frag, gpu.mask_frag})
        dkMemBlockDestroy(sh.memory.mem);
    for (auto sh : {gpu.probe_first_vert, gpu.probe_first_frag, gpu.probe_broken_vert})
        if (sh.memory.mem)
            dkMemBlockDestroy(sh.memory.mem);
    dkMemBlockDestroy(gpu.descriptors.mem);
    dkQueueDestroy(gpu.queue);
    dkDeviceDestroy(gpu.device);
    gpu = Context{};
}

#ifdef MOON_DEKO_DIAGNOSTICS
extern "C" Result __real_nvMapCreate(NvMap *, void *, u32, u32, NvKind, bool);
extern "C" Result __wrap_nvMapCreate(NvMap *map, void *address, u32 size, u32 alignment,
                                     NvKind kind, bool cached)
{
    Result result = __real_nvMapCreate(map, address, size, alignment, kind, cached);
    if (R_FAILED(result))
        std::fprintf(
            stderr,
            "Switch GPU nvMapCreate failed: result=%08x module=%u description=%u size=%u align=%u cpu=%p\n",
            result, R_MODULE(result), R_DESCRIPTION(result), size, alignment, address);
    return result;
}
extern "C" Result __real_nvAddressSpaceMap(NvAddressSpace *, u32, bool, NvKind, iova_t *);
extern "C" Result __wrap_nvAddressSpaceMap(NvAddressSpace *space, u32 handle, bool cached,
                                           NvKind kind, iova_t *address)
{
    Result result = __real_nvAddressSpaceMap(space, handle, cached, kind, address);
    if (R_FAILED(result))
        std::fprintf(
            stderr,
            "Switch GPU nvAddressSpaceMap failed: result=%08x module=%u description=%u handle=%u kind=%u\n",
            result, R_MODULE(result), R_DESCRIPTION(result), handle, (unsigned)kind);
    return result;
}
#endif
