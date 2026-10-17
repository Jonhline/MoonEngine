/* Shader and vertex-layout bindings persist across draws on one command queue.
   Invalidate at frame start; targets, clears and transfers do not alter these
   bindings. Mutable material/depth/blend/uniform/texture state is never cached. */
#ifndef MOON_DEKO_PIPELINE_CACHE_H
#define MOON_DEKO_PIPELINE_CACHE_H
#include <deko3d.h>
struct nd_pipeline_cache {
    const DkShader *vertex = nullptr, *fragment = nullptr;
    unsigned layout = 0;
};
static inline void nd_bind_pipeline_commands(nd_pipeline_cache &cache, DkCmdBuf cmd,
                                             const DkShader *const shaders[2], bool mesh)
{
    if (cache.vertex != shaders[0] || cache.fragment != shaders[1]) {
        dkCmdBufBindShaders(cmd, DkStageFlag_GraphicsMask, shaders, 2);
        cache.vertex = shaders[0];
        cache.fragment = shaders[1];
    }
    unsigned layout = mesh ? 2 : 1;
    if (cache.layout == layout)
        return;
    DkVtxAttribState attributes[9]{};
    DkVtxBufferState buffer{};
    if (mesh) {
        const unsigned offsets[]{0, 12, 24, 32, 44, 56, 68, 76, 92};
        const DkVtxAttribSize sizes[]{
            DkVtxAttribSize_3x32, DkVtxAttribSize_3x32, DkVtxAttribSize_2x32,
            DkVtxAttribSize_3x32, DkVtxAttribSize_3x32, DkVtxAttribSize_3x32,
            DkVtxAttribSize_2x32, DkVtxAttribSize_4x32, DkVtxAttribSize_4x32};
        for (unsigned i = 0; i < 9; i++) {
            attributes[i].offset = offsets[i];
            attributes[i].size = sizes[i];
            attributes[i].type = DkVtxAttribType_Float;
        }
        buffer.stride = 108;
    } else {
        attributes[0].size = DkVtxAttribSize_2x32;
        attributes[0].type = DkVtxAttribType_Float;
        attributes[1] = attributes[0];
        attributes[1].offset = 8;
        attributes[2].offset = 16;
        attributes[2].size = DkVtxAttribSize_4x32;
        attributes[2].type = DkVtxAttribType_Float;
        buffer.stride = 32;
    }
    dkCmdBufBindVtxAttribState(cmd, attributes, mesh ? 9 : 3);
    dkCmdBufBindVtxBufferState(cmd, &buffer, 1);
    cache.layout = layout;
}
#endif
