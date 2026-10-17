/* Host conversion environment; solver scalar operations use integer helpers. */
#include "phys.h"
#include <fenv.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__)
#include <xmmintrin.h>
#endif

/* PS2 FPU behaviour on the host: rounding toward zero, and no denormals
   (results flushed to zero, denormal inputs read as zero), which matters
   for the tiny terms of the sin/cos polynomials. */
void gt4_phys_fpu_enter(void)
{
    fesetround(FE_TOWARDZERO);
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE__)
    _mm_setcsr(_mm_getcsr() | 0x8040u); /* FTZ | DAZ */
#elif defined(__SWITCH__) && defined(__aarch64__)
    /* ARM FPCR.FZ flushes both denormal inputs and results for scalar FP. */
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= UINT64_C(1) << 24;
    __asm__ volatile("msr fpcr, %0\n\tisb" : : "r"(fpcr) : "memory");
#endif
}
