/*
 * G6.40 — the debug overlay.
 *
 * One compact readout drawn over whatever is on screen (launcher, menus and
 * races), off at every start, toggled by F3 on the desktop and by the
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
static uint64_t last_frame_ns;
static double frame_ms[60];
static int frame_ms_count, frame_ms_next;
static uint64_t sample_last_ns;
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
    frame_ms_count = frame_ms_next = 0;
    sample_last_ns = 0;
    cpu_percent = -1;
    gpu_ms = -1;
    gpu_vendor_percent = -1;
#if defined(MOON_PC_VULKAN)
    gpu_have_prev = 0;
#endif
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

void moon_debug_overlay_draw(void)
{
    if (!overlay_on)
        return;
    uint64_t now = plat_time_ns();
    if (last_frame_ns) {
        double ms = (double)(now - last_frame_ns) / 1e6;
        if (ms > 0 && ms < 10000.0) {
            frame_ms[frame_ms_next] = ms;
            frame_ms_next = (frame_ms_next + 1) % 60;
            if (frame_ms_count < 60)
                frame_ms_count++;
        }
    }
    last_frame_ns = now;
    sample_slow(now);

    double sum = 0, worst = 0;
    for (int i = 0; i < frame_ms_count; i++) {
        sum += frame_ms[i];
        if (frame_ms[i] > worst)
            worst = frame_ms[i];
    }
    double mean = frame_ms_count ? sum / frame_ms_count : 0;
    double fps = mean > 0 ? 1000.0 / mean : 0;

    r3d_stats st = r3d_frame_stats();
    unsigned late = 0;
#if defined(MOON_PC_VULKAN)
    late = (unsigned)video_frame_late_count();
#endif

    int fw = 640, fh = 480;
    {
        int dw = 0, dh = 0;
        SDL_Window *win = plat_sdl_window();
        if (win) {
#if defined(MOON_PC_VULKAN)
            if (moon_vk_render_active())
                moon_vk_render_size(win, &dw, &dh);
            else
#endif
                SDL_GL_GetDrawableSize(win, &dw, &dh);
        }
        if (dw >= 1)
            fw = dw;
        if (dh >= 1)
            fh = dh;
    }

    char api[64], device[72];
    overlay_api_strings(api, sizeof api, device, sizeof device);
    const char *renderer =
#if defined(MOON_PC_VULKAN)
        moon_vk_render_active() ? moon_renderer_name(MOON_RENDERER_VULKAN)
                                : moon_renderer_name(MOON_RENDERER_OPENGL);
#else
        moon_renderer_name(MOON_RENDERER_OPENGL);
#endif
    /* Keep line 4 compact: one short API token and a bounded device name. */
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

    char gpu[40];
    if (gpu_ms >= 0)
        snprintf(gpu, sizeof gpu, "%.2fMS", gpu_ms);
    else
        snprintf(gpu, sizeof gpu, "n/a");

    char cpu[24];
    if (cpu_percent >= 0)
        snprintf(cpu, sizeof cpu, "%.0f%%", cpu_percent);
    else
        snprintf(cpu, sizeof cpu, "n/a");
    char rss[24];
    if (rss_bytes)
        snprintf(rss, sizeof rss, "%.1fMB", (double)rss_bytes / (1024.0 * 1024.0));
    else
        snprintf(rss, sizeof rss, "n/a");

    char lines[4][100];
    snprintf(lines[0], sizeof lines[0], "FPS %5.1f  %6.2fMS  WORST %6.2fMS", fps, mean, worst);
    if (gpu_vendor_percent >= 0)
        snprintf(lines[1], sizeof lines[1], "CPU %s  RSS %s  GPU %s  LOAD %.0f%%", cpu, rss, gpu,
                 gpu_vendor_percent);
    else
        snprintf(lines[1], sizeof lines[1], "CPU %s  RSS %s  GPU %s", cpu, rss, gpu);
    snprintf(lines[2], sizeof lines[2], "DRAWS %u  TRIS %u  CULL %u  STATE %u  TEX %u", st.draws,
             st.triangles, st.culled_groups, st.state_cache_hits, st.texture_cache_hits);
    snprintf(lines[3], sizeof lines[3], "%s %s | %dx%d | %s %s | LATE %u", renderer, api_short, fw,
             fh, overlay_platform(), device_short, late);

    /* Fixed 640x480 UI space (the same one the touch overlay uses); the platform
       scales it to the window, so a font pixel grows with the display. */
    plat_video_set_view(640, 480);
    float p = (float)fh / 720.0f;
    if (p < 1)
        p = 1;
    if (p > 2)
        p = 2;
    float pad = 4 * p, margin = 8;
    size_t maxlen = 0;
    for (int i = 0; i < 4; i++) {
        size_t len = strlen(lines[i]);
        if (len > maxlen)
            maxlen = len;
    }
    /* Keep the panel inside the 640-wide UI space. */
    size_t limit = (size_t)((640 - 2 * margin - 2 * pad) / (6 * p));
    if (limit < 8)
        limit = 8;
    if (maxlen > limit) {
        maxlen = limit;
        for (int i = 0; i < 4; i++)
            lines[i][limit] = 0;
    }
    float lh = 9 * p;
    float width = maxlen * 6 * p + 2 * pad, height = 4 * lh + 2 * pad;
    overlay_plate(margin, margin, width, height);
    const uint8_t ink[4] = {235, 238, 242, 255};
    for (int i = 0; i < 4; i++)
        overlay_text(lines[i], margin + pad, margin + pad + i * lh, p, ink);
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
        event->key.keysym.scancode == SDL_SCANCODE_F3)
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
