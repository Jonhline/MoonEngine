/* Maxwell blend commands for the native Switch target (no desktop changes). */
#ifndef OPENGT4_DEKO_BLEND_COMMANDS_H
#define OPENGT4_DEKO_BLEND_COMMANDS_H
#include <deko3d.h>

static inline uint32_t nd_blend_factor_command(unsigned value)
{
    /* DkBlendFactor stores D3D factors directly and marks the GL constant
       factors with bit 5; the Maxwell methods use C001..C004 for those. */
    value &= 63u;
    return (value & 31u) | ((value & 32u) ? 0xc000u : 0u);
}
static inline void nd_bind_blend_commands(DkCmdBuf cmd, const DkBlendState *blend)
{
    /* NVIDIA open-gpu-doc/classes/3d/clb197.h:
       SET_BLEND_PER_TARGET_SEPARATE_FOR_ALPHA(0)=1E00, followed by the six
       independent RGB/alpha operations/factors at 1E04..1E18.
       deko3d 0.5 gpu_3d_state.cpp omits 1E00 and emits dstColorBlendFactor
       at 1E18 instead of dstAlphaBlendFactor. Emit the complete target-0
       state through the supported replay API, without a second draw pass.
       GPFIFO increasing packet: mode 1, seven words, 3D subchannel 0,
       method index 1E00/4. See deko3d 0.5 maxwell/command.h. */
    enum { Target0SeparateAlpha = 0x1e00u / 4u, Increasing = 1u, Values = 7u };
    const uint32_t words[8] = {
        (Increasing << 29) | (Values << 16) | Target0SeparateAlpha,
        1,
        (unsigned)blend->colorBlendOp & 7u,
        nd_blend_factor_command((unsigned)blend->srcColorBlendFactor),
        nd_blend_factor_command((unsigned)blend->dstColorBlendFactor),
        (unsigned)blend->alphaBlendOp & 7u,
        nd_blend_factor_command((unsigned)blend->srcAlphaBlendFactor),
        nd_blend_factor_command((unsigned)blend->dstAlphaBlendFactor),
    };
    dkCmdBufReplayCmds(cmd, words, 8);
}
#endif
