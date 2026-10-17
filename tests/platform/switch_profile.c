/* Deterministic timing fixture, with a deliberately incorrect expectation. */
#define MOON_SWITCH_PROFILE_TEST
#include "../../src/platform/common/switch_profile.c"
#include <stdarg.h>
#include <stdlib.h>
#include "../../src/platform/common/switch_log_queue.h"

static TLS uint64_t now;
static char output[16384];
static size_t used;
uint64_t swp_test_ticks(void)
{
    return now;
}
void swp_test_output(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int count = vsnprintf(output + used, sizeof output - used, format, args);
    va_end(args);
    if (count > 0 && (size_t)count < sizeof output - used)
        used += count;
}
#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            fprintf(stderr, "profile line %d: %s\n", __LINE__, #value);                            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#ifdef _WIN32
static DWORD WINAPI concurrent_worker(void *unused)
{
    (void)unused;
    now = 100;
    for (int i = 0; i < 1000; i++) {
        swp_push(SWP_SCENE);
        now += 2;
        swp_push(SWP_GL);
        now += 3;
        swp_pop();
        now += 3;
        swp_pop();
    }
    return thread.depth != 0;
}
#endif

int main(int argc, char **argv)
{
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    /* A stalled sink must not block producers or overwrite queued records. */
    swp_log_queue *queue = calloc(1, sizeof *queue);
    CHECK(queue != NULL);
    char line[SWP_LOG_LINE];
    CHECK(!swp_log_pop(queue, line));
    for (unsigned i = 0; i < SWP_LOG_CAPACITY; i++) {
        snprintf(line, sizeof line, "record-%u", i);
        CHECK(swp_log_push(queue, line));
    }
    CHECK(!swp_log_push(queue, "overflow"));
    for (unsigned i = 0; i < SWP_LOG_CAPACITY; i++) {
        char expected[32];
        snprintf(expected, sizeof expected, "record-%u", i);
        CHECK(swp_log_pop(queue, line) && !strcmp(line, expected));
        CHECK(swp_log_push(queue, "wrapped"));
    }
    for (unsigned i = 0; i < SWP_LOG_CAPACITY; i++)
        CHECK(swp_log_pop(queue, line) && !strcmp(line, "wrapped"));
    CHECK(!swp_log_pop(queue, line));
    char oversized[SWP_LOG_LINE + 1];
    memset(oversized, 'x', sizeof oversized - 1);
    oversized[sizeof oversized - 1] = 0;
    CHECK(!swp_log_push(queue, oversized));
    free(queue);
    now = 0;
    swp_push(SWP_GPU_DATE);
    now += 3;
    swp_push(SWP_GPU_COPY);
    now += 7;
    swp_pop();
    now += 5;
    swp_pop();
    CHECK(busy[SWP_GPU_DATE] == 8 && busy[SWP_GPU_COPY] == 7);
    CHECK(calls[SWP_GPU_DATE] == 1 && calls[SWP_GPU_COPY] == 1);
    CHECK(!strcmp(names[SWP_GPU_DATE], "gpu_date"));
    CHECK(!strcmp(names[SWP_GPU_COPY], "gpu_copy"));
    busy[SWP_GPU_DATE] = busy[SWP_GPU_COPY] = 0;
    calls[SWP_GPU_DATE] = calls[SWP_GPU_COPY] = 0;
    now = 100;
    swp_push(SWP_SCENE);
    now += 10;
    swp_push(SWP_GL);
    now += 20;
    swp_push(SWP_DISC);
    now += 30;
    swp_pop();
    now += 40;
    swp_pop();
    now += 50;
    swp_pop();
    CHECK(busy[SWP_SCENE] == 60);
    CHECK(busy[SWP_GL] == 60);
    CHECK(busy[SWP_DISC] == (negative ? 31 : 30));
    CHECK(calls[SWP_DISC] == 1 && thread.depth == 0);
    swp_push(SWP_GPU_FENCE);
    now += 7;
    swp_pop();
    swp_push(SWP_GPU_ACQUIRE);
    now += 9;
    swp_pop();
    CHECK(busy[SWP_GPU_FENCE] == 7 && busy[SWP_GPU_ACQUIRE] == 9);
    CHECK(calls[SWP_GPU_FENCE] == 1 && calls[SWP_GPU_ACQUIRE] == 1);
    now = 1000000000;
    swp_context("arcade", "TopRoot", "", "", 0);
    now += 5000000;
    swp_ready(1);
    CHECK(strstr(output, "visit=1 request_to_ready_ms=5.000"));
    used = 0;
    output[0] = 0;
    swp_frame(1280, 720);
    swp_push(SWP_PHYSICS);
    now += 5000000;
    swp_pop();
    now += 1990000000;
    swp_frame(1280, 720);
    CHECK(strstr(output, "frames=2") && strstr(output, "physics_ms=2.500"));
    CHECK(strstr(output, "max_ms=1995.000") && strstr(output, "docked=0 size=1280x720"));
    CHECK(busy[SWP_PHYSICS] == 0);
    swp_context("gtmode", "MapRoot", "", "", 0);
    swp_context("arcade", "TopRoot", "", "", 0);
    swp_ready(0);
    swp_asset_status(0);
    swp_ready(1);
    CHECK(!strstr(output, "visit=2"));
    swp_context("arcade", "TopRoot", "", "", 0);
    now += 2000000;
    swp_ready(1);
    CHECK(strstr(output, "visit=2 request_to_ready_ms=2.000"));
    swp_load_begin("tsukuba");
    now += 10000000;
    swp_load_end();
    CHECK(strstr(output, "Switch load: tsukuba total_ms=10.000"));
    swp_load_begin("project");
    now += 5000000;
    swp_load_begin("root");
    now += 2000000;
    swp_load_end();
    now += 3000000;
    swp_load_end();
    CHECK(strstr(output, "Switch load: root total_ms=2.000"));
    CHECK(strstr(output, "Switch load: project total_ms=10.000"));
    CHECK(thread.depth == 0);
    swp_context("arcade", "TopRoot", "tsukuba", "ka_01", 2);
    swp_frame(1280, 720);
    output[0] = 0;
    used = 0;
    swp_push(SWP_PHYSICS);
    now += 1000000;
    swp_pop();
    swp_push(SWP_SCENE);
    now += 30000000;
    swp_pop();
    if (argc > 1 && !strcmp(argv[1], "--slow-negative"))
        thread.frame_busy[SWP_SCENE] += 1000000;
    swp_frame(1280, 720);
    CHECK(strstr(output,"Switch slow-frame: frame_ms=31.000"));
    CHECK(strstr(output,"physics_ms=1.000") && strstr(output,"scene_ms=30.000"));
    CHECK(thread.frame_busy[SWP_PHYSICS]==0 && thread.frame_busy[SWP_SCENE]==0);
#ifdef _WIN32
    for (unsigned i = 0; i < SWP_STAGE_COUNT; i++) {
        TAKE(busy + i);
        TAKE(calls + i);
    }
    swp_push(SWP_LOADING);
    HANDLE workers[2];
    for (unsigned i = 0; i < 2; i++) {
        workers[i] = CreateThread(NULL, 0, concurrent_worker, NULL, 0, NULL);
        CHECK(workers[i] != NULL);
    }
    CHECK(WaitForMultipleObjects(2, workers, TRUE, 10000) == WAIT_OBJECT_0);
    for (unsigned i = 0; i < 2; i++) {
        DWORD result;
        CHECK(GetExitCodeThread(workers[i], &result) && result == 0);
        CloseHandle(workers[i]);
    }
    CHECK(thread.depth == 1);
    swp_pop();
    CHECK(busy[SWP_SCENE] == 10000 && busy[SWP_GL] == 6000);
    CHECK(calls[SWP_SCENE] == 2000 && calls[SWP_GL] == 2000);
#endif
    return 0;
}
