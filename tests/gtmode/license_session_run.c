/* Original license decisions with existing native car/queue code as explicit
   boundaries. Physics transitions and event packing have separate oracles. */
#include "ee.h"
#include "car.h"
#include "race_run.h"
#include "license_session.h"
#include "career_license_disc.h"
#include "disc_replay.h"
#include "fields_conductor.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "license session line %d case %u pc %08x: %s\n", __LINE__, test, pc,   \
                    #x);                                                                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static unsigned test;
static uint32_t pc;
static void finish(ee_ctx *c)
{
    gt4_car_finish_drive(c->r[4].uw[0], (int)c->r[5].uw[0]);
}
static void release(ee_ctx *c)
{
    gt4_car_take_over(c->r[4].uw[0], 5);
}
static void event(ee_ctx *c)
{
    gt4_race_event(c->r[4].uw[0], (int)c->r[5].uw[0], c->r[6].uw[0], (int)c->r[7].uw[0]);
}
static void lap(ee_ctx *c)
{
    gt4_race_lap_event(c->r[4].uw[0], c->r[5].uw[0], c->r[6].uw[0], c->r[8].uw[0], c->r[9].uw[0]);
}
static void gap(ee_ctx *c)
{
    gt4_race_gap(c->r[4].uw[0], c->r[5].uw[0], c->r[6].uw[0]);
}
static void car_at(ee_ctx *c)
{
    c->r[2].ud[0] = c->r[4].uw[0] + 0x6698u + c->r[5].uw[0] * CAR_SIZE;
}
static void record(ee_ctx *c)
{
    (void)c;
}
static ee_fn boundary(uint32_t p)
{
    switch (p) {
    case 0x343d78:
        return finish;
    case 0x3454a8:
        return release;
    case 0x35df80:
        return event;
    case 0x35e550:
        return lap;
    case 0x35e078:
        return gap;
    case 0x34c190:
        return car_at;
    case 0x359c10:
        return record;
    default:
        return NULL;
    }
}
static ee_fn lookup(uint32_t p)
{
    ee_fn f = boundary(p);
    return f ? f : recomp_lookup(p);
}
static int call(ee_ctx *c, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, float force)
{
    memset(c->r, 0, sizeof c->r);
    memset(c->fpr, 0, sizeof c->fpr);
    c->r[4].uw[0] = a0;
    c->r[5].uw[0] = a1;
    c->r[6].uw[0] = a2;
    c->r[7].uw[0] = a3;
    c->fpr[12].f = force;
    c->r[29].uw[0] = 0x1ef0000;
    jmp_buf jump;
    c->exit_jmp = &jump;
    gt4_phys_mem = c->ram;
    if (setjmp(jump)) {
        fprintf(stderr, "original stopped at %08x: %s\n", c->end_pc, c->end_detail);
        return 0;
    }
    recomp_lookup(pc)(c);
    return 1;
}
static uint32_t next(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}
/* Original: func_003F8D70 +0x00 */
/* These are real disc-prepared native
   checkpoints, not a comparison of the original constructor or solver.
   Only the original caller's test stack is excluded (run_diff.py). */
static int actual_checkpoint(ee_ctx *original, gt4_race_run *race, uint8_t *before)
{
    uint8_t *native = gt4_phys_mem;
    uint32_t cond = gt4_race_run_conductor(race), car = gt4_race_run_car(race, 0);
    memcpy(before, native, EE_RAM_SIZE);
    memcpy(original->ram, native, EE_RAM_SIZE);
    gt4_license_evaluate(cond, car);
    pc = 0x003f8d70;
    CHECK(call(original, cond, car, 0, 0, 0));
    if (getenv("GT4_LICENSE_ACTUAL_NEGATIVE"))
        native[car + CAR_PLAYER] ^= 1;
    enum { ORACLE_STACK_BEGIN = 0x01e18000, ORACLE_STACK_END = 0x01ef0000 };
    CHECK(!memcmp(native, original->ram, ORACLE_STACK_BEGIN));
    CHECK(!memcmp(native + ORACLE_STACK_END, original->ram + ORACLE_STACK_END,
                  EE_RAM_SIZE - ORACLE_STACK_END));
    gt4_phys_mem = native;
    memcpy(native, before, EE_RAM_SIZE);
    return 0;
}

static int actual_idle(void *user, uint32_t car, uint8_t *command, const uint8_t *event)
{
    (void)user;
    (void)car;
    memset(command, 0, 32);
    command[1] = 1;
    command[3] = event[3];
    return 1;
}

static int actual_wheel(void *user, uint32_t car, int16_t state[2])
{
    (void)user;
    (void)car;
    state[0] = state[1] = 0;
    return 1;
}

static int actual_vibration(void *user, uint32_t car, const uint8_t *data)
{
    (void)user;
    (void)car;
    (void)data;
    return 1;
}

static int actual_exam(const char *disc, const gt4_career_race_lookup_data *lookup,
                       const char *code, ee_ctx *original, uint8_t *before)
{
    gt4_career_license_exam exam;
    char error[256];
    CHECK(gt4_career_license_prepare(&exam, disc, lookup, code, error, sizeof error));
    gt4_race_run *race = gt4_race_run_open(disc, &exam.desc, error, sizeof error);
    CHECK(race);
    gt4_phys_pad.read = actual_idle;
    gt4_phys_pad.wheel_state = actual_wheel;
    gt4_phys_pad.write = actual_vibration;
    gt4_phys_fpu_enter();
    for (unsigned frame = 0; frame <= 1200; frame++) {
        if (!(frame % 300)) {
            test++;
            CHECK(!actual_checkpoint(original, race, before));
        }
        if (frame < 1200)
            CHECK(gt4_race_run_frame(race));
    }
    printf("PASS actual license %s: 5 native checkpoints, original evaluation/RAM exact\n", code);
    fflush(stdout);
    gt4_race_run_close(race);
    return 0;
}

typedef struct {
    uint32_t player;
    const gt4_replay_file *tape;
    size_t cursor;
} teacher_input;

static int teacher_pad(void *user, uint32_t car, uint8_t *command, const uint8_t *event)
{
    teacher_input *input = user;
    if (car != input->player)
        return 0;
    if (input->cursor < input->tape->steps)
        memcpy(command, input->tape->tape + 32 * input->cursor++, 32);
    else
        actual_idle(NULL, car, command, event);
    return 1;
}

/* Original: func_003F8D70 +0x00 */
/* Same conductor/race comparison windows
   as the synthetic oracle below. Car/event callbacks remain explicit
   boundaries; this is not a differential run of the complete solver. */
static int teacher_checkpoint(ee_ctx *original, gt4_race_run *run, uint8_t *before)
{
    uint8_t *native = gt4_phys_mem;
    uint32_t cond = gt4_race_run_conductor(run), car = gt4_race_run_car(run, 0);
    uint32_t race = GT4_U32(cond);
    size_t cond_size = CONDUCTOR_FIELD_SESSION_VTABLE + sizeof(uint32_t);
    size_t race_size = 0xa00;
    memcpy(before, native + cond, cond_size);
    memcpy(before + cond_size, native + race, race_size);
    memcpy(original->ram + cond, native + cond, cond_size);
    memcpy(original->ram + race, native + race, race_size);
    gt4_license_evaluate(cond, car);
    pc = 0x003f8d70;
    CHECK(call(original, cond, car, 0, 0, 0));
    if (getenv("GT4_LICENSE_ACTUAL_NEGATIVE"))
        native[car + CAR_PLAYER] ^= 1;
    CHECK(!memcmp(native + cond, original->ram + cond, cond_size));
    CHECK(!memcmp(native + race, original->ram + race, race_size));
    gt4_phys_mem = native;
    memcpy(native + cond, before, cond_size);
    memcpy(native + race, before + cond_size, race_size);
    return 0;
}

static int teacher_exam(const char *disc, const gt4_career_race_lookup_data *tables,
                        const char *code, ee_ctx *original, uint8_t *before,
                        gt4_disc_replay *replays)
{
    char path[80], error[256];
    snprintf(path, sizeof path, "/replay/license/us%s", code);
    gt4_replay_file tape;
    gt4_career_license_exam exam;
    CHECK(gt4_disc_replay_read(replays, path, &tape, error, sizeof error));
    CHECK(tape.steps && gt4_career_license_prepare(&exam, disc, tables, code, error, sizeof error));
    /* Original: func_00110230 +0x98 */
    /* Disc commands drive the player exam;
       this is intentionally distinct from the original demonstration scene. */
    exam.desc.manual = tape.manual;
    exam.desc.assist_asm = tape.assist_asm;
    exam.desc.assist_tcs = tape.assist_tcs;
    gt4_race_run *run = gt4_race_run_open(disc, &exam.desc, error, sizeof error);
    CHECK(run);
    teacher_input input = {gt4_race_run_car(run, 0), &tape, 0};
    gt4_phys_pad.ctx = &input;
    gt4_phys_pad.read = teacher_pad;
    gt4_phys_pad.wheel_state = actual_wheel;
    gt4_phys_pad.write = actual_vibration;
    gt4_phys_fpu_enter();
    memcpy(original->ram, gt4_phys_mem, EE_RAM_SIZE);
    size_t frames = 0;
    /* Test timeout only; it is not a gameplay timing parameter. */
    while (frames < tape.steps + 1800 && !gt4_race_run_finished(run)) {
        CHECK(gt4_race_run_frame(run));
        CHECK(!teacher_checkpoint(original, run, before));
        test++;
        frames++;
    }
    int32_t state;
    uint32_t elapsed;
    CHECK(gt4_race_run_license_result(run, &state, &elapsed));
    printf("PASS license evaluation %s frames=%zu tape=%zu/%zu finished=%d state=%d time=%u\n",
           code, frames, input.cursor, tape.steps, gt4_race_run_finished(run), state, elapsed);
    fflush(stdout);
    gt4_phys_pad.ctx = NULL;
    gt4_race_run_close(run);
    gt4_replay_free(&tape);
    return 0;
}

static int actual_catalog(const char *disc, int tapes)
{
    gt4_career_race_lookup_data tables;
    CHECK(gt4_career_race_lookup_data_load(disc, &tables));
    ee_ctx original;
    CHECK(!ee_init(&original));
    original.lookup = lookup;
    original.hook = boundary;
    uint8_t *before = malloc(EE_RAM_SIZE);
    CHECK(before);
    const char *error;
    gt4_vol *vol = tapes ? gt4_vol_open(disc, &error) : NULL;
    gt4_disc_replay *replays = vol ? gt4_disc_replay_open(vol) : NULL;
    CHECK(!tapes || replays);
    const char *groups[] = {"l0b", "l0a", "lib", "lia", "l0s"};
    for (unsigned group = 0; group < 5; group++)
        for (unsigned number = 1; number <= 16; number++) {
            char code[32];
            snprintf(code, sizeof code, "%s%04u", groups[group], number);
            if (tapes)
                CHECK(!teacher_exam(disc, &tables, code, &original, before, replays));
            else
                CHECK(!actual_exam(disc, &tables, code, &original, before));
        }
    if (replays)
        gt4_disc_replay_close(replays);
    if (vol)
        gt4_vol_close(vol);
    free(before);
    ee_free(&original);
    if (tapes) {
        printf(
            "PASS 80 disc command tapes / %u frame evaluation comparisons; shared car/event boundaries, no complete PS2 trajectory or rendering claim.\n",
            test);
        return 0;
    }
    puts(
        "PASS 80 actual native exams / 400 original evaluation checkpoints; constructor, solver, controls and complete runs are separate proof boundaries.");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[2], "--catalog-frames"))
        return actual_catalog(argv[1], 0);
    if (argc == 3 && !strcmp(argv[2], "--catalog-tapes"))
        return actual_catalog(argv[1], 1);
    CHECK(argc==2);
    char message[256];
    gt4_race_desc desc = {0};
    desc.cars = 1;
    desc.car[0] = 0x3b;
    strcpy(desc.course, "tsukuba");
    gt4_race_run *race = gt4_race_run_open(argv[1], &desc, message, sizeof message);
    if (!race) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    uint32_t c = gt4_race_run_conductor(race), car = gt4_race_run_car(race, 0), r = GT4_U32(c);
    GT4_U32(c + 0x10140) = GT4_LICENSE_SESSION_VT;
    GT4_U32(GT4_U32(r + 0x84) + 0x64) = 0x6807a8;
    uint8_t *native = gt4_phys_mem, *base = malloc(0x2000000);
    CHECK(base);
    memcpy(base, native, 0x2000000);
    ee_ctx original;
    CHECK(!ee_init(&original));
    original.lookup = lookup;
    original.hook = boundary;
    memcpy(original.ram, native, 0x2000000);
    uint32_t seed = 0x20261006;
    gt4_phys_fpu_enter();
    /* Query/control domain, including signed counters and every policy byte. */
    for (test = 0; test < 8192; test++) {
        gt4_phys_mem = native;
        memcpy(native + c, base + c, 0x10150);
        GT4_U8(c + 0xf859) = (uint8_t)(test & 255);
        GT4_U8(c + 0xf81a) = (uint8_t)((test >> 8) & 7);
        GT4_U8(c + 0xf858) = (uint8_t)(test >> 11);
        GT4_U8(c + 0xf85e) = (uint8_t)(test >> 12);
        memcpy(original.ram + c, native + c, 0x10150);
        int a = gt4_license_finish_mode(c, test & 1);
        pc = 0x3f8818;
        CHECK(call(&original,c,test&1,0,0,0));
        CHECK(a==(int)original.r[2].uw[0]);
        gt4_phys_mem = native;
        GT4_U8(c + 0xf85c) = (uint8_t)next(&seed);
        GT4_U16(c + 0x1003e) = (uint16_t)next(&seed);
        memcpy(original.ram + c, native + c, 0x10150);
        int n = gt4_license_wait(c, 1);
        pc = 0x3fa248;
        CHECK(call(&original,c,0,0,0,0));
        CHECK(n==(int32_t)original.r[2].uw[0]);
        CHECK(!memcmp(native+c+0x1003e,original.ram+c+0x1003e,2));
        gt4_phys_mem = native;
        n = gt4_license_wait(c, 0);
        pc = 0x3fa2b0;
        CHECK(call(&original,c,0,0,0,0));
        CHECK(n==(int32_t)original.r[2].uw[0]);
    }
    /* Contact thresholds: both contact classes, masses, disabled/zero limits,
    already finished/pit cars, and timed touches (0035DF28). */
    for (test = 0; test < 8192; test++) {
        gt4_phys_mem = native;
        memcpy(native + c, base + c, 0x10150);
        GT4_U8(car + 0x56a) = (uint8_t)(test % 11 == 0);
        GT4_U8(car + 0x56b) = (uint8_t)(test % 13 == 0);
        GT4_U8(car + 0x5b6) = (uint8_t)(test % 17 == 0);
        GT4_U8(c + 0xf860) = (uint8_t)next(&seed);
        GT4_U8(c + 0xf861) = (uint8_t)next(&seed);
        GT4_U8(c + 0xf85e) = (uint8_t)(test % 5 ? next(&seed) : 0);
        GT4_U16(c + 0x1003c) = (uint16_t)next(&seed);
        uint32_t kind = test % 4, detail = (test / 4) % 18;
        float force = (float)((int)(next(&seed) % 20001) - 10000);
        memcpy(original.ram + c, native + c, 0x10150);
        gt4_license_collision(c, car, kind, detail, force);
        pc = 0x35fe80;
        CHECK(call(&original,c,car,kind,detail,force));
        CHECK(!memcmp(native+c,original.ram+c,0x10150));
    }
    /* Stop area: wheel masks and speeds on both sides of the exact threshold. */
    for (test = 0; test < 4096; test++) {
        gt4_phys_mem = native;
        memcpy(native + c, base + c, 0x10150);
        for (unsigned i = 0; i < 4; i++)
            GT4_U8(car + 0x7eb + i * 0x34) = (uint8_t)((test >> i) & 1 ? 4 : 0);
        GT4_F32(car + 0x648) = (float)(test % 257) / 512;
        GT4_F32(car + 0x64c) = 0;
        GT4_F32(car + 0x650) = 0;
        memcpy(original.ram + c, native + c, 0x10150);
        float fraction;
        int hit = gt4_license_stop(car, &fraction);
        pc = 0x3f91f0;
        CHECK(call(&original,car,0x1e30000,0,0,0));
        CHECK(hit==(int)original.r[2].uw[0]);
        CHECK(!memcmp(&fraction,original.ram+0x1e30000,4));
    }
    /* Completion matrix: thresholds/ties, finish policy, economy, positional
    results, replay/demo/guest cars. Actual car and queue transitions execute. */
    for (test = 0; test < 2048; test++) {
        gt4_phys_mem = native;
        memcpy(native, base, 0x2000000);
        GT4_U8(c + 0xf81a) = 2;
        GT4_U8(c + 0xf858) = (uint8_t)(test % 5);
        GT4_U8(c + 0xf859) = (uint8_t)((test / 5) % 4);
        GT4_U8(c + 0xf85e) = 0;
        GT4_U8(c + 0xcbdc) = (uint8_t)(test % 19 == 0);
        GT4_U32(r + 0xdc) = test % 23 == 0;
        GT4_U32(r + 0xf0) = test % 29 == 0;
        GT4_U8(car + 0x545) = (uint8_t)(test % 31 == 0);
        GT4_U8(car + 0x5b3) = (uint8_t)(test % 7);
        GT4_U8(c + 0xf85d) = (uint8_t)(test % 4 ? 0 : next(&seed));
        GT4_U32(0x6244e8) = test % 4;
        GT4_F32(car + 0x5cc) = (float)(test % 99);
        GT4_F32(car + 0x5c8) = (float)(test % 11) * 0.01f;
        uint8_t *row = gt4_ptr(GT4_U32(r + 0x70));
        memset(row + 0x76, 0, 6);
        row[0x77] = 10;
        row[0x79] = 11;
        row[0x7b] = 12;
        uint32_t elapsed = 9999 + (test % 7) * 500;
        GT4_U32(r + 0x968) = 11000;
        memcpy(original.ram, native, 0x2000000);
        gt4_license_finish(c, car, elapsed);
        pc = 0x3f8ab0;
        CHECK(call(&original,c,car,elapsed,0,0));
        if (getenv("GT4_LICENSE_SESSION_NEGATIVE") && !test)
            native[r + 0x88] ^= 1;
        for (uint32_t i = c; i < c + 0x10150; i++)
            if (native[i] != original.ram[i]) {
                fprintf(stderr, "cond %x: %02x vs %02x\n", i, native[i], original.ram[i]);
                CHECK(0);
            }
        CHECK(!memcmp(native+r,original.ram+r,0xa00));
    }
    /* Per-frame completion/failure and direct failure policy, including
    timer expiration, interrupted lead, flag precedence and demonstration. */
    for (test = 0; test < 8192; test++) {
        gt4_phys_mem = native;
        memcpy(native, base, 0x2000000);
        GT4_U8(c + 0xf81a) = (uint8_t)(test % 7);
        GT4_U8(c + 0xf858) = (uint8_t)(test % 4);
        GT4_U8(c + 0xf859) = (uint8_t)((test / 7) % 4);
        GT4_U8(c + 0xf85e) = (uint8_t)(test % 3 ? 0 : 1);
        GT4_U16(c + 0x1003c) = (uint16_t)(test % 2);
        GT4_U8(c + 0xf85c) = (uint8_t)(test % 5 ? 0 : 1);
        GT4_U16(c + 0x1003e) = (uint16_t)(test % 3 ? 0 : 59);
        GT4_U8(car + 0x5b3) = (uint8_t)(test % 5);
        GT4_U8(car + 0x5b7) = (uint8_t)(test % 8);
        GT4_U8(car + 0x545) = (uint8_t)(test % 31 == 0);
        GT4_U8(car + 0x5b6) = (uint8_t)(test % 23 == 0);
        GT4_U8(car + 0x5b9) = (uint8_t)(test % 17 == 0);
        GT4_U8(car + 0x56a) = (uint8_t)(test % 13 == 0 ? 2 : 0);
        GT4_U32(r + 0xf0) = test % 19 == 0;
        GT4_U8(c + 0xcbdc) = (uint8_t)(test % 29 == 0);
        GT4_U32(r + 0xdc) = test % 37 == 0;
        for (unsigned i = 0; i < 4; i++)
            GT4_U8(car + 0x7eb + i * 0x34) = (uint8_t)(test % 2 ? 4 : 0);
        GT4_U32(c + 0xf898) = 30000;
        GT4_F32(car + 0x648) = 0.125f;
        GT4_F32(car + 0x64c) = GT4_F32(car + 0x650) = 0;
        uint8_t *row = gt4_ptr(GT4_U32(r + 0x70));
        memset(row + 0x76, 0, 6);
        row[0x77] = 11;
        row[0x79] = 12;
        row[0x7b] = 13;
        memcpy(original.ram, native, 0x2000000);
        if (test & 1) {
            gt4_license_evaluate(c, car);
            pc = 0x3f8d70;
            CHECK(call(&original,c,car,0,0,0));
        } else {
            int reason = (int)(test % 9) - 1;
            gt4_license_fail(c, car, reason);
            pc = 0x3f8980;
            CHECK(call(&original,c,car,(uint32_t)reason,0,0));
        }
        for (uint32_t i = c; i < c + 0x10150; i++)
            if (native[i] != original.ram[i]) {
                fprintf(stderr, "cond %x: %02x vs %02x\n", i, native[i], original.ram[i]);
                CHECK(0);
            }
        CHECK(!memcmp(native+r,original.ram+r,0xa00));
    }
    free(base);
    ee_free(&original);
    gt4_phys_mem = native;
    gt4_race_run_close(race);
    puts(
        "PASS licence session: 30720 original policy/contact/stop/clock/completion/failure/frame cases; car and event code are explicit boundaries.");
    return 0;
}
