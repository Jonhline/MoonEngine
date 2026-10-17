/* Standalone timing tool: loading/warm-up are outside each measured interval.
   The same file builds against both sides of an arithmetic change. No assets
   or captured state are embedded. Timing is diagnostic, never a pass gate. */
#include "race_run.h"
#include "disc_replay.h"
#include "car.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__SWITCH__)
#include <switch.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static int idle_pad(void *context, uint32_t car, uint8_t out[32], const uint8_t event[32])
{
    if (car != *(uint32_t *)context)
        return 0;
    memset(out, 0, 32);
    out[1] = 1;
    out[3] = event[3];
    memcpy(out + 16, event + 16, event[3] <= 16 ? event[3] : 16);
    return 1;
}

static int wheel(void *context, uint32_t car, int16_t out[2])
{
    (void)context;
    (void)car;
    out[0] = out[1] = 0;
    return 1;
}

static int vibration(void *context, uint32_t car, const uint8_t out[16])
{
    (void)context;
    (void)car;
    (void)out;
    return 1;
}

static double seconds(void)
{
#if defined(__SWITCH__)
    return (double)armTicksToNs(armGetSystemTick()) / 1e9;
#elif defined(_WIN32)
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    return (double)count.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
#endif
}

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 6)
        return 2;
    const char *course = argc > 2 ? argv[2] : "tsukuba";
    int cars = argc > 3 ? atoi(argv[3]) : 6;
    int ticks = argc > 4 ? atoi(argv[4]) : 3000;
    int repeats = argc > 5 ? atoi(argv[5]) : 5;
    if (cars < 1 || cars > 6 || ticks < 1 || repeats < 1)
        return 2;
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    gt4_disc_replay *reader = vol ? gt4_disc_replay_open(vol) : NULL;
    gt4_replay_file donor;
    char error[256];
    if (!reader ||
        !gt4_disc_replay_read(reader, "/replay/Rdemo0/Rdemo0", &donor, error, sizeof error))
        return 2;
    for (int repeat = 0; repeat < repeats; repeat++) {
        gt4_race_desc desc = {0};
        snprintf(desc.course, sizeof desc.course, "%s", course);
        desc.cars = cars;
        desc.bspec = 1;
        for (int car = 0; car < cars; car++)
            desc.car[car] = donor.car_id[0];
        gt4_race_run *run = gt4_race_run_open(argv[1], &desc, error, sizeof error);
        if (!run) {
            fprintf(stderr, "%s\n", error);
            return 1;
        }
        uint32_t player = gt4_race_run_car(run, 0);
        gt4_phys_pad = (gt4_phys_pads){&player, idle_pad, wheel, vibration};
        for (int warm = 0; warm < 180; warm++)
            if (!gt4_race_run_frame(run))
                return 1;
        double start = seconds();
        for (int tick = 0; tick < ticks; tick++)
            if (!gt4_race_run_frame(run))
                return 1;
        double duration = seconds() - start;
        printf("course=%s cars=%d repeat=%d ticks=%d us_per_step=%.6f\n", course, cars, repeat,
               ticks, duration * 1e6 / ticks);
        fflush(stdout);
        gt4_race_run_close(run);
        gt4_phys_pad = (gt4_phys_pads){0};
    }
    gt4_replay_free(&donor);
    gt4_disc_replay_close(reader);
    gt4_vol_close(vol);
    return 0;
}
