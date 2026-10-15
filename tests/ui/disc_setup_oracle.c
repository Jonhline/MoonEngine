/* Original: func_003DDAD8 +0x00. Compare the entire entry, including links. */
#include "disc_replay.h"
#include "race_run.h"
#include "phys.h"
#include "car.h"
#include <math.h>
#include "ee.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "setup line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int compare_entry(ee_ctx *e, const uint8_t *record, int negative)
{
    const uint32_t entry = 0x1800000, source = 0x1810000;
    uint8_t expected[0x3490];
    memset(e->ram + entry, 0xa7, sizeof expected);
    memcpy(e->ram + source, record, 0x228);
    e->r[4].uw[0] = entry;
    e->r[5].uw[0] = source;
    e->r[29].uw[0] = 0x1ff0000;
    recomp_lookup(0x3ddad8)(e);
    CHECK(e->end_reason == EE_END_NONE);
    memcpy(expected, e->ram + entry, sizeof expected);
    memset(e->ram + entry, 0xa7, sizeof expected);
    gt4_phys_mem = e->ram;
    gt4_race_restore_entry(entry, record);
    if (negative)
        GT4_U8(entry + 0x3444) ^= 1;
    CHECK(!memcmp(expected, e->ram + entry, sizeof expected));
    return 0;
}
typedef struct {
    uint32_t player;
    const uint8_t *tape;
    size_t at, steps;
} playback;

static int read_command(void *ctx, uint32_t car, uint8_t *out, const uint8_t *events)
{
    playback *p = ctx;
    (void)events;
    if (car != p->player)
        return 0;
    if (p->at < p->steps)
        memcpy(out, p->tape + p->at++ * 32, 32);
    else
        memset(out, 0, 32);
    return 1;
}

static int wheel_state(void *ctx, uint32_t car, int16_t state[2])
{
    (void)ctx;
    (void)car;
    state[0] = state[1] = 1;
    return 1;
}

static int write_command(void *ctx, uint32_t car, const uint8_t *out)
{
    (void)ctx;
    (void)car;
    (void)out;
    return 1;
}

static int verify_bspec(const char *disc, gt4_disc_replay *reader, int negative)
{
    static const unsigned demos[] = {1, 3, 5, 8};
    for (unsigned n = 0; n < sizeof demos / sizeof demos[0]; n++) {
        char path[64], error[256];
        snprintf(path, sizeof path, "/replay/Rdemo%u/Rdemo%u", demos[n], demos[n]);
        gt4_replay_file file;
        CHECK(gt4_disc_replay_read(reader, path, &file, error, sizeof error));
        CHECK(file.disc_bspec && file.disc_has_settings);
        gt4_race_desc d = {0};
        strcpy(d.course, file.course);
        d.cars = file.cars;
        d.bspec = !negative;
        d.manual = file.manual;
        d.disc_settings = file.event_row;
        d.disc_race_options = file.disc_info + 0x50;
        for (int i = 0; i < d.cars; i++) {
            d.car[i] = file.car_id[i];
            d.owned_car[i] = file.owned[i];
            d.disc_entry[i] = file.disc_info + 0x960 + i * 0x228;
        }
        gt4_race_run *run = gt4_race_run_open(disc, &d, error, sizeof error);
        CHECK(run);
        uint32_t car = gt4_race_run_car(run, 0);
        CHECK(GT4_U8(car + CAR_56A) == 1);
        playback p = {car, file.tape, 0, file.steps};
        gt4_phys_pad = (gt4_phys_pads){&p, read_command, wheel_state, write_command};
        float before[3], after[3], rotation[9], wheels[4][3];
        CHECK(gt4_race_run_pose(run, 0, before, rotation, wheels));
        for (unsigned tick = 0; tick < 1200; tick++)
            CHECK(gt4_race_run_frame(run));
        CHECK(p.at == 1200);
        CHECK(gt4_race_run_pose(run, 0, after, rotation, wheels));
        CHECK(isfinite(after[0]) && isfinite(after[1]) && isfinite(after[2]));
        CHECK(memcmp(before, after, sizeof before));
        printf("PASS %s: recorded B-Spec mode, 1200 commands and movement.\n", path);
        memset(&gt4_phys_pad, 0, sizeof gt4_phys_pad);
        gt4_race_run_close(run);
        gt4_replay_free(&file);
    }
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2 || argc == 3);
    const char *why;
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol);
    gt4_disc_replay *reader = gt4_disc_replay_open(vol);
    CHECK(reader);
    if (argc == 3 && !strncmp(argv[2], "--runtime", 9)) {
        int status = verify_bspec(argv[1], reader, !strcmp(argv[2], "--runtime-negative"));
        gt4_disc_replay_close(reader);
        gt4_vol_close(vol);
        return status;
    }
    char paths[32][64], error[256];
    int count = gt4_disc_replay_list(vol, paths, 32, 0);
    CHECK(count == 13);
    ee_ctx e;
    ee_init(&e);
    e.lookup = recomp_lookup;
    unsigned entries = 0;
    for (int i = 0; i < count; i++) {
        gt4_replay_file file;
        CHECK(gt4_disc_replay_read(reader, paths[i], &file, error, sizeof error));
        for (int car = 0; car < file.cars; car++) {
            CHECK(!compare_entry(&e, file.disc_info + 0x960 + car * 0x228, argc == 3));
            entries++;
        }
        gt4_replay_free(&file);
    }
    /* Exercise signed options and noncanonical true values absent from the catalog. */
    uint32_t seed = 1;
    for (int i = 0; i < 128; i++) {
        uint8_t record[0x228];
        for (unsigned j = 0; j < sizeof record; j++) {
            seed = seed * 1664525u + 1013904223u;
            record[j] = (uint8_t)(seed >> 24);
        }
        CHECK(!compare_entry(&e, record, 0));
    }
    printf("PASS %u authored entries and 128 varied records: all 0x3490 bytes match 003DDAD8.\n", entries);
    gt4_phys_mem = NULL;
    ee_free(&e);
    gt4_disc_replay_close(reader);
    gt4_vol_close(vol);
    return 0;
}
