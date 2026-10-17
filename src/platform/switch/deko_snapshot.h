/* Conservative horizontal bounds for the shipped mesh.vert projection. */
#ifndef MOON_DEKO_SNAPSHOT_H
#define MOON_DEKO_SNAPSHOT_H
#include <math.h>
#include <float.h>
struct nd_snapshot_span {
    unsigned x, width;
};
static inline struct nd_snapshot_span nd_snapshot_horizontal(const float p[64][4],
                                                             const float lo[3], const float hi[3],
                                                             const int viewport[4], unsigned width)
{
    struct nd_snapshot_span full = {0, width};
    double low = width, high = 0;
    /* Skin/transient bounds are deliberately kept on the full-copy path. */
    if (p[14][0] > 0 || viewport[2] <= 0)
        return full;
    /* Bound float dot-product error over the entire source box, including
       large world/camera coordinates which nearly cancel. The factor covers
       the shader's multiply/add sequence with or without fused operations. */
    double error_x = 0, error_z = 0;
    for (unsigned row = 0; row < 3; ++row) {
        double magnitude = fabs(p[3][row]) + fabs(p[4][row]);
        for (unsigned k = 0; k < 3; ++k)
            magnitude += fabs(p[row][k]) * fmax(fabs(lo[k]), fabs(hi[k]));
        error_x += 32 * FLT_EPSILON * magnitude * fabs(p[5][row]);
        error_z += 32 * FLT_EPSILON * magnitude * fabs(p[7][row]);
    }
    for (unsigned corner = 0; corner < 8; ++corner) {
        double world[3], rel[3], x = 0, z = 0;
        for (unsigned row = 0; row < 3; ++row) {
            world[row] = p[3][row];
            for (unsigned k = 0; k < 3; ++k)
                world[row] += p[row][k] * (double)((corner & (1u << k)) ? hi[k] : lo[k]);
        }
        if (p[7][3] > 0)
            world[1] = -world[1];
        for (unsigned k = 0; k < 3; ++k) {
            rel[k] = world[k] - p[4][k];
            x += rel[k] * p[5][k];
            z += rel[k] * p[7][k];
        }
        /* A perspective ratio reaches its extrema at box corners only while
           the entire box is in the positive-depth half-space. */
        if (!isfinite(x) || !isfinite(z) || z - error_z <= fmax(.2, p[8][2]))
            return full;
        for (unsigned edge = 0; edge < 4; ++edge) {
            double ratio = (x + ((edge & 1) ? error_x : -error_x)) * p[8][0] /
                           (z + ((edge & 2) ? error_z : -error_z));
            double pixel = viewport[0] + (.5 + .5 * (ratio + p[15][0])) * viewport[2];
            double roundoff = 32 * FLT_EPSILON * (fabs(ratio) + fabs(p[15][0]) + 1) * viewport[2];
            if (!isfinite(pixel) || !isfinite(roundoff))
                return full;
            low = fmin(low, pixel - roundoff);
            high = fmax(high, pixel + roundoff);
        }
    }
    /* Outward rounding retains boundary samples after the float error bound.
       Retain full height: no assumption about viewport Y orientation. */
    low = fmax(0, fmin(width, floor(low) - 2));
    high = fmax(0, fmin(width, ceil(high) + 2));
    if (high <= low)
        return full;
    struct nd_snapshot_span span = {(unsigned)low, (unsigned)(high - low)};
    return span;
}
struct nd_snapshot_rect {
    unsigned x, y, width, height;
};
static inline struct nd_snapshot_rect nd_snapshot_bounds(const float p[64][4], const float lo[3],
                                                         const float hi[3], const int viewport[4],
                                                         unsigned width, unsigned height)
{
    struct nd_snapshot_span x = nd_snapshot_horizontal(p, lo, hi, viewport, width);
    float vertical[64][4] = {{0}};
    for (unsigned row = 0; row < 9; ++row)
        for (unsigned k = 0; k < 4; ++k)
            vertical[row][k] = p[row][k];
    for (unsigned k = 0; k < 3; ++k)
        vertical[5][k] = -p[6][k];
    vertical[8][0] = p[8][1];
    vertical[14][0] = p[14][0];
    vertical[15][0] = -p[15][1];
    /* deko3d gpu_3d_base.cpp: default UpperLeft/YAxisPointsUp flips
       viewport ScaleY. Project -camera-Y using the same conservative helper. */
    int y_viewport[4] = {viewport[1], 0, viewport[3], 0};
    struct nd_snapshot_span y = nd_snapshot_horizontal(vertical, lo, hi, y_viewport, height);
    struct nd_snapshot_rect result = {x.x, y.x, x.width, y.width};
    return result;
}
#endif
