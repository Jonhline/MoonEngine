#include "phys.h"

uint8_t *gt4_phys_mem;
float (*gt4_orig_call)(uint32_t addr, uint32_t a0, uint32_t a1, float f12, float f13);
uint32_t gt4_orig_v0;
uint32_t gt4_orig_a2, gt4_orig_a3, gt4_orig_t0, gt4_orig_t1, gt4_orig_t2, gt4_orig_t3;
float gt4_orig_f14, gt4_orig_f15;
int gt4_phys_unverified;
int gt4_phys_headless;

/* 00359568: narrow by bisection while the interval spans at least four
   points, then scan; an exact midpoint hit returns the stored y directly.
   Native GT4R v1 retains the former linear scan for playback compatibility. */
static float eval(int n, const float *xs, const float *ys, float x)
{
    int last = n - 1, i;

    if (x <= xs[0])
        return ys[0];
    if (xs[last] <= x)
        return ys[last];
#ifndef MOON_LEGACY_RECORDING
    /* 00359568 +0x44..0x104 returns a bisection hit without interpolation. */
    int lo = 0, hi = last;
    while (hi - lo >= 4) {
        int mid = (lo + hi) / 2;
        if (x < xs[mid])
            hi = mid;
        else {
            lo = mid;
            if (x == xs[mid])
                return ys[mid];
        }
    }
    for (i = lo; i < hi && !(x < xs[i]); i++) {
    }
#else
    /* Retain arithmetic/search behaviour of existing native recordings. */
    for (i = 1; i < last && !(x < xs[i]); i++) {
    }
#endif
    return ys[i - 1] + (ys[i] - ys[i - 1]) * (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
}

float gt4_curve_eval(const gt4_curve *c, float x)
{
    return eval(c->n, gt4_ptr(c->x), gt4_ptr(c->y), x);
}

/* 00359830 +0xA0..0xC4 multiplies the x difference first. The EE's
   multiplier is not bitwise commutative; 00359568 +0x140 uses y first. */
static float eval_linear(int n, const float *xs, const float *ys, float x)
{
    int last = n - 1, i;
    if (x <= xs[0])
        return ys[0];
    if (xs[last] <= x)
        return ys[last];
    for (i = 1; i < last && !(x < xs[i]); i++) {
    }
    return ys[i - 1] + (x - xs[i - 1]) * (ys[i] - ys[i - 1]) / (xs[i] - xs[i - 1]);
}

float gt4_curve_eval_linear(const gt4_curve *c, float x)
{
    return eval_linear(c->n, gt4_ptr(c->x), gt4_ptr(c->y), x);
}

/* 003598F8 +0xBC..0x100: shared x coordinates, x difference multiplied first. */
void gt4_curve_eval2(const gt4_curve *a, const gt4_curve *b, float x, float *ya, float *yb)
{
    const float *xs = gt4_ptr(a->x), *ys = gt4_ptr(a->y), *zs = gt4_ptr(b->y);
    int last = a->n - 1, i;
    if (x <= xs[0]) {
        *ya = ys[0];
        *yb = zs[0];
        return;
    }
    if (xs[last] <= x) {
        *ya = ys[last];
        *yb = zs[last];
        return;
    }
    for (i = 1; i < last && !(x < xs[i]); i++) {
    }
    float dx = x - xs[i - 1], span = xs[i] - xs[i - 1];
    *ya = ys[i - 1] + dx * (ys[i] - ys[i - 1]) / span;
    *yb = zs[i - 1] + dx * (zs[i] - zs[i - 1]) / span;
}
