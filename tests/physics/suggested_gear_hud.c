/* 0039E150/0039E1BC: HUD consumes the watched car's signed model hint, not its gear. */
#include "race_run.h"
#include "car.h"
#include "vol.h"
#include "specdb.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "hint bridge line %d: %s\n", __LINE__, #x);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_race_desc d = {0};
    d.cars = 2;
    d.split_battle = 1;
    snprintf(d.course, sizeof d.course, "tsukuba");
    gt4_vol *v = gt4_vol_open(argv[1], NULL);
    CHECK(v);
    size_t size;
    uint8_t *labels = gt4_vol_read(v, "/specdb/GT4_US2560/GENERIC_CAR.idi", &size);
    CHECK(labels && gt4_specdb_label(labels, size, "ka_01", &d.car[0]));
    d.car[1] = d.car[0];
    free(labels);
    gt4_vol_close(v);
    char message[256];
    gt4_race_run *r = gt4_race_run_open(argv[1], &d, message, sizeof message);
    CHECK(r);
    static const int hints[] = {-1, 0, 1, 3, 6};
    for (unsigned sample = 0; sample < sizeof hints / sizeof *hints; sample++) {
        for (int i = 0; i < 2; i++) {
            uint32_t car = gt4_race_run_car(r, i);
            GT4_U8(car + CAR_FIELD_SUGGESTED_GEAR) = (uint8_t)hints[sample];
            GT4_U8(car + CAR_FIELD_SUGGESTED_GEAR_FLASH) = (uint8_t)i;
        }
        for (int i = 0; i < 2; i++) {
            gt4_race_hud h;
            CHECK(gt4_race_run_hud(r, i, &h));
            CHECK(h.suggested_gear == (argc > 2 ? 99 : hints[sample]));
            CHECK(h.suggested_gear_flash == i);
        }
    }
    gt4_race_run_close(r);
    puts("HUD reads signed model hints and independent flash flags for both watched cars");
    return 0;
}
