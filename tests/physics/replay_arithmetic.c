/* Original-code physics oracle arithmetic. Normal finite instruction results use the
   existing hardware-derived integer model. Host helpers retain status bookkeeping;
   these trajectory tests compare RAM, not FCR31 exception/overflow behaviour. */
#define ee_fpu_add host_fpu_add
#define ee_fpu_sub host_fpu_sub
#define ee_fpu_mul host_fpu_mul
#define ee_fpu_div host_fpu_div
#define ee_fpu_sqrt host_fpu_sqrt
#define ee_fpu_rsqrt host_fpu_rsqrt
#define ee_fpu_adda host_fpu_adda
#define ee_fpu_suba host_fpu_suba
#define ee_fpu_mula host_fpu_mula
#define ee_fpu_madda host_fpu_madda
#define ee_fpu_msuba host_fpu_msuba
#define ee_fpu_madd host_fpu_madd
#define ee_fpu_msub host_fpu_msub
#include "../../src/ee/fpu.c"
#include "../../third_party/ps2float/ps2float.h"
#undef ee_fpu_add
#undef ee_fpu_sub
#undef ee_fpu_mul
#undef ee_fpu_div
#undef ee_fpu_sqrt
#undef ee_fpu_rsqrt
#undef ee_fpu_adda
#undef ee_fpu_suba
#undef ee_fpu_mula
#undef ee_fpu_madda
#undef ee_fpu_msuba
#undef ee_fpu_madd
#undef ee_fpu_msub
uint32_t ee_fpu_add(ee_ctx *c, uint32_t a, uint32_t b)
{
    (void)host_fpu_add(c, a, b);
    return ps2f_raw(ps2f_add(a, b));
}
uint32_t ee_fpu_sub(ee_ctx *c, uint32_t a, uint32_t b)
{
    (void)host_fpu_sub(c, a, b);
    return ps2f_raw(ps2f_sub(a, b));
}
uint32_t ee_fpu_mul(ee_ctx *c, uint32_t a, uint32_t b)
{
    (void)host_fpu_mul(c, a, b);
    return ps2f_raw(ps2f_mul(a, b));
}
uint32_t ee_fpu_div(ee_ctx *c, uint32_t a, uint32_t b)
{
    (void)host_fpu_div(c, a, b);
    return ps2f_raw(ps2f_div(a, b));
}
uint32_t ee_fpu_sqrt(ee_ctx *c, uint32_t b)
{
    (void)host_fpu_sqrt(c, b);
    return ps2f_raw(ps2f_sqrt(b));
}
uint32_t ee_fpu_rsqrt(ee_ctx *c, uint32_t a, uint32_t b)
{
    (void)host_fpu_rsqrt(c, a, b);
    return ps2f_raw(ps2f_rsqrt(a, b));
}
void ee_fpu_adda(ee_ctx *c, uint32_t a, uint32_t b)
{
    host_fpu_adda(c, a, b);
    set_acc(c, ps2f_raw(ps2f_add(a, b)));
}
void ee_fpu_suba(ee_ctx *c, uint32_t a, uint32_t b)
{
    host_fpu_suba(c, a, b);
    set_acc(c, ps2f_raw(ps2f_sub(a, b)));
}
void ee_fpu_mula(ee_ctx *c, uint32_t a, uint32_t b)
{
    host_fpu_mula(c, a, b);
    set_acc(c, ps2f_raw(ps2f_mul(a, b)));
}
void ee_fpu_madda(ee_ctx *c, uint32_t a, uint32_t b)
{
    uint32_t prior = acc_bits(c);
    (void)host_fpu_madda(c, a, b);
    set_acc(c, ps2f_raw(ps2f_madd(prior, a, b, 0)));
}
void ee_fpu_msuba(ee_ctx *c, uint32_t a, uint32_t b)
{
    uint32_t prior = acc_bits(c);
    (void)host_fpu_msuba(c, a, b);
    set_acc(c, ps2f_raw(ps2f_msub(prior, a, b, 0)));
}
uint32_t ee_fpu_madd(ee_ctx *c, uint32_t a, uint32_t b)
{
    uint32_t prior = acc_bits(c);
    (void)host_fpu_madd(c, a, b);
    return ps2f_raw(ps2f_madd(prior, a, b, 0));
}
uint32_t ee_fpu_msub(ee_ctx *c, uint32_t a, uint32_t b)
{
    uint32_t prior = acc_bits(c);
    (void)host_fpu_msub(c, a, b);
    return ps2f_raw(ps2f_msub(prior, a, b, 0));
}
