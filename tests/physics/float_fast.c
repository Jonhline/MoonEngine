/* Compare full result/exception words with the pre-fast-path recurrences. */
#include "../../third_party/ps2float/ps2float.h"
#include <stdio.h>
#include <stdlib.h>

ps2f_u64 reference_ps2f_mul(ps2f_u32, ps2f_u32);
ps2f_u64 reference_ps2f_div(ps2f_u32, ps2f_u32);

static uint32_t next(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int equal(uint32_t a, uint32_t b, int divide, int negative)
{
    uint64_t expected = divide ? reference_ps2f_div(a, b) : reference_ps2f_mul(a, b);
    uint64_t actual = divide ? ps2f_div(a, b) : ps2f_mul(a, b);
    if (negative)
        actual ^= 1;
    if (expected == actual)
        return 1;
    fprintf(stderr, "%s %08x/%08x: expected=%016llx actual=%016llx\n", divide ? "DIV" : "MUL", a, b,
            (unsigned long long)expected, (unsigned long long)actual);
    return 0;
}

int main(int argc, char **argv)
{
    (void)argv;
    uint32_t state = 0x938ab426;
    unsigned count = 0;
    for (uint32_t mantissa = 0; mantissa < 128; mantissa++)
        for (uint32_t low = 0; low < 65536; low++) {
            uint32_t a = next(&state), b = 0x3f800000u | mantissa << 16;
            if (!equal(a, b, 0, argc > 1))
                return 1;
            count++;
        }
    for (uint32_t ae = 1; ae <= 255; ae++)
        for (uint32_t be = 1; be <= 255; be++) {
            uint32_t fraction = next(&state) & 0x7fffffu;
            uint32_t a = ae << 23 | fraction, b = be << 23;
            if (!equal(a, b, 1, 0) || !equal(a, b | fraction, 1, 0) ||
                !equal(a | 0x80000000u, b, 1, 0))
                return 1;
            count += 3;
        }
    for (unsigned i = 0; i < 2000000; i++) {
        uint32_t a = next(&state), b = next(&state);
        if (!equal(a, b, 0, 0) || !equal(a, b, 1, 0) || !equal(a, b & 0xff800000u, 1, 0))
            return 1;
        count += 3;
    }
    printf(
        "PASS %u MUL/DIV result-and-flag comparisons, all fast multiplier groups, exponent pairs and random words\n",
        count);
    return 0;
}
