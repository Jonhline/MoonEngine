#include "switch_fpu.h"
#include "phys.h"
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int moon_switch_fpu_check(void)
{
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    volatile float one = 1.f, fraction = 0x1.8p-23f;
    float sum = one + fraction;
    uint32_t bits;
    memcpy(&bits, &sum, sizeof bits);
    uint32_t tiny_bits = 1, flushed_bits;
    float tiny_value;
    memcpy(&tiny_value, &tiny_bits, sizeof tiny_value);
    volatile float tiny = tiny_value;
    float flushed = tiny * one;
    memcpy(&flushed_bits, &flushed, sizeof flushed_bits);
    int ok = !flushed_bits && fegetround() == FE_TOWARDZERO && ((fpcr >> 22) & 3) == 3 &&
             ((fpcr >> 24) & 1) && bits == 0x3f800001u;
    fprintf(stderr, "Switch FPU: FPCR=%llx sum=%08x %s\n", (unsigned long long)fpcr, bits,
            ok ? "PASS" : "FAIL");
    return ok;
}
