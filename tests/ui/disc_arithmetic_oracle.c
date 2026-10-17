/* Authored recordings: RAM outside the oracle stack after each original/native step. Both
   start from the same native context, so this proves step parity, not original
   constructor or full-console trajectory parity. Game data remains local. */
#include "ee.h"
#include "disc_replay.h"
#include "disc_checkpoint.h"
#include "race_run.h"
#include "car.h"
#include "specdb.h"
#include "spectables.h"
#include "fields_conductor.h"
#include "career_license_disc.h"
#include "race_mission.h"

#include <stdlib.h>
#include <fenv.h>
#include <string.h>
#include "../gtmode/mission_pad_fixture.h"

ee_fn recomp_lookup(uint32_t);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "trajectory line %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
enum {
    TEST_STACK_BOTTOM = 0x01e18000,
    TEST_STACK_TOP = 0x01ef0000,
    TEST_STEP_BUFFER = 0x01eeffc0,
    TEST_RACE_EVENT_QUEUE = 0xf4, /* 00389758 +0x28 */
    TEST_RACE_REPLAY_CLOCK = 0xbc /* 00359D40: checkpoint restart counter */
};
typedef struct {
    const uint8_t *command;
    uint32_t player, missing;
    int16_t wheel_value;
} trajectory_input;

static int read_pad(void *ctx, uint32_t car, uint8_t out[32], const uint8_t event[32])
{
    trajectory_input *input = ctx;
    if (car != input->player)
        return 0;
    memcpy(out, input->command, 32);
    (void)event;
    return 1;
}

static int wheel(void *ctx, uint32_t car, int16_t state[2])
{
    (void)ctx;
    (void)car;
    state[0] = state[1] = 1;
    return 1;
}

static int vibration(void *ctx, uint32_t car, const uint8_t data[16])
{
    (void)ctx;
    (void)car;
    (void)data;
    return 1;
}

/* Original: func_00426D50 +0x00. Host controller boundary only. */
static void original_pad(ee_ctx *e)
{
    const trajectory_input *input = gt4_phys_pad.ctx;
    uint32_t object = ee_r32(e, input->player + CAR_FIELD_CONTROLLER);
    if (e->r[4].uw[0] == object)
        memcpy(e->ram + e->r[5].uw[0], input->command, 32);
    else {
        /* Original: func_00426D50 +0x20. Inactive guide controllers clear
           their command through the original function, not the player tape. */
        e->r[4].uw[0] = e->r[5].uw[0];
        recomp_lookup(0x003557e8)(e);
    }
}

/* Original: func_00426CE0 +0x00. No wheel is attached in this fixture. */
static void original_wheel(ee_ctx *e)
{
    const trajectory_input *input = gt4_phys_pad.ctx;
    ee_w16(e, e->r[5].uw[0], input->wheel_value);
    ee_w16(e, e->r[5].uw[0] + 2, input->wheel_value);
}

static void presentation_boundary(ee_ctx *e)
{
    (void)e;
}

static void headless_record(ee_ctx *e)
{
    e->r[2].uw[0] = 1;
}

static ee_fn oracle_hook(uint32_t address)
{
    switch (address) {
    case 0x00426d50:
        return original_pad;
    case 0x00426ce0:
        return original_wheel;
    /* Same explicit presentation boundaries as gt4_step/headless. */
    case 0x0035be60:
    case 0x00354a78:
    case 0x0035f658:
    case 0x003f71c8:
    case 0x00329858:
        return presentation_boundary;
    case 0x003f59a0:
    case 0x003f5780:
        return headless_record;
    default:
        return NULL;
    }
}

static float reject_original(uint32_t address, uint32_t a0, uint32_t a1, float f12, float f13)
{
    trajectory_input *input = gt4_phys_pad.ctx;
    input->missing = address;
    (void)a0;
    (void)a1;
    (void)f12;
    (void)f13;
    return 0;
}

static int original_call(ee_ctx *e, uint32_t address, uint32_t arg, uint32_t buffer)
{
    memset(e->r, 0, sizeof e->r);
    memset(e->fpr, 0, sizeof e->fpr);
    e->r[4].uw[0] = arg;
    e->r[6].uw[0] = buffer;
    e->r[29].uw[0] = TEST_STACK_TOP;
    jmp_buf jump;
    e->exit_jmp = &jump;
    if (setjmp(jump)) {
        fprintf(stderr, "original stopped %08x: %s\n", e->end_pc, e->end_detail);
        return 1;
    }
    ee_fn function = recomp_lookup(address);
    CHECK(function);
    function(e);
    return 0;
}

/* Original: func_00389B08 +0x00, func_00389758 +0x00. World integration
   invokes queue age, before-step, full solver and after-step in this order. */
static int compare_ram(const ee_ctx *e, uint8_t *native, unsigned frame)
{
    if (!memcmp(native, e->ram, TEST_STACK_BOTTOM) &&
        !memcmp(native + TEST_STACK_TOP, e->ram + TEST_STACK_TOP, EE_RAM_SIZE - TEST_STACK_TOP))
        return 0;
    for (uint32_t i = 0; i < EE_RAM_SIZE; i++) {
        if (i == TEST_STACK_BOTTOM)
            i = TEST_STACK_TOP;
        if (native[i] != e->ram[i]) {
            fprintf(stderr, "frame=%u address=%08x native=%02x original=%02x\n", frame, i,
                    native[i], e->ram[i]);
            return 1;
        }
    }
    return 1;
}

static int differential_frame(ee_ctx *e, uint8_t *native, uint32_t cond, unsigned frame)
{
    uint32_t race = GT4_U32(cond);
    gt4_phys_mem = e->ram;
    gt4_phys_fpu_enter();
    CHECK(!original_call(e, 0x003b75d8, race + TEST_RACE_EVENT_QUEUE, 0));
    CHECK(!original_call(e, 0x003c0208, race, 0));
    CHECK(!original_call(e, 0x0034db30, cond, TEST_STEP_BUFFER));
    CHECK(!original_call(e, 0x003c0310, race, 0));
    gt4_phys_mem = native;
    gt4_phys_fpu_enter();
    gt4_race_queue_age(race + TEST_RACE_EVENT_QUEUE);
    gt4_race_before_step(race);
    gt4_step(cond, 0, TEST_STEP_BUFFER);
    gt4_race_after_step(race);
    trajectory_input *input = gt4_phys_pad.ctx;
    CHECK(!input->missing);
    if (getenv("MOON_REPLAY_ARITHMETIC_NEGATIVE"))
        native[input->player + CAR_PLAYER] ^= 1;
    return compare_ram(e, native, frame);
}

static int check_lap_event(ee_ctx *original, uint8_t *native, uint32_t cond)
{
    /* 003F5B80 +0x64..0x80: replay mode skips persistent records but
       retains the lap and time messages through 0035E550. */
    memset(original->r, 0, sizeof original->r);
    original->r[4].uw[0] = cond;
    original->r[6].uw[0] = 1;
    original->r[8].uw[0] = 90000;
    original->r[9].uw[0] = 80000;
    original->r[29].uw[0] = TEST_STACK_TOP;
    gt4_phys_mem = original->ram;
    gt4_phys_fpu_enter();
    recomp_lookup(0x003f5b80)(original);
    gt4_phys_mem = native;
    CHECK(original->end_reason == EE_END_NONE);
    gt4_session(cond, 0xa8, 0, 1, 0, 90000, 80000, NULL);
    if (getenv("MOON_REPLAY_ARITHMETIC_NEGATIVE"))
        native[GT4_U32(cond) + TEST_RACE_EVENT_QUEUE + 8] ^= 1;
    return compare_ram(original, native, 0);
}

static int check_recording(const char *disc, unsigned demo, int full, int lap_only,
                           int choices_only)
{
    const char *why;
    char error[256], path[64];
    gt4_vol *vol = gt4_vol_open(disc, &why);
    gt4_disc_replay *reader = gt4_disc_replay_open(vol);
    gt4_replay_file file;
    snprintf(path, sizeof path, "/replay/Rdemo%u/Rdemo%u", demo, demo);
    CHECK(reader && gt4_disc_replay_read(reader, path, &file, error, sizeof error));
    gt4_race_desc desc = {0};
    snprintf(desc.course, sizeof desc.course, "%s", file.course);
    desc.cars = file.cars;
    desc.manual = file.manual;
    desc.bspec = file.disc_bspec;
    desc.disc_settings = file.disc_has_settings ? file.event_row : NULL;
    desc.disc_race_options = file.disc_info + 0x50;
    desc.time_trial = file.time_trial;
    desc.world_mode = file.world_mode;
    for (int i = 0; i < desc.cars; i++) {
        desc.car[i] = file.car_id[i];
        desc.owned_car[i] = file.owned[i];
        desc.disc_entry[i] = file.disc_info + 0x960 + i * 0x228;
    }
    gt4_race_run *run = gt4_race_run_open(disc, &desc, error, sizeof error);
    CHECK(run);
    CHECK(gt4_race_run_cars(run) == file.cars);
    if (file.disc_segments) {
        /* 00359D40 +0xA4..0x134. Restoration itself has a separate
           0035ABA8 oracle; this comparator starts from that shared state. */
        CHECK(file.disc_segments == 1 && !file.disc_segment_at[0]);
        uint32_t cond = gt4_race_run_conductor(run);
        GT4_U32(cond + CONDUCTOR_FIELD_RACE_CLOCK) = 100;
        GT4_U32(GT4_U32(cond) + TEST_RACE_REPLAY_CLOCK) = 0;
        disc_checkpoint_restore(cond, 0, file.disc_info + file.disc_segment_offset[0]);
        GT4_F32(gt4_race_run_car(run, 0) + CAR_VIEW_BLEND) = 0;
    }
    uint8_t *native = gt4_phys_mem;
    trajectory_input input = {file.tape, gt4_race_run_car(run, 0), 0, 1};
    gt4_phys_pad = (gt4_phys_pads){&input, read_pad, wheel, vibration};
    ee_ctx original;
    ee_init(&original);
    original.lookup = recomp_lookup;
    original.hook = oracle_hook;
    memcpy(original.ram, native, EE_RAM_SIZE);
    if (lap_only)
        CHECK(!check_lap_event(&original, native, gt4_race_run_conductor(run)));
    if (choices_only) {
        /* 003F1790 +0xB8..0xE4 divides all fifteen pace ratios by 100.
           The old PCSX2 capture used host nearest division, not EE SRT. */
        gt4_phys_mem = original.ram;
        CHECK(!original_call(&original, 0x003f1790, 0, 0));
        gt4_phys_mem = native;
        if (getenv("MOON_REPLAY_ARITHMETIC_NEGATIVE"))
            native[0x006220b0] ^= 1;
        CHECK(!compare_ram(&original, native, 0));
        printf("PASS original B-Spec initializer: level4=%08x/%08x, fifteen EE ratios\n",
               GT4_U32(0x006220b0 + 4 * 4), GT4_U32(0x006220c8 + 4 * 4));
    }
    size_t steps = (lap_only || choices_only) ? 0
                   : full                     ? file.steps
                   : file.steps < 1200        ? file.steps
                                              : 1200;
    for (size_t tick = 0; tick < steps; tick++) {
        input.command = file.tape + tick * 32;
        if (differential_frame(&original, native, gt4_race_run_conductor(run), (unsigned)tick)) {
            fprintf(stderr, "recording %s failed\n", path);
            return 1;
        }
    }
    printf(
        "PASS %s: %zu consecutive RAM comparisons outside oracle stack, shared initial context\n",
        path, steps);
    fflush(stdout);
    ee_free(&original);
    gt4_race_run_close(run);
    gt4_replay_free(&file);
    gt4_disc_replay_close(reader);
    gt4_vol_close(vol);
    return 0;
}

/* External pad policy is sampled once; both solvers receive the same bytes.
   This compares dynamic state, not a fresh console constructor or player skill. */
static int check_mission(const char *disc, const char *code)
{
    gt4_career_race_lookup_data lookup;
    gt4_career_license_exam exam;
    char error[256];
    CHECK(gt4_career_race_lookup_data_load(disc, &lookup));
    CHECK(gt4_career_mission_prepare(&exam, disc, &lookup, code, error, sizeof error));
    gt4_race_run *run = gt4_race_run_open(disc, &exam.desc, error, sizeof error);
    CHECK(run);
    uint8_t command[32] = {0}, *native = gt4_phys_mem;
    trajectory_input input = {command, gt4_race_run_car(run, 0), 0, 0};
    gt4_phys_pad = (gt4_phys_pads){&input, read_pad, wheel, vibration};
    gt4_orig_call = reject_original;
    ee_ctx original;
    CHECK(!ee_init(&original));
    original.lookup = recomp_lookup;
    original.hook = oracle_hook;
    memcpy(original.ram, native, EE_RAM_SIZE);
    unsigned tick;
    int outcome = 0;
    for (tick = 0; tick < 86400 && !outcome; tick++) {
        memset(command, 0, sizeof command);
        mission_pad_fixture(run, input.player, command);
        CHECK(!differential_frame(&original, native, gt4_race_run_conductor(run), tick));
        outcome = gt4_mission_presentation(
            gt4_ptr(GT4_U32(gt4_race_run_conductor(run)) + TEST_RACE_EVENT_QUEUE), outcome);
    }
    gt4_race_hud hud;
    CHECK(outcome && gt4_race_run_hud(run, 0, &hud));
    if (getenv("MOON_MISSION_EXPECT_RANK"))
        CHECK(hud.finished && hud.rank == atoi(getenv("MOON_MISSION_EXPECT_RANK")));
    printf(
        "PASS mission %s: %u consecutive original/native steps, outcome=%d rank=%d finished=%d\n",
        code, tick, outcome, hud.rank, hud.finished);
    ee_free(&original);
    gt4_race_run_close(run);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2 || argc == 3);
    if (argc == 3 && !strncmp(argv[2], "--mission=", 10))
        return check_mission(argv[1], argv[2] + 10);
    int full = argc == 3 && !strcmp(argv[2], "--full");
    int lap_only = argc == 3 && !strcmp(argv[2], "--lap-event");
    int choices_only = argc == 3 && !strcmp(argv[2], "--bspec-choices");
    unsigned first = 0, end = 13;
    if (argc == 3 && !strncmp(argv[2], "--full-demo=", 12)) {
        first = (unsigned)strtoul(argv[2] + 12, NULL, 10);
        CHECK(first < end);
        end = first + 1;
        full = 1;
    }
    if (lap_only || choices_only) {
        first = 9;
        end = 10;
    }
    for (unsigned demo = first; demo < end; demo++) {
        CHECK(!check_recording(argv[1], demo, full, lap_only, choices_only));
    }
    return 0;
}
