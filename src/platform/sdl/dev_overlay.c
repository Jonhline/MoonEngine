/*
 * G6.40 — the debug overlay.
 *
 * One compact readout drawn over whatever is on screen (launcher, menus and
 * races), off at every start, toggled by F4 on the desktop and by the
 * volume-up + volume-down chord on Android. It reads and draws only: the
 * renderers, the frame's own work and the physics step are untouched.
 *
 * Everything it prints is measured. Anything the kernel or backend cannot
 * answer prints "n/a", never a zero.
 */
#include "dev_overlay.h"

#include "platform/platform.h"
#include "platform/renderer_backend.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "render3d.h"
#if defined(MOON_PC_VULKAN)
#include "vk_render.h"
#endif
#if !defined(MOON_DEKO3D)
#include "gl.h"
#endif

#include <SDL.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <pdh.h>
#ifndef PDH_MORE_DATA
#define PDH_MORE_DATA ((PDH_STATUS)0x800007D2L)
#endif
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

/* 5x7 glyphs, ASCII 32..126; each row is 5 bits, bit 4 leftmost. */
static const uint8_t overlay_font[95][7] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}, /* ! */
    {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00}, /* " */
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A}, /* # */
    {0x0E, 0x15, 0x14, 0x0E, 0x05, 0x15, 0x0E}, /* $ */
    {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13}, /* % */
    {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D}, /* & */
    {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ' */
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}, /* ( */
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}, /* ) */
    {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00}, /* * */
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}, /* + */
    {0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x08}, /* , */
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}, /* - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, /* . */
    {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}, /* / */
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, /* 0 */
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* 1 */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, /* 2 */
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, /* 3 */
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, /* 4 */
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, /* 5 */
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, /* 6 */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, /* 7 */
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, /* 8 */
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}, /* 9 */
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}, /* : */
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x08}, /* ; */
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02}, /* < */
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00}, /* = */
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08}, /* > */
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}, /* ? */
    {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E}, /* @ */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* A */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* B */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* C */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* D */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* E */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* F */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, /* G */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* H */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* I */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* J */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* K */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* L */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* M */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* O */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* P */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* Q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* R */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* S */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* T */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* U */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* V */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* W */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* X */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* Y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* Z */
    {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}, /* [ */
    {0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01}, /* \ */
    {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}, /* ] */
    {0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00}, /* ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F}, /* _ */
    {0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}, /* ` */
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* a */
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}, /* b */
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}, /* c */
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}, /* d */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, /* e */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, /* f */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}, /* g */
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, /* h */
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}, /* i */
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}, /* j */
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, /* k */
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, /* l */
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}, /* m */
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, /* n */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* o */
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}, /* p */
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}, /* q */
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}, /* r */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, /* s */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* t */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, /* u */
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}, /* v */
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}, /* w */
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}, /* x */
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}, /* y */
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}, /* z */
    {0x0C, 0x04, 0x04, 0x08, 0x04, 0x04, 0x0C}, /* { */
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, /* | */
    {0x0C, 0x04, 0x04, 0x02, 0x04, 0x04, 0x0C}, /* } */
    {0x00, 0x00, 0x09, 0x15, 0x12, 0x00, 0x00}, /* ~ */
};

/* The overlay's single piece of state: off at every start (law 12). */
static int overlay_on;
static int overlay_ready;

/* ---- measurement ---- */
#define OVERLAY_WINDOW_NS 1000000000ull
#define OVERLAY_SAMPLE_NS 250000000ull /* 4 Hz: never once per frame */
#define OVERLAY_SAMPLE_MAX 1024
static uint64_t last_frame_ns;
static struct {
    uint64_t ns;
    double ms;
} frame_samples[OVERLAY_SAMPLE_MAX];
static int frame_sample_count, frame_sample_next;
static uint64_t sample_last_ns;
/* The race state the app publishes for the RACE rows. kmh < 0 = not racing. */
static float overlay_kmh = -1;
static int overlay_assist, overlay_speed_percent;
static double cpu_percent = -1;
static uint64_t ram_used, ram_total;
static double gpu_percent = -1;
static double vram_used = -1, vram_total = -1;
static double gpu_ms = -1;
static double overlay_cost_ms = -1, overlay_cost_sum;
static unsigned overlay_cost_count;
#if defined(MOON_PC_VULKAN)
static uint64_t gpu_ns_prev;
static uint64_t gpu_samples_prev;
static int gpu_have_prev;
#endif

static void reset_sampling(void)
{
    last_frame_ns = 0;
    frame_sample_count = frame_sample_next = 0;
    sample_last_ns = 0;
    cpu_percent = -1;
    ram_used = ram_total = 0;
    gpu_percent = -1;
    vram_used = vram_total = -1;
    gpu_ms = -1;
    overlay_cost_ms = -1;
    overlay_cost_sum = 0;
    overlay_cost_count = 0;
#if defined(MOON_PC_VULKAN)
    gpu_have_prev = 0;
#endif
}

void moon_debug_overlay_set_race(float kmh, int assist_line, int speed_percent)
{
    overlay_kmh = kmh;
    overlay_assist = assist_line;
    overlay_speed_percent = speed_percent;
}

/* ---- CPU and RAM: the whole machine, once per slow sample ---- */
static int system_cpu(double *out)
{
    if (!out)
        return 0;
#if defined(_WIN32)
    static int have;
    static uint64_t prev_idle, prev_total;
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return 0;
    ULARGE_INTEGER i, k, u;
    i.LowPart = idle.dwLowDateTime, i.HighPart = idle.dwHighDateTime;
    k.LowPart = kernel.dwLowDateTime, k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime, u.HighPart = user.dwHighDateTime;
    uint64_t idle_ticks = i.QuadPart, total_ticks = k.QuadPart + u.QuadPart;
    int ok = 0;
    if (have && total_ticks > prev_total) {
        double dtotal = (double)(total_ticks - prev_total);
        double didle = (double)(idle_ticks - prev_idle);
        *out = 100.0 * (dtotal - didle) / dtotal;
        if (*out < 0)
            *out = 0;
        if (*out > 100)
            *out = 100;
        ok = 1;
    }
    prev_idle = idle_ticks;
    prev_total = total_ticks;
    have = 1;
    return ok;
#elif defined(__SWITCH__)
    (void)out;
    return 0;
#else
    static int have;
    static uint64_t prev_idle, prev_total;
    FILE *f = fopen("/proc/stat", "r");
    if (!f)
        return 0;
    char line[256];
    if (!fgets(line, sizeof line, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0,
                       softirq = 0, steal = 0;
    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &user, &nice, &system, &idle,
               &iowait, &irq, &softirq, &steal) < 4)
        return 0;
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
    uint64_t idle_all = idle + iowait;
    int ok = 0;
    if (have && total > prev_total) {
        double dtotal = (double)(total - prev_total);
        double didle = (double)(idle_all - prev_idle);
        *out = 100.0 * (dtotal - didle) / dtotal;
        if (*out < 0)
            *out = 0;
        if (*out > 100)
            *out = 100;
        ok = 1;
    }
    prev_idle = idle_all;
    prev_total = total;
    have = 1;
    return ok;
#endif
}

static int system_memory(uint64_t *used, uint64_t *total)
{
    if (!used || !total)
        return 0;
#if defined(_WIN32)
    MEMORYSTATUSEX m;
    memset(&m, 0, sizeof m);
    m.dwLength = sizeof m;
    if (!GlobalMemoryStatusEx(&m))
        return 0;
    *total = (uint64_t)m.ullTotalPhys;
    *used = (uint64_t)(m.ullTotalPhys - m.ullAvailPhys);
    return 1;
#elif defined(__SWITCH__)
    return 0;
#else
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return 0;
    unsigned long long total_kb = 0, avail_kb = 0;
    char key[32];
    unsigned long long value;
    char unit[16];
    while (fscanf(f, "%31s %llu %15s", key, &value, unit) == 3) {
        if (!strcmp(key, "MemTotal:"))
            total_kb = value;
        else if (!strcmp(key, "MemAvailable:"))
            avail_kb = value;
        if (total_kb && avail_kb)
            break;
        /* skip the rest of the line */
        int c;
        while ((c = fgetc(f)) != '\n' && c != EOF)
            ;
    }
    fclose(f);
    if (!total_kb)
        return 0;
    *total = total_kb * 1024ull;
    *used = (total_kb > avail_kb ? total_kb - avail_kb : 0) * 1024ull;
    return 1;
#endif
}

/* ---- GPU: Windows PDH counters, Linux NVIDIA NVML (dlopen) ---- */
#if defined(_WIN32)
typedef PDH_STATUS(WINAPI *pdh_open_query_fn)(LPCWSTR, DWORD_PTR, PDH_HQUERY *);
typedef PDH_STATUS(WINAPI *pdh_add_english_fn)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
typedef PDH_STATUS(WINAPI *pdh_add_counter_fn)(PDH_HQUERY, LPCWSTR, DWORD_PTR, PDH_HCOUNTER *);
typedef PDH_STATUS(WINAPI *pdh_collect_fn)(PDH_HQUERY);
typedef PDH_STATUS(WINAPI *pdh_array_fn)(PDH_HCOUNTER, DWORD, LPDWORD, LPDWORD,
                                         PDH_FMT_COUNTERVALUE_ITEM_W *);
static struct {
    int tried, ok;
    HMODULE lib;
    pdh_open_query_fn open;
    pdh_add_english_fn add_english;
    pdh_add_counter_fn add_counter;
    pdh_collect_fn collect;
    pdh_array_fn array;
    PDH_HQUERY query;
    PDH_HCOUNTER engine, vram_used_counter, vram_total_counter;
} pdh;
static int pdh_counter_sum(PDH_HCOUNTER counter, double *sum)
{
    if (!counter || !pdh.array)
        return 0;
    DWORD size = 0, count = 0;
    PDH_STATUS status = pdh.array(counter, PDH_FMT_DOUBLE, &size, &count, NULL);
    if (status != PDH_MORE_DATA && status != ERROR_SUCCESS)
        return 0;
    if (!size)
        return 0;
    PDH_FMT_COUNTERVALUE_ITEM_W *items = malloc(size);
    if (!items)
        return 0;
    status = pdh.array(counter, PDH_FMT_DOUBLE, &size, &count, items);
    double total = 0;
    int got = 0;
    if (status == ERROR_SUCCESS) {
        for (DWORD i = 0; i < count; i++)
            if (items[i].FmtValue.CStatus == ERROR_SUCCESS) {
                total += items[i].FmtValue.doubleValue;
                got = 1;
            }
    }
    free(items);
    if (got)
        *sum = total;
    return got;
}
static void gpu_sample(void)
{
    if (!pdh.tried) {
        pdh.tried = 1;
        pdh.lib = LoadLibraryW(L"pdh.dll");
        if (pdh.lib) {
            pdh.open = (pdh_open_query_fn)GetProcAddress(pdh.lib, "PdhOpenQueryW");
            pdh.add_english =
                (pdh_add_english_fn)GetProcAddress(pdh.lib, "PdhAddEnglishCounterW");
            pdh.add_counter = (pdh_add_counter_fn)GetProcAddress(pdh.lib, "PdhAddCounterW");
            pdh.collect = (pdh_collect_fn)GetProcAddress(pdh.lib, "PdhCollectQueryData");
            pdh.array =
                (pdh_array_fn)GetProcAddress(pdh.lib, "PdhGetFormattedCounterArrayW");
            if (pdh.open && pdh.collect && pdh.array &&
                (pdh.add_english || pdh.add_counter) &&
                pdh.open(NULL, 0, &pdh.query) == ERROR_SUCCESS) {
                pdh_add_english_fn add = pdh.add_english;
                if (!add) {
                    /* PdhAddCounterW takes the same arguments; on an English
                       Windows the counter path is already readable anyway. */
                    add = (pdh_add_english_fn)pdh.add_counter;
                }
                if (add(pdh.query, L"\\GPU Engine(*)\\Utilization Percentage", 0,
                        &pdh.engine) != ERROR_SUCCESS)
                    pdh.engine = NULL;
                if (add(pdh.query, L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0,
                        &pdh.vram_used_counter) != ERROR_SUCCESS)
                    pdh.vram_used_counter = NULL;
                if (add(pdh.query, L"\\GPU Adapter Memory(*)\\Dedicated Limit", 0,
                        &pdh.vram_total_counter) != ERROR_SUCCESS)
                    pdh.vram_total_counter = NULL;
                pdh.ok = 1;
            }
        }
    }
    if (!pdh.ok || pdh.collect(pdh.query) != ERROR_SUCCESS)
        return;
    double value;
    if (pdh_counter_sum(pdh.engine, &value)) {
        if (value < 0)
            value = 0;
        if (value > 100)
            value = 100;
        gpu_percent = value;
    } else
        gpu_percent = -1;
    if (pdh_counter_sum(pdh.vram_used_counter, &value)) {
        vram_used = value / (1024.0 * 1024.0 * 1024.0);
        if (pdh_counter_sum(pdh.vram_total_counter, &value) && value > 0)
            vram_total = value / (1024.0 * 1024.0 * 1024.0);
        else
            vram_total = -1;
    } else {
        vram_used = vram_total = -1;
    }
}
#elif !defined(__SWITCH__)
typedef struct {
    unsigned int gpu, memory;
} nvml_utilization;
typedef struct {
    unsigned long long total, free, used;
} nvml_memory;
static struct {
    int tried, ok;
    void *lib, *device;
    int (*init)(void);
    int (*shutdown)(void);
    int (*handle)(unsigned int, void **);
    int (*util)(void *, nvml_utilization *);
    int (*mem)(void *, nvml_memory *);
} nvml;
static void nvml_load(void)
{
    if (nvml.tried)
        return;
    nvml.tried = 1;
    nvml.lib = dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!nvml.lib)
        return;
    nvml.init = (int (*)(void))dlsym(nvml.lib, "nvmlInit_v2");
    nvml.shutdown = (int (*)(void))dlsym(nvml.lib, "nvmlShutdown");
    nvml.handle = (int (*)(unsigned int, void **))dlsym(nvml.lib, "nvmlDeviceGetHandleByIndex_v2");
    nvml.util = (int (*)(void *, nvml_utilization *))dlsym(nvml.lib,
                                                           "nvmlDeviceGetUtilizationRates");
    nvml.mem = (int (*)(void *, nvml_memory *))dlsym(nvml.lib, "nvmlDeviceGetMemoryInfo");
    if (nvml.init && nvml.handle && nvml.util && nvml.mem && nvml.init() == 0 &&
        nvml.handle(0, &nvml.device) == 0)
        nvml.ok = 1;
}
/* Best-effort non-NVIDIA fallback: AMD and Mali sysfs percentages. */
static int gpu_sysfs(double *out)
{
    unsigned long long a = 0, b = 0;
    FILE *f = fopen("/sys/class/kgsl/kgsl-3d0/gpubusy", "r");
    if (f) {
        int got = fscanf(f, "%llu %llu", &a, &b);
        fclose(f);
        if (got == 2 && b) {
            *out = 100.0 * (double)a / (double)b;
            return 1;
        }
    }
    static const char *mali[] = {"/sys/kernel/gpu/gpu_busy",
                                 "/sys/class/misc/mali0/device/utilisation",
                                 "/sys/devices/platform/mali.0/utilisation"};
    for (size_t i = 0; i < sizeof mali / sizeof *mali; i++) {
        f = fopen(mali[i], "r");
        if (!f)
            continue;
        int got = fscanf(f, "%llu", &a);
        fclose(f);
        if (got == 1 && a <= 100) {
            *out = (double)a;
            return 1;
        }
    }
    for (int card = 0; card < 8; card++) {
        char path[128];
        snprintf(path, sizeof path, "/sys/class/drm/card%d/device/gpu_busy_percent", card);
        f = fopen(path, "r");
        if (!f)
            continue;
        int got = fscanf(f, "%llu", &a);
        fclose(f);
        if (got == 1 && a <= 100) {
            *out = (double)a;
            return 1;
        }
    }
    return 0;
}
static void gpu_sample(void)
{
    nvml_load();
    if (nvml.ok) {
        nvml_utilization u;
        if (nvml.util(nvml.device, &u) == 0)
            gpu_percent = (double)u.gpu;
        else
            gpu_percent = -1;
        nvml_memory m;
        if (nvml.mem(nvml.device, &m) == 0 && m.total) {
            vram_used = (double)m.used / (1024.0 * 1024.0 * 1024.0);
            vram_total = (double)m.total / (1024.0 * 1024.0 * 1024.0);
        } else {
            vram_used = vram_total = -1;
        }
        return;
    }
    double percent;
    if (gpu_sysfs(&percent)) {
        gpu_percent = percent;
        vram_used = vram_total = -1;
    } else {
        gpu_percent = -1;
        vram_used = vram_total = -1;
    }
}
#else
static void gpu_sample(void)
{
}
#endif

static void sample_slow(uint64_t now)
{
    if (sample_last_ns && now - sample_last_ns < OVERLAY_SAMPLE_NS)
        return;
    sample_last_ns = now;
    double cpu;
    if (system_cpu(&cpu))
        cpu_percent = cpu;
    uint64_t used, total;
    if (system_memory(&used, &total)) {
        ram_used = used;
        ram_total = total;
    }
    gpu_sample();
#if defined(MOON_PC_VULKAN)
    if (moon_vk_render_active()) {
        moon_vk_render_stats s = moon_vk_render_statistics();
        if (gpu_have_prev && s.gpu_samples > gpu_samples_prev)
            gpu_ms = (double)(s.gpu_ns - gpu_ns_prev) /
                     (double)(s.gpu_samples - gpu_samples_prev) / 1e6;
        gpu_ns_prev = s.gpu_ns;
        gpu_samples_prev = s.gpu_samples;
        gpu_have_prev = 1;
    } else
        gpu_ms = -1;
#endif
}

/* ---- drawing ---- */
static void overlay_text(const char *s, float x, float y, float p, const uint8_t color[4])
{
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch < 32 || ch > 126)
            ch = '?';
        const uint8_t *g = overlay_font[ch - 32];
        for (int row = 0; row < 7; row++) {
            uint8_t bits = g[row];
            if (!bits)
                continue;
            for (int col = 0; col < 5; col++) {
                if (!(bits & (1u << (4 - col))))
                    continue;
                float gx = x + col * p, gy = y + row * p;
                const float xy[4][2] = {
                    {gx, gy}, {gx + p, gy}, {gx, gy + p}, {gx + p, gy + p}};
                plat_draw_solid_quad(xy, color);
            }
        }
        x += 6 * p;
    }
}

static void overlay_plate(float x, float y, float w, float h)
{
    const uint8_t plate[4] = {10, 12, 16, 168};
    const float xy[4][2] = {{x, y}, {x + w, y}, {x, y + h}, {x + w, y + h}};
    plat_draw_solid_quad(xy, plate);
}

static void overlay_api_strings(char *api, size_t acap, char *device, size_t dcap)
{
    api[0] = 0;
    device[0] = 0;
#if !defined(MOON_DEKO3D)
    const GLubyte *version = glGetString(GL_VERSION);
    const GLubyte *renderer = glGetString(GL_RENDERER);
    if (version)
        snprintf(api, acap, "%s", (const char *)version);
    if (renderer)
        snprintf(device, dcap, "%s", (const char *)renderer);
#else
    (void)acap;
    (void)dcap;
#endif
}

static const char *overlay_platform(void)
{
#if defined(__ANDROID__)
    return "android";
#elif defined(_WIN32)
    return "windows";
#elif defined(__SWITCH__)
    return "switch";
#else
    return "linux";
#endif
}

static void overlay_row(char labels[][28], char values[][56], int *n, const char *label,
                        const char *value)
{
    if (*n >= 24)
        return;
    snprintf(labels[*n], 28, "%s", label ? label : "");
    snprintf(values[*n], 56, "%s", value ? value : "");
    (*n)++;
}

static void overlay_rowf(char labels[][28], char values[][56], int *n, const char *label,
                         const char *fmt, ...)
{
    char value[56];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(value, sizeof value, fmt, ap);
    va_end(ap);
    overlay_row(labels, values, n, label, value);
}

void moon_debug_overlay_draw(void)
{
    if (!overlay_on)
        return;
    uint64_t draw_started = plat_time_ns();
    uint64_t now = draw_started;
    double last_ms = -1;
    if (last_frame_ns) {
        double ms = (double)(now - last_frame_ns) / 1e6;
        if (ms > 0 && ms < 10000.0) {
            last_ms = ms;
            frame_samples[frame_sample_next].ns = now;
            frame_samples[frame_sample_next].ms = ms;
            frame_sample_next = (frame_sample_next + 1) % OVERLAY_SAMPLE_MAX;
            if (frame_sample_count < OVERLAY_SAMPLE_MAX)
                frame_sample_count++;
        }
    }
    last_frame_ns = now;
    sample_slow(now);

    double sum = 0, worst = 0;
    int count = 0;
    for (int i = 0; i < frame_sample_count; i++) {
        int at = (frame_sample_next - 1 - i + OVERLAY_SAMPLE_MAX) % OVERLAY_SAMPLE_MAX;
        if (now - frame_samples[at].ns > OVERLAY_WINDOW_NS)
            break;
        sum += frame_samples[at].ms;
        if (frame_samples[at].ms > worst)
            worst = frame_samples[at].ms;
        count++;
    }
    double mean = count ? sum / (double)count : -1;

    r3d_stats st = r3d_frame_stats();
#if !defined(MOON_DEKO3D)
    /* The GL path submits its own 2D draws; add them to the 3D counters so
       the overlay is not blank in menus (and reports the phone's real work). */
    unsigned gl_draws = 0, gl_quads = 0;
    video_gl_frame_stats(&gl_draws, &gl_quads);
    st.draws += gl_draws;
    st.triangles += gl_quads * 2;
#ifdef __ANDROID__
    unsigned gl_state_skips = 0, gl_texture_skips = 0;
    moon_gl_state_stats(&gl_state_skips, &gl_texture_skips);
    st.state_cache_hits += gl_state_skips;
    st.texture_cache_hits += gl_texture_skips;
#endif
#endif
    unsigned late = 0;
#if defined(MOON_PC_VULKAN)
    late = (unsigned)video_frame_late_count();
#endif



    int out_w = 0, out_h = 0, scene_w = 0, scene_h = 0;
    {
        SDL_Window *win = plat_sdl_window();
        if (win) {
#if defined(MOON_PC_VULKAN)
            if (moon_vk_render_active())
                moon_vk_render_size(win, &out_w, &out_h);
            else
#endif
                SDL_GL_GetDrawableSize(win, &out_w, &out_h);
        }
    }
#if !defined(MOON_DEKO3D)
    video_scene_target_size(&scene_w, &scene_h);
#else
    scene_w = scene_h = 0; /* the deko3d backend has no scene-target accessor */
#endif

    char labels[16][28], values[16][56];
    int n = 0, separate_after = -1, race_start = -1;

    if (count && sum > 0)
        overlay_rowf(labels, values, &n, "FPS", "%d  %.1f/%.1f ms", (int)(count * 1000.0 / sum + .5),
                     mean, worst);
    else if (last_ms >= 0)
        overlay_rowf(labels, values, &n, "FPS", "-  %.1f/n/a ms", last_ms);
    else
        overlay_row(labels, values, &n, "FPS", "n/a");
    separate_after = n - 1;

    if (gpu_percent >= 0) {
        if (gpu_ms >= 0)
            overlay_rowf(labels, values, &n, "GPU", "%.0f%%  %.1f ms", gpu_percent, gpu_ms);
        else
            overlay_rowf(labels, values, &n, "GPU", "%.0f%%", gpu_percent);
    } else
        overlay_row(labels, values, &n, "GPU", "n/a");

    if (cpu_percent >= 0)
        overlay_rowf(labels, values, &n, "CPU", "%.0f%%", cpu_percent);
    else
        overlay_row(labels, values, &n, "CPU", "n/a");

    if (vram_used >= 0 && vram_total > 0)
        overlay_rowf(labels, values, &n, "VRAM", "%.1f/%.1f GB", vram_used, vram_total);
    else if (vram_used >= 0)
        overlay_rowf(labels, values, &n, "VRAM", "%.1f GB", vram_used);
    else
        overlay_row(labels, values, &n, "VRAM", "n/a");

    if (ram_total)
        overlay_rowf(labels, values, &n, "RAM", "%.1f/%.1f GB",
                     (double)ram_used / (1024.0 * 1024.0 * 1024.0),
                     (double)ram_total / (1024.0 * 1024.0 * 1024.0));
    else
        overlay_row(labels, values, &n, "RAM", "n/a");

    overlay_rowf(labels, values, &n, "draws", "%u  tris %.1fk", st.draws,
                 (double)st.triangles / 1000.0);
    if (scene_w > 0 && scene_h > 0 && out_w > 0 && out_h > 0)
        overlay_rowf(labels, values, &n, "scene", "%dx%d  out %dx%d", scene_w, scene_h, out_w,
                     out_h);
    else
        overlay_row(labels, values, &n, "scene", "n/a");

    race_start = n;
    if (overlay_kmh >= 0) {
        overlay_rowf(labels, values, &n, "km/h", "%.0f", overlay_kmh);
        if (overlay_assist)
            overlay_rowf(labels, values, &n, "assist",
                         "on  speed %.2f", (double)overlay_speed_percent / 100.0);
        else
            overlay_row(labels, values, &n, "assist", "off");
    } else {
        overlay_row(labels, values, &n, "km/h", "n/a");
        overlay_row(labels, values, &n, "assist", "n/a");
    }

    if (overlay_cost_ms >= 0)
        overlay_rowf(labels, values, &n, "ovl", "%.2f ms", overlay_cost_ms);
    else
        overlay_row(labels, values, &n, "ovl", "n/a");

    /* Fixed 640x480 UI space; the platform scales it to the window. */
    plat_video_set_view(640, 480);
    float p = (float)out_h / 720.0f;
    if (p < 1)
        p = 1;
    if (p > 2)
        p = 2;
    const float lh = 9.5f * p;
    const float pad = 4 * p, margin = 8 * p;
    const int label_col = 8; /* characters */
    size_t max_value = 0;
    for (int i = 0; i < n; i++) {
        size_t len = strlen(values[i]);
        if (len > max_value)
            max_value = len;
    }
    /* <= ~40 characters per line, even on the longest row. */
    if (max_value + (size_t)label_col > 40)
        max_value = 40 - (size_t)label_col;
    float width = (float)(label_col + (int)max_value) * 6 * p + 2 * pad;
    float height = (float)n * lh + 2 * pad;
    overlay_plate(margin, margin, width, height);
    const uint8_t ink[4] = {238, 241, 245, 255};
    const uint8_t dim[4] = {150, 156, 166, 255};
    const uint8_t accent[4] = {255, 190, 80, 255};
    const uint8_t rule[4] = {255, 255, 255, 46};
    for (int i = 0; i < n; i++) {
        float y = margin + pad + i * lh;
        values[i][max_value] = 0;
        overlay_text(labels[i], margin + pad, y, p, dim);
        overlay_text(values[i], margin + pad + label_col * 6 * p, y, p,
                     i == 0 ? accent : ink);
        if (i == separate_after || i + 1 == race_start)
            plat_draw_quad(NULL, margin + pad, y + lh - 1, width - 2 * pad, 1, 0, 0, 1, 1, rule);
    }

    double cost = (double)(plat_time_ns() - draw_started) / 1e6;
    overlay_cost_sum += cost;
    if (++overlay_cost_count >= 8) {
        overlay_cost_ms = overlay_cost_sum / (double)overlay_cost_count;
        overlay_cost_sum = 0;
        overlay_cost_count = 0;
    }
    static uint64_t reported_ns;
    if (now - reported_ns > 2000000000ull) {
        reported_ns = now;
        if (overlay_cost_ms >= 0)
            fprintf(stderr, "overlay: draw %.3f ms\n", overlay_cost_ms);
    }
}

/* ---- the toggle ---- */
void moon_debug_overlay_toggle(void)
{
    overlay_on = !overlay_on;
    reset_sampling();
}

int moon_debug_overlay_on(void)
{
    return overlay_on;
}

static int overlay_event_watch(void *user, SDL_Event *event)
{
    (void)user;
    if (event->type == SDL_KEYDOWN && !event->key.repeat &&
        event->key.keysym.scancode == SDL_SCANCODE_F4)
        moon_debug_overlay_toggle();
    return 1;
}

void moon_debug_overlay_init(void)
{
    if (overlay_ready)
        return;
    overlay_ready = 1;
    SDL_AddEventWatch(overlay_event_watch, NULL);
    /* Headless capture hook: an explicit dev flag, never a stored setting, so
       the overlay is still off at every ordinary start (law 12). */
    const char *force = getenv("MOON_DEBUG_OVERLAY");
    if (force && force[0] && strcmp(force, "0"))
        overlay_on = 1;
}
