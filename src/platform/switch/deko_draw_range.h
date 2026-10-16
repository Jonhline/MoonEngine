/* Native vertex-buffer boundary; does not change draw order or geometry. */
#ifndef MOON_DEKO_DRAW_RANGE_H
#define MOON_DEKO_DRAW_RANGE_H
#include <stdint.h>

static inline int nd_draw_range_valid(uint32_t bytes, uint32_t stride, uint32_t first,
                                      uint32_t count)
{
    /* Subtraction avoids overflow in (first + count) * stride. SDK
       dkCmdBufBindVtxBuffer supplies a byte extent, Draw a vertex extent. */
    if (!stride || !count)
        return 0;
    uint32_t vertices = bytes / stride;
    return first <= vertices && count <= vertices - first;
}
#endif
