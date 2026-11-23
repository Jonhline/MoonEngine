/* P: wall time on CPU threads, not GPU execution time. No device synchronization. */
#include "platform/switch_profile.h"
#include <stdio.h>
#include <string.h>
#ifdef __SWITCH__
#include <switch.h>
#include <stdatomic.h>

void swp_checkpoint(const char *stage)
{
    uint64_t used = 0, total = 0;
    Result a = svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    Result b = svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    fprintf(stderr, "Switch checkpoint: %s used=%llu total=%llu memory_status=%08x/%08x\n",
            stage, (unsigned long long)used, (unsigned long long)total, a, b);
    fflush(stderr);
}
typedef atomic_uint_fast64_t counter;
#define ADD(p, n) atomic_fetch_add_explicit(p, n, memory_order_relaxed)
#define TAKE(p) atomic_exchange_explicit(p, 0, memory_order_relaxed)
#define TLS _Thread_local
static uint64_t ticks(void)
{
    return armGetSystemTick();
}
static double milliseconds(uint64_t value)
{
    return armTicksToNs(value) / 1e6;
}
#include "switch_log_queue.h"
#include <stdarg.h>
#include <stdlib.h>

static struct {
    swp_log_queue queue;
    Mutex lock;
    CondVar wake;
    Thread worker;
    int initialized, running, stop;
} output;

static void output_worker(void *unused)
{
    (void)unused;
    char line[SWP_LOG_LINE];
    for (;;) {
        mutexLock(&output.lock);
        while (!output.queue.count && !output.stop)
            condvarWait(&output.wake, &output.lock);
        int available = swp_log_pop(&output.queue, line);
        int done = output.stop && !available;
        mutexUnlock(&output.lock);
        if (done)
            break;
        if (available)
            fputs(line, stderr);
    }
}

static void output_shutdown(void)
{
    if (!output.running)
        return;
    mutexLock(&output.lock);
    output.stop = 1;
    condvarWakeOne(&output.wake);
    mutexUnlock(&output.lock);
    threadWaitForExit(&output.worker);
    threadClose(&output.worker);
    output.running = 0;
}

/* Never hold the queue mutex while writing stdio: SD/network diagnostics can
   wait behind a movie read for hundreds of milliseconds on the console. */
static void output_line(const char *format, ...)
{
    mutexLock(&output.lock);
    if (!output.initialized) {
        output.initialized = 1;
        /* libnx thread.h: 0x3B is the preemptive worker priority on cores 0..2. */
        Result result = threadCreate(&output.worker, output_worker, NULL, NULL, 64 * 1024, 0x3b, 2);
        if (R_FAILED(result))
            result = threadCreate(&output.worker, output_worker, NULL, NULL, 64 * 1024, 0x3b, -2);
        if (R_FAILED(result)) {
            mutexUnlock(&output.lock);
            fprintf(stderr, "Switch profiler: output worker creation failed: %08x\n", result);
            return;
        }
        result = threadStart(&output.worker);
        if (R_FAILED(result)) {
            threadClose(&output.worker);
            mutexUnlock(&output.lock);
            fprintf(stderr, "Switch profiler: output worker start failed: %08x\n", result);
            return;
        }
        output.running = 1;
        atexit(output_shutdown);
    }
    int running = output.running;
    mutexUnlock(&output.lock);
    if (!running)
        return;
    char line[SWP_LOG_LINE];
    va_list args;
    va_start(args, format);
    int size = vsnprintf(line, sizeof line, format, args);
    va_end(args);
    if (size < 0 || (size_t)size >= sizeof line)
        return;
    mutexLock(&output.lock);
    if (swp_log_push(&output.queue, line))
        condvarWakeOne(&output.wake);
    mutexUnlock(&output.lock);
}
#define OUTPUT(...) output_line(__VA_ARGS__)
#else
/* The host fixture controls the clock; counters still support concurrent workers. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef volatile LONG64 counter;
#define ADD(p, n) InterlockedExchangeAdd64(p, (LONG64)(n))
#define TAKE(p) ((uint64_t)InterlockedExchange64(p, 0))
#define TLS __declspec(thread)
#else
#include <stdatomic.h>
typedef atomic_uint_fast64_t counter;
#define ADD(p, n) atomic_fetch_add_explicit(p, n, memory_order_relaxed)
#define TAKE(p) atomic_exchange_explicit(p, 0, memory_order_relaxed)
#define TLS _Thread_local
#endif
extern uint64_t swp_test_ticks(void);
extern void swp_test_output(const char *, ...);
static uint64_t ticks(void)
{
    return swp_test_ticks();
}
static double milliseconds(uint64_t value)
{
    return value / 1000000.0;
}
#define OUTPUT(...) swp_test_output(__VA_ARGS__)
#endif

static counter busy[SWP_STAGE_COUNT], calls[SWP_STAGE_COUNT];
static const char *names[] = {"physics",   "scene",       "gl_submit",     "swap",
                              "audio_mix", "audio_synth", "audio_control", "disc",
                              "inflate",   "loading",     "logic",         "idle",
                              "gpu_fence", "gpu_acquire", "gpu_copy",      "gpu_date"};
static TLS struct {
    enum swp_stage stack[32];
    unsigned depth, overflow;
    uint64_t changed;
    uint64_t frame_busy[SWP_STAGE_COUNT];
    char load[8][128];
    uint64_t load_start[8];
    unsigned load_depth, load_overflow;
} thread;
static char context[256] = "startup";
static uint64_t window, previous, frame_total, frame_max;
static unsigned frames;
static int last_width, last_height;
static uint64_t requested;
static int ready_logged;
static int assets_pending;
static struct {
    char key[256];
    unsigned visits;
} visits[128];

/* Nested spans charge only their exclusive time: disc/inflate do not inflate
   their parent scene/loading counters. Separate threads can overlap in wall time. */
static void charge(uint64_t now)
{
    if (thread.depth) {
        unsigned stage = thread.stack[thread.depth - 1];
        uint64_t elapsed = now - thread.changed;
        ADD(busy + stage, elapsed);
        thread.frame_busy[stage] += elapsed;
    }
    thread.changed = now;
}

void swp_push(enum swp_stage stage)
{
    if ((unsigned)stage >= SWP_STAGE_COUNT)
        return;
    charge(ticks());
    if (thread.depth == 32 || thread.overflow) {
        thread.overflow++;
        return;
    }
    thread.stack[thread.depth++] = stage;
    ADD(calls + stage, 1);
}

void swp_pop(void)
{
    charge(ticks());
    if (thread.overflow)
        thread.overflow--;
    else if (thread.depth)
        thread.depth--;
}

static void report(uint64_t now, int w, int h)
{
    if (!frames)
        return;
#ifdef __SWITCH__
    int docked = appletGetOperationMode() == AppletOperationMode_Console;
#else
    int docked = 0;
#endif
    char line[4096];
    size_t used = (size_t)snprintf(
        line, sizeof line,
        "Switch perf: context=%s docked=%d size=%dx%d frames=%u fps=%.2f mean_ms=%.3f max_ms=%.3f",
        context, docked, w, h, frames, frames * 1000.0 / milliseconds(now - window),
        milliseconds(frame_total) / frames, milliseconds(frame_max));
    for (unsigned i = 0; i < SWP_STAGE_COUNT; i++) {
        uint64_t elapsed = TAKE(busy + i), count = TAKE(calls + i);
        used +=
            (size_t)snprintf(line + used, sizeof line - used, " %s_ms=%.3f %s_calls=%llu", names[i],
                             milliseconds(elapsed) / frames, names[i], (unsigned long long)count);
    }
    OUTPUT("%s\n", line);
    frame_total = frame_max = frames = 0;
    window = now;
}

/* Main-thread attribution for isolated stalls; the existing worker writes it. */
static void report_slow_frame(uint64_t elapsed)
{
    if (milliseconds(elapsed) > 25 && strstr(context, "driving=2")) {
        char line[2048];
        size_t used = (size_t)snprintf(line, sizeof line, "Switch slow-frame: frame_ms=%.3f",
                                       milliseconds(elapsed));
        for (unsigned i = 0; i < SWP_STAGE_COUNT; i++)
            used += (size_t)snprintf(line + used, sizeof line - used, " %s_ms=%.3f", names[i],
                                     milliseconds(thread.frame_busy[i]));
        OUTPUT("%s\n", line);
    }
    memset(thread.frame_busy, 0, sizeof thread.frame_busy);
}
void swp_frame(int width, int height)
{
    last_width = width;
    last_height = height;
    uint64_t now = ticks();
    charge(now);
    if (!window) {
        memset(thread.frame_busy, 0, sizeof thread.frame_busy);
        window = previous = now;
        for (unsigned i = 0; i < SWP_STAGE_COUNT; i++) {
            TAKE(busy + i);
            TAKE(calls + i);
        }
        return;
    }
    uint64_t elapsed = now - previous;
    report_slow_frame(elapsed);
    previous = now;
    frame_total += elapsed;
    if (elapsed > frame_max)
        frame_max = elapsed;
    frames++;
    if (milliseconds(now - window) >= 2000)
        report(now, width, height);
}

void swp_context(const char *project, const char *page, const char *course, const char *car,
                 int driving)
{
    assets_pending = 0;
    char next[sizeof context];
    snprintf(next, sizeof next, "%s/%s/course=%s/car=%s/driving=%d", project ? project : "",
             page ? page : "", course ? course : "", car ? car : "", driving);
    if (strcmp(context, next)) {
        uint64_t now = ticks();
        charge(now);
        report(now, last_width, last_height);
        snprintf(context, sizeof context, "%s", next);
        window = previous = now;
        requested = now;
        ready_logged = 0;
        OUTPUT("Switch context: %s\n", context);
    }
}

void swp_ready(int ready)
{
    if (!ready || assets_pending || ready_logged || !requested)
        return;
    unsigned visit = 0;
    for (unsigned i = 0; i < 128; i++)
        if (!visits[i].key[0] || !strcmp(visits[i].key, context)) {
            snprintf(visits[i].key, sizeof visits[i].key, "%s", context);
            visit = ++visits[i].visits;
            break;
        }
    OUTPUT("Switch ready: context=%s visit=%u request_to_ready_ms=%.3f\n", context, visit,
           milliseconds(ticks() - requested));
    ready_logged = 1;
}

void swp_asset_status(int ready)
{
    if (!ready)
        assets_pending = 1;
}

void swp_load_begin(const char *name)
{
    swp_push(SWP_LOADING);
    if (thread.load_depth == 8 || thread.load_overflow) {
        thread.load_overflow++;
        return;
    }
    unsigned index = thread.load_depth++;
    snprintf(thread.load[index], sizeof thread.load[index], "%s", name ? name : "");
    thread.load_start[index] = ticks();
}

void swp_load_end(void)
{
    swp_pop();
    if (thread.load_overflow) {
        thread.load_overflow--;
        return;
    }
    if (!thread.load_depth)
        return;
    unsigned index = --thread.load_depth;
    OUTPUT("Switch load: %s total_ms=%.3f\n", thread.load[index],
           milliseconds(ticks() - thread.load_start[index]));
}
