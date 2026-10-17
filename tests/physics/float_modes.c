/* Live races always use EE arithmetic; GT4R v1 alone selects the old solver. */
#include "disc_replay.h"
#include "car.h"
#include "race_run.h"
#include "replay_run.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum { TEST_RAM_SIZE = 32 * 1024 * 1024, TEST_FRAMES = 600 };
typedef struct {
    uint32_t player;
    const uint8_t *command;
} test_input;

static int read_pad(void *context, uint32_t car, uint8_t out[32], const uint8_t event[32])
{
    const test_input *input = context;
    (void)event;
    if (car != input->player)
        return 0;
    memcpy(out, input->command, 32);
    return 1;
}

static int wheel(void *context, uint32_t car, int16_t out[2])
{
    (void)context;
    (void)car;
    out[0] = out[1] = 1;
    return 1;
}

static int vibration(void *context, uint32_t car, const uint8_t out[16])
{
    (void)context;
    (void)car;
    (void)out;
    return 1;
}

static int sample(const char *disc, const gt4_replay_file *file, int legacy, int direct,
                  uint8_t *out)
{
    gt4_race_desc desc = {0};
    char error[256];
    snprintf(desc.course, sizeof desc.course, "%s", file->course);
    desc.cars = 1;
    desc.car[0] = file->car_id[0];
    desc.manual = file->manual;
    desc.legacy_recording = legacy;
    gt4_phys_pad = (gt4_phys_pads){0};
    gt4_race_run *run = direct ? legacy_gt4_race_run_open(disc, &desc, error, sizeof error)
                               : gt4_race_run_open(disc, &desc, error, sizeof error);
    if (!run) {
        fprintf(stderr, "%s\n", error);
        return 0;
    }
    test_input input = {gt4_race_run_car(run, 0), file->tape};
    gt4_phys_pad = (gt4_phys_pads){&input, read_pad, wheel, vibration};
    int ok = 1;
    for (size_t tick = 0; tick < TEST_FRAMES && ok; tick++) {
        input.command = file->tape + tick * 32;
        ok = direct ? legacy_gt4_race_run_frame(run) : gt4_race_run_frame(run);
    }
    memcpy(out, gt4_phys_mem, TEST_RAM_SIZE);
    gt4_race_run_close(run);
    gt4_phys_pad = (gt4_phys_pads){0};
    return ok;
}

int main(int argc, char **argv)
{
    if (argc != 2 && argc != 3)
        return 2;
    const char *why;
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    gt4_disc_replay *reader = vol ? gt4_disc_replay_open(vol) : NULL;
    gt4_replay_file file;
    if (!reader ||
        !gt4_disc_replay_read(reader, "/replay/Rdemo0/Rdemo0", &file, error, sizeof error))
        return 2;
    uint8_t *expected = malloc(TEST_RAM_SIZE), *actual = malloc(TEST_RAM_SIZE);
    if (!expected || !actual || file.steps < TEST_FRAMES)
        return 2;
    int ok = sample(argv[1], &file, 1, 1, expected) && sample(argv[1], &file, 1, 0, actual) &&
             !memcmp(actual, expected, TEST_RAM_SIZE);
    if (argc == 3)
        expected[0] ^= 1;
    ok = ok && !memcmp(actual, expected, TEST_RAM_SIZE);
    ok = ok && sample(argv[1], &file, 0, 0, actual) && memcmp(actual, expected, TEST_RAM_SIZE) != 0;
    free(actual);
    free(expected);
    gt4_replay_free(&file);
    gt4_disc_replay_close(reader);
    gt4_vol_close(vol);
    puts(ok ? "PASS v1 dispatch matches legacy; live races use corrected arithmetic"
            : "FAIL physics recording arithmetic boundary");
    return ok ? 0 : 1;
}
