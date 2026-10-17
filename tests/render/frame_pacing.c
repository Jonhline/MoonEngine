#include "frame_pacing.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "pacing line %d: %s\n", __LINE__, #x);                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    (void)argv;
    moon_frame_pacer pacer = {0};
    const uint64_t anchor = 123456789;
    CHECK(moon_frame_deadline(&pacer, anchor, 144) == anchor);
    for (uint64_t i = 1; i <= 144 * 3600; i++) {
        uint64_t expected = anchor + i * 1000000000ull / 144;
        uint64_t actual = moon_frame_deadline(&pacer, expected, 144);
        if (argc > 1)
            actual += i; /* Deliberate accumulated rounding drift. */
        CHECK(actual == expected);
    }
    uint64_t resumed = anchor + 4000000000000ull;
    CHECK(moon_frame_deadline(&pacer, resumed, 144) == resumed);
    CHECK(moon_frame_deadline(&pacer, resumed, 144) == resumed + 1000000000ull / 144);
    CHECK(moon_frame_deadline(&pacer, resumed, 60) == resumed);
    CHECK(moon_frame_deadline(&pacer, resumed, 60) == resumed + 1000000000ull / 60);
    CHECK(moon_frame_deadline(&pacer, resumed + 1, 0) == resumed + 1);
    puts("one-hour deadlines have no drift; long stalls do not cause catch-up bursts");
    return 0;
}
