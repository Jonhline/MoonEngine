/* Shared native transfer ordering; host tests supply a delayed SDK command sink. */
#ifndef MOON_DEKO_TRANSFER_H
#define MOON_DEKO_TRANSFER_H
#include <deko3d.h>

static inline void nd_transfer_begin(DkCmdBuf cmd)
{
    /* deko3d 0.5 gpu_base.cpp: Fragments orders 3D fragments only. The
       independent copy/2D engines and prior texture reads must finish too. */
    dkCmdBufBarrier(cmd, DkBarrier_Full, 0);
}
static inline void nd_transfer_end(DkCmdBuf cmd)
{
    /* None invalidates without waiting: a consumer could fetch a stale DATE
       mask or capture while the 2D engine was still writing it. GPU-only;
       do not flush the queue or wait on the CPU here. */
    dkCmdBufBarrier(cmd, DkBarrier_Full, DkInvalidateFlags_Image);
}
static inline void nd_blit_commands(DkCmdBuf cmd, const DkImageView *src, const DkImageRect *from,
                                    const DkImageView *dst, const DkImageRect *to)
{
    nd_transfer_begin(cmd);
    dkCmdBufBlitImage(cmd, src, from, dst, to, DkBlitFlag_FilterLinear, 0);
    nd_transfer_end(cmd);
}
static inline void nd_copy_commands(DkCmdBuf cmd, const DkImageView *src, const DkImageRect *from,
                                    const DkImageView *dst, const DkImageRect *to)
{
    nd_transfer_begin(cmd);
    dkCmdBufCopyImage(cmd, src, from, dst, to, 0);
    nd_transfer_end(cmd);
}
static inline void nd_upload_commands(DkCmdBuf cmd, const DkCopyBuf *src, const DkImageView *dst,
                                      const DkImageRect *rect)
{
    nd_transfer_begin(cmd);
    dkCmdBufCopyBufferToImage(cmd, src, dst, rect, 0);
    nd_transfer_end(cmd);
}
#endif
