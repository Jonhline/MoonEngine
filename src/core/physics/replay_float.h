/* Shared native physics arithmetic values. Original EE instructions ADD.S, SUB.S,
   MUL.S, DIV.S and SQRT.S, including the operand-sensitive multiplier.
   Used in every game mode; a fidelity correction, never a user option. */
#ifndef MOON_REPLAY_FLOAT_H
#define MOON_REPLAY_FLOAT_H
#include "../../../third_party/ps2float/ps2float.h"
#include <string.h>
static inline uint32_t gt4_replay_f_bits(float x)
{
    uint32_t v;
    memcpy(&v, &x, 4);
    return v;
}
static inline float gt4_replay_f_float(uint64_t x)
{
    uint32_t v = (uint32_t)x;
    float r;
    memcpy(&r, &v, 4);
    return r;
}
static inline float gt4_replay_f_add(float a, float b)
{
    return gt4_replay_f_float(ps2f_add(gt4_replay_f_bits(a), gt4_replay_f_bits(b)));
}
static inline float gt4_replay_f_add_assign(float *a, float b)
{
    return *a = gt4_replay_f_add(*a, b);
}
static inline float gt4_replay_f_sub(float a, float b)
{
    return gt4_replay_f_float(ps2f_sub(gt4_replay_f_bits(a), gt4_replay_f_bits(b)));
}
static inline float gt4_replay_f_sub_assign(float *a, float b)
{
    return *a = gt4_replay_f_sub(*a, b);
}
static inline float gt4_replay_f_mul(float a, float b)
{
    return gt4_replay_f_float(ps2f_mul(gt4_replay_f_bits(a), gt4_replay_f_bits(b)));
}
static inline float gt4_replay_f_mul_assign(float *a, float b)
{
    return *a = gt4_replay_f_mul(*a, b);
}
static inline float gt4_replay_f_div(float a, float b)
{
    return gt4_replay_f_float(ps2f_div(gt4_replay_f_bits(a), gt4_replay_f_bits(b)));
}
static inline float gt4_replay_f_div_assign(float *a, float b)
{
    return *a = gt4_replay_f_div(*a, b);
}
static inline float gt4_replay_f_sqrt(float x)
{
    return gt4_replay_f_float(ps2f_sqrt(gt4_replay_f_bits(x)));
}
#endif
