/* P: offline command search. Speculative RAM is discarded; only a fresh
   normal race replaying the recorded commands can establish a victory. */
#include "career_license_disc.h"
#include "car.h"
#include "fields_race.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mission_pad_fixture.h"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "search line %d: %s\n", __LINE__, #x);                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
enum { TEST_RAM_BYTES = 32 * 1024 * 1024, TEST_STEP_BUFFER = 0x01eeffc0 };
typedef struct {
    gt4_race_run *run;
    uint32_t player;
    int offset, pedal, planning;
    FILE *output;
    uint8_t *tape;
    size_t cursor, commands;
} search_input;

static int wheel(void *user, uint32_t car, int16_t state[2])
{
    (void)user;
    (void)car;
    state[0] = state[1] = 0;
    return 1;
}

static int vibration(void *user, uint32_t car, const uint8_t *data)
{
    (void)user;
    (void)car;
    (void)data;
    return 1;
}

static int pad(void *user, uint32_t car, uint8_t out[32], const uint8_t event[32])
{
    search_input *input = user;
    if (car != input->player)
        return 0;
    memset(out, 0, 32);
    out[1] = 1;
    out[3] = event[3];
    if (input->tape) {
        if (input->cursor < input->commands)
            memcpy(out, input->tape + 32 * input->cursor++, 32);
        return 1;
    }
    mission_pad_fixture(input->run, car, out);
    int16_t axis, throttle, brake, reverse;
    memcpy(&axis, out + 4, 2);
    int value = axis + input->offset;
    axis = (int16_t)(value < -4096 ? -4096 : value > 4096 ? 4096 : value);
    memcpy(&throttle, out + 8, 2);
    memcpy(&brake, out + 10, 2);
    if (input->pedal >= 0) {
        throttle = input->pedal == 0 ? 4096 : 0;
        brake = input->pedal == 1 ? 2048 : input->pedal == 2 ? 4096 : 0;
    }
    reverse = input->pedal == 3 ? 4096 : 0;
    out[0] = 15;
    memcpy(out + 4, &axis, 2);
    memcpy(out + 8, &throttle, 2);
    memcpy(out + 10, &brake, 2);
    memcpy(out + 12, &reverse, 2);
    if (!input->planning && input->output)
        fwrite(out, 1, 32, input->output);
    return 1;
}

static float line_penalty(search_input *input)
{
    uint32_t car = input->player;
    float d = GT4_F32(car + CAR_FIELD_COURSE_DISTANCE), a[3], b[3];
    if (!mission_fixture_point(input->run, d, a) || !mission_fixture_point(input->run, d + 5, b))
        return 1000.f;
    float x = b[0] - a[0], z = b[2] - a[2], length = fmaxf(.001f, hypotf(x, z));
    float dx = GT4_F32(car + CAR_POSITION) - a[0];
    float dz = -GT4_F32(car + CAR_POSITION + sizeof(float)) - a[2];
    float lateral = (dx * z - dz * x) / length;
    float fx = GT4_F32(car + CAR_MATRIX), fz = -GT4_F32(car + CAR_MATRIX + sizeof(float));
    float angle = atan2f(fz * x - fx * z, fx * x + fz * z);
    return lateral * lateral + 8.f * angle * angle;
}

/* Original: func_00389B08 +0x00, func_00389758 +0x28. Speculative core
   steps omit host presentation. The committed race uses race_run_frame. */
static void forecast_step(uint32_t cond)
{
    uint32_t race = GT4_U32(cond);
    gt4_race_queue_age(race + RACE_FIELD_EVENT_QUEUE);
    gt4_race_before_step(race);
    gt4_step(cond, 0, TEST_STEP_BUFFER);
    gt4_race_after_step(race);
}

static void choose_commands(search_input *input, uint8_t *snapshot)
{
    uint8_t *ram = gt4_phys_mem;
    memcpy(snapshot, ram, TEST_RAM_BYTES);
    float best = -1e30f, distance = GT4_F32(input->player + CAR_DISTANCE);
    float course_distance = GT4_F32(input->player + CAR_FIELD_COURSE_DISTANCE);
    int best_axis = 0, best_pedal = 0;
    input->planning = 1;
    const char *weight = getenv("GT4_MISSION_SEARCH_LINE_WEIGHT");
    float line_weight = weight ? (float)atof(weight) : 1.f;
    int pedals = getenv("GT4_MISSION_SEARCH_REVERSE") ? 4 : 3;
    int baseline = getenv("GT4_MISSION_SEARCH_BASELINE_PEDALS") != NULL;
    if (baseline)
        pedals = 1;
    const char *ahead = getenv("GT4_MISSION_SEARCH_HORIZON");
    unsigned horizon = ahead ? (unsigned)atoi(ahead) : 48;
    if (horizon < 12 || horizon > 120)
        horizon = 48;
    for (int axis = -1500; axis <= 1500; axis += 750)
        for (int pedal = 0; pedal < pedals; pedal++) {
            memcpy(ram, snapshot, TEST_RAM_BYTES);
            input->offset = axis;
            input->pedal = baseline ? -1 : pedal;
            float penalty = 0;
            for (unsigned frame = 0; frame < horizon; frame++) {
                forecast_step(gt4_race_run_conductor(input->run));
                if (!(frame % 6))
                    penalty += line_penalty(input);
            }
            float progress = GT4_F32(input->player + CAR_DISTANCE) - distance;
            if (getenv("GT4_MISSION_SEARCH_PROJECTED_PROGRESS")) {
                float projected = GT4_F32(input->player + CAR_FIELD_COURSE_DISTANCE) - course_distance;
                /* P: wrap detection for the search score, never a race rule. */
                if (fabsf(projected) <= fabsf(progress) + 100.f)
                    progress = projected;
            }
            float score = progress - penalty * line_weight;
            if (score > best) {
                best = score;
                best_axis = axis;
                best_pedal = input->pedal;
            }
        }
    memcpy(ram, snapshot, TEST_RAM_BYTES);
    input->offset = best_axis;
    input->pedal = best_pedal;
    input->planning = 0;
}

static void install(search_input *input)
{
    gt4_phys_pad.ctx = input;
    gt4_phys_pad.read = pad;
    gt4_phys_pad.wheel_state = wheel;
    gt4_phys_pad.write = vibration;
}

static int outcome(gt4_race_run *run)
{
    gt4_race_hud hud;
    int32_t state;
    CHECK(gt4_race_run_hud(run, 0, &hud));
    CHECK(gt4_race_run_license_result(run, &state, NULL));
    printf("physical finished=%d rank=%d state=%d time=%u\n", hud.finished, hud.rank, state,
           hud.line_time);
    return !(hud.finished && hud.rank == 1 && state == 2);
}

static int verify(const char *disc, const gt4_race_desc *desc, const char *path)
{
    FILE *file = fopen(path, "rb");
    CHECK(file && !fseek(file, 0, SEEK_END));
    long bytes = ftell(file);
    CHECK(bytes > 0 && !(bytes % 32) && !fseek(file, 0, SEEK_SET));
    uint8_t *tape = malloc(bytes);
    CHECK(tape && fread(tape, 1, bytes, file) == (size_t)bytes);
    fclose(file);
    char error[256];
    gt4_race_run *run = gt4_race_run_open(disc, desc, error, sizeof error);
    CHECK(run);
    search_input input = {0};
    input.run = run;
    input.player = gt4_race_run_car(run, 0);
    input.tape = tape;
    input.commands = bytes / 32;
    install(&input);
    for (unsigned frame = 0; frame < 86400 && !gt4_race_run_finished(run); frame++)
        CHECK(gt4_race_run_frame(run));
    CHECK(!outcome(run));
    puts("PASS fresh native victory from commands only; no speculative RAM reused");
    gt4_race_run_close(run);
    gt4_phys_pad.ctx = NULL;
    free(tape);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 4);
    gt4_career_race_lookup_data tables;
    gt4_career_license_exam mission;
    char error[256];
    CHECK(gt4_career_race_lookup_data_load(argv[1], &tables));
    CHECK(gt4_career_mission_prepare(&mission, argv[1], &tables, argv[2], error, sizeof error));
    gt4_race_run *run = gt4_race_run_open(argv[1], &mission.desc, error, sizeof error);
    CHECK(run);
    search_input input = {0};
    input.run = run;
    input.player = gt4_race_run_car(run, 0);
    input.output = fopen(argv[3], "wb");
    uint8_t *snapshot = malloc(TEST_RAM_BYTES);
    CHECK(input.output && snapshot);
    install(&input);
    gt4_phys_fpu_enter();
    float previous_distance = GT4_F32(input.player + CAR_DISTANCE);
    unsigned last_progress = 0;
    for (unsigned frame = 0; frame < 60000 && !gt4_race_run_finished(run); frame++) {
        /* Original: func_0034C1C8 +0x00. A mission's authored head start
           is not a stalled driver; do not search before the player starts. */
        int waiting = gt4_race_run_countdown(run, 0) > 0;
        if (waiting) {
            input.offset = input.pedal = 0;
            last_progress = frame;
        } else if (!(frame % 12))
            choose_commands(&input, snapshot);
        CHECK(gt4_race_run_frame(run));
        float distance = GT4_F32(input.player + CAR_DISTANCE);
        if (fabsf(distance - previous_distance) > 1.f) {
            previous_distance = distance;
            last_progress = frame;
        }
        if (!waiting && frame - last_progress > 600) {
            fprintf(stderr, "search stalled; no victory accepted\n");
            break;
        }
        if (!(frame % 1200)) {
            printf("search %s frame=%u distance=%.2f\n", argv[2], frame,
                   GT4_F32(input.player + CAR_DISTANCE));
            fflush(stdout);
        }
    }
    int lost = outcome(run);
    fclose(input.output);
    free(snapshot);
    gt4_race_run_close(run);
    gt4_phys_pad.ctx = NULL;
    CHECK(!lost);
    return verify(argv[1], &mission.desc, argv[3]);
}
