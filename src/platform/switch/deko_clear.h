/* Clears must not inherit read-only particle/UI state from the prior draw. */
#ifndef OPENGT4_DEKO_CLEAR_H
#define OPENGT4_DEKO_CLEAR_H
#include <deko3d.h>

static inline void nd_clear_depth_commands(DkCmdBuf cmd)
{
    /* Same boundary as render3d.c r3d_begin/r3d_clear_depth: enable depth
       writes before clearing. Every subsequent draw binds its own state. */
    DkDepthStencilState depth;
    dkDepthStencilStateDefaults(&depth);
    /* SDK 0.5 BindDepthStencilState only emits DepthWriteEnable when the
       depth test is enabled. Keep its default true for this clear. */
    depth.depthWriteEnable = true;
    dkCmdBufBindDepthStencilState(cmd, &depth);
    dkCmdBufClearDepthStencil(cmd, true, 1, 0, 0);
}
static inline void nd_clear_color_commands(DkCmdBuf cmd, unsigned channels, const float rgba[4])
{
    DkColorWriteState mask;
    dkColorWriteStateDefaults(&mask);
    dkColorWriteStateSetMask(&mask, 0, channels);
    dkCmdBufBindColorWriteState(cmd, &mask);
    dkCmdBufClearColor(cmd, 0, channels, rgba);
}
static inline void nd_begin_3d_commands(DkCmdBuf cmd)
{
    nd_clear_depth_commands(cmd);
    /* render3d.c r3d_begin resets DATE alpha to 1 without erasing the page
       RGB. Carrying the previous scene's coefficients changes DATE results. */
    const float alpha[4] = {0, 0, 0, 1};
    nd_clear_color_commands(cmd, DkColorMask_A, alpha);
}
#endif
