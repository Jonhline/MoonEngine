/* Exercise native clear commands after read-only draws and stale DATE alpha.
   This is a state/command fixture, not a Maxwell clear implementation. */
#include "../../src/platform/switch/deko_clear.h"
#include <array>
#include <cstdio>
#include <cstring>

namespace {
std::array<float, 4> color;
float depth;
unsigned mask;
bool depth_write;
const char *negative;
} // namespace
extern "C" void dkCmdBufBindDepthStencilState(DkCmdBuf, const DkDepthStencilState *state)
{
    if (std::strcmp(negative, "--depth-negative") && state->depthTestEnable)
        depth_write = state->depthWriteEnable;
}
extern "C" void dkCmdBufClearDepthStencil(DkCmdBuf, bool clear, float value, uint8_t, uint8_t)
{
    if (clear && depth_write)
        depth = value;
}
extern "C" void dkCmdBufBindColorWriteState(DkCmdBuf, const DkColorWriteState *state)
{
    if (std::strcmp(negative, "--mask-negative"))
        mask = state->masks & 15;
}
extern "C" void dkCmdBufClearColor(DkCmdBuf, uint32_t target, uint32_t channels, const void *data)
{
    if (!std::strcmp(negative, "--alpha-negative"))
        channels &= ~DkColorMask_A;
    const auto *rgba = static_cast<const float *>(data);
    if (target != 0)
        return;
    for (unsigned c = 0; c < 4; c++)
        if (channels & mask & (1u << c))
            color[c] = rgba[c];
}
int main(int argc, char **argv)
{
    negative = argc > 1 ? argv[1] : "";
    unsigned cases = 0;
    for (unsigned writes = 0; writes < 2; writes++)
        for (unsigned channels = 0; channels < 16; channels++)
            for (unsigned alpha = 0; alpha < 3; alpha++) {
                color = {.15f, .6f, .25f, alpha * .5f};
                depth = .125f;
                mask = channels;
                depth_write = writes != 0;
                nd_begin_3d_commands(nullptr);
                if (color != std::array<float, 4>{.15f, .6f, .25f, 1} || depth != 1) {
                    std::fprintf(stderr,
                                 "scene clear failed: previous depth-write=%u mask=%x alpha=%u\n",
                                 writes, channels, alpha);
                    return 1;
                }
                const float rgba[4]{.2f, .3f, .4f, .5f};
                nd_clear_color_commands(nullptr, DkColorMask_RGBA, rgba);
                if (color != std::array<float, 4>{.2f, .3f, .4f, .5f})
                    return 1;
                cases++;
            }
    std::printf("%u prior depth-write/RGBA-mask/DATE states: scene clears PASS\n", cases);
    return 0;
}
