/* Whole-step differential licence attempts. Constructors and presentation
   remain separate boundaries; no physics rewrite is installed in the oracle. */
#include "ee.h"
#include "career_license_disc.h"
#include "disc_replay.h"
#include "car.h"
#include "fields_conductor.h"
#include "fields_race.h"
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "trajectory line %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
enum { TEST_STACK_BOTTOM = 0x01e18000, TEST_STACK_TOP = 0x01ef0000, TEST_STEP_BUFFER = 0x01eeffc0 };
typedef struct {
    const uint8_t *command;
    uint32_t player, missing;
    int controller_passthrough;
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
    state[0] = state[1] = 0;
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
    trajectory_input *input = gt4_phys_pad.ctx;
    uint32_t object = ee_r32(e, input->player + CAR_FIELD_CONTROLLER);
    if (e->r[4].uw[0] == object)
        memcpy(e->ram + e->r[5].uw[0], input->command, 32);
    else {
        /* Original: func_00426D50 +0x00. Execute the guide's original
           reader; only the player's host controller supplies the tape. */
        input->controller_passthrough = 1;
        recomp_lookup(0x00426d50)(e);
        input->controller_passthrough = 0;
    }
}

/* Original: func_00426CE0 +0x00. No wheel is attached in this fixture. */
static void original_wheel(ee_ctx *e)
{
    memset(e->ram + e->r[5].uw[0], 0, 4);
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
        return ((trajectory_input *)gt4_phys_pad.ctx)->controller_passthrough ? NULL : original_pad;
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
    CHECK(!original_call(e, 0x003b75d8, race + RACE_FIELD_EVENT_QUEUE, 0));
    CHECK(!original_call(e, 0x003c0208, race, 0));
    CHECK(!original_call(e, 0x0034db30, cond, TEST_STEP_BUFFER));
    CHECK(!original_call(e, 0x003c0310, race, 0));
    gt4_phys_mem = native;
    gt4_phys_fpu_enter();
    gt4_race_queue_age(race + RACE_FIELD_EVENT_QUEUE);
    gt4_race_before_step(race);
    gt4_step(cond, 0, TEST_STEP_BUFFER);
    gt4_race_after_step(race);
    trajectory_input *input = gt4_phys_pad.ctx;
    CHECK(!input->missing);
    if (getenv("GT4_LICENSE_TRAJECTORY_NEGATIVE"))
        native[input->player + CAR_PLAYER] ^= 1;
    return compare_ram(e, native, frame);
}

static int attempt(const char *disc, const char *code)
{
    gt4_career_race_lookup_data tables;
    CHECK(gt4_career_race_lookup_data_load(disc, &tables));
    char error[256], path[80];
    const char *why;
    gt4_vol *vol = gt4_vol_open(disc, &why);
    gt4_disc_replay *reader = vol ? gt4_disc_replay_open(vol) : NULL;
    CHECK(reader);
    snprintf(path, sizeof path, "/replay/license/us%s", code);
    gt4_replay_file tape;
    gt4_career_license_exam exam;
    CHECK(gt4_disc_replay_read(reader, path, &tape, error, sizeof error));
    CHECK(gt4_career_license_prepare(&exam, disc, &tables, code, error, sizeof error));
    /* Original: func_00110230 +0x98 */
    exam.desc.manual = tape.manual;
    exam.desc.assist_asm = tape.assist_asm;
    exam.desc.assist_tcs = tape.assist_tcs;
    gt4_race_run *run = gt4_race_run_open(disc, &exam.desc, error, sizeof error);
    CHECK(run);
    uint8_t *native = gt4_phys_mem;
    ee_ctx original;
    CHECK(!ee_init(&original));
    original.lookup = recomp_lookup;
    original.hook = oracle_hook;
    memcpy(original.ram, native, EE_RAM_SIZE);
    trajectory_input input = {tape.tape, gt4_race_run_car(run, 0), 0};
    gt4_phys_pad.ctx = &input;
    gt4_phys_pad.read = read_pad;
    gt4_phys_pad.wheel_state = wheel;
    gt4_phys_pad.write = vibration;
    gt4_orig_call = reject_original;
    unsigned frame = 0;
    uint8_t idle[32] = {0};
    idle[1] = 1;
    /* Explicit test timeout, never a game timing parameter. */
    for (; frame < tape.steps + 1800 && !gt4_race_run_finished(run); frame++) {
        input.command = frame < tape.steps ? tape.tape + frame * 32 : idle;
        CHECK(!differential_frame(&original, native, gt4_race_run_conductor(run), frame));
    }
    printf(
        "PASS whole-step comparison %s frames=%u finished=%d; native constructor, headless host boundaries; not an exam-pass claim\n",
        code, frame, gt4_race_run_finished(run));
    gt4_phys_mem = native;
    ee_free(&original);
    gt4_race_run_close(run);
    gt4_phys_pad.ctx = NULL;
    gt4_replay_free(&tape);
    gt4_disc_replay_close(reader);
    gt4_vol_close(vol);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 3);
    if (!strcmp(argv[2], "--all")) {
        const char *groups[] = {"l0b", "l0a", "lib", "lia", "l0s"};
        for (unsigned group = 0; group < 5; group++)
            for (unsigned number = 1; number <= 16; number++) {
                char code[32];
                snprintf(code, sizeof code, "%s%04u", groups[group], number);
                CHECK(!attempt(argv[1], code));
                fflush(stdout);
            }
        puts(
            "PASS 80 independent full-step command comparisons; constructor/presentation and 80 successful exams not proved");
        return 0;
    }
    return attempt(argv[1], argv[2]);
}
