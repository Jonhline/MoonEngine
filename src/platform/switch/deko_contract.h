/* CPU-visible shader ABI and readback comparator, shared with host tests. */
#ifndef OPENGT4_DEKO_CONTRACT_H
#define OPENGT4_DEKO_CONTRACT_H
#include <stddef.h>
#include <stdint.h>
typedef struct nd_uniform {
    float v[64][4];
    float bones[20][16];
} nd_uniform;
/* Borrowed sprite images may not carry TEX0 TW/TH. Their UV domain is
   normalized to the resident image; never send a zero texture extent. */
static inline void nd_texture_extent(nd_uniform *u, int nominal_w, int nominal_h, unsigned width,
                                     unsigned height)
{
    u->v[43][0] = nominal_w > 0 ? (float)nominal_w : (float)width;
    u->v[43][1] = nominal_h > 0 ? (float)nominal_h : (float)height;
    u->v[43][2] = (float)width;
    u->v[43][3] = (float)height;
}
/* The UI always fits 16:9, including offscreen targets. Sample centers must
   lie inside the clamped quarters of the 2x2 probe, away from linear blends. */
enum { ND_UI_PROBE_WIDTH = 32, ND_UI_PROBE_HEIGHT = 18 };
static const unsigned nd_ui_probe_xy[4][2] = {{5, 3}, {26, 3}, {5, 14}, {26, 14}};
static inline int nd_rgba_equal(const uint8_t *a, const uint8_t *b, size_t n, unsigned tolerance)
{
    if (!a || !b)
        return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned difference = a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
        if (difference > tolerance)
            return 0;
    }
    return 1;
}
#endif
