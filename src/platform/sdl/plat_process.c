/*
 * Per-process memory and CPU for the debug overlay (G6.40).
 *
 * One reading per OS, no new dependency: /proc on Linux and Android,
 * GetProcessMemoryInfo/GetProcessTimes on Windows. The overlay samples these
 * about once per second; every caller must treat 0 as "unavailable" and print
 * n/a rather than a zero.
 */
#include "platform/platform.h"

#ifdef _WIN32

#include <windows.h>
#include <psapi.h>
#include <SDL.h>

int plat_process_memory(uint64_t *rss_bytes, uint64_t *total_bytes)
{
    int ok = 0;
    if (rss_bytes) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
            *rss_bytes = (uint64_t)pmc.WorkingSetSize;
            ok = 1;
        } else
            *rss_bytes = 0;
    }
    if (total_bytes) {
        /* SDL owns the total-RAM answer on the SDL backend; keep it here too so
           the whole reading is one call. 0 means unknown. */
        *total_bytes = (uint64_t)SDL_GetSystemRAM() * 1024ull * 1024ull;
    }
    return ok;
}

int plat_process_cpu(double *percent)
{
    static int have;
    static uint64_t prev_ticks, prev_ns;
    FILETIME created, exited, kernel, user;
    if (!percent)
        return 0;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return 0;
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    uint64_t ticks = k.QuadPart + u.QuadPart; /* 100 ns units */
    uint64_t now = plat_time_ns();
    int ok = 0;
    if (have && now > prev_ns) {
        double cpu_s = (double)(ticks - prev_ticks) * 1e-7;
        double wall_s = (double)(now - prev_ns) * 1e-9;
        if (wall_s > 0) {
            *percent = 100.0 * cpu_s / wall_s;
            ok = 1;
        }
    }
    prev_ticks = ticks;
    prev_ns = now;
    have = 1;
    return ok;
}

#elif defined(__SWITCH__)

int plat_process_memory(uint64_t *rss_bytes, uint64_t *total_bytes)
{
    if (rss_bytes)
        *rss_bytes = 0;
    if (total_bytes)
        *total_bytes = 0;
    return 0;
}

int plat_process_cpu(double *percent)
{
    (void)percent;
    return 0;
}

#else /* Linux and Android: /proc */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>

int plat_process_memory(uint64_t *rss_bytes, uint64_t *total_bytes)
{
    int ok = 0;
    if (rss_bytes) {
        *rss_bytes = 0;
        FILE *f = fopen("/proc/self/statm", "r");
        if (f) {
            unsigned long size = 0, resident = 0;
            if (fscanf(f, "%lu %lu", &size, &resident) == 2) {
                long ps = sysconf(_SC_PAGESIZE);
                if (ps > 0) {
                    *rss_bytes = (uint64_t)resident * (uint64_t)ps;
                    ok = 1;
                }
            }
            fclose(f);
        }
    }
    if (total_bytes)
        *total_bytes = (uint64_t)SDL_GetSystemRAM() * 1024ull * 1024ull;
    return ok;
}

int plat_process_cpu(double *percent)
{
    static int have;
    static uint64_t prev_ticks, prev_ns;
    if (!percent)
        return 0;
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f)
        return 0;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (!n)
        return 0;
    buf[n] = 0;
    /* The process name is parenthesised and may contain spaces: skip past the
       last ')' before reading the numeric fields. */
    char *p = strrchr(buf, ')');
    if (!p)
        return 0;
    p++;
    char state = 0;
    unsigned long ppid = 0, pgrp = 0, session = 0, tty = 0, tpgid = 0, flags = 0;
    unsigned long minflt = 0, cminflt = 0, majflt = 0, cmajflt = 0, utime = 0, stime = 0;
    if (sscanf(p, " %c %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &state, &ppid, &pgrp,
               &session, &tty, &tpgid, &flags, &minflt, &cminflt, &majflt, &cmajflt, &utime,
               &stime) != 13)
        return 0;
    (void)state;
    (void)ppid;
    (void)pgrp;
    (void)session;
    (void)tty;
    (void)tpgid;
    (void)flags;
    (void)minflt;
    (void)cminflt;
    (void)majflt;
    (void)cmajflt;
    long clk = sysconf(_SC_CLK_TCK);
    if (clk <= 0)
        return 0;
    uint64_t ticks = (uint64_t)utime + (uint64_t)stime;
    uint64_t now = plat_time_ns();
    int ok = 0;
    if (have && now > prev_ns) {
        double cpu_s = (double)(ticks - prev_ticks) / (double)clk;
        double wall_s = (double)(now - prev_ns) * 1e-9;
        if (wall_s > 0) {
            *percent = 100.0 * cpu_s / wall_s;
            ok = 1;
        }
    }
    prev_ticks = ticks;
    prev_ns = now;
    have = 1;
    return ok;
}

#endif
