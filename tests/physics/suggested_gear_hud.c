/* 0039E150/0039E1BC: HUD consumes the watched car's signed model hint, not its gear. */
#include "race_run.h"
#include "car.h"
#include "fields_conductor.h"
#include "vol.h"
#include "specdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    for (int i = 0; i < 2; i++) {
        uint32_t car = gt4_race_run_car(r, i), params = GT4_U32(car + CAR_PARAMS);
        GT4_F32(params + PARAMS_ENGINE + ENGINE_TURBO) = i ? 1.f : 0.f;
        GT4_F32(car + CAR_FIELD_BOOST_SMOOTH) = .375f + i;
        gt4_race_hud h;
        CHECK(gt4_race_run_hud(r, i, &h));
        CHECK(h.has_boost == i && h.boost == .375f + i);
    }
    /* 003677F8 boundary colours; both session flags and both players. */
    static const int gauge[] = {-128, -64, -63, -1, 0, 31, 32, 63, 64, 127};
    static const uint8_t rgb[][3] = {
        {0,0,0}, {0,0,0}, {0,0,252}, {0,248,4}, {0,252,0},
        {248,252,0}, {252,252,0}, {252,128,0}, {252,126,0}, {252,0,0}};
    uint32_t cond = gt4_race_run_conductor(r);
    for (int player = 0; player < 2; player++) {
        uint32_t car = gt4_race_run_car(r, player);
        GT4_U8(car + CAR_POSITION + 0x441) = 0;
        GT4_U8(car + CAR_POSITION + 0x466) = 0;
        uint32_t layout = 0x00620478u + GT4_U8(GT4_U32(car + CAR_PARAMS) + 0x30) * 0x54u;
        uint8_t saved = GT4_U8(layout);
        for (int flag = 0; flag < 3; flag++) {
            GT4_U8(cond + CONDUCTOR_FIELD_TYRE_STATE + 0x68) = flag == 1;
            GT4_U8(cond + CONDUCTOR_FIELD_TYRE_STATE + 0x69) = flag == 2;
            for (int kind = 0; kind < 4; kind++) {
                GT4_U8(layout) = kind;
                for (int sample = 0; sample < 10; sample++) {
                    for (int wheel = 0; wheel < 4; wheel++)
                        GT4_U8(car + wheel * 0xec + 0x215) = gauge[(sample + wheel) % 10];
                    gt4_race_hud h;
                    CHECK(gt4_race_run_hud(r, player, &h));
                    if (getenv("MOON_TIRE_HUD_NEGATIVE"))
                        memset(h.tire_rgb, 0, sizeof h.tire_rgb);
                    for (int wheel = 0; wheel < 4; wheel++) {
                        static const int indices[4][4] = {{0,1,2,3},{0,0,1,2},{0,1,2,2},{0,0,1,1}};
                        int sample_index = (sample + indices[kind][wheel]) % 10;
                        for (int channel = 0; channel < 3; channel++)
                            CHECK(h.tire_rgb[wheel][channel] == (flag ? rgb[sample_index][channel] : 0));
                    }
                }
            }
        }
        GT4_U8(layout) = saved;
    }
    gt4_race_run_close(r);
    puts("HUD reads signed model hints and independent flash flags for both watched cars");
    return 0;
}
