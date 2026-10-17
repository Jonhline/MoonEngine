/* Command-state equivalence across repeated draws, layout/program changes and
   frame resets. No GPU pixel-equivalence claim is made by this host fixture. */
#include "../../src/platform/switch/deko_pipeline_cache.h"
#include <cstdio>
#include <cstring>
static const DkShader *bound_vertex, *bound_fragment;
static DkVtxAttribState bound_attributes[9];
static unsigned bound_count, bound_stride, shader_calls, layout_calls;
extern "C" void dkCmdBufBindShaders(DkCmdBuf, uint32_t, const DkShader *const shaders[],
                                    uint32_t count)
{
    if (count == 2) {
        bound_vertex = shaders[0];
        bound_fragment = shaders[1];
        shader_calls++;
    }
}
extern "C" void dkCmdBufBindVtxAttribState(DkCmdBuf, const DkVtxAttribState *attributes,
                                           uint32_t count)
{
    bound_count = count;
    std::memcpy(bound_attributes, attributes, count * sizeof *attributes);
    layout_calls++;
}
extern "C" void dkCmdBufBindVtxBufferState(DkCmdBuf, const DkVtxBufferState *buffers,
                                           uint32_t count)
{
    if (count == 1)
        bound_stride = buffers[0].stride;
}
int main(int argc, char **argv)
{
    const char *negative = argc > 1 ? argv[1] : "";
    DkShader vertex[2]{}, fragment[3]{};
    unsigned draws = 0;
    for (unsigned frame = 0; frame < 4; frame++) {
        nd_pipeline_cache cache;
        bound_vertex = bound_fragment = nullptr;
        bound_count = bound_stride = 0;
        if (!std::strcmp(negative, "--reset-negative"))
            cache = {&vertex[0], &fragment[0], 1};
        for (unsigned draw = 0; draw < 2000; draw++) {
            bool mesh = (draw / 200) % 2 != 0;
            const DkShader *shaders[]{&vertex[mesh], &fragment[(draw / 100) % 3]};
            if (!std::strcmp(negative, "--shader-negative")) {
                cache.vertex = shaders[0];
                cache.fragment = shaders[1];
            }
            if (!std::strcmp(negative, "--layout-negative"))
                cache.layout = mesh ? 2 : 1;
            nd_bind_pipeline_commands(cache, nullptr, shaders, mesh);
            if (bound_vertex != shaders[0] || bound_fragment != shaders[1] ||
                bound_count != (mesh ? 9u : 3u) || bound_stride != (mesh ? 108u : 32u)) {
                std::fprintf(stderr, "binding mismatch frame=%u draw=%u count=%u stride=%u\n",
                             frame, draw, bound_count, bound_stride);
                return 1;
            }
            const unsigned offsets[]{0, 12, 24, 32, 44, 56, 68, 76, 92};
            const DkVtxAttribSize sizes[]{
                DkVtxAttribSize_3x32, DkVtxAttribSize_3x32, DkVtxAttribSize_2x32,
                DkVtxAttribSize_3x32, DkVtxAttribSize_3x32, DkVtxAttribSize_3x32,
                DkVtxAttribSize_2x32, DkVtxAttribSize_4x32, DkVtxAttribSize_4x32};
            for (unsigned i = 0; i < bound_count; i++) {
                unsigned offset = mesh ? offsets[i] : i * 8;
                DkVtxAttribSize size = mesh     ? sizes[i]
                                       : i == 2 ? DkVtxAttribSize_4x32
                                                : DkVtxAttribSize_2x32;
                if (bound_attributes[i].offset != offset || bound_attributes[i].size != size ||
                    (static_cast<unsigned>(bound_attributes[i].type) & 7u) !=
                        DkVtxAttribType_Float) {
                    std::fprintf(
                        stderr,
                        "attribute mismatch frame=%u draw=%u attribute=%u offset=%u expected=%u\n",
                        frame, draw, i, bound_attributes[i].offset, offset);
                    return 1;
                }
            }
            draws++;
        }
    }
    if (shader_calls != 80 || layout_calls != 40) {
        std::fprintf(stderr, "bind counts shaders=%u layouts=%u\n", shader_calls, layout_calls);
        return 1;
    }
    std::printf("%u draws: shader binds=%u layout binds=%u, state equivalence PASS\n", draws,
                shader_calls, layout_calls);
    return 0;
}
