/* Deterministic timing fixture, with a deliberately incorrect expectation. */
#define MOON_SWITCH_PROFILE_TEST
#include "../../src/platform/common/switch_profile.c"
#include <stdarg.h>
#include <stdlib.h>

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
