/* Directional comparator from 0026F0A8/0026F2B8; projected rectangles from 0026F630.
   Menu coordinates only. No race simulation arithmetic is involved. */
#ifndef MOON_MENU_NAVIGATION_H
#define MOON_MENU_NAVIGATION_H
#include <math.h>
#include <stdint.h>
#include <string.h>
typedef struct {
    float l, t, r, b;
} menu_nav_rect;
typedef struct {
    uint32_t id;
    int group;
    menu_nav_rect source, target;
} menu_nav_item;
typedef struct {
    float clip_angle;
    int start_front, wraparound;
    uint32_t current;
    menu_nav_rect source;
} menu_nav;
/* 0026F868 and 0026F9E0: an empty intersection collapses BOTH axes. */
static menu_nav_rect menu_nav_intersect(menu_nav_rect a, menu_nav_rect b)
{
    menu_nav_rect r = {fmaxf(a.l, b.l), fmaxf(a.t, b.t), fminf(a.r, b.r), fminf(a.b, b.b)};
    if (r.r <= r.l || r.b <= r.t) {
        r.r = r.l;
        r.b = r.t;
    }
    return r;
}
static int menu_nav_positive(menu_nav_rect r)
{
    return r.l < r.r && r.t < r.b;
}
static menu_nav_rect menu_nav_project(menu_nav_rect r, float dx, float dy)
{
    /* 0026F630 projects the edge midpoints, rather than the four corners. */
    float cx = (r.l + r.r) * .5f, cy = (r.t + r.b) * .5f;
    float x[4] = {cx, r.l, cx, r.r}, y[4] = {r.t, cy, r.b, cy};
    menu_nav_rect out = {999999, 999999, -999999, -999999};
    for (int i = 0; i < 4; i++) {
        float u = x[i] * dx - y[i] * -dy, v = x[i] * -dy + y[i] * dx;
        if (u < out.l)
            out.l = u;
        if (u > out.r)
            out.r = u;
        if (v < out.t)
            out.t = v;
        if (v > out.b)
            out.b = v;
    }
    return out;
}
static float menu_nav_float(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}
/* 0057D008 / 0057D118 rational angle, same coefficients as the executable. */
static float menu_nav_atan(float x)
{
    float r = fabsf(x);
    int mode = 0;
    if (r > menu_nav_float(0x401a827a)) {
        r = 1.f / r;
        mode = 1;
    } else if (r > menu_nav_float(0x3ed413cd)) {
        r = (1 - r) / (1 + r);
        mode = -1;
    }
    float r2 = r * r, r4 = r2 * r2, r3 = r2 * r, r5 = r3 * r2;
    float denominator = 1 + menu_nav_float(0x3f8e38e4) * r2;
    denominator += menu_nav_float(0x3e73cf3d) * r4;
    float inverse = 1.f / denominator, result = r + 0;
    result += menu_nav_float(0x3f471c72) * r3;
    result += menu_nav_float(0x3d8ab356) * r5;
    result *= inverse;
    if (mode < 0)
        result = menu_nav_float(0x3f490fdb) - result;
    else if (mode > 0)
        result = menu_nav_float(0x3fc90fdb) - result;
    return signbit(x) ? -result : result;
}
static float menu_nav_atan2(float y, float x)
{
    if (x == 0 && y == 0)
        return 0;
    int bits = 0;
    if (y < 0) {
        y = -y;
        bits |= 4;
    }
    if (x < 0) {
        x = -x;
        bits |= 2;
    }
    float ratio;
    if (y < x)
        ratio = y / x;
    else {
        ratio = x / y;
        bits |= 1;
    }
    float angle = menu_nav_atan(ratio);
    if (bits & 1)
        angle = menu_nav_float(0x3fc90fda) - angle;
    if (bits & 2)
        angle = menu_nav_float(0x40490fda) - angle;
    return bits & 4 ? -angle : angle;
}
static int menu_nav_measure(const menu_nav *n, menu_nav_rect source, menu_nav_rect target,
                            float *distance, float *angle)
{
    *distance = 10000;
    *angle = 0;
    if (!((n->source.l + n->source.r) * .5f < (source.l + source.r) * .5f))
        return 0;
    float left = target.l - n->source.l + 1, right = target.r - n->source.l + 1;
    if (n->start_front) {
        left = target.l - n->source.r + 1;
        right = target.r - n->source.r + 1;
    }
    float perpendicular, along;
    if (target.b <= n->source.t) {
        along = fabsf(left) < fabsf(right) ? left : right;
        perpendicular = n->source.t - target.b;
    } else if (n->source.b <= target.t) {
        along = fabsf(left) < fabsf(right) ? left : right;
        perpendicular = target.t - n->source.b;
    } else {
        if (left >= 0)
            *distance = right >= 0 && !(left < right) ? right : left;
        else if (right >= 0)
            *distance = right;
        else
            return 0;
        return 1;
    }
    float radians = menu_nav_atan2(perpendicular, along);
    *distance = sqrtf(along * along + perpendicular * perpendicular);
    *angle = (radians / 3.141592502593994140625f) * 180;
    return *angle < n->clip_angle;
}
static int menu_nav_less_shift(const menu_nav *n, const menu_nav_item *a, const menu_nav_item *b,
                               float shift)
{
    menu_nav_rect as = a->source, at = a->target, bs = b->source, bt = b->target;
    as.l += shift;
    as.r += shift;
    at.l += shift;
    at.r += shift;
    bs.l += shift;
    bs.r += shift;
    bt.l += shift;
    bt.r += shift;
    float ad = 10000, aa = 0, bd = 10000, ba = 0;
    int av = a->id != n->current && menu_nav_measure(n, as, at, &ad, &aa);
    int bv = b->id != n->current && menu_nav_measure(n, bs, bt, &bd, &ba);
    if (av) {
        if (!bv)
            return 1;
        if (a->group != b->group)
            return a->group > b->group;
        if (aa != ba)
            return aa < ba;
        if (ad != bd)
            return ad < bd;
        float center = (n->source.t + n->source.b) * .5f;
        float ay = fabsf((as.t + as.b) * .5f - center);
        float by = fabsf((bs.t + bs.b) * .5f - center);
        if (ay != by)
            return ay < by;
        return a->id < b->id;
    }
    if (bv)
        return 0;
    if (n->wraparound && shift == 0)
        return menu_nav_less_shift(n, a, b, 10000);
    if (a->id == n->current)
        return 1;
    return b->id != n->current && a->id < b->id;
}
#endif
