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
static uint64_t rss_bytes, ram_bytes;
static double gpu_ms = -1;
static double gpu_vendor_percent = -1;
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
    gpu_ms = -1;
    gpu_vendor_percent = -1;
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

/* Best-effort vendor GPU load, each node guarded; 0 = absent (prints n/a). */
static int gpu_vendor_load(double *out)
{
#if defined(__SWITCH__)
    (void)out;
    return 0;
#else
    unsigned long long a = 0, b = 0;
    /* Qualcomm Adreno: two counters, busy and total, in one sysfs node. */
    FILE *f = fopen("/sys/class/kgsl/kgsl-3d0/gpubusy", "r");
    if (f) {
        int got = fscanf(f, "%llu %llu", &a, &b);
        fclose(f);
        if (got == 2 && b) {
            *out = 100.0 * (double)a / (double)b;
            return 1;
        }
    }
    /* ARM Mali: a single percentage node, under a few kernel names. */
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
    /* AMD: per-card busy percentage. */
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
#endif
}

static void sample_slow(uint64_t now)
{
    if (sample_last_ns && now - sample_last_ns < 1000000000ull)
        return;
    sample_last_ns = now;
    double cpu;
    if (plat_process_cpu(&cpu))
        cpu_percent = cpu;
    uint64_t rss = 0, ram = 0;
    if (plat_process_memory(&rss, &ram))
        rss_bytes = rss;
    if (ram)
        ram_bytes = ram;
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
    double vendor;
    gpu_vendor_percent = gpu_vendor_load(&vendor) ? vendor : -1;
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
    uint64_t now = plat_time_ns();
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

    /* The same samples for last, 1 s average, 1 s worst and FPS. */
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

    char api[64], device[72];
    overlay_api_strings(api, sizeof api, device, sizeof device);
    const char *renderer =
#if defined(MOON_PC_VULKAN)
        moon_vk_render_active() ? moon_renderer_name(MOON_RENDERER_VULKAN)
                                : moon_renderer_name(MOON_RENDERER_OPENGL);
#else
        moon_renderer_name(MOON_RENDERER_OPENGL);
#endif
    char api_short[20];
    if (!strcmp(renderer, "vulkan"))
        snprintf(api_short, sizeof api_short, "vulkan");
    else {
        size_t i = 0;
        while (api[i] && api[i] != ' ' && i + 1 < sizeof api_short) {
            api_short[i] = api[i];
            i++;
        }
        api_short[i] = 0;
        if (!api_short[0])
            snprintf(api_short, sizeof api_short, "n/a");
    }
    char device_short[28];
    if (device[0])
        snprintf(device_short, sizeof device_short, "%.26s", device);
    else
        snprintf(device_short, sizeof device_short, "n/a");

    char labels[24][28], values[24][56];
    int n = 0;
    overlay_row(labels, values, &n, "", "DEBUG OVERLAY   F4");
    overlay_row(labels, values, &n, "", "FRAME");
    if (last_ms >= 0)
        overlay_rowf(labels, values, &n, "Last frame", "%.2f ms", last_ms);
    else
        overlay_row(labels, values, &n, "Last frame", "n/a");
    if (count)
        overlay_rowf(labels, values, &n, "1 s average", "%.2f ms", mean);
    else
        overlay_row(labels, values, &n, "1 s average", "n/a");
    if (count)
        overlay_rowf(labels, values, &n, "1 s worst", "%.2f ms", worst);
    else
        overlay_row(labels, values, &n, "1 s worst", "n/a");
    if (count && sum > 0)
        overlay_rowf(labels, values, &n, "FPS (1 s)", "%.1f", (double)count * 1000.0 / sum);
    else
        overlay_row(labels, values, &n, "FPS (1 s)", "n/a");

    overlay_row(labels, values, &n, "", "SCENE");
    overlay_rowf(labels, values, &n, "Draw calls", "%u", st.draws);
    overlay_rowf(labels, values, &n, "Triangles", "%u", st.triangles);
    if (scene_w > 0 && scene_h > 0)
        overlay_rowf(labels, values, &n, "Scene render", "%d x %d", scene_w, scene_h);
    else
        overlay_row(labels, values, &n, "Scene render", "n/a");
    if (out_w > 0 && out_h > 0)
        overlay_rowf(labels, values, &n, "Output", "%d x %d", out_w, out_h);
    else
        overlay_row(labels, values, &n, "Output", "n/a");
    overlay_rowf(labels, values, &n, "Culled groups", "%u", st.culled_groups);

    overlay_row(labels, values, &n, "", "RACE");
    if (overlay_kmh >= 0)
        overlay_rowf(labels, values, &n, "Speed", "%.0f km/h", overlay_kmh);
    else
        overlay_row(labels, values, &n, "Speed", "n/a");
    if (overlay_kmh >= 0)
        overlay_row(labels, values, &n, "Assist line", overlay_assist ? "on" : "off");
    else
        overlay_row(labels, values, &n, "Assist line", "n/a");
    if (overlay_kmh >= 0 && overlay_assist && overlay_speed_percent >= 0)
        overlay_rowf(labels, values, &n, "Speed level", "%d%%", overlay_speed_percent);
    else
        overlay_row(labels, values, &n, "Speed level",
                    overlay_kmh >= 0 && overlay_assist ? "n/a" : "off");

    overlay_row(labels, values, &n, "", "SYSTEM");
    if (cpu_percent >= 0)
        overlay_rowf(labels, values, &n, "CPU", "%.0f%%", cpu_percent);
    else
        overlay_row(labels, values, &n, "CPU", "n/a");
    if (rss_bytes)
        overlay_rowf(labels, values, &n, "Memory (RSS)", "%.1f MB",
                     (double)rss_bytes / (1024.0 * 1024.0));
    else
        overlay_row(labels, values, &n, "Memory (RSS)", "n/a");
    if (gpu_ms >= 0)
        overlay_rowf(labels, values, &n, "GPU time", "%.2f ms", gpu_ms);
    else
        overlay_row(labels, values, &n, "GPU time", "n/a");
    if (gpu_vendor_percent >= 0)
        overlay_rowf(labels, values, &n, "GPU load", "%.0f%%", gpu_vendor_percent);
    else
        overlay_row(labels, values, &n, "GPU load", "n/a");
#if defined(MOON_PC_VULKAN)
    overlay_rowf(labels, values, &n, "Late frames", "%u", late);
#else
    overlay_row(labels, values, &n, "Late frames", "n/a");
#endif
    overlay_rowf(labels, values, &n, "Renderer", "%s %s", renderer, api_short);
    overlay_rowf(labels, values, &n, "Device", "%s", device_short);

    /* Fixed 640x480 UI space; the platform scales it to the window. */
    plat_video_set_view(640, 480);
    float p = (float)out_h / 720.0f;
    if (p < 1)
        p = 1;
    if (p > 2)
        p = 2;
    const float lh = 9.5f * p;
    const float pad = 4 * p, margin = 8;
    const int label_col = 20; /* characters */
    size_t max_value = 0;
    for (int i = 0; i < n; i++) {
        size_t len = strlen(values[i]);
        if (len > max_value)
            max_value = len;
    }
    size_t limit = (size_t)((640 - 2 * margin - 2 * pad) / (6 * p));
    if (limit < (size_t)label_col + 8)
        limit = (size_t)label_col + 8;
    if (max_value + label_col > limit)
        max_value = limit - label_col;
    float width = (float)(label_col + (int)max_value) * 6 * p + 2 * pad;
    float height = (float)n * lh + 2 * pad;
    overlay_plate(margin, margin, width, height);
    const uint8_t ink[4] = {235, 238, 242, 255};
    const uint8_t dim[4] = {150, 156, 166, 255};
    const uint8_t accent[4] = {255, 190, 80, 255};
    for (int i = 0; i < n; i++) {
        float y = margin + pad + i * lh;
        if (!labels[i][0]) {
            overlay_text(values[i], margin + pad, y, p, accent);
            continue;
        }
        values[i][max_value] = 0;
        overlay_text(labels[i], margin + pad, y, p, dim);
        overlay_text(values[i], margin + pad + label_col * 6 * p, y, p, ink);
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
