/*
 * Moon Engine viewer: opens the user's GT4 disc image and shows the game's UI
 * screens with the native UI renderer. First step towards the menu; there is
 * no game logic yet.
 *
 *   moonengine <disc.iso> [project] [screen] [--shot out.png]
 *   arrows = focus, Enter = activate, Escape = cancel, F10 = quit.
 *   F1/F2 = debug screen browsing, PageUp/PageDown = debug project browsing.
 */
#include "moon_pref.h"
#ifdef __ANDROID__
#include "android_host.h"
#endif
#include <sys/stat.h>
#include "platform/switch_profile.h"
#include "platform/platform.h"
#include "plat_sdl.h"
#include "movie.h"
#include "showroom.h"
#include "car_model.h"
#include "mot1.h"
#include "race_view.h"
#include "photo_travel.h"
#include "race_game.h"
#include "race_hud.h"
#include "race_run.h"
#include "boot_notice.h"
#include "loading_screen.h"
#include "menu.h"
#include "gtmode_game.h"
#include "career_parts.h"
#include "career_vehicle.h"
#include "music.h"
#include "pregrid_audio.h"
#include "ui.h"
#include "ui_motion.h"
#include "render3d.h"
#ifdef MOON_PC_VULKAN
#include "vk_render.h"
#include "switch_render.h"
#endif
#ifdef __SWITCH__
#include "switch_render.h"
#endif
#include "vol.h"
#include "video_settings.h"
#include "save_package.h"
#include "music_library.h"
#include "music_folder.h"
#ifdef __SWITCH__
#include "platform/switch_scene.h"
#endif
#include "desktop_input.h"
#include "launcher.h"
#include "../platform/common/moon_dirent.h"
#include "font_layout.h"
#include "launcher_update.h"
#include "mod_manifest.h"
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include "specdb.h"
#ifndef __SWITCH__
#include "launcher_audio.h"
#include "launcher_mixer.h"
#endif
#ifndef __SWITCH__
#include "renderer_startup.h"
#include "platform/renderer_backend.h"
#endif
#include "save_ui.h"
#include "discord_presence.h"
#include "save.h"
#include "replay_file.h"
#include "career_event.h"
#include "career_license_disc.h"
#include "video_gl.h"
#include "dev_overlay.h"
#include "rtext.h"
#include "font.h"
#include "gpb.h"
#include "tex1.h"
#include "glyph_sdf.h"
#include "launcher_fonts_data.h"
#include "../../third_party/stb/stb_truetype.h"

#include "miniz.h"

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

static void app_jukebox_attach(menu *m);
static int app_jukebox_pcm(menu *m, int allowed);
static void app_jukebox_leave(void);
static int app_jukebox_fixture(menu *m);
#include "custom_modes.inc"
#include "main_support.inc"
static int bench_freeze;
#include "main_race_flow.inc"

/* The product was OpenGT4, then VULCAN 4, now MOON ENGINE. User data has to follow the name or the
 * captain's existing saves look lost. Best effort and once: rename() refuses an existing target, so
 * new saves always win over old ones.
 * shortcut: only `saves` is carried over; the settings file is re-created on first run. */
static void adopt_saves_from_old_names(void)
{
    char *dst = SDL_GetPrefPath("MoonEngine", "MoonEngine");
    if (!dst)
        return;
    static const char *old_names[] = {"Vulcan4", "OpenGT4"};
    for (size_t i = 0; i < sizeof old_names / sizeof old_names[0]; i++) {
        char *old = SDL_GetPrefPath(old_names[i], old_names[i]);
        if (!old)
            continue;
        char from[1200], to[1200];
        snprintf(from, sizeof from, "%ssaves", old);
        snprintf(to, sizeof to, "%ssaves", dst);
        if (rename(from, to) == 0)
            fprintf(stderr, "moonengine: adopted saves from %s\n", from);
        SDL_free(old);
    }
    SDL_free(dst);
}

static plat_texture *dock_icon_load_crop(gt4_vol *vol, const char *pkg_path, const char *asset,
                                         int *out_w, int *out_h, float crop[4]);
static plat_texture *dock_icon_load(gt4_vol *vol, const char *pkg_path, const char *asset,
                                    int *out_w, int *out_h)
{
    return dock_icon_load_crop(vol, pkg_path, asset, out_w, out_h, NULL);
}
/* ---- the dock: the launcher's buttons, drawn over the room ----
   Four of them, and their icons are the captain's picks from the disc's own UI art - the
   assets are read out of his own image at run time (never shipped, never in the repo), the
   names under them are drawn in the disc's own face, and the selection is the game's own
   pointer. No panel and no scrim: the icons and names sit straight on the room. */
static music_library *app_music_library;
static void app_music_ensure(int force);
static void app_music_tick(void);
static int guide_options_busy(void);

/* Discord presence: one gate and one context build for every render loop. */
static int app_presence_enabled;
static void app_presence_log(const char *line);

static const char *app_presence_car_name(menu *m, const char *label)
{
    if (!label || !*label)
        return NULL;
    if (m) {
        const char *name = menu_car_name(m, label);
        return name ? name : label;
    }
    static uint8_t *labels, *names, *strings;
    static size_t labels_size, names_size, strings_size;
    static int loaded;
    if (!loaded) {
        loaded = 1;
        if (presentation_disc) {
            const char *error = NULL;
            gt4_vol *vol = gt4_vol_open(presentation_disc, &error);
            if (vol) {
                labels = gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &labels_size);
                names = gt4_vol_read(vol, "/specdb/GT4_US2560/CAR_NAME_american.dbt", &names_size);
                strings =
                    gt4_vol_read(vol, "/specdb/GT4_US2560/american_StrDB.sdb", &strings_size);
                gt4_vol_close(vol);
            }
        }
    }
    uint32_t id;
    uint8_t row[256];
    if (labels && names && strings && gt4_specdb_label(labels, labels_size, label, &id) &&
        gt4_specdb_row(names, names_size, id, row, sizeof row) == 16) {
        uint32_t offset = (uint32_t)row[0] | (uint32_t)row[1] << 8 | (uint32_t)row[2] << 16 |
                          (uint32_t)row[3] << 24;
        const char *name = gt4_specdb_string(strings, strings_size, offset);
        if (name && *name)
            return name;
    }
    return label;
}

static void app_startup_log(const char *fmt, ...)
{
    char pref[1200], path[1300], line[512];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    snprintf(path, sizeof path, "%sstartup.log", pref);
    FILE *f = fopen(path, "rb");
    long size = 0;
    if (f) {
        if (!fseek(f, 0, SEEK_END))
            size = ftell(f);
        fclose(f);
    }
    f = fopen(path, size > 64 * 1024 ? "wb" : "ab");
    if (!f)
        return;
    fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}
static void app_presence_log(const char *line)
{
    char pref[1200], path[1300];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    snprintf(path, sizeof path, "%spresence.log", pref);
    FILE *f = fopen(path, "rb");
    long size = 0;
    if (f) {
        if (!fseek(f, 0, SEEK_END))
            size = ftell(f);
        fclose(f);
    }
    f = fopen(path, size > 64 * 1024 ? "wb" : "ab");
    if (!f)
        return;
    time_t now = time(NULL);
    struct tm date;
    memset(&date, 0, sizeof date);
#ifdef _WIN32
    int ok = !localtime_s(&date, &now);
#else
    int ok = localtime_r(&now, &date) != NULL;
#endif
    if (ok)
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d %s\n", date.tm_year + 1900, date.tm_mon + 1,
                date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, line);
    else
        fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}

static void app_presence_poll(const char *mode, const char *course, menu *car_menu,
                              const char *car_label, int driving, int project)
{
    static uint64_t next;
    if (!app_presence_enabled)
        return;
    uint64_t now = plat_time_ns();
    if (now < next)
        return;
    next = now + 1000000000ull;
    const char *actual_mode = mode;
    const char *actual_car = car_label;
    static char project_mode[64];
    if (!course) {
        if (car_menu && menu_active_root(car_menu) >= 0) {
            if (project >= 0 && project < NPROJECTS) {
                snprintf(project_mode, sizeof project_mode, "%s", projects[project]);
                if (project_mode[0] >= 'a' && project_mode[0] <= 'z')
                    project_mode[0] = (char)(project_mode[0] - 'a' + 'A');
                actual_mode = project_mode;
            }
        } else if (!car_menu && !guide_options_busy()) {
            actual_mode = "Launcher";
        }
        if (car_menu) {
            const char *selected = menu_selected_car(car_menu);
            actual_car = selected && *selected ? selected : NULL;
        }
    }
    gen_presence presence = {0};
    presence.mode = actual_mode;
    presence.course = course;
    presence.car = app_presence_car_name(car_menu, actual_car);
    static char last_mode[64], last_course[256], last_car[256];
    static int have_last;
    const char *course_text = presence.course ? presence.course : "-";
    const char *car_text = presence.car ? presence.car : "-";
    if (!have_last || strcmp(last_mode, actual_mode) || strcmp(last_course, course_text) ||
        strcmp(last_car, car_text)) {
        have_last = 1;
        snprintf(last_mode, sizeof last_mode, "%s", actual_mode);
        snprintf(last_course, sizeof last_course, "%s", course_text);
        snprintf(last_car, sizeof last_car, "%s", car_text);
        char trace[512];
        snprintf(trace, sizeof trace,
                 "discord presence: driving=%d mode=%s course=%s car=%s", driving, actual_mode,
                 course_text, car_text);
        fprintf(stderr, "%s\n", trace);
        app_presence_log(trace);
    }
    discord_presence_tick(&presence);
    static int last_linked = -1;
    int linked = discord_presence_linked();
    if (linked != last_linked) {
        last_linked = linked;
        char trace[96];
        snprintf(trace, sizeof trace, "discord presence: link %s", linked ? "ready" : "not connected");
        fprintf(stderr, "%s\n", trace);
        app_presence_log(trace);
    }
}

#define DOCK_COUNT 4
#define DOCK_START 0
#define DOCK_OPTIONS 1
#define DOCK_MODS 2
#define DOCK_EXIT 3
static const char *const dock_names[DOCK_COUNT] = {"START", "OPTIONS", "MODS", "EXIT"};
/* START leads the row and is the largest; the others share the rest evenly. */
static const float dock_weight[DOCK_COUNT] = {1.f, 1.f, 1.f, 1.f};
typedef struct {
    const char *pkg;
    const char *asset;
    plat_texture *tex;
    int w, h;
    float u0, v0, u1, v1; /* the inked part of the picture (its transparent margin cut away) */
} dock_icon;
static dock_icon dock_band = {"/projects/gtmode/US/DealerRoot.gpb",
                              "image/gtmode/dealer/wall_usa.png", NULL, 0, 0};
static dock_icon dock_icons[DOCK_COUNT] = {
    {"/projects/gtmode/US/CourseEntryRoot.gpb", "image/gtmode/course/icon_freerun.png", NULL, 0, 0},
    {"/projects/gtmode/US/CarViewRoot.gpb", "image/gtmode/home/assessment.png", NULL, 0, 0},
    {"/projects/gtmode/US/LicenseDetailRoot.gpb", "image/gtmode/license/coffee_gold.png", NULL, 0,
     0},
    {"/projects/quick-arcade/US/ArcadeRoot.gpb", "image/quick/JP/quick_exit.png", NULL, 0, 0},
};
/* The selection: the game's own menu pointer - CursorRoot's cursor, the sprite the menus
   turn about its tip at (16,0) of the 32x32 image (ui.c ui_draw). Read off the disc like
   the icons. */
static dock_icon dock_arrow = {"/projects/cursor/US/CursorRoot.gpb", "image/cursor/cursor.png",
                               NULL, 0, 0};
/* One icon out of a package on the disc: the same decode gt4img uses. */
static plat_texture *dock_icon_load_crop(gt4_vol *vol, const char *pkg_path, const char *asset,
                                         int *out_w, int *out_h, float crop[4])
{
    if (!vol)
        return NULL;
    size_t size = 0;
    uint8_t *pkg = gt4_vol_read(vol, pkg_path, &size);
    if (!pkg)
        return NULL;
    size_t asize = 0;
    uint8_t *blob = gpb_load(pkg, size, asset, &asize);
    free(pkg);
    if (!blob)
        return NULL;
    tex1_image *im = NULL;
    int count = tex1_decode(blob, asize, &im);
    free(blob);
    if (count < 1 || !im)
        return NULL;
    plat_texture *t = plat_texture_create(im[0].width, im[0].height, im[0].rgba);
    if (t) {
        *out_w = im[0].width;
        *out_h = im[0].height;
        /* the icons carry different transparent margins: measure the ink so each one is
           sized by what is actually drawn */
        int x0 = im[0].width, y0 = im[0].height, x1 = -1, y1 = -1;
        for (int y = 0; y < im[0].height; y++)
            for (int x = 0; x < im[0].width; x++)
                if (im[0].rgba[((size_t)y * im[0].width + x) * 4 + 3] > 24) {
                    x0 = x < x0 ? x : x0;
                    y0 = y < y0 ? y : y0;
                    x1 = x > x1 ? x : x1;
                    y1 = y > y1 ? y : y1;
                }
        if (crop) {
            if (x1 >= x0 && y1 >= y0) {
                crop[0] = (float)x0 / im[0].width;
                crop[1] = (float)y0 / im[0].height;
                crop[2] = (float)(x1 + 1) / im[0].width;
                crop[3] = (float)(y1 + 1) / im[0].height;
            } else {
                crop[0] = crop[1] = 0;
                crop[2] = crop[3] = 1;
            }
        }
    }
    tex1_free(im, count);
    return t;
}

/* The disc face the names are drawn in, and how big a name runs. */
#define DOCK_NAME_FACE "hm_16"
#define DOCK_NAME_SCALE 0.8f
/* The dock's on-screen size: its design pixels are drawn DOCK_SCALE:1 in the framebuffer. */
#define DOCK_SCALE 1.5f

typedef struct {
    gt4_font font;                /* the disc's own face */
    const tex1_image *font_atlas; /* its uv space */
    plat_texture *font_tex;       /* the ink atlas as a signed distance field */
    int font_ok;
    /* The game's pointer, run by the engine's own cursor port (gt4_cursor_update):
       the tip chases (cx,cy) and the tilt chases ca, one step per 60Hz tick. */
    float cx, cy, ca;    /* tip position and angle, in the 640x448 view */
    float ctx, cty, cta; /* where the tip and angle are heading */
    double ticks;
    float ox, oy; /* the dock's pixel offset: design coords -> framebuffer pixels */
    float scale;  /* on-screen design->pixel scale for this framebuffer */
    float hover[DOCK_COUNT]; /* 0..1, eased: the chosen slot's icon lifts and brightens */
    float fw, fh;            /* the framebuffer the dock was laid out for */
    int mouse;               /* the pointer follows the mouse at (mx,my), framebuffer pixels */
    float mx, my;
} studio_dock;

/* One name in the disc's own face, white over a soft dark shadow so it reads on the bright
   room. The glyph quad is ui.c's own (gt4_font_glyph + plat_draw_glyph). */
/* a line's width with the game's own pen steps (func_004940E8, font_layout.c) */
static float dock_text_width(const gt4_font *f, const char *text)
{
    float w = 0;
    unsigned previous = 0;
    for (const char *c = text; *c;) {
        unsigned code = gt4_text_next(&c);
        if (code < 32)
            continue;
        if (!gt4_font_glyph(f, code) && !gt4_font_glyph(f, '?'))
            continue;
        float ink = 0;
        w += gt4_font_original_step(f, previous, code, 1, &ink);
        previous = code;
    }
    return w;
}
/* Every Options label, fact and preview caption shares the panel lifetime. */
static int studio_panel_ink_limit = -1;
static int studio_panel_ink_audit;
static void dock_name_pass(const studio_dock *d, const char *text, float cx, float base, float dx,
                           float dy, const uint8_t rgba[4], float size)
{
    uint8_t faded[4] = {rgba[0], rgba[1], rgba[2], rgba[3]};
    if (studio_panel_ink_limit >= 0) {
        if (!(studio_panel_ink_audit && getenv("MOONSTUDIO_PANEL_INK_NEGATIVE")) &&
            faded[3] > studio_panel_ink_limit)
            faded[3] = (uint8_t)studio_panel_ink_limit;
        if (studio_panel_ink_audit && faded[3] > studio_panel_ink_limit) {
            fprintf(stderr, "panel text outlived its opacity: %s\n", text);
            exit(86);
        }
        rgba = faded;
        if (!rgba[3]) return;
    }
    const gt4_font *f = &d->font;
    const tex1_image *a = d->font_atlas;
    const float s = DOCK_NAME_SCALE * size * (d->scale > 0 ? d->scale : DOCK_SCALE);
    float x = cx - dock_text_width(f, text) * s / 2.f;
    unsigned previous = 0;
    for (const char *c = text; *c;) {
        unsigned code = gt4_text_next(&c);
        if (code < 32)
            continue;
        const gt4_glyph *g = gt4_font_glyph(f, code);
        if (!g)
            g = gt4_font_glyph(f, '?');
        if (!g)
            continue;
        float ink = (float)g->x;
        float step = gt4_font_original_step(f, previous, code, 1, &ink);
        if (g->w && g->h) /* a space has no ink, only its advance */
            plat_draw_glyph(d->font_tex, x + ink * s + dx, base + g->y * s + dy, g->w * s,
                            g->h * s, base, 0, (float)g->u / a->width, (float)g->v / a->height,
                            (float)(g->u + g->w) / a->width, (float)(g->v + g->h) / a->height,
                            rgba);
        x += step * s;
        previous = code;
    }
}
static void dock_name(const studio_dock *d, const char *text, float cx, float base,
                      const uint8_t col[4])
{
    if (!d->font_ok)
        return;
    static const uint8_t s1[4] = {6, 7, 9, 120}, s2[4] = {6, 7, 9, 70};
    float s = d->scale > 0 ? d->scale : DOCK_SCALE;
    dock_name_pass(d, text, cx, base, s, s, s1, 1.f);
    dock_name_pass(d, text, cx, base, 2.f * s, 2.f * s, s2, 1.f);
    dock_name_pass(d, text, cx, base, 0.f, 0.f, col, 1.f);
}
/* Text in the disc's face at a size, left (align 0), centred (1) or right (2) of x. */
static void dock_text(const studio_dock *d, const char *text, float x, float base, float size,
                      int align, const uint8_t col[4])
{
    if (!d->font_ok || !text)
        return;
    float s = DOCK_NAME_SCALE * size * (d->scale > 0 ? d->scale : DOCK_SCALE);
    float w = dock_text_width(&d->font, text) * s;
    float cx = align == 1 ? x : align == 2 ? x - w / 2.f : x + w / 2.f;
    dock_name_pass(d, text, cx, base, 0.f, 0.f, col, size);
}
/* P: fit a complete label inside its allotted column; keep input geometry
   on the same measured scale as the rendered value. */
static float studio_text_fit(const studio_dock *d, const char *text, float size, float room)
{
    float scale = d->scale > 0 ? d->scale : DOCK_SCALE;
    float width = dock_text_width(&d->font, text) * DOCK_NAME_SCALE * size * scale;
    return width > room && width > 0 ? size * fmaxf(0, room) / width : size;
}
/* P: the launcher's room moods (Dante: "rgb colors that will reflect on the car and the
   floor") and its light/dark theme. */
typedef struct {
    const char *name;
    float rgb[3];
} studio_mood;
static const studio_mood studio_moods[] = {
    {"White", {1, 1, 1}},       {"Warm", {1, .85f, .65f}},  {"Red", {1, .32f, .28f}},
    {"Orange", {1, .6f, .25f}}, {"Yellow", {1, .92f, .35f}}, {"Green", {.4f, 1, .45f}},
    {"Cyan", {.35f, .95f, 1}},  {"Blue", {.35f, .55f, 1}},  {"Purple", {.7f, .4f, 1}},
    {"Pink", {1, .45f, .8f}}};
#define STUDIO_MOODS (int)(sizeof studio_moods / sizeof *studio_moods)
static void dock_window_to_pixels(int x, int y, float *px, float *py);
static int studio_dark; /* the theme the room and the corner text follow */
static float studio_scene_shade; /* the Scene's fade in: a shade at the top, light text over it */
#define STUDIO_LIGHT_INK (studio_dark || studio_scene_shade > .5f)
static launcher_preferences *studio_prefs; /* launcher.ini (defined with the studio) */
/* What the home screen tells the player before START (P: PC presentation). */
typedef struct {
    int disc_ok;
    char disc[160], profile[160], car[160], mods[80], renderer[80];
} studio_info;
static studio_info studio_status;
static void studio_info_draw(const studio_dock *d)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE, w = d->fw > 0 ? d->fw : 640.f * S;
    const float pad = 16.f * S, line = 15.f * S;
    static const uint8_t ink_l[4] = {38, 40, 45, 235}, soft_l[4] = {96, 99, 106, 230},
                         ink_d[4] = {228, 230, 235, 240}, soft_d[4] = {150, 153, 160, 230};
    const uint8_t *ink = STUDIO_LIGHT_INK ? ink_d : ink_l, *soft = STUDIO_LIGHT_INK ? soft_d : soft_l;
    if (studio_scene_shade > 0) { /* a picture behind: the text gets a shade to stand on */
        static const uint8_t soft_s[4] = {205, 208, 214, 235};
        if (studio_scene_shade > .5f)
            soft = soft_s;
        const uint8_t top = (uint8_t)(215 * studio_scene_shade),
                      shade[4][4] = {{0, 0, 0, top}, {0, 0, 0, top}, {0, 0, 0, 0}, {0, 0, 0, 0}};
        plat_draw_quad4(NULL, 0, 0, w, 165.f * S, 0, 0, 1, 1, shade);
    }
    /* placeholder wordmark until the artist's logo exists */
    dock_text(d, "MOON ENGINE", pad, pad + 22.f * S, 1.9f, 0, ink);
    dock_text(d, "v" MOON_VERSION "   (logo placeholder)", pad + 1.f * S, pad + 36.f * S, .7f, 0,
              soft);
    /* status, right-aligned; the disc first, with its light */
    float y = pad + 12.f * S, right = w - pad;
    const uint8_t lamp[4] = {studio_status.disc_ok ? 40 : 205, studio_status.disc_ok ? 170 : 50,
                             studio_status.disc_ok ? 80 : 45, 255};
    float status_room = w * .55f - pad;
    float disc_size = studio_text_fit(d, studio_status.disc, .78f, status_room - 12.f * S);
    float tw = d->font_ok
                   ? dock_text_width(&d->font, studio_status.disc) * DOCK_NAME_SCALE * disc_size * S
                   : 0;
    plat_draw_quad(NULL, right - tw - 12.f * S, y - 7.f * S, 7.f * S, 7.f * S, 0, 0, 1, 1, lamp);
    dock_text(d, studio_status.disc, right, y, disc_size, 2, ink);
    const char *rows[] = {studio_status.profile, studio_status.car, studio_status.mods};
    for (int i = 0; i < 3; i++)
        if (rows[i][0])
            dock_text(d, rows[i], right, y + line * (i + 1),
                      studio_text_fit(d, rows[i], .7f, status_room), 2, soft);
}
#ifndef __SWITCH__
#include "launcher_mixer.inc"
#endif

/* OPTIONS: a see-through panel inside the launcher (Dante's rule: the room stays visible
   behind it). GENERAL includes the room's theme and light colour (launcher.ini).
   VIDEO, INTERFACE and QUALITY use the same store as the game's Screen Settings,
   saved as they change and applied at START. */
#define STUDIO_TABS 8
static const char *const studio_tab_names[STUDIO_TABS] = {"GENERAL", "VIDEO",    "INTERFACE",
                                                          "SOUND",   "CONTROLS", "QUALITY",
                                                          "TWEAKS",  "ADVANCED"};
/* The game's own sound and pad settings (the earlier launcher's pages): launcher_options,
   read once and written to the game's options file and save when the panel closes. */
static launcher_options studio_game_options;
static int studio_game_options_ready, studio_game_options_dirty; /* dirty: groups to save */
static char studio_video_file[LAUNCHER_PATH];                     /* <prefs>video.ini */
static int studio_pad_port;
static const char *const studio_volume_names[7] = {"Menu Music",   "Menu Effects",
                                                   "Race Music",   "Race Effects",
                                                   "Replay Music", "Replay Effects",
                                                   "Slide Show Music"};
#include "launcher_bindings.inc"
static void studio_game_options_load(void)
{
    if (studio_game_options_ready || !studio_video_file[0] || !studio_prefs)
        return;
    studio_game_options_ready =
        launcher_options_load(&studio_game_options, studio_video_file, studio_prefs->saves);
    gt4_save_init(studio_prefs->saves); /* launcher_options_load shut the game's session */
    if (!studio_game_options_ready)
        launcher_options_defaults(&studio_game_options);
    studio_game_options_ready = 1;
}
static void studio_game_options_save(void)
{
    if (!studio_game_options_dirty || !studio_video_file[0] || !studio_prefs)
        return;
    if (!launcher_options_save(&studio_game_options, studio_video_file, studio_prefs->saves,
                               studio_game_options_dirty))
        fprintf(stderr, "studio: sound/controller settings could not be saved\n");
    gt4_save_init(studio_prefs->saves); /* it shut the game's session; reopen it, with them */
    studio_game_options_dirty = 0;
}
static const char *studio_scene_name(void); /* launcher_scene.inc */
/* the rows a tab lists. General: -1 theme, -2 light colour, -3 scene, then video ids; sound: 1000+k;
   controls: 2000 the player, 2001+i the functions */
static int studio_tab_rows(int tab, int rows[24])
{
    int n = 0;
    if (tab == 0) {
        rows[n++] = -1;
        rows[n++] = -2;
        rows[n++] = -3;
#ifdef _WIN32
        if (app_video_settings && app_video_settings->current.preset_tools) {
            rows[n++]=3000; rows[n++]=3001;
        }
#endif
    }
    if (tab == 3) {
        for (int k = 0; k < 7; k++) rows[n++] = 1000 + k;
#ifdef _WIN32
        rows[n++] = -4; /* Open the actual custom-music folder. */
#endif
    }
    if (tab == 7) {
        static const char *const labels[] = {"3D Resolution", "Realistic Lighting", "Material Shading",
                                             "Shadow Maps", "Car Shadows", "Reflection Quality",
                                             "Anti-Aliasing", "HDR", "Sun Rays",
                                             "Ambient Occlusion", "Night Sky"};
        for (unsigned k = 0; k < sizeof labels / sizeof labels[0]; k++)
            for (int i = 0; i < video_setting_count(); i++)
                if (!strcmp(video_setting(i)->label, labels[k])) {
                    rows[n++] = i;
                    break;
                }
    }
    if (tab == 6) {
        /* The same ordered rows as the in-game Tweaks page, from one table.
           Closing Sense of Speed hides its children; the rows below move up
           because the list is built without them. */
        int open = !app_video_settings || app_video_settings->tweaks_open;
        int group = video_setting_tweak_group();
        for (int i = 0; i < video_settings_tweak_count() && n < 24; i++) {
            int id = group + i;
            if (!open && video_setting_tweak_is_child(id))
                continue;
            rows[n++] = id;
        }
    } else if (tab <= 3 || tab >= 5) {
        const char *group = tab == 5 ? "Quality"
                            : tab == 0 ? "General"
                            : tab == 1 ? "Video"
                            : tab == 3 ? "Sound" /* P: the Jukebox's options, after the volumes */
                                       : "Interface";
        for (int i = 0; i < video_setting_count() && n < 24; i++)
            if (!strcmp(video_setting(i)->group, group))
                rows[n++] = i;
    } else
        for (int k = 0; k < 12; k++)
            rows[n++] = 2000 + k;
    if (tab == 3 && n < 24)
        rows[n++] = -5; /* Download music: after the music/jukebox rows */
    return n;
}
/* Debug gate: both settings pages must expose the same ordered Tweaks rows.
   MOON_SETTINGS_PARITY=1 prints the listing and exits non-zero on a drift. */
static int studio_tweaks_parity(void)
{
    if (!app_video_settings)
        return 0;
    video_settings base = *app_video_settings;
    video_settings_preset_values(&base.current, 0);
    base.current.skip_intro = 0;
    base.current.assist_line = 0;
    base.current.discord_presence = 0;
    base.tweaks_open = 1;
    int saved = app_video_settings->tweaks_open, ok = 1;
    for (int open = 1; open >= 0; open--) {
        app_video_settings->tweaks_open = open;
        int ids[24], n = studio_tab_rows(6, ids), k = 0;
        for (int i = 0; i < video_settings_tweak_count(); i++) {
            int id = video_setting_tweak_group() + i;
            if (!open && video_setting_tweak_is_child(id))
                continue;
            if (k >= n || ids[k] != id) {
                ok = 0;
                break;
            }
            const video_setting_info *in = video_setting(id);
            int cur = video_setting_get(app_video_settings, id);
            int def = video_setting_get(&base, id);
            printf("parity: %s row=%d %s default=%s current=%s\n", open ? "open" : "closed", k,
                   in->label, def >= 0 && def < in->count ? in->names[def] : "-",
                   cur >= 0 && cur < in->count ? in->names[cur] : "-");
            k++;
        }
        if (k != n)
            ok = 0;
    }
    app_video_settings->tweaks_open = saved;
    return ok;
}
/* P: Import and Export Preset are buttons (they open a file window), not choices */
static int studio_row_button(int id)
{
    return id == -4 || id >= 3000;
}
static const char *studio_row_label(int id)
{
    if (id == -4) return "Open Music Folder";
    if (id == -5) return "Download music...";
    if (id>=3000) return id==3000 ? "Import preset..." : "Export preset...";
    if (id == -1)
        return "Theme";
    if (id == -2)
        return "Light Colour";
    if (id == -3)
        return "Scene";
    if (id >= 2000)
        return id == 2000 ? "Controller" : studio_binding_names[id - 2001];
    if (id >= 1000)
        return studio_volume_names[id - 1000];
    return video_setting(id)->label;
}
static const char *studio_row_help(int id)
{
    if (id == -4) return "Open Music Folder: Add WAV, MP3, FLAC, OGG or M4A files here, then reopen Jukebox.";
    if (id == -5) return "Download Music: Paste a song or playlist link; the file lands in the Music folder.";
    static char line[192];
    if (id>=3000) return app_video_settings && app_video_settings->message[0] ? app_video_settings->message : "Import or export a personal preset file. Display and save paths stay local.";
    if (id == -1)
        return "Theme: The room in light or dark.";
    if (id == -2)
        return "Light Colour: The room's light; it shows on the car and the floor.";
    if (id == -3)
        return "Scene: What stands behind the car: the room, a Photo Travel place or a course from "
               "the disc.";
    if (id == 2000)
        return "Controller: Whose buttons the list below sets.";
    if (id > 2000) {
        snprintf(line, sizeof line, "%s: Open the list, choose a button and confirm. Cancel keeps the current mapping.",
                 studio_binding_names[id - 2001]);
        return line;
    }
    if (id >= 1000) {
        snprintf(line, sizeof line, "%s: The game's own volume for it.", studio_volume_names[id - 1000]);
        return line;
    }
    snprintf(line, sizeof line, "%s: %s", video_setting(id)->label, video_setting(id)->help);
    return line;
}
static int studio_field_size_warn(int id)
{
    if (!video_setting_is_field_size(id) || !app_video_settings)
        return 0;
    int index = video_setting_get(app_video_settings, id);
    return video_settings_field_size_warning(MOON_CONDUCTOR_CARS + index);
}
static const char *studio_row_value(int id, int theme, int mood)
{
    if (id == -4) return "Open Folder...";
    if (id == -5) return "Open...";
    static char text[96];
    if (id>=3000) return id==3000 ? "Choose File..." : "Save As...";
    if (id == -1)
        return theme ? "Dark" : "Light";
    if (id == -2)
        return studio_moods[mood].name;
    if (id == -3)
        return studio_scene_name();
    if (id >= 1000) {
        studio_game_options_load();
        if (id == 2000)
            return studio_pad_port ? "Player 2" : "Player 1";
        if (id > 2000) {
            int v = studio_binding_get(&studio_game_options.pad[studio_pad_port], id - 2001);
            return id == 2001 ? studio_steer_names[v & 3] : studio_key_names[v % 21];
        }
        snprintf(text, sizeof text, "%d%%",
                 (studio_game_options.volume[id - 1000] * 100 + 64) / 128);
        return text;
    }
    const video_setting_info *in = video_setting(id);
    int v = app_video_settings ? video_setting_get(app_video_settings, id) : 0;
    if (!strcmp(in->label, "3D Resolution")) {
        static const int pct[] = {35, 50, 65, 75, 100, 125};
        static const int fps[] = {143, 119, 99, 88, 65, 0};
        int k = v >= 0 && v < 6 ? v : 4;
        if (fps[k])
            snprintf(text, sizeof text, "%d%%  (about %d fps on GTX 1060)", pct[k], fps[k]);
        else
            snprintf(text, sizeof text, "%d%%", pct[k]);
        return text;
    }
    if (!strcmp(in->label,"PC Preset") && app_video_settings) {
        snprintf(text,sizeof text,"%s%s",app_video_settings->store.preset,app_video_settings->store.modified ? " (modified)" : "");
        return text;
    }
    return v >= 0 && v < in->count ? in->names[v] : "-";
}
/* one row's change, for every kind of row except the launcher's own two */
static void studio_row_change(int id, int delta)
{
    if (id == -5) {
        fprintf(stderr, "studio: download music row\n");
        app_music_download_open();
        return;
    }
    if (id == -4) {
#ifdef _WIN32
        char folder[1100];
        if (app_video_settings && app_music_folder(app_video_settings->store.path, folder, sizeof folder)) {
            wchar_t wide[1100];
            int ok = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1, wide, 1100);
            if (!ok || (INT_PTR)ShellExecuteW(NULL, L"open", wide, NULL, NULL, SW_SHOWNORMAL) <= 32)
                snprintf(app_video_settings->message, sizeof app_video_settings->message, "Could not open the Music folder.");
        } else if (app_video_settings)
            snprintf(app_video_settings->message, sizeof app_video_settings->message, "Could not create the Music folder. Check folder permissions.");
#endif
        return;
    }

    if (id>=3000) {
        if (app_video_settings) video_settings_preset_dialog(app_video_settings,id==3001,0);
        return;
    }
    if (id >= 2000) {
        studio_game_options_load();
        if (id == 2000) {
            studio_pad_port ^= 1;
            return;
        }
        studio_binding_begin(&studio_binding_picker,&studio_game_options.pad[studio_pad_port],
                             id-2001,studio_pad_port);
    } else if (id >= 1000) {
        studio_game_options_load();
        int *v = &studio_game_options.volume[id - 1000];
        *v += delta * 8;
        *v = *v < 0 ? 0 : *v > 128 ? 128 : *v;
        studio_game_options_dirty |= 2;
    } else if (id >= 0 && app_video_settings)
        video_setting_set(app_video_settings, id, video_setting_get(app_video_settings, id) + delta);
}
typedef struct {
    float x, y, w, h, row0, pitch, tab_y;
    float right; /* where the values end; 0: the panel's right margin */
} studio_panel;
/* the tabs, side by side at their own widths; seven labels shrink rather than drop */
static float studio_tab_scale(const studio_dock *d)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float base = .9f, total = (STUDIO_TABS - 1) * 22.f * S;
    for (int t = 0; t < STUDIO_TABS; t++)
        total += dock_text_width(&d->font, studio_tab_names[t]) * DOCK_NAME_SCALE * base * S;
    float avail = d->fw > 0 ? d->fw - 40.f * S : 640.f * S;
    if (total > avail && total > 0) {
        base *= avail / total;
        if (base < .5f)
            base = .5f;
    }
    return base;
}
static float studio_tab_w(const studio_dock *d, int t)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    return dock_text_width(&d->font, studio_tab_names[t]) * DOCK_NAME_SCALE * studio_tab_scale(d) * S;
}
static float studio_tab_x(const studio_dock *d, int t)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float x = 12.f * S;
    for (int k = 0; k < t; k++)
        x += studio_tab_w(d, k) + 22.f * S;
    return x;
}
static studio_panel studio_panel_at(const studio_dock *d, int rows)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    (void)rows;
    const float fw = d->fw > 0 ? d->fw : 640.f * S;
    studio_panel p = {14.f * S, 80.f * S, fw - 28.f * S, 0, 0, 0, 0};
    p.h = d->oy + (363.f - 30.f) * S - p.y; /* down to above the icons (the band starts at 363) */
    p.tab_y = p.y + 24.f * S;
    p.row0 = p.y + 56.f * S;
    p.pitch = 20.f * S; /* the longest tab (12 rows) fits the panel */
    if (p.row0 + p.pitch * 12 > p.y + p.h)
        p.pitch = (p.y + p.h - p.row0 - 4.f * S) / 12.f;
    p.right = p.x + p.w - 14.f * S;
    return p;
}
/* P: INTERFACE and QUALITY in the first-drive guide's two columns: the rows on the left, the
   focused option's comparison and the guide's words on the right (guide_options_draw) */
#define STUDIO_GUIDE_COLUMN 270.f
static int studio_tab_guided(int tab)
{
    (void)tab; /* P: every tab: each option has its demonstration (launcher_visuals.inc) */
    return 1;
}
static studio_panel studio_options_panel(const studio_dock *d, int tab)
{
    int ids[24];
    studio_panel p = studio_panel_at(d, studio_tab_rows(tab, ids));
    if (studio_tab_guided(tab))
        p.right =
            p.x + fminf(STUDIO_GUIDE_COLUMN * (d->scale > 0 ? d->scale : DOCK_SCALE), p.w * .48f);
    return p;
}
/* A tab longer than the panel (QUALITY) scrolls: studio_options_top is its first shown row,
   moved only as far as keeps the selected row in view. */
static int studio_options_top;
static int studio_options_visible(const studio_panel *p, float S)
{
    return (int)((p->y + p->h - p->row0 - 4.f * S) / p->pitch);
}
static void studio_options_follow(int row, int n, int visible)
{
    if (row < studio_options_top)
        studio_options_top = row;
    if (row >= studio_options_top + visible)
        studio_options_top = row - visible + 1;
    if (studio_options_top > n - visible)
        studio_options_top = n - visible;
    if (studio_options_top < 0)
        studio_options_top = 0;
}
/* which arrow of a row's value a framebuffer x is on: -1 the "<" half, 1 the ">" half,
   0 off the value (the name: a click there goes forward) */
static int studio_value_side(const studio_dock *d, const studio_panel *p, const char *value,
                             float px)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float right = p->right > 0 ? p->right : p->x + p->w - 14.f * S,
          size = studio_text_fit(d, value, .8f, (right - p->x - 24.f * S) * .48f),
          w = dock_text_width(&d->font, value) * DOCK_NAME_SCALE * size * S;
    if (px >= right - w - 6.f * S && px < right - w / 2.f)
        return -1;
    if (px >= right - w / 2.f && px <= right + 10.f * S)
        return 1;
    return 0;
}
static int studio_tab_hover = -1; /* the tab under the mouse, -1 none */
static void studio_options_draw(const studio_dock *d, int tab, int row, int theme, int mood,
                                float alpha, int arrow)
{
    if (alpha <= 0)
        return;
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    int ids[24], n = studio_tab_rows(tab, ids);
    studio_panel p = studio_options_panel(d, tab);
    const int guided = studio_tab_guided(tab);
    const uint8_t panel[4] = {12, 13, 16, (uint8_t)((guided ? 205 : 175) * alpha)},
                  edge[4] = {255, 255, 255, (uint8_t)(90 * alpha)},
                  head[4] = {255, 255, 255, (uint8_t)(240 * alpha)},
                  dim[4] = {160, 163, 170, (uint8_t)(220 * alpha)},
                  hl[4] = {255, 255, 255, (uint8_t)(30 * alpha)};
    plat_draw_quad(NULL, p.x, p.y, p.w, p.h, 0, 0, 1, 1, panel);
    plat_draw_quad(NULL, p.x, p.y, p.w, fmaxf(1.f, .7f * S), 0, 0, 1, 1, edge);
    /* the tabs, the open one lit and underlined */
    for (int t = 0; t < STUDIO_TABS; t++) {
        float tx = p.x + studio_tab_x(d, t), tw = studio_tab_w(d, t);
        int hot = t == studio_tab_hover && t != tab;
        if (hot) { /* the tab the mouse is on: a soft box and a fainter underline */
            const uint8_t box[4] = {255, 255, 255, (uint8_t)(30 * alpha)},
                          line[4] = {255, 255, 255, (uint8_t)(110 * alpha)};
            plat_draw_quad(NULL, tx - 6.f * S, p.tab_y - 15.f * S, tw + 12.f * S, 24.f * S, 0, 0, 1,
                           1, box);
            plat_draw_quad(NULL, tx, p.tab_y + 5.f * S, tw, fmaxf(1.f, S), 0, 0, 1, 1, line);
        }
        dock_text(d, studio_tab_names[t], tx, p.tab_y, studio_tab_scale(d), 0,
                  t == tab || hot ? head : dim);
        if (t == tab)
            plat_draw_quad(NULL, tx, p.tab_y + 5.f * S, tw, fmaxf(1.f, S), 0, 0, 1, 1, head);
    }
    plat_draw_quad(NULL, p.x + 8.f * S, p.tab_y + 12.f * S, p.w - 16.f * S, fmaxf(1.f, .5f * S), 0,
                   0, 1, 1, hl);
    int visible = studio_options_visible(&p, S);
    studio_options_follow(row, n, visible);
    /* two columns: the guide's line between them, the scroll bar on it.
       The long Tweaks list marks its scroll on the left edge, clear of labels. */
    int tweaks = tab == STUDIO_TABS - 1;
    float bar = tweaks ? p.x + 2.f * S : guided ? p.right + 7.f * S : p.x + p.w - 5.f * S;
    if (guided && n <= visible)
        plat_draw_quad(NULL, p.right + 8.f * S, p.tab_y + 18.f * S, fmaxf(1.f, .5f * S),
                       p.y + p.h - p.tab_y - 26.f * S, 0, 0, 1, 1, hl);
    if (n > visible) { /* a thin scroll bar at the right edge: where the shown rows sit */
        float top = p.row0 - p.pitch * .72f, len = p.pitch * visible;
        const uint8_t track[4] = {255, 255, 255, (uint8_t)(24 * alpha)},
                      thumb[4] = {255, 255, 255, (uint8_t)(120 * alpha)};
        plat_draw_quad(NULL, bar, top, 2.f * S, len, 0, 0, 1, 1, track);
        plat_draw_quad(NULL, bar, top + len * studio_options_top / n, 2.f * S,
                       len * visible / n, 0, 0, 1, 1, thumb);
    }
    if(row>=0 && row<n && studio_binding_is_choice(ids[row]))arrow=0;
    for (int r = studio_options_top; r < n && r < studio_options_top + visible; r++) {
        float ry = p.row0 + p.pitch * (r - studio_options_top);
        if (r == row)
            plat_draw_quad(NULL, p.x + 4.f * S, ry - p.pitch * .72f,
                           guided ? p.right - p.x : p.w - 8.f * S, p.pitch * .95f, 0, 0, 1, 1, hl);
        char v[80];
        snprintf(v, sizeof v, studio_binding_is_choice(ids[r]) ? "%s..." : "<  %s  >", studio_row_value(ids[r], theme, mood));
        float right = p.right, left = p.x + 14.f * S;
        float value_room = (right - left - 10.f * S) * .48f;
        float value_size = studio_text_fit(d, v, .8f, value_room);
        float value_width = dock_text_width(&d->font, v) * DOCK_NAME_SCALE * value_size * S;
        int field_warn = studio_field_size_warn(ids[r]);
        const uint8_t red[4] = {235, 54, 54, (uint8_t)(255 * alpha)};
        const char *label = field_warn ? "Field Size - Experimental above 20 cars"
                                       : studio_row_label(ids[r]);
        float label_size = studio_text_fit(d, label, .8f, right - value_width - 10.f * S - left);
        dock_text(d, label, left, ry, label_size, 0, field_warn ? red : r == row ? head : dim);
        if (field_warn) {
            float bx = p.x + 4.f * S, by = ry - p.pitch * .72f,
                  bw = (guided ? p.right - p.x : p.w - 8.f * S), bh = p.pitch * .95f,
                  t = fmaxf(1.f, S);
            plat_draw_quad(NULL, bx, by, bw, t, 0, 0, 1, 1, red);
            plat_draw_quad(NULL, bx, by + bh - t, bw, t, 0, 0, 1, 1, red);
            plat_draw_quad(NULL, bx, by, t, bh, 0, 0, 1, 1, red);
            plat_draw_quad(NULL, bx + bw - t, by, t, bh, 0, 0, 1, 1, red);
        }
        if (getenv("MOONSTUDIO_LAYOUT_TRACE")) {
            static unsigned reported[STUDIO_TABS];
            if (!(reported[tab] & (1u << r))) {
                reported[tab] |= 1u << r;
                printf("STUDIO_ROW %d %d label=%.3f value=%.3f left=%.3f right=%.3f raw=%.3f\n",
                       tab, r, dock_text_width(&d->font, label) * DOCK_NAME_SCALE * label_size * S,
                       value_width, left, right,
                       dock_text_width(&d->font, label) * DOCK_NAME_SCALE * .8f * S);
            }
        }
        if (studio_row_button(ids[r])) { /* a button: a box with its action, no arrows */
            const char *a = studio_row_value(ids[r], theme, mood);
            float action_size = studio_text_fit(d, a, .78f, value_room - 24.f * S);
            float bw = dock_text_width(&d->font, a) * DOCK_NAME_SCALE * action_size * S + 24.f * S,
                  by = ry - p.pitch * .7f, bh = p.pitch * .86f;
            const uint8_t fill[4] = {255, 255, 255, (uint8_t)((r == row ? 46 : 18) * alpha)},
                          gold[4] = {255, 204, 64, (uint8_t)(230 * alpha)};
            plat_draw_quad(NULL, right - bw, by, bw, bh, 0, 0, 1, 1, fill);
            if (r == row) {
                float t = fmaxf(1.f, 1.2f * S);
                plat_draw_quad(NULL, right - bw, by, bw, t, 0, 0, 1, 1, gold);
                plat_draw_quad(NULL, right - bw, by + bh - t, bw, t, 0, 0, 1, 1, gold);
                plat_draw_quad(NULL, right - bw, by, t, bh, 0, 0, 1, 1, gold);
                plat_draw_quad(NULL, right - t, by, t, bh, 0, 0, 1, 1, gold);
            }
            dock_text(d, a, right - bw * .5f, ry, action_size, 1, r == row ? head : dim);
            continue;
        }
        if (r == row && arrow) {
            /* the arrow the mouse would press: a soft box and the arrow in gold */
            float w = value_width,
                  aw = dock_text_width(&d->font, "<") * DOCK_NAME_SCALE * value_size * S,
                  ax = arrow < 0 ? right - w : right - aw;
            const uint8_t box[4] = {255, 255, 255, (uint8_t)(46 * alpha)};
            plat_draw_quad(NULL, ax - 5.f * S, ry - p.pitch * .68f, aw + 10.f * S, p.pitch * .86f,
                           0, 0, 1, 1, box);
        }
        dock_text(d, v, right, ry, value_size, 2, field_warn ? red : r == row ? head : dim);
        if (r == row && arrow) {
            const uint8_t gold[4] = {255, 204, 64, (uint8_t)(255 * alpha)};
            if (arrow < 0) {
                float w = value_width;
                dock_text(d, "<", right - w, ry, value_size, 0, gold);
            } else
                dock_text(d, ">", right, ry, value_size, 2, gold);
        }
        if (ids[r] == -2) { /* the colour itself */
            const studio_mood *md = &studio_moods[mood];
            const uint8_t sw[4] = {(uint8_t)(255 * md->rgb[0]), (uint8_t)(255 * md->rgb[1]),
                                   (uint8_t)(255 * md->rgb[2]), (uint8_t)(255 * alpha)};
            plat_draw_quad(NULL, p.x + 170.f * S, ry - 9.f * S, 9.f * S, 9.f * S, 0, 0, 1, 1, sw);
        }
    }
}
/* what is under a framebuffer point: a row (>= 0), a tab (-10 - t), the guide's column (-2),
   or -1 */
static int studio_options_hit(const studio_dock *d, int tab, float px, float py)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    int ids[24], n = studio_tab_rows(tab, ids);
    studio_panel p = studio_options_panel(d, tab);
    if (px < p.x || px > p.x + p.w || py < p.y || py > p.y + p.h)
        return -1;
    if (py < p.tab_y + 10.f * S) {
        for (int t = 0; t < STUDIO_TABS; t++)
            if (px >= p.x + studio_tab_x(d, t) - 6.f * S &&
                px < p.x + studio_tab_x(d, t) + studio_tab_w(d, t) + 6.f * S)
                return -10 - t;
        return -1;
    }
    int visible = studio_options_visible(&p, S);
    if (px > p.right + 6.f * S)
        return -2; /* the guide's column: in the panel, on no row */
    for (int r = studio_options_top; r < n && r < studio_options_top + visible; r++) {
        float ry = p.row0 + p.pitch * (r - studio_options_top);
        if (py >= ry - p.pitch * .72f && py < ry + p.pitch * .28f)
            return r;
    }
    return -1;
}
#include "launcher_binding_picker.inc"
/* MODS: the mods installed in <prefs>/mods, each on or off, in the OPTIONS panel's look
   (P). The tick list is <prefs>/mods.ini, the very file the engine reads (mod_manifest.c);
   a change is written at once and the engine's list is read again, so START loads it. The
   last row opens the folder. MOON_OVERLAY (a dev hand-over) wins over the list. */
#define STUDIO_MODS_MAX 32
static struct {
    int n;
    char dir[1400];
    char folder[STUDIO_MODS_MAX][128], name[STUDIO_MODS_MAX][96], help[STUDIO_MODS_MAX][200];
    int on[STUDIO_MODS_MAX];
} studio_mods;
static void studio_mods_status(void)
{
    int on = 0;
    for (int i = 0; i < studio_mods.n; i++)
        on += studio_mods.on[i];
    const char *env = getenv("MOON_OVERLAY");
    if (env && *env)
        snprintf(studio_status.mods, sizeof studio_status.mods, "Mods: set by MOON_OVERLAY");
    else
        snprintf(studio_status.mods, sizeof studio_status.mods,
                 on ? "Mods: %d active" : "Mods: none active", on);
}
/* A mod as the SDK hands it out (a .zip with mod.txt at its root) is unpacked into the folder
   of its name, once: entries with ".." or a drive or a leading slash are refused. */
static int studio_mods_unpack(const char *zip, const char *dest)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zip, 0))
        return 0;
    int ok = 1, n = (int)mz_zip_reader_get_num_files(&z);
    for (int i = 0; i < n && ok; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, (mz_uint)i, &st) || st.m_is_directory)
            continue;
        const char *name = st.m_filename;
        if (strstr(name, "..") || name[0] == '/' || name[0] == '\\' || strchr(name, ':')) {
            ok = 0;
            break;
        }
        char out[1700];
        snprintf(out, sizeof out, "%s/%s", dest, name);
        for (char *p = out + strlen(dest) + 1; *p; p++) /* the folders on the way */
            if (*p == '/' || *p == '\\') {
                char c = *p;
                *p = 0;
                gt4_save_mkdir(out);
                *p = c;
            }
        ok = mz_zip_reader_extract_to_file(&z, (mz_uint)i, out, 0);
    }
    mz_zip_reader_end(&z);
    return ok;
}
static void studio_mods_scan(void)
{
    memset(&studio_mods, 0, sizeof studio_mods);
    if (!moon_mods_dir(studio_mods.dir, sizeof studio_mods.dir))
        return;
    DIR *d = opendir(studio_mods.dir);
    if (d) { /* zips first: each one without its folder becomes that folder */
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t n = strlen(e->d_name);
            if (e->d_type == DT_DIR || n < 5 || n >= 128 || strcasecmp(e->d_name + n - 4, ".zip"))
                continue;
            char zip[1600], dest[1600];
            snprintf(zip, sizeof zip, "%s/%s", studio_mods.dir, e->d_name);
            snprintf(dest, sizeof dest, "%s/%.*s", studio_mods.dir, (int)(n - 4), e->d_name);
            DIR *there = opendir(dest);
            if (there) {
                closedir(there);
                continue;
            }
            gt4_save_mkdir(dest);
            int ok = studio_mods_unpack(zip, dest);
            printf("studio: mods: %s %s\n", e->d_name, ok ? "unpacked" : "could NOT be unpacked");
        }
        closedir(d);
    }
    d = opendir(studio_mods.dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL && studio_mods.n < STUDIO_MODS_MAX) {
            if (e->d_name[0] == '.' || e->d_type != DT_DIR || strlen(e->d_name) >= 128)
                continue;
            int k = studio_mods.n++;
            snprintf(studio_mods.folder[k], sizeof studio_mods.folder[k], "%s", e->d_name);
            for (; k > 0 && strcasecmp(studio_mods.folder[k], studio_mods.folder[k - 1]) < 0; k--) {
                char t[128];
                memcpy(t, studio_mods.folder[k], sizeof t);
                memcpy(studio_mods.folder[k], studio_mods.folder[k - 1], sizeof t);
                memcpy(studio_mods.folder[k - 1], t, sizeof t);
            }
        }
        closedir(d);
    }
    for (int i = 0; i < studio_mods.n; i++) {
        char full[1600], desc[160] = "", by[64] = "", ver[32] = "", car[64] = "", course[64] = "";
        snprintf(full, sizeof full, "%s/%s", studio_mods.dir, studio_mods.folder[i]);
        if (!moon_manifest_field(full, "name", studio_mods.name[i], sizeof studio_mods.name[i]))
            snprintf(studio_mods.name[i], sizeof studio_mods.name[i], "%s", studio_mods.folder[i]);
        moon_manifest_field(full, "description", desc, sizeof desc);
        moon_manifest_field(full, "author", by, sizeof by);
        moon_manifest_field(full, "version", ver, sizeof ver);
        if (!desc[0]) { /* no prose: what it adds, from its manifest */
            moon_manifest_field(full, "car", car, sizeof car);
            moon_manifest_field(full, "course", course, sizeof course);
            strtok(car, " \t");
            strtok(course, " \t");
            snprintf(desc, sizeof desc, "%s%s%s%s%s", car[0] ? "Adds the car " : "", car,
                     car[0] && course[0] ? "; " : "", course[0] ? "Adds the course " : "", course);
            if (desc[0])
                strncat(desc, ".", sizeof desc - strlen(desc) - 1);
        }
        snprintf(studio_mods.help[i], sizeof studio_mods.help[i], "%s%s%s: %s%s%s", studio_mods.name[i],
                 ver[0] ? " " : "", ver, desc[0] ? desc : "A mod folder.", by[0] ? "  By " : "", by);
    }
    char ini[1500], line[256];
    snprintf(ini, sizeof ini, "%s.ini", studio_mods.dir);
    FILE *f = fopen(ini, "rb");
    while (f && fgets(line, sizeof line, f)) {
        char *hash = strchr(line, '#'), *p = line;
        if (hash)
            *hash = 0;
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' ||
                     line[n - 1] == '\t'))
            line[--n] = 0;
        while (*p == ' ' || *p == '\t')
            p++;
        for (int i = 0; i < studio_mods.n; i++)
            if (*p && !strcmp(studio_mods.folder[i], p))
                studio_mods.on[i] = 1;
    }
    if (f)
        fclose(f);
    studio_mods_status();
}
static void studio_mods_save(void)
{
    char ini[1500];
    snprintf(ini, sizeof ini, "%s.ini", studio_mods.dir);
    FILE *f = studio_mods.dir[0] ? fopen(ini, "wb") : NULL;
    if (!f) {
        fprintf(stderr, "studio: mods: %s could not be written\n", ini);
        return;
    }
    fprintf(f,
            "# MOON ENGINE - the mods that are on. Written by the manager, read by the engine.\n");
    for (int i = 0; i < studio_mods.n; i++)
        if (studio_mods.on[i])
            fprintf(f, "%s\n", studio_mods.folder[i]);
    fclose(f);
    moon_mod_roots_reload();
    studio_mods_status();
    printf("studio: mods: %s\n", moon_mod_roots()[0] ? moon_mod_roots() : "none");
}
static void studio_mods_open_folder(void)
{
#ifdef _WIN32
    wchar_t w[1400];
    if (studio_mods.dir[0] && MultiByteToWideChar(CP_UTF8, 0, studio_mods.dir, -1, w, 1400))
        ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
#endif
}
static int studio_mods_rows(void)
{
    return studio_mods.n + 1;
} /* the mods, then the folder */
static const char *studio_mods_help(int row)
{
    static char line[200];
    if (row < studio_mods.n)
        return studio_mods.help[row];
    snprintf(line, sizeof line, "%s",
             studio_mods.n ? "Mods Folder: Open it to add or remove mods; each mod is one folder."
                           : "Mods Folder: No mods yet. Put each mod in its own folder here.");
    return line;
}
/* the first row shown, so the chosen one stays inside the panel's 12 */
static int studio_mods_first(int row)
{
    return row > 11 ? row - 11 : 0;
}
static void studio_mods_draw(const studio_dock *d, int row, float alpha, int arrow)
{
    if (alpha <= 0)
        return;
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    studio_panel p = studio_panel_at(d, 12);
    const uint8_t panel[4] = {12, 13, 16, (uint8_t)(175 * alpha)},
                  edge[4] = {255, 255, 255, (uint8_t)(90 * alpha)},
                  head[4] = {255, 255, 255, (uint8_t)(240 * alpha)},
                  dim[4] = {160, 163, 170, (uint8_t)(220 * alpha)},
                  hl[4] = {255, 255, 255, (uint8_t)(30 * alpha)};
    plat_draw_quad(NULL, p.x, p.y, p.w, p.h, 0, 0, 1, 1, panel);
    plat_draw_quad(NULL, p.x, p.y, p.w, fmaxf(1.f, .7f * S), 0, 0, 1, 1, edge);
    /* one tab, lit and underlined; how many are on, at the right */
    float tx = p.x + 12.f * S, tw = dock_text_width(&d->font, "MODS") * DOCK_NAME_SCALE * .9f * S;
    dock_text(d, "MODS", tx, p.tab_y, .9f, 0, head);
    plat_draw_quad(NULL, tx, p.tab_y + 5.f * S, tw, fmaxf(1.f, S), 0, 0, 1, 1, head);
    dock_text(d, studio_status.mods + 6, p.x + p.w - 14.f * S, p.tab_y, .8f, 2, dim);
    plat_draw_quad(NULL, p.x + 8.f * S, p.tab_y + 12.f * S, p.w - 16.f * S, fmaxf(1.f, .5f * S), 0,
                   0, 1, 1, hl);
    int first = studio_mods_first(row), rows = studio_mods_rows();
    for (int r = first; r < rows && r < first + 12; r++) {
        float ry = p.row0 + p.pitch * (r - first), right = p.x + p.w - 14.f * S;
        int folder = r == studio_mods.n;
        if (r == row)
            plat_draw_quad(NULL, p.x + 4.f * S, ry - p.pitch * .72f, p.w - 8.f * S, p.pitch * .95f,
                           0, 0, 1, 1, hl);
        dock_text(d, folder ? "Mods Folder" : studio_mods.name[r], p.x + 14.f * S, ry, .8f, 0,
                  r == row ? head : dim);
        char v[32];
        snprintf(v, sizeof v, "%s", folder ? "Open" : studio_mods.on[r] ? "<  On  >" : "<  Off  >");
        if (r == row && arrow && !folder) {
            float w = dock_text_width(&d->font, v) * DOCK_NAME_SCALE * .8f * S,
                  aw = dock_text_width(&d->font, "<") * DOCK_NAME_SCALE * .8f * S,
                  ax = arrow < 0 ? right - w : right - aw;
            const uint8_t box[4] = {255, 255, 255, (uint8_t)(46 * alpha)};
            plat_draw_quad(NULL, ax - 5.f * S, ry - p.pitch * .68f, aw + 10.f * S, p.pitch * .86f,
                           0, 0, 1, 1, box);
        }
        const uint8_t gold[4] = {255, 204, 64, (uint8_t)(255 * alpha)};
        dock_text(d, v, right, ry, .8f, 2,
                  r == row && arrow && folder ? gold
                  : r == row                  ? head
                                              : dim);
        if (r == row && arrow && !folder) {
            if (arrow < 0)
                dock_text(d, "<", right - dock_text_width(&d->font, v) * DOCK_NAME_SCALE * .8f * S,
                          ry, .8f, 0, gold);
            else
                dock_text(d, ">", right, ry, .8f, 2, gold);
        }
    }
}
/* the row under a framebuffer point, or -1 (inside the panel: -2) */
static int studio_mods_hit(const studio_dock *d, int row, float px, float py)
{
    studio_panel p = studio_panel_at(d, 12);
    if (px < p.x || px > p.x + p.w || py < p.y || py > p.y + p.h)
        return -1;
    int first = studio_mods_first(row), rows = studio_mods_rows();
    for (int r = first; r < rows && r < first + 12; r++) {
        float ry = p.row0 + p.pitch * (r - first);
        if (py >= ry - p.pitch * .72f && py < ry + p.pitch * .28f)
            return r;
    }
    return -2;
}
/* The line under the bar that says what the chosen slot does (the game's tooltip strip). */
static const char *const dock_captions[DOCK_COUNT] = {
    "Start: Begin Gran Turismo 4.", "Options: Video, sound and controller settings.",
    "Mods: Choose the cars and courses to load with the game.", "Exit: Close Moon Engine."};
static const char *dock_caption_override; /* the OPTIONS panel's help line, while it is open */
static void dock_caption(const studio_dock *d, const char *text, float cx, float base,
                         const uint8_t col[4])
{
    if (!d->font_ok)
        return;
    static const uint8_t sh[4] = {0, 0, 0, 110};
    float s = d->scale > 0 ? d->scale : DOCK_SCALE, size = .92f;
    /* a long help line shrinks to the strip (centred, so 2*cx is its width) instead of
       running off both edges */
    float wide = dock_text_width(&d->font, text) * DOCK_NAME_SCALE * size * s, room = 2.f * cx * .94f;
    if (wide > room)
        size *= room / wide;
    dock_name_pass(d, text, cx, base, .7f * s, .7f * s, sh, size);
    dock_name_pass(d, text, cx, base, 0.f, 0.f, col, size);
}
/* Decode the names' face from the user's own disc (/font/system.fnt is FT01; font.h) and
   turn its ink atlas into the signed distance field the glyph quad samples (glyph_sdf.h),
   exactly as ui.c presents the disc's own faces. */
static int dock_font_load(studio_dock *d, gt4_vol *vol)
{
    if (d->font_ok)
        return 1;
    if (!vol)
        return 0;
    size_t n = 0;
    uint8_t *data = gt4_vol_read(vol, "/font/system.fnt", &n);
    if (!data)
        return 0;
    gt4_font f;
    int ok = gt4_font_decode(data, n, DOCK_NAME_FACE, &f);
    free(data);
    if (ok != 1) {
        fprintf(stderr, "studio: dock names: face %s unavailable\n", DOCK_NAME_FACE);
        return 0;
    }
    const tex1_image *ink = &f.atlases[f.natlases > 1 ? 1 : 0];
    int w = ink->width, h = ink->height, sw = 0, sh = 0;
    int big = w > h ? w : h, k = big > 0 ? 2048 / big : 1;
    if (k > 4)
        k = 4;
    if (k < 1)
        k = 1;
    uint8_t *sdf = gt4_glyph_sdf(ink->rgba, w, h, k, &sw, &sh);
    if (!sdf) {
        gt4_font_free(&f);
        return 0;
    }
    d->font = f;
    d->font_atlas = ink;
    d->font_tex = plat_texture_create(sw, sh, sdf);
    free(sdf);
    d->font_ok = d->font_tex != NULL;
    printf("studio: dock names in the disc's own face %s%s\n", DOCK_NAME_FACE,
           d->font_ok ? "" : " - unavailable");
    return d->font_ok;
}
/* The dock's slot geometry: four cells, 26px side margins, the pointer's tip parked
   under each name. The tilt across the row is the engine's own cursor_align table
   (ui.c {45,22.5,0,-22.5,-45,...}), its bottom edge left to right. The dock is a desktop
   toolbar: its design pixels are drawn 1:1 in the framebuffer, centred and bottom-anchored. */
/* The game's own menu bar (the dealers'), measured in the 640x448 design: four slots 128
   apart and centred, a 55-line band from 363, the black help strip from 418 to the bottom,
   48-line icons rising 23 above the band, names on 408, the help text on 436. */
#define DOCK_PAD 64.f
#define DOCK_BOX 48.f
#define DOCK_BAND_TOP 363.f
#define DOCK_STRIP_TOP 418.f
#define DOCK_CURSOR_Y 404.f
#define DOCK_BOTTOM_MARGIN 0.f
#define DOCK_WIDTH 640.f
static const float dock_tilt[DOCK_COUNT] = {22.5f, 0.f, -22.5f, -45.f};
/* a slot's width and its centre, in design pixels */
static float dock_slot_w(int i)
{
    float sum = 0;
    for (int j = 0; j < DOCK_COUNT; j++)
        sum += dock_weight[j];
    return (DOCK_WIDTH - DOCK_PAD * 2.f) * dock_weight[i] / sum;
}
static float dock_slot_cx(int i)
{
    float x = DOCK_PAD;
    for (int j = 0; j < i; j++)
        x += dock_slot_w(j);
    return x + dock_slot_w(i) / 2.f;
}
/* The dock's pixel placement for the current framebuffer: centred, bottom-anchored. Its size
   follows the window height (anchored at the 1280x960 default, where it is DOCK_SCALE), so the
   buttons look the same on a 1080p and a 1440p window; it never overflows a narrow one. */
static void dock_metrics(studio_dock *d, int fw, int fh)
{
    /* the design spans the window's width (as the game's bar spans its frame), never
       taller than the window */
    float s = fminf((float)fw / DOCK_WIDTH, (float)fh / 448.f);
    if (s < 0.4f)
        s = 0.4f;
    d->scale = s;
    d->fw = (float)fw;
    d->fh = (float)fh;
    d->ox = ((float)fw - DOCK_WIDTH * s) * .5f;
    d->oy = (float)fh - DOCK_BOTTOM_MARGIN * (s / DOCK_SCALE) - 448.f * s;
}
static void dock_cursor_target(studio_dock *d, int sel)
{
    if (sel < 0)
        sel = 0;
    if (sel >= DOCK_COUNT)
        sel = DOCK_COUNT - 1;
    d->ctx = dock_slot_cx(sel);
    d->cty = DOCK_CURSOR_Y;
    d->cta = dock_tilt[sel];
}
/* Hovering a slot selects it, exactly as the arrow keys do. The dock is drawn in framebuffer
   pixels (view = framebuffer, fill mode), so the window point maps straight to a pixel and the
   hit-test is in the same coordinates as the drawing. */
static int dock_hover(studio_dock *d, int *sel, int mx, int my)
{
    SDL_Window *win = plat_sdl_window();
    int ww = 0, wh = 0, fw = 0, fh = 0;
    if (!win)
        return 0;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GL_GetDrawableSize(win, &fw, &fh);
    if (ww < 1 || wh < 1)
        return 0;
    float vx = (float)mx * (fw > 0 ? fw : ww) / (float)ww,
          vy = (float)my * (fh > 0 ? fh : wh) / (float)wh;
    float px = (vx - d->ox) / (d->scale > 0 ? d->scale : DOCK_SCALE),
          py = (vy - d->oy) / (d->scale > 0 ? d->scale : DOCK_SCALE);
    if (py < 448.f - 120.f)
        return 0; /* only the dock band selects */
    for (int i = 0; i < DOCK_COUNT; i++) {
        float cx = dock_slot_cx(i), slot = dock_slot_w(i);
        if (px >= cx - slot / 2.f && px < cx + slot / 2.f) {
            if (i != *sel) {
                *sel = i;
                dock_cursor_target(d, i);
                printf("studio: dock %s\n", dock_names[i]);
            }
            return 1;
        }
    }
    return 0;
}
/* One frame of the game's cursor: accumulate the 60Hz ticks and step the engine's
   port once per tick, exactly as ui_animate drives it (ui.c:2398). Ratio 1 is the
   value the menus set (menu.c:5352). */
/* the window point in framebuffer pixels (the dock draws 1:1 there) */
static void dock_window_to_pixels(int x, int y, float *px, float *py)
{
    SDL_Window *win = plat_sdl_window();
    int ww = 1, wh = 1, fw = 0, fh = 0;
    if (win) {
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GL_GetDrawableSize(win, &fw, &fh);
    }
    *px = (float)x * (fw > 0 ? fw : ww) / (float)(ww > 0 ? ww : 1);
    *py = (float)y * (fh > 0 ? fh : wh) / (float)(wh > 0 ? wh : 1);
}
static void dock_hover_step(studio_dock *d, int sel, float seconds)
{
    float k = 1 - expf(-seconds * 12.f);
    for (int i = 0; i < DOCK_COUNT; i++)
        d->hover[i] += ((i == sel ? 1.f : 0.f) - d->hover[i]) * k;
}
static void dock_cursor_step(studio_dock *d, float seconds)
{
    if (!(seconds > 0))
        return;
    d->ticks += (double)seconds * 60.0;
    while (d->ticks >= 1.0 - 1e-6) {
        d->ticks -= 1;
        gt4_cursor_update(&d->cx, &d->cy, &d->ca, d->ctx, d->cty, d->cta, 1.f, 0, 1, 1);
    }
}
/* A one-shot diagnostic (MOONSTUDIO_DOCK_TRACE=1): rest the pointer on the first slot,
   re-aim it at the third like a right-press would, and print its tip and tilt every tick,
   so the engine's chase-then-turn is visible in the numbers. It leaves the dock untouched. */
static void dock_cursor_trace(studio_dock *d)
{
    dock_cursor_target(d, 0);
    d->cx = d->ctx;
    d->cy = d->cty;
    d->ca = d->cta;
    d->ticks = 0;
    dock_cursor_target(d, 2);
    printf("studio: dock cursor trace: slot 0 -> slot 2, every tick (x, y, angle)\n");
    for (int k = 1; k <= 40; k++) {
        dock_cursor_step(d, 1.f / 60.f);
        printf("studio: dock cursor tick %2d  x=%7.3f y=%7.3f angle=%7.3f\n", k, d->cx, d->cy,
               d->ca);
    }
}
/* The dock itself: four big icons and their names, drawn 1:1 in framebuffer pixels (the caller
   sets the view to the framebuffer), centred and bottom-anchored, plus the game's own pointer
   under the chosen one. A desktop toolbar: it never scales with the window. */
static int studio_skip_dock(void)
{
    const char *v = getenv("MOON_STUDIO_SKIP");
    return v && *v && strstr(v, "dock") != NULL;
}
static void dock_draw(const studio_dock *d, int sel, int pointer)
{
    static const uint8_t opaque[4] = {255, 255, 255, 255};
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE, ox = d->ox, oy = d->oy;
    const float name_base = (DOCK_BAND_TOP + 45.f) * S;
    {
        /* P: the bar, after the game's own menu bars (a band the icons rise out of, a fine
           light edge on top, the description strip under it), in smoked glass so the white
           room and the floor's reflection show through. Full window width. */
        const float top = oy + DOCK_BAND_TOP * S, strip = oy + DOCK_STRIP_TOP * S,
                    bottom = d->fh > 0 ? d->fh : oy + 448.f * S, w = d->fw > 0 ? d->fw : ox * 2.f + 640.f * S;
        /* the band is the dealers' brushed metal (DealerRoot wall_usa, a strip of it), a fine
           light edge on top; the help line under it is the game's black strip */
        static const uint8_t edge[4] = {255, 255, 255, 110}, dark[4] = {0, 0, 0, 240},
                             seam[4] = {70, 72, 76, 255}, metal[4] = {255, 255, 255, 240};
        if (dock_band.tex)
            plat_draw_quad(dock_band.tex, 0, top, w, strip - top, 0, .30f, 1, .62f, metal);
        else {
            static const uint8_t band[4] = {38, 40, 44, 230};
            plat_draw_quad(NULL, 0, top, w, strip - top, 0, 0, 1, 1, band);
        }
        plat_draw_quad(NULL, 0, top, w, fmaxf(1.f, S * .7f), 0, 0, 1, 1, edge);
        plat_draw_quad(NULL, 0, strip, w, bottom - strip, 0, 0, 1, 1, dark);
        plat_draw_quad(NULL, 0, strip, w, fmaxf(1.f, S * .5f), 0, 0, 1, 1, seam);
        static const uint8_t ink[4] = {214, 216, 220, 255};
        if (sel >= 0 && sel < DOCK_COUNT)
            dock_caption(d, dock_caption_override ? dock_caption_override : dock_captions[sel],
                         w * .5f, oy + 436.f * S, ink);
    }
    for (int i = 0; i < DOCK_COUNT; i++) {
        dock_icon *ic = &dock_icons[i];
        float cx = ox + dock_slot_cx(i) * S;
        /* every icon the same size, centred a line under the band's top edge; the chosen
           one grows a little and rises */
        float lift = d->hover[i];
        float box = DOCK_BOX * S * (1.f + .14f * lift),
              cy = oy + (DOCK_BAND_TOP + 1.f - 4.f * lift) * S;
        if (ic->tex) {
            float iw = ic->w * (ic->u1 - ic->u0), ih = ic->h * (ic->v1 - ic->v0);
            if (iw < 1 || ih < 1) {
                iw = (float)ic->w;
                ih = (float)ic->h;
            }
            /* the same inked area for every icon (a wide one is not made small by its width),
               none wider than 1.5 boxes nor taller than 1.15 */
            float k = box / sqrtf(iw * ih);
            k = fminf(k, fminf(box * 1.5f / iw, box * 1.15f / ih));
            float w = iw * k, h = ih * k;
            uint8_t shade = (uint8_t)(200 + 55 * lift);
            const uint8_t tone[4] = {shade, shade, shade, 255};
            plat_draw_quad(ic->tex, cx - w / 2.f, cy - h / 2.f, w, h, ic->u0, ic->v0, ic->u1,
                           ic->v1, tone);
        }
        uint8_t col[4] = {255, 255, 255, (uint8_t)(170 + 85 * lift)};
        dock_name(d, dock_names[i], cx, oy + name_base, col);
    }
    /* the game's own pointer, 32x32 with its tip at (d->cx,d->cy), turned about that tip
       by d->ca - the same draw the menus use (ui.c:2081). */
    /* The game's own pointer replaces the system cursor: on the mouse it follows the hand
       (tip at the mouse, upright); on a pad or the keyboard it parks under the chosen slot. */
    if (pointer && dock_arrow.tex && d->mouse)
        plat_draw_rotated(dock_arrow.tex, d->mx - 16.f * S, d->my, 32.f * S, 32.f * S, d->mx,
                          d->my, 0, opaque);
    else if (pointer && dock_arrow.tex)
        plat_draw_rotated(dock_arrow.tex, ox + d->cx * S - 16.f * S, oy + d->cy * S, 32.f * S,
                          32.f * S, ox + d->cx * S, oy + d->cy * S, d->ca, opaque);
}

/* ---- first-run setup wizard ----
   A plain white screen, shown once: point Moon Engine at the user's disc, pick the graphics
   API, save both to launcher.ini. It is our own 2D on white - quiet text in the bundled face
   (the engine cannot lend its glyphs here: they are decoded from the disc, which is exactly
   what this screen chooses), no panels, no boxes. Arrows move, Enter confirms, Escape cancels. */
#define FRONT_CAR "gt40_race_69" /* the room's car when the GT Mode save rides none */

static plat_texture *wiz_font;
static stbtt_bakedchar wiz_ch[96];
static int wiz_font_tried;
static int wiz_font_init(void)
{
    if (wiz_font_tried)
        return wiz_font != NULL;
    wiz_font_tried = 1;
    static unsigned char bmp[512 * 512];
    if (stbtt_BakeFontBitmap(launcher_ttf_BARLOW_REGULAR, 0, 44.0f, bmp, 512, 512, 32, 96,
                             wiz_ch) <= 0)
        return 0;
    uint8_t *rgba = malloc((size_t)512 * 512 * 4);
    if (!rgba)
        return 0;
    for (size_t i = 0; i < (size_t)512 * 512; i++) {
        rgba[i * 4] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bmp[i];
    }
    wiz_font = plat_texture_create(512, 512, rgba);
    free(rgba);
    return wiz_font != NULL;
}
static float wiz_text_width(const char *s, float sc)
{
    float x = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s - 32;
        if (c < 0 || c >= 96)
            continue;
        x += wiz_ch[c].xadvance * sc;
    }
    return x;
}
static void wiz_text(const char *s, float x, float y, float sc, const uint8_t col[4])
{
    for (; *s; s++) {
        int c = (unsigned char)*s - 32;
        if (c < 0 || c >= 96)
            continue;
        const stbtt_bakedchar *b = &wiz_ch[c];
        if (b->x1 > b->x0)
            plat_draw_quad(wiz_font, x + b->xoff * sc, y + b->yoff * sc, (b->x1 - b->x0) * sc,
                           (b->y1 - b->y0) * sc, b->x0 / 512.f, b->y0 / 512.f, b->x1 / 512.f,
                           b->y1 / 512.f, col);
        x += b->xadvance * sc;
    }
}
static void wiz_text_center(const char *s, float cx, float y, float sc, const uint8_t col[4])
{
    wiz_text(s, cx - wiz_text_width(s, sc) / 2.f, y, sc, col);
}

/* The AFK clock: a small piece of the room, not a HUD readout. Same baked
   launcher face as the wizard, with deliberate tracking; the colon breathes
   once a second and the whole block fades with the cinematic. */
static float clock_text_width(const char *s, float sc, float tracking)
{
    float x = 0;
    for (; *s; s++) {
        int c = (unsigned char)*s - 32;
        if (c < 0 || c >= 96)
            continue;
        x += wiz_ch[c].xadvance * sc + tracking;
    }
    return x > tracking ? x - tracking : x;
}
static void clock_text(const char *s, float x, float y, float sc, float tracking,
                       const uint8_t col[4])
{
    for (; *s; s++) {
        int c = (unsigned char)*s - 32;
        if (c < 0 || c >= 96)
            continue;
        char one[2] = {(char)*s, 0};
        wiz_text(one, x, y, sc, col);
        x += wiz_ch[c].xadvance * sc + tracking;
    }
}
static void studio_clock_draw(const studio_dock *d, float alpha, double seconds)
{
    if (!d || alpha <= .004f || !wiz_font_init())
        return;
    const float unit = d->fh > 0 ? (float)d->fh / 720.f : 1.f;
    const float big = 1.15f * unit, date_scale = .38f * unit;
    const float track_big = 4.4f * unit, track_small = 2.6f * unit;
    const float x = 34.f * unit, y = 54.f * unit;
    time_t now = time(NULL);
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char hh[4], mm[4], date[32];
    snprintf(hh, sizeof hh, "%02d", tm.tm_hour);
    snprintf(mm, sizeof mm, "%02d", tm.tm_min);
    static const char *const weekday[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char *const month[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    int wd = tm.tm_wday >= 0 && tm.tm_wday < 7 ? tm.tm_wday : 0;
    int mo = tm.tm_mon >= 0 && tm.tm_mon < 12 ? tm.tm_mon : 0;
    snprintf(date, sizeof date, "%s %02d %s", weekday[wd], tm.tm_mday, month[mo]);

    float time_w = clock_text_width(hh, big, track_big) + track_big +
                   clock_text_width(":", big, track_big) + track_big +
                   clock_text_width(mm, big, track_big);
    float date_w = clock_text_width(date, date_scale, track_small);
    float block_w = fmaxf(time_w, date_w);

    /* A soft left-edge fade, then a shadow pass: legible on the white room. */
    uint8_t fade[4][4] = {{0, 0, 0, (uint8_t)(118 * alpha)}, {0, 0, 0, 0},
                          {0, 0, 0, (uint8_t)(72 * alpha)}, {0, 0, 0, 0}};
    plat_draw_quad4(NULL, 0, 0, block_w + 72.f * unit, 160.f * unit, 0, 0, 1, 1, fade);
    uint8_t shadow[4] = {0, 0, 0, (uint8_t)(150 * alpha)};
    float sx = x + 2.f * unit, sy = y + 2.f * unit;
    clock_text(hh, sx, sy, big, track_big, shadow);
    sx += clock_text_width(hh, big, track_big) + track_big;
    clock_text(":", sx, sy, big, track_big, shadow);
    sx += clock_text_width(":", big, track_big) + track_big;
    clock_text(mm, sx, sy, big, track_big, shadow);
    clock_text(date, x + 2.f * unit, y + 66.f * unit, date_scale, track_small, shadow);

    const uint8_t ink[4] = {238, 240, 244, (uint8_t)(250 * alpha)},
                  dim[4] = {205, 208, 214, (uint8_t)(220 * alpha)},
                  red[4] = {244, 36, 61, (uint8_t)(235 * alpha)};
    float tx = x;
    clock_text(hh, tx, y, big, track_big, ink);
    tx += clock_text_width(hh, big, track_big) + track_big;
    float pulse = .55f + .45f * (float)(.5 + .5 * cos(6.28318530718 * seconds));
    uint8_t colon[4] = {ink[0], ink[1], ink[2], (uint8_t)(ink[3] * pulse)};
    clock_text(":", tx, y, big, track_big, colon);
    tx += clock_text_width(":", big, track_big) + track_big;
    clock_text(mm, tx, y, big, track_big, ink);
    clock_text(date, x, y + 66.f * unit, date_scale, track_small, dim);
    plat_draw_quad(NULL, x, y + 108.f * unit, block_w * .62f, fmaxf(1.f, unit), 0, 0, 1, 1, red);
}

/* Both Windows toolchains have _putenv; only POSIX has setenv/unsetenv, and the
   MinGW build is the one that notices. An empty value removes the variable in the
   Windows CRT, which is what the POSIX branch does explicitly. */
static void wiz_disc_hint(const char *value)
{
#ifdef _WIN32
    char line[1100];
    snprintf(line, sizeof line, "MOONSTUDIO_DISC_HINT=%s", value ? value : "");
    _putenv(line);
#else
    if (value)
        setenv("MOONSTUDIO_DISC_HINT", value, 1);
    else
        unsetenv("MOONSTUDIO_DISC_HINT");
#endif
}
static int wiz_pick_disc(char *out, size_t size, const char *hint) /* the platform's own open dialog */
{
    /* The chooser opens in the folder of the disc already named, so the image the
       player is replacing is on screen instead of a home directory they must hunt. */
    wiz_disc_hint(hint && *hint ? hint : NULL);
    return plat_pick_disc(out, size);
}
static int wiz_valid(const char *path, char *msg, size_t n)
{
    if (!path || !path[0]) {
        if (msg)
            snprintf(msg, n, "Select your disc first.");
        return 0;
    }
    return launcher_validate_disc(path, msg, n);
}
static const char *wiz_shot_prefix;   /* dev (--setup-shot): capture the wizard steps headless */
static int wiz_came; /* the setup ran: the home screen grows out of its drawn bar */
static const char *start_shot_prefix; /* dev: capture the START fade mid-way and at black */
static int start_launch_env; /* dev: auto-press START and let the game boot (headless check) */

/* ---- first-run setup: the home screen's look before the disc exists (P) ----
   Nothing here may come from the game: the room is the generated white sweep, the bar is
   drawn (graphite band, fine light edge, black help strip, in the home bar's proportions),
   the type is the bundled Barlow and the buttons are words. Once the disc is chosen the
   home screen takes over with the disc's own metal, face and icons. */
static int wiz_dark; /* the saved theme: the setup is dark too */
static void wiz_room(float fw, float fh)
{
    const uint8_t a = wiz_dark ? 24 : 254, b = wiz_dark ? 46 : 242;
    const uint8_t wall[4] = {a, a, (uint8_t)(a + (wiz_dark ? 3 : -1)), 255},
                  ground[4] = {b, b, (uint8_t)(b + 3), 255};
    const uint8_t sweep[4][4] = {{wall[0], wall[1], wall[2], 255}, {wall[0], wall[1], wall[2], 255},
                                 {ground[0], ground[1], ground[2], 255},
                                 {ground[0], ground[1], ground[2], 255}};
    float y0 = fh * 250.f / 448.f, y1 = fh * 400.f / 448.f;
    plat_draw_quad(NULL, 0, 0, fw, y0, 0, 0, 1, 1, wall);
    plat_draw_quad4(NULL, 0, y0, fw, y1 - y0, 0, 0, 1, 1, sweep);
    plat_draw_quad(NULL, 0, y1, fw, fh - y1, 0, 0, 1, 1, ground);
}
typedef struct {
    float S, ox, oy, fw, fh;
    float hover[4];
    int n; /* slots on the bar, spread evenly */
} wiz_layout;
static void wiz_metrics(wiz_layout *l, float fw, float fh)
{
    l->S = fminf(fw / 640.f, fh / 448.f);
    l->fw = fw;
    l->fh = fh;
    l->ox = (fw - 640.f * l->S) * .5f;
    l->oy = fh - 448.f * l->S;
}
static float wiz_slot_x(const wiz_layout *l, int i)
{
    float step = 640.f / ((l->n > 0 ? l->n : 4) + 1);
    return l->ox + step * (i + 1) * l->S;
}
/* the slot under a window point, -1 off the bar */
static int wiz_slot_at(const wiz_layout *l, float x, float y)
{
    if (y < l->oy + 345.f * l->S || y > l->oy + 418.f * l->S)
        return -1;
    for (int i = 0; i < l->n; i++)
        if (fabsf(x - wiz_slot_x(l, i)) < 64.f * l->S)
            return i;
    return -1;
}
static void wiz_bar(const wiz_layout *l, const char *const labels[4], const int enabled[4],
                    int sel, const char *caption, float alpha)
{
    const float S = l->S, top = l->oy + 363.f * S, strip = l->oy + 418.f * S;
    const uint8_t a8 = (uint8_t)(255 * alpha);
    const uint8_t band[4][4] = {{62, 64, 70, (uint8_t)(235 * alpha)},
                                {62, 64, 70, (uint8_t)(235 * alpha)},
                                {32, 34, 38, (uint8_t)(235 * alpha)},
                                {32, 34, 38, (uint8_t)(235 * alpha)}};
    const uint8_t edge[4] = {255, 255, 255, (uint8_t)(110 * alpha)},
                  dark[4] = {0, 0, 0, (uint8_t)(240 * alpha)},
                  seam[4] = {70, 72, 76, a8};
    plat_draw_quad4(NULL, 0, top, l->fw, strip - top, 0, 0, 1, 1, band);
    plat_draw_quad(NULL, 0, top, l->fw, fmaxf(1.f, S * .7f), 0, 0, 1, 1, edge);
    plat_draw_quad(NULL, 0, strip, l->fw, l->fh - strip, 0, 0, 1, 1, dark);
    plat_draw_quad(NULL, 0, strip, l->fw, fmaxf(1.f, S * .5f), 0, 0, 1, 1, seam);
    for (int i = 0; i < l->n; i++) {
        float h = l->hover[i], sc = (.5f + .05f * h) * S / 1.5f;
        uint8_t v = enabled[i] ? (uint8_t)(175 + 80 * h) : 105;
        const uint8_t shadow[4] = {0, 0, 0, (uint8_t)(120 * alpha)},
                      col[4] = {v, v, v, a8};
        float x = wiz_slot_x(l, i) - wiz_text_width(labels[i], sc) / 2.f,
              y = l->oy + (400.f - 4.f * h) * S;
        wiz_text(labels[i], x + S, y + S, sc, shadow);
        wiz_text(labels[i], x, y, sc, col);
        if (i == sel) { /* the chosen word's underline, for pad and keyboard */
            const uint8_t line[4] = {255, 255, 255, (uint8_t)(170 * h * alpha)};
            float w = wiz_text_width(labels[i], sc);
            plat_draw_quad(NULL, wiz_slot_x(l, i) - w / 2.f, y + 7.f * S, w, fmaxf(1.f, S * .8f), 0,
                           0, 1, 1, line);
        }
    }
    if (caption) {
        const uint8_t ink[4] = {214, 216, 220, a8};
        wiz_text_center(caption, l->fw * .5f, l->oy + 437.f * S, .42f * S / 1.5f, ink);
    }
    (void)sel;
}
/* The checks behind launcher_validate_disc, in order; its message names the first failure. */
static int wiz_checks_passed(const char *path, int valid, const char *message)
{
    if (!path || !path[0])
        return -1;
    if (valid)
        return 4;
    if (strstr(message, "could not be found"))
        return 0;
    if (strstr(message, "not a PS2 disc image"))
        return 1;
    if (strstr(message, "not the NTSC-U"))
        return 2;
    return 3;
}
/* ---- a dropped folder: the disc is looked for inside it (and only there) ---- */
#define WIZ_HITS 6
static char wiz_hits[WIZ_HITS][LAUNCHER_PATH];
static SDL_atomic_t wiz_hit_count, wiz_scan_dirs;
static int wiz_scan_skip(const char *name)
{
    static const char *const skip[] = {"Windows", "Program Files", "Program Files (x86)",
                                       "ProgramData", "$Recycle.Bin", "System Volume Information",
                                       "AppData", "node_modules", ".git", "Recovery", "PerfLogs"};
    if (name[0] == '.')
        return 1;
    for (size_t i = 0; i < sizeof skip / sizeof *skip; i++)
        if (!strcasecmp(name, skip[i]))
            return 1;
    return 0;
}
static void wiz_scan_dir(const char *dir, int depth)
{
    if (SDL_AtomicGet(&wiz_hit_count) >= WIZ_HITS || SDL_AtomicGet(&wiz_scan_dirs) > 6000)
        return;
    SDL_AtomicAdd(&wiz_scan_dirs, 1);
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && SDL_AtomicGet(&wiz_hit_count) < WIZ_HITS) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        char path[LAUNCHER_PATH];
        size_t dl = strlen(dir);
        if (snprintf(path, sizeof path, "%s%s%s", dir,
                     dl && (dir[dl - 1] == '\\' || dir[dl - 1] == '/') ? "" : "\\",
                     e->d_name) >= (int)sizeof path)
            continue;
        if (e->d_type == DT_DIR) {
            if (depth > 0 && !wiz_scan_skip(e->d_name))
                wiz_scan_dir(path, depth - 1);
            continue;
        }
        size_t n = strlen(e->d_name);
        if (n < 4 || strcasecmp(e->d_name + n - 4, ".iso"))
            continue;
        char msg[256];
        if (launcher_validate_disc(path, msg, sizeof msg)) {
            int k = SDL_AtomicGet(&wiz_hit_count);
            int seen = 0;
            for (int i = 0; i < k; i++)
                seen |= !strcasecmp(wiz_hits[i], path);
            if (!seen && k < WIZ_HITS) {
                snprintf(wiz_hits[k], sizeof wiz_hits[k], "%s", path);
                SDL_AtomicSet(&wiz_hit_count, k + 1);
            }
        }
    }
    closedir(d);
}
/* A dropped file or folder: the disc itself, or the first Gran Turismo 4 disc inside the
   folder (three levels down). 1 with `out` set when one is found. */
static int wiz_dropped_disc(const char *path, char *out, size_t size)
{
    char msg[256];
    DIR *d = opendir(path);
    if (!d) {
        snprintf(out, size, "%s", path);
        return 1; /* a file: validated by the caller, which shows why it is not the disc */
    }
    closedir(d);
    SDL_AtomicSet(&wiz_hit_count, 0);
    SDL_AtomicSet(&wiz_scan_dirs, 0);
    wiz_scan_dir(path, 3);
    if (SDL_AtomicGet(&wiz_hit_count) > 0 &&
        launcher_validate_disc(wiz_hits[0], msg, sizeof msg)) {
        snprintf(out, size, "%s", wiz_hits[0]);
        return 1;
    }
    return 0;
}
static const char *wiz_base(const char *path)
{
    const char *a = strrchr(path, '/'), *b = strrchr(path, '\\');
    if (b && (!a || b > a))
        a = b;
    return a ? a + 1 : path;
}
#ifdef _WIN32
static void wiz_restart(void) /* the new graphics API takes effect in a fresh process */
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    wchar_t command[2048];
    wcsncpy(command, GetCommandLineW(), 2047);
    command[2047] = 0;
    if (CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
#endif
/* The launcher is the one phase that wants a finger to drive its existing pointer
   code: SDL synthesizes the mouse events the wizard and the studio already read.
   In-game touch belongs to the overlay (mobile_controls.c), so the hint goes back
   to the Android default as soon as the launcher hands over. */
static void launcher_touch_mouse(int on)
{
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, on ? "1" : "0");
    fprintf(stderr, "launcher: touch-mouse synthesis %s\n", on ? "on" : "off");
}
static void wiz_log(const char *fmt, ...)
{
    char pref[1200], path[1300], line[512];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    snprintf(path, sizeof path, "%swizard.log", pref);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    FILE *f = fopen(path, "rb");
    long size = 0;
    if (f) {
        if (!fseek(f, 0, SEEK_END))
            size = ftell(f);
        fclose(f);
    }
    f = fopen(path, size > 64 * 1024 ? "wb" : "ab");
    if (!f)
        return;
    time_t now = time(NULL);
    struct tm date;
    memset(&date, 0, sizeof date);
#ifdef _WIN32
    int ok = !localtime_s(&date, &now);
#else
    int ok = localtime_r(&now, &date) != NULL;
#endif
    if (ok)
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d %s\n", date.tm_year + 1900, date.tm_mon + 1,
                date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec, line);
    else
        fprintf(f, "%s\n", line);
    fflush(f);
    fclose(f);
}
static int app_win_x, app_win_y, app_win_w = 960, app_win_h = 720;
static int app_win_maximized, app_win_fullscreen, app_win_have_rect, app_win_dirty;
static uint64_t app_win_changed_ns;

static int app_window_int(const char *key, int fallback)
{
    const char *v = app_video_settings ? moon_options_get(&app_video_settings->store, key, "") : "";
    char *end;
    long n;
    if (!v || !*v)
        return fallback;
    n = strtol(v, &end, 10);
    return *end ? fallback : (int)n;
}

static void app_window_default_rect(int *x, int *y, int *w, int *h)
{
    SDL_Rect r;
    *w = 960;
    *h = 720;
    if (SDL_GetDisplayUsableBounds(0, &r) == 0) {
        *x = r.x + (r.w - *w) / 2;
        *y = r.y + (r.h - *h) / 2;
    } else
        *x = *y = 0;
}

static int app_window_clamp(int *x, int *y, int *w, int *h)
{
    int displays = SDL_GetNumVideoDisplays(), best_area = 0;
    SDL_Rect best = {0, 0, 0, 0}, want = {*x, *y, *w, *h};
    if (displays < 1)
        return 0;
    for (int i = 0; i < displays; i++) {
        SDL_Rect bounds, overlap;
        if (SDL_GetDisplayUsableBounds(i, &bounds) != 0)
            continue;
        if (SDL_IntersectRect(&want, &bounds, &overlap)) {
            int area = overlap.w * overlap.h;
            if (area > best_area) {
                best_area = area;
                best = bounds;
            }
        }
    }
    if (best_area < 64 * 64)
        return 0;
    if (*w > best.w)
        *w = best.w;
    if (*h > best.h)
        *h = best.h;
    if (*x < best.x)
        *x = best.x;
    if (*x + *w > best.x + best.w)
        *x = best.x + best.w - *w;
    if (*y < best.y)
        *y = best.y;
    if (*y + *h > best.y + best.h)
        *y = best.y + best.h - *h;
    return 1;
}

static void app_window_geometry_restore(plat_video_desc *d)
{
    if (!d || !app_video_settings)
        return;
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        SDL_Init(SDL_INIT_VIDEO);
    int w = app_window_int("window.width", 0), h = app_window_int("window.height", 0);
    int x = app_window_int("window.x", -1000000), y = app_window_int("window.y", -1000000);
    int maximized = app_window_int("window.maximized", 0);
    int fullscreen = app_window_int("window.fullscreen", d->fullscreen);
    int valid = w >= 480 && h >= 360;
    if (!valid) {
        app_window_default_rect(&x, &y, &w, &h);
        app_win_have_rect = 0;
        d->placed = 0;
    } else {
        if (x == -1000000 || y == -1000000) {
            SDL_Rect r;
            if (SDL_GetDisplayUsableBounds(0, &r) == 0) {
                x = r.x + (r.w - w) / 2;
                y = r.y + (r.h - h) / 2;
            } else
                x = y = 0;
            d->placed = 0;
        } else
            d->placed = 1;
        if (!app_window_clamp(&x, &y, &w, &h)) {
            app_window_default_rect(&x, &y, &w, &h);
            app_win_have_rect = 0;
            d->placed = 0;
        } else
            app_win_have_rect = 1;
    }
    app_win_x = x;
    app_win_y = y;
    app_win_w = w;
    app_win_h = h;
    app_win_maximized = maximized ? 1 : 0;
    app_win_fullscreen = fullscreen ? 1 : 0;
    d->width = w;
    d->height = h;
    d->x = x;
    d->y = y;
    d->maximized = app_win_maximized;
    d->fullscreen = app_win_fullscreen;
    app_win_dirty = 0;
    app_win_changed_ns = 0;
}

static void app_window_geometry_tick(int force)
{
    SDL_Window *win = plat_sdl_window();
    if (!app_video_settings || !win)
        return;
    /* a hidden (test) window has no geometry of the player's to keep */
    const char *hidden = getenv("MOON_WINDOW_HIDDEN");
    if (hidden && !strcmp(hidden, "1"))
        return;
    Uint32 flags = SDL_GetWindowFlags(win);
    int fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    int maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0 && !fullscreen;
    int changed = 0;
    if (!fullscreen && !maximized) {
        int x = 0, y = 0, w = 0, h = 0;
        SDL_GetWindowPosition(win, &x, &y);
        SDL_GetWindowSize(win, &w, &h);
        if (w > 0 && h > 0 &&
            (!app_win_have_rect || x != app_win_x || y != app_win_y || w != app_win_w ||
             h != app_win_h)) {
            app_win_x = x;
            app_win_y = y;
            app_win_w = w;
            app_win_h = h;
            app_win_have_rect = 1;
            changed = 1;
        }
    }
    if (app_win_maximized != maximized) {
        app_win_maximized = maximized;
        changed = 1;
    }
    if (app_win_fullscreen != fullscreen) {
        app_win_fullscreen = fullscreen;
        changed = 1;
    }
    if (changed) {
        app_win_dirty = 1;
        app_win_changed_ns = plat_time_ns();
    }
    if (!app_win_dirty || (!force && plat_time_ns() - app_win_changed_ns < 500000000ull))
        return;
    char value[32];
    int modified = app_video_settings->store.modified;
#define APP_WIN_KEY(key, number)                                                                    \
    do {                                                                                           \
        snprintf(value, sizeof value, "%d", (number));                                             \
        moon_options_set(&app_video_settings->store, key, value);                                  \
    } while (0)
    APP_WIN_KEY("window.x", app_win_x);
    APP_WIN_KEY("window.y", app_win_y);
    APP_WIN_KEY("window.width", app_win_w);
    APP_WIN_KEY("window.height", app_win_h);
    APP_WIN_KEY("window.maximized", app_win_maximized);
    APP_WIN_KEY("window.fullscreen", app_win_fullscreen);
#undef APP_WIN_KEY
    app_video_settings->store.modified = modified;
    moon_options_save(&app_video_settings->store);
    app_win_dirty = 0;
}

#ifndef __SWITCH__
/* Runs the setup on the current window: 1 = saved, 2 = saved and restarted for the new
   graphics API (the caller quits), 0 = cancelled (nothing written). */
static int first_run_wizard(launcher_preferences *prefs)
{
    launcher_touch_mouse(1);
    if (!wiz_font_init()) {
        launcher_touch_mouse(0);
        return 0;
    }
    char disc[LAUNCHER_PATH], status[256] = "";
    snprintf(disc, sizeof disc, "%s", prefs->disc);
    wiz_dark = prefs->theme;
    int valid = disc[0] && launcher_validate_disc(disc, status, sizeof status);
    if (valid)
        status[0] = 0;
    int step = 0, sel = 0, saved = 0, frame = 0, scanning = 0, list_sel = -1, pad_prev[6] = {0};
    int api = app_video_settings ? app_video_settings->current.renderer : MOON_RENDERER_OPENGL;
    int vk_probed = 0, vk_ok = 0;
    char vk_text[256] = "";
    wiz_layout L = {0};
    SDL_AtomicSet(&wiz_hit_count, 0);
    SDL_AtomicSet(&wiz_scan_dirs, 0);
    video_set_fill_window(1);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    int last_fw = -1, last_fh = -1, last_tw = -1, last_th = -1, last_full = -1;
    for (;;) {
        int fw = 0, fh = 0;
        SDL_GL_GetDrawableSize(plat_sdl_window(), &fw, &fh);
        if (fw < 1)
            fw = 640;
        if (fh < 1)
            fh = 448;
        wiz_metrics(&L, (float)fw, (float)fh);
        L.n = step ? 4 : 3;
        const char *const labels0[4] = {"CHOOSE DISC", "CONTINUE", "EXIT", ""};
        const char *const labels1[4] = {"VULKAN", "OPENGL", "BACK", "CONTINUE"};
        const char *const *labels = step ? labels1 : labels0;
        const int enabled0[4] = {1, valid, 1, 0}, enabled1[4] = {vk_ok, 1, 1, 1};
        const int *enabled = step ? enabled1 : enabled0;
        int activate = -1, quit = 0;
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (ev.type == SDL_DROPFILE) { /* the .iso (or its folder) dropped on the window */
                char found[LAUNCHER_PATH];
                if (ev.drop.file && step == 0) {
                    if (wiz_dropped_disc(ev.drop.file, found, sizeof found)) {
                        snprintf(disc, sizeof disc, "%s", found);
                        valid = launcher_validate_disc(disc, status, sizeof status);
                    } else {
                        valid = 0;
                        snprintf(disc, sizeof disc, "%s", ev.drop.file);
                        snprintf(status, sizeof status,
                                 "No Gran Turismo 4 disc in that folder.");
                    }
                    scanning = 0;
                    if (valid) {
                        status[0] = 0;
                        sel = 1;
                    }
                }
                SDL_free(ev.drop.file);
                continue;
            }
            float mx = 0, my = 0;
            if (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN) {
                int ww = 1, wh = 1;
                SDL_GetWindowSize(plat_sdl_window(), &ww, &wh);
                int x = ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x,
                    y = ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y;
                mx = (float)x * fw / (ww > 0 ? ww : 1);
                my = (float)y * fh / (wh > 0 ? wh : 1);
                int hit = wiz_slot_at(&L, mx, my);
                if (hit >= 0) {
                    sel = hit;
                    list_sel = -1;
                    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT)
                        activate = hit;
                }
                /* the found discs, as rows */
                int hits = SDL_AtomicGet(&wiz_hit_count);
                for (int i = 0; step == 0 && scanning && i < hits; i++) {
                    float ry = L.oy + (232.f + 18.f * i) * L.S;
                    if (my > ry - 13.f * L.S && my < ry + 5.f * L.S && fabsf(mx - fw * .5f) < 250.f * L.S) {
                        list_sel = i;
                        if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT)
                            activate = 100 + i;
                    }
                }
                continue;
            }
            if (ev.type != SDL_KEYDOWN)
                continue;
            int k = ev.key.keysym.sym;
            if (!ev.key.repeat &&
                (k == SDLK_F11 ||
                 ((k == SDLK_RETURN || k == SDLK_KP_ENTER) &&
                  (ev.key.keysym.mod & KMOD_ALT)))) {
                Uint32 flags = SDL_GetWindowFlags(plat_sdl_window());
                SDL_SetWindowFullscreen(plat_sdl_window(), (flags & SDL_WINDOW_FULLSCREEN)
                                                               ? 0
                                                               : SDL_WINDOW_FULLSCREEN_DESKTOP);
                continue;
            }
            if (k == SDLK_ESCAPE)
                activate = 2; /* BACK, or EXIT on the disc step */
            else if (k == SDLK_LEFT)
                sel = (sel + L.n - 1) % L.n, list_sel = -1;
            else if (k == SDLK_RIGHT)
                sel = (sel + 1) % L.n, list_sel = -1;
            else if (k == SDLK_UP && scanning && SDL_AtomicGet(&wiz_hit_count))
                list_sel = list_sel <= 0 ? SDL_AtomicGet(&wiz_hit_count) - 1 : list_sel - 1;
            else if (k == SDLK_DOWN && list_sel >= 0)
                list_sel = list_sel + 1 >= SDL_AtomicGet(&wiz_hit_count) ? -1 : list_sel + 1;
            else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
                activate = list_sel >= 0 ? 100 + list_sel : sel;
        }
        if (quit)
            break;
        {   /* the pad: left/right, up/down for the found discs, A confirms, B goes back */
            SDL_GameController *pad = plat_sdl_pad(0);
            if (pad) {
                int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
                int now[6] = {
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -16000,
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > 16000,
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP),
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN),
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A),
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B)};
                int hits = SDL_AtomicGet(&wiz_hit_count);
                if (frame > 1) {
                    if (now[0] && !pad_prev[0])
                        sel = (sel + L.n - 1) % L.n, list_sel = -1;
                    if (now[1] && !pad_prev[1])
                        sel = (sel + 1) % L.n, list_sel = -1;
                    if (now[2] && !pad_prev[2] && scanning && hits)
                        list_sel = list_sel <= 0 ? hits - 1 : list_sel - 1;
                    if (now[3] && !pad_prev[3] && list_sel >= 0)
                        list_sel = list_sel + 1 >= hits ? -1 : list_sel + 1;
                    if (now[4] && !pad_prev[4])
                        activate = list_sel >= 0 ? 100 + list_sel : sel;
                    if (now[5] && !pad_prev[5])
                        activate = 2;
                }
                memcpy(pad_prev, now, sizeof now);
            }
        }
        if (activate >= 100) { /* a found disc */
            snprintf(disc, sizeof disc, "%s", wiz_hits[activate - 100]);
            valid = launcher_validate_disc(disc, status, sizeof status);
            if (valid)
                status[0] = 0;
            scanning = 0;
            list_sel = -1;
            sel = 2;
        } else if (activate >= 0 && enabled[activate]) {
            if (step == 0) {
                if (activate == 0) {
                    char chosen[LAUNCHER_PATH];
                    if (wiz_pick_disc(chosen, sizeof chosen, disc)) {
                        snprintf(disc, sizeof disc, "%s", chosen);
                        valid = launcher_validate_disc(disc, status, sizeof status);
                        if (valid) {
                            status[0] = 0;
                            sel = 1;
                        }
                    }
                } else if (activate == 1) {
                    step = 1;
                    sel = api == MOON_RENDERER_VULKAN ? 0 : 1;
                } else
                    break; /* EXIT */
            } else {
                if (activate == 0)
                    api = MOON_RENDERER_VULKAN;
                else if (activate == 1)
                    api = MOON_RENDERER_OPENGL;
                else if (activate == 2) {
                    step = 0;
                    sel = 1;
                } else {
                    saved = 1;
                    break;
                }
            }
        }
        if (step == 1 && !vk_probed) {
            vk_probed = 1;
            vk_ok = moon_renderer_probe_vulkan(vk_text, sizeof vk_text);
            if (!vk_ok && api == MOON_RENDERER_VULKAN)
                api = MOON_RENDERER_OPENGL;
        }
        for (int i = 0; i < 4; i++)
            L.hover[i] += ((i == sel && list_sel < 0 ? 1.f : 0.f) - L.hover[i]) * .25f;

        {
            int draw_fw = 0, draw_fh = 0;
            SDL_GL_GetDrawableSize(plat_sdl_window(), &draw_fw, &draw_fh);
            if (draw_fw > 0 && draw_fh > 0 && (draw_fw != fw || draw_fh != fh)) {
                fw = draw_fw;
                fh = draw_fh;
                wiz_metrics(&L, (float)fw, (float)fh);
                L.n = step ? 4 : 3;
            }
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }
        plat_video_set_view((float)fw, (float)fh);
        wiz_room((float)fw, (float)fh);
        {
            int tw = 0, th = 0, full;
            video_target_size(&tw, &th);
            full = (SDL_GetWindowFlags(plat_sdl_window()) & SDL_WINDOW_FULLSCREEN) != 0;
            if (last_fw >= 0 &&
                (fw != last_fw || fh != last_fh || tw != last_tw || th != last_th ||
                 full != last_full))
                wiz_log("wizard: drawable %dx%d -> %dx%d fullscreen=%d renderer=%s target=%dx%d "
                        "font=512x512",
                        last_fw, last_fh, fw, fh, full,
                        moon_renderer_name(app_video_settings
                                               ? app_video_settings->current.renderer
                                               : MOON_RENDERER_OPENGL),
                        tw, th);
            last_fw = fw;
            last_fh = fh;
            last_tw = tw;
            last_th = th;
            last_full = full;
        }
        const float S = L.S, cx = fw * .5f;
        static const uint8_t ink_l[4] = {38, 40, 45, 255}, soft_l[4] = {110, 113, 120, 255},
                             ink_d[4] = {228, 230, 235, 255}, soft_d[4] = {150, 153, 160, 255},
                             warn[4] = {200, 70, 70, 255}, good[4] = {40, 150, 80, 255};
        const uint8_t *ink = wiz_dark ? ink_d : ink_l, *soft = wiz_dark ? soft_d : soft_l;
        /* the corner marks, as on the home screen */
        wiz_text("MOON ENGINE", 16.f * S, 38.f * S, .9f * S / 1.5f, ink);
        wiz_text("v" MOON_VERSION "   (logo placeholder)", 17.f * S, 52.f * S, .36f * S / 1.5f,
                 soft);
        {
            const char *line = valid ? "Gran Turismo 4 disc ready (SCUS-97328)" : "No disc chosen";
            float sc = .42f * S / 1.5f, w = wiz_text_width(line, sc), x = fw - 16.f * S - w;
            const uint8_t lamp[4] = {valid ? 40 : 205, valid ? 170 : 50, valid ? 80 : 45, 255};
            plat_draw_quad(NULL, x - 12.f * S, 21.f * S, 7.f * S, 7.f * S, 0, 0, 1, 1, lamp);
            wiz_text(line, x, 28.f * S, sc, ink);
            const char *steps = step ? "Setup  2 of 2  -  Graphics" : "Setup  1 of 2  -  Disc";
            wiz_text(steps, fw - 16.f * S - wiz_text_width(steps, .36f * S / 1.5f), 43.f * S,
                     .36f * S / 1.5f, soft);
        }
        const char *caption = NULL;
        if (step == 0) {
            wiz_text_center("Find your Gran Turismo 4 disc.", cx, L.oy + 120.f * S, .66f * S / 1.5f, ink);
            if (!disc[0])
                wiz_text_center("Choose the .iso, or drop it (or its folder) on this window.",
                                cx, L.oy + 142.f * S, .42f * S / 1.5f, soft);
            else {
                wiz_text_center(wiz_base(disc), cx, L.oy + 142.f * S, .46f * S / 1.5f, soft);
                const char *checks[4] = {"Disc image found", "PlayStation 2 disc image",
                                         "Gran Turismo 4, NTSC-U (SCUS-97328)", "Game files readable"};
                int passed = wiz_checks_passed(disc, valid, status);
                float x = cx - 120.f * S;
                for (int i = 0; i < 4 && !scanning; i++) {
                    float y = L.oy + (172.f + 18.f * i) * S;
                    const uint8_t mark[4] = {i < passed ? 40 : i == passed ? 205 : 190,
                                             i < passed ? 170 : i == passed ? 50 : 190,
                                             i < passed ? 80 : i == passed ? 45 : 194, 255};
                    plat_draw_quad(NULL, x, y - 8.f * S, 7.f * S, 7.f * S, 0, 0, 1, 1, mark);
                    wiz_text(checks[i], x + 14.f * S, y, .42f * S / 1.5f, i <= passed ? ink : soft);
                }
                if (!valid && status[0] && !scanning)
                    wiz_text_center(status, cx, L.oy + 252.f * S, .42f * S / 1.5f, warn);
            }
            static const char *const help0[3] = {
                "Choose disc: Find the Gran Turismo 4 disc image (.iso) on your computer.",
                "Continue: Next, the graphics.", "Exit: Close Moon Engine."};
            caption = help0[sel % 3];
            if (sel == 1 && !valid)
                caption = "Continue: Choose a valid disc first.";
        } else {
            wiz_text_center("Choose the graphics.", cx, L.oy + 120.f * S, .66f * S / 1.5f, ink);
            char line[320];
            snprintf(line, sizeof line, vk_ok ? "Graphics card: %s" : "Vulkan is not available here (%s).",
                     vk_text);
            wiz_text_center(line, cx, L.oy + 148.f * S, .44f * S / 1.5f, vk_ok ? soft : warn);
            snprintf(line, sizeof line, "Selected:  %s%s", api == MOON_RENDERER_VULKAN ? "Vulkan" : "OpenGL",
                     api == MOON_RENDERER_VULKAN ? "  (recommended)" : "");
            wiz_text_center(line, cx, L.oy + 190.f * S, .56f * S / 1.5f, ink);
            if (app_video_settings && api != app_video_settings->current.renderer)
                wiz_text_center("Moon Engine will reopen with it.", cx, L.oy + 212.f * S,
                                .38f * S / 1.5f, soft);
            static const char *const help1[4] = {
                "Vulkan: The modern renderer, recommended when your card supports it.",
                "OpenGL: The compatible renderer, for older graphics cards.",
                "Back: Choose another disc.", "Continue: Save and open Moon Engine."};
            caption = help1[sel];
            (void)good;
        }
        wiz_bar(&L, labels, enabled, list_sel < 0 ? sel : -1, caption, 1.f);
        moon_debug_overlay_draw(); /* G6.40: over the first-run setup too */
        /* dev (--setup-shot): script the setup and capture each step, no keyboard needed */
        if (wiz_shot_prefix && getenv("MOONSTUDIO_SETUP_DROP") && frame == 3) {
            /* dev: a file or folder dropped on the window */
            SDL_Event drop;
            memset(&drop, 0, sizeof drop);
            drop.type = SDL_DROPFILE;
            drop.drop.file = SDL_strdup(getenv("MOONSTUDIO_SETUP_DROP"));
            SDL_PushEvent(&drop);
        }
        if (wiz_shot_prefix && getenv("MOONSTUDIO_SETUP_DROP") && frame == 8) {
            char path[1200];
            snprintf(path, sizeof path, "%s-drop.png", wiz_shot_prefix);
            screenshot(path);
            fprintf(stderr, "wizard: dropped %s -> %s (%s)\n", getenv("MOONSTUDIO_SETUP_DROP"), disc,
                   valid ? "valid" : status);
            {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
            break;
        }
        if (wiz_shot_prefix && getenv("MOONSTUDIO_SETUP_DROP"))
            frame++;
        else if (wiz_shot_prefix) {
            frame++;
            char path[1200];
            if (step == 0 && frame == 4) {
                snprintf(path, sizeof path, "%s-1.png", wiz_shot_prefix);
                screenshot(path);
                printf("wizard: disc step -> %s\n", path);
            }
            if (step == 0 && frame >= 8) {
                const char *env = getenv("MOONSTUDIO_ISO");
                if (env && *env)
                    snprintf(disc, sizeof disc, "%s", env);
                valid = launcher_validate_disc(disc, status, sizeof status);
                if (valid) {
                    status[0] = 0;
                    step = 1;
                    sel = 0;
                    frame = 0;
                } else
                    frame = 0;
            }
            if (step == 1 && frame == 4) {
                snprintf(path, sizeof path, "%s-2.png", wiz_shot_prefix);
                screenshot(path);
                printf("wizard: api step -> %s\n", path);
            }
            if (step == 1 && frame >= 8) {
                api = app_video_settings ? app_video_settings->current.renderer : api;
                saved = 1;
                break;
            }
        } else
            frame++;
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
        app_window_geometry_tick(0);
        app_presence_poll("Launcher", NULL, NULL, NULL, -1, -1);
        plat_sleep_ms(16);
    }
    app_window_geometry_tick(1);
    if (!saved) {
        launcher_touch_mouse(0);
        return 0;
    }
    snprintf(prefs->disc, sizeof prefs->disc, "%s", disc);
    snprintf(prefs->api, sizeof prefs->api, "%s", moon_renderer_name(api));
    launcher_preferences_save(prefs);
    if (app_video_settings && api != app_video_settings->current.renderer) {
        video_settings_set_renderer(app_video_settings, api);
#ifdef _WIN32
        wiz_restart();
        launcher_touch_mouse(0);
        return 2;
#endif
    }
    launcher_touch_mouse(0);
    return 1;
}

#endif

/* The captain's rig (--studio): where he put the camera and the car. Saved as plain
   numbers next to the engine's other user data, so a position survives a restart and
   can be read back with any text tool. */
static void rig_defaults(float eye[3], float target[3], float car[3], float *angle)
{
    /* The captain's own camera: he framed the room on his machine on 2026-10-16 and this is
       where he left it (studio-rig.txt on Baki). The room opens looking like this. */
    const float e[3] = {9.0671f, 1.4608f, 1.9929f}, t[3] = {-0.6860f, 0.9667f, 3.1414f},
                c[3] = {-0.7700f, 0, 0.0733f};
    memcpy(eye, e, sizeof e);
    memcpy(target, t, sizeof t);
    memcpy(car, c, sizeof c);
    *angle = 0;
}
static int rig_load(const char *path, float eye[3], float target[3], float car[3], float *angle,
                    char *code, size_t code_size, int *locked)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    char key[32];
    float a, b, c;
    int got = 0, locked_int = 0;
    while (fscanf(f, "%31s", key) == 1) {
        if (!strcmp(key, "camera") && fscanf(f, "%f %f %f", &a, &b, &c) == 3) {
            eye[0] = a;
            eye[1] = b;
            eye[2] = c;
            got |= 1;
        } else if (!strcmp(key, "target") && fscanf(f, "%f %f %f", &a, &b, &c) == 3) {
            target[0] = a;
            target[1] = b;
            target[2] = c;
            got |= 2;
        } else if (!strcmp(key, "car") && fscanf(f, "%f %f %f", &a, &b, &c) == 3) {
            car[0] = a;
            car[1] = b;
            car[2] = c;
            got |= 4;
        } else if (!strcmp(key, "car_angle") && fscanf(f, "%f", &a) == 1) {
            *angle = a;
            got |= 8;
        } else if (!strcmp(key, "car_code") && fscanf(f, "%47s", code) == 1) {
            got |= 16;
        } else if (!strcmp(key, "camera_lock") && fscanf(f, "%d", &locked_int) == 1) {
            if (locked)
                *locked = locked_int;
            got |= 32;
        }
    }
    fclose(f);
    return got == 31;
}
static void rig_save(const char *path, const float eye[3], const float target[3],
                     const float car[3], float angle, const char *code, int locked)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fprintf(
        f,
        "# Moon Engine studio rig - where the captain put the camera and the car.\n"
        "# camera/target: look-at, scene units. car: offset on the floor. car_angle: radians.\n"
        "camera %.4f %.4f %.4f\ntarget %.4f %.4f %.4f\ncar %.4f %.4f %.4f\ncar_angle %.4f\ncar_code %s\ncamera_lock %d\n",
        eye[0], eye[1], eye[2], target[0], target[1], target[2], car[0], car[1], car[2], angle,
        code ? code : "", locked ? 1 : 0);
    fclose(f);
    printf(
        "studio rig: %s (camera %.2f %.2f %.2f  target %.2f %.2f %.2f  car %.2f %.2f %.2f  angle %.2f)\n",
        path, eye[0], eye[1], eye[2], target[0], target[1], target[2], car[0], car[1], car[2],
        angle);
}
/* Orbit the eye around the target: yaw about the world's up, pitch clamped so the
   camera never ends up under the floor. */
static void rig_orbit(float eye[3], const float target[3], float dyaw, float dpitch)
{
    float d[3] = {eye[0] - target[0], eye[1] - target[1], eye[2] - target[2]};
    float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-4f)
        return;
    float cy = cosf(dyaw), sy = sinf(dyaw);
    float nx = d[0] * cy - d[2] * sy, nz = d[0] * sy + d[2] * cy;
    float yaw = atan2f(nx, nz), pitch = atan2f(d[1], sqrtf(nx * nx + nz * nz)) + dpitch;
    if (pitch > 1.55f)
        pitch = 1.55f;
    if (pitch < -.02f)
        pitch = -.02f;
    float cd = cosf(pitch) * len, sd = sinf(pitch) * len;
    eye[0] = target[0] + sinf(yaw) * cd;
    eye[1] = target[1] + sd;
    eye[2] = target[2] + cosf(yaw) * cd;
}
/* One fade duration and one easing for every handover in the studio (the wizard->room rise
   and the START->cinematic fall to black): ~0.5 s at 60 Hz, eased in and out. */
#define FADE_FRAMES 30
static float fade_curve(float t)
{
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    return t * t * (3 - 2 * t);
}

/* ---- the launcher's cameras (P: PC presentation, not game camera data) ----
   Idle, the room makes one continuous, slow orbit of the parked car: a low, close
   lens looking up at it, no cuts and no time limit. Any input eases the camera back
   to the dock's framing. Each dock slot frames the car its own way: MODS closes in,
   OPTIONS looks down on it, START is the room's own framing, EXIT pulls back. The
   free rig (L) is never animated. */
#define LCAM_IDLE_SECONDS 20.f
#define LCAM_ORBIT_RADIUS 4.2f   /* metres from the car's middle */
#define LCAM_ORBIT_PERIOD 120.f  /* seconds per revolution */
#define LCAM_ORBIT_EYE_Y .45f    /* the lens sits low, under the car's shoulder */
#define LCAM_ORBIT_TARGET_Y .95f /* and looks up at the body */
#define LCAM_ORBIT_EASE 2.f      /* seconds to ease in and back out */
#define LCAM_MID_Z 0.f /* the body's middle along its length, in the car's frame */
static const float LCAM_HOME_EYE[3] = {-7.6f, .75f, LCAM_MID_Z}, LCAM_HOME_TARGET[3] = {0, .55f, LCAM_MID_Z};
/* The rig file's own camera (the room's first framing, the car leaving to the right) has
   the studio's lens: camera.mot's focal 979.88 over 224 lines is 25.7 degrees. */
#define LCAM_RIG_FOV 25.7f
#define LCAM_HOME_FOV 32.f
/* START chosen: the front three-quarter, the room's first framing of the car */
static const float LCAM_PLAY_EYE[3] = {-4.8f, 1.35f, -5.6f}, LCAM_PLAY_TARGET[3] = {.27f, .55f, -.23f};
/* The car's frame to the room: the showroom places the car as rot(angle) * p + offset. */
static void lcam_place(const float car[3], float angle, const float in[3], float out[3])
{
    float cs = cosf(angle), sn = sinf(angle);
    out[0] = cs * in[0] + sn * in[2] + car[0];
    out[1] = in[1] + car[1];
    out[2] = -sn * in[0] + cs * in[2] + car[2];
}
/* The idle orbit: a low lens on a fixed radius, looking up at the car. `yaw` is in
   the car's frame; rig_orbit turns the eye about the car's vertical axis and keeps
   the pitch sane, so the camera can never fall through the floor. */
static void lcam_orbit(float yaw, float eye[3], float aim[3], float *fov)
{
    const float axis[3] = {0, LCAM_ORBIT_EYE_Y, LCAM_MID_Z};
    eye[0] = 0;
    eye[1] = LCAM_ORBIT_EYE_Y;
    eye[2] = LCAM_ORBIT_RADIUS;
    rig_orbit(eye, axis, -yaw, 0.f);
    aim[0] = 0;
    aim[1] = LCAM_ORBIT_TARGET_Y;
    aim[2] = LCAM_MID_Z;
    *fov = LCAM_HOME_FOV;
}
/* Where each dock slot puts the camera, from the room's own framing (eye, target). */
static void lcam_slot(int slot, const float eye[3], const float target[3], const float car[3],
                      float out_eye[3], float out_target[3])
{
    const float centre[3] = {target[0], car[1] + .6f, target[2]}; /* the framed middle */
    memcpy(out_eye, eye, 3 * sizeof(float));
    memcpy(out_target, target, 3 * sizeof(float));
    if (slot == DOCK_MODS) { /* MODS: close in on the car */
        for (int k = 0; k < 3; k++) {
            out_target[k] = centre[k];
            out_eye[k] = centre[k] + (eye[k] - centre[k]) * .7f;
        }
        if (out_eye[1] < .35f)
            out_eye[1] = .35f;
    } else if (slot == DOCK_OPTIONS) { /* OPTIONS: look down on it */
        float d[3] = {eye[0] - centre[0], 0, eye[2] - centre[2]};
        float l = sqrtf(d[0] * d[0] + d[2] * d[2]);
        if (l < 1e-3f) {
            d[2] = 1;
            l = 1;
        }
        for (int k = 0; k < 3; k++)
            out_target[k] = centre[k];
        out_eye[0] = centre[0] + d[0] / l * 4.2f;
        out_eye[1] = centre[1] + 8.f;
        out_eye[2] = centre[2] + d[2] / l * 4.2f;
    } else if (slot == DOCK_EXIT) { /* EXIT: swing round to the car's tail, low, as it would leave */
        float d[3] = {eye[0] - centre[0], 0, eye[2] - centre[2]};
        float l = sqrtf(d[0] * d[0] + d[2] * d[2]), turn = 2.1f;
        if (l < 1e-3f) {
            d[2] = 1;
            l = 1;
        }
        float cs = cosf(turn), sn = sinf(turn);
        for (int k = 0; k < 3; k++)
            out_target[k] = centre[k];
        out_eye[0] = centre[0] + (d[0] * cs - d[2] * sn) / l * fmaxf(3.f, l * .75f);
        out_eye[1] = .45f;
        out_eye[2] = centre[2] + (d[0] * sn + d[2] * cs) / l * fmaxf(3.f, l * .75f);
    }
}

/* --studio <car>: the white studio room on its own - no menu, no game, no disc
   navigation. The captain's front door lands here once the ISO and the graphics
   backend are chosen; --shot FILE turns it into a picture so it can be judged.

   Interactive, it is his rig: the mouse orbits the camera, the wheel dollies, WASD pans
   the aim point, Q/E lift it, the arrows move the car on the floor and , . turn it. Every
   change is written to the rig file, so the position he finds is the position next time. */
static const char *studio_disc; /* the disc the front-end runs from (the launcher's audio) */
static launcher_preferences *studio_prefs; /* launcher.ini, for a disc dropped on the room */
static char studio_wheel[32], studio_wing[32]; /* the saved car's fitted wheel and wing */
/* The GT Mode save's riding car, its paint and its fitted wheel and wing (the earlier
   launcher's launcher_scene_saved_vehicle; func_00440938 serialized body +0x468, the
   career decoder picks the wing by the active setup sheet). 0 when the save rides none. */
static int studio_saved_car(const char *saves, gt4_vol *vol, char code[96], int *color)
{
    studio_wheel[0] = studio_wing[0] = 0;
    (void)saves;
    /* The game opened its save session already (gt4_save_init in main); read that one.
       Opening and shutting another here wiped the game's session: no save loaded, none made. */
    if (!gt4_save_active())
        return 0;
    ah_vm *vm = ah_vm_new();
    ah_obj *game = ah_module_new(vm, "game", ah_root(vm), NULL, NULL);
    gt4_save_restore(vm, game);
    ah_val garage, cars, index, label, paint;
    ah_obj *car = NULL;
    if (ah_get_member(vm, game, "garage", &garage) &&
        ah_get_member(vm, ah_as_obj(garage, AO_MODULE), "cars", &cars) &&
        ah_get_member(vm, ah_as_obj(garage, AO_MODULE), "riding_index", &index))
        car = ah_as_obj(ah_array_get(ah_as_obj(cars, AO_ARRAY), (int)ah_num(index)), AO_MODULE);
    int found = car && ah_get_member(vm, car, "car_code", &label) && ah_cstr(label) &&
                *ah_cstr(label) && strlen(ah_cstr(label)) < 96;
    {
        ah_val name, money;
        int cars_n = ah_get_member(vm, game, "garage", &garage) &&
                             ah_get_member(vm, ah_as_obj(garage, AO_MODULE), "cars", &cars) &&
                             ah_as_obj(cars, AO_ARRAY)
                         ? ah_array_size(ah_as_obj(cars, AO_ARRAY))
                         : 0;
        const char *who = ah_get_member(vm, game, "username", &name) && ah_cstr(name) &&
                                  *ah_cstr(name)
                              ? ah_cstr(name)
                              : NULL;
        long long cr = ah_get_member(vm, game, "money", &money) ? (long long)ah_num(money) : -1;
        if (who || cars_n) {
            char credits[32] = "-";
            if (cr >= 0) { /* 1,234,567 */
                char raw[24];
                int n = snprintf(raw, sizeof raw, "%lld", cr), o = 0;
                for (int i = 0; i < n && o < (int)sizeof credits - 2; i++) {
                    if (i && (n - i) % 3 == 0)
                        credits[o++] = ',';
                    credits[o++] = raw[i];
                }
                credits[o] = 0;
            }
            snprintf(studio_status.profile, sizeof studio_status.profile,
                     "GT Mode: %s   Cr. %s   %d car%s", who ? who : "Player", credits, cars_n,
                     cars_n == 1 ? "" : "s");
        }
    }
    if (found) {
        strcpy(code, ah_cstr(label));
        *color = ah_get_member(vm, car, "color_index", &paint) ? (int)ah_num(paint) : 0;
        uint8_t body[0x4b8];
        if (vol && gt4_game_car_body(vm, car, body))
            gt4_body_part_labels(vol, body, studio_wheel, studio_wing);
    }
    ah_vm_free(vm);
    return found;
}
#include "music_library_ui.inc"
#include "music_download.inc"
#include "launcher_scene.inc"
#include "launcher_guide.inc"

/* Scope the text boundary, including captions emitted by option previews. */
static void studio_options_paint(const studio_dock *d, int tab, int row, int theme, int mood,
                                 float alpha, int arrow)
{
    int ids[24];
    int n = studio_tab_rows(tab, ids);
    if (row < 0 || row >= n) return;
    studio_panel_ink_limit = (int)(255.f * fminf(1.f, fmaxf(0.f, alpha)));
    studio_options_draw(d, tab, row, theme, mood, alpha, arrow);
    if (studio_tab_guided(tab))
        guide_options_draw(d, tab, ids[row], theme, mood, alpha);
    studio_panel_ink_limit = -1;
}
/* Dev: exercise every real option's labels, facts and preview captions while fading. */
static void studio_options_ink_check(const studio_dock *d, int theme, int mood)
{
    int total = 0;
    studio_panel_ink_audit = 1;
    for (int tab = 0; tab < STUDIO_TABS; tab++) {
        int ids[24], n = studio_tab_rows(tab, ids);
        for (int row = 0; row < n; row++) {
            studio_options_paint(d, tab, row, theme, mood, .25f, 0);
            studio_options_paint(d, tab, row, theme, mood, .001f, 0);
            fprintf(stderr, "STUDIO_PANEL_INK %d %d %d\n", tab, row, ids[row]);
            total++;
        }
    }
    studio_panel_ink_audit = 0;
    fprintf(stderr, "STUDIO_PANEL_INK_TOTAL %d\n", total);
}

static int studio_run(showroom *cars, gt4_vol *vol, const char *code, const char *shot, int color,
                      int fade_in)
{
    app_music_ensure(0);
    launcher_touch_mouse(1);
    float eye[3], target[3], car[3], angle;
    int locked =
        1; /* the room opens with the camera held: he frames it once, then works on the rest */
    studio_dock dock = {0};
    int dock_sel = 0, dock_dirty = 1;
    int guide_intro = (!shot || getenv("MOONSTUDIO_GUIDE_SHOT")) && (!getenv("MOON_WINDOW_HIDDEN") || getenv("MOONSTUDIO_GUIDE_SHOT")) && app_video_settings && moon_options_int(&app_video_settings->store, "launcher.guided_intro", 1, 0, 1) && !moon_options_int(&app_video_settings->store, "launcher.guide_seen", 0, 0, 1);
    int guide_request = guide_intro || (getenv("MOONSTUDIO_GUIDE_SHOT") && getenv("MOON_WINDOW_HIDDEN"));
    int start_fade = 0, start_launch = 0, fade_prints = 0,
        start_shot_mid = 0; /* START: fade the whole launcher to black, then the cinematic */
    /* The studio fills the window: no 4:3 bars, and a resize changes the resolution
       (the framebuffer already tracks the drawable; this drops the letterbox). */
    video_set_fill_window(1);
    for (int i = 0; i < DOCK_COUNT; i++) {
        float crop[4];
        dock_icons[i].tex = dock_icon_load_crop(vol, dock_icons[i].pkg, dock_icons[i].asset,
                                                &dock_icons[i].w, &dock_icons[i].h, crop);
        dock_icons[i].u0 = crop[0];
        dock_icons[i].v0 = crop[1];
        dock_icons[i].u1 = crop[2];
        dock_icons[i].v1 = crop[3];
        printf("studio: dock %s icon %s%s\n", dock_names[i],
               dock_icons[i].tex ? "loaded from the disc" : "MISSING",
               dock_icons[i].tex ? "" : " - check the asset name");
    }
    dock_arrow.tex =
        dock_icon_load(vol, dock_arrow.pkg, dock_arrow.asset, &dock_arrow.w, &dock_arrow.h);
    if (!dock_band.tex)
        dock_band.tex =
            dock_icon_load(vol, dock_band.pkg, dock_band.asset, &dock_band.w, &dock_band.h);
    printf("studio: dock arrow %s %s\n", dock_arrow.asset,
           dock_arrow.tex ? "loaded from the disc" : "MISSING - check the asset name");
    if (getenv("MOONSTUDIO_DOCK_TRACE"))
        dock_cursor_trace(&dock);
    /* the pointer opens resting on the chosen slot; a selection change re-aims it */
    dock_cursor_target(&dock, dock_sel);
    dock.cx = dock.ctx;
    dock.cy = dock.cty;
    dock.ca = dock.cta;
    {
        int fw = 0, fh = 0;
        SDL_GL_GetDrawableSize(plat_sdl_window(), &fw, &fh);
        dock_metrics(&dock, fw > 0 ? fw : 640, fh > 0 ? fh : 448);
    }
    char rig_path[1200] = "", chosen[48] = "";
    snprintf(chosen, sizeof chosen, "%s", code);

    {
        char *pref = moon_pref_path();
        if (pref) {
            snprintf(rig_path, sizeof rig_path, "%sstudio-rig.txt", pref);
            SDL_free(pref);
        }
    }
    rig_defaults(eye, target, car, &angle);
    if (rig_path[0] &&
        rig_load(rig_path, eye, target, car, &angle, chosen, sizeof chosen, &locked) &&
        getenv("MOON_STUDIO_RIG_CAR"))
        fprintf(stderr, "studio rig: loaded %s\n", rig_path);
    if (fade_in)
        snprintf(chosen, sizeof chosen, "%s", code); /* the save's car, not the rig file's */
    showroom_set_wheel(cars, studio_wheel);
    showroom_set_wing(cars, studio_wing);
    showroom_set_rig(cars, eye, target, car, angle);

    uint64_t start = plat_time_ns();
    /* The scene must see the selected paint on its first request, not the
       default model while an asynchronous recolour waits for a later draw. */
    showroom_set_selection(cars, chosen, color);
    while (showroom_prepare(cars, chosen) == 0) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT) {
                launcher_touch_mouse(0);
                return 0;
            }
        if (plat_time_ns() - start > UINT64_C(60000000000)) {
            fprintf(stderr, "studio: assets did not arrive for %s\n", chosen);
            launcher_touch_mouse(0);
            return 1;
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
        app_presence_poll("Launcher", NULL, NULL, chosen, -1, -1);
        plat_sleep_ms(10);
    }
    if (!shot)
        printf("studio: the rig is yours. drag = orbit, wheel = dolly, WASD = pan,\n"
               "        Q/E = crane the camera up/down, R/F = swing around the car,\n"
               "        arrows = move the car, ',' '.' = turn it.\n"
               "        The camera is %s - press L to %s it. Every change is saved to %s\n",
               locked ? "LOCKED" : "free", locked ? "unlock" : "lock",
               rig_path[0] ? rig_path : "(no user folder)");
    const int frames = shot ? 120 : 0; /* the entrance and the light timeline settle first */
    int shot_hold = 0;
    int dragging = 0, dirty = 0;
    uint64_t saved_at = 0;
    /* the launcher's cameras: where the camera is now, idle time, the cinematic's clock */
    float cam_eye[3], cam_target[3], cam_fov = 0, idle = 0, cine_time = 0;
    float dock_eye[3], dock_target[3], dock_fov = LCAM_HOME_FOV;
    float orbit_yaw0 = 0, orbit_time = 0, orbit_mix = 0;
    float clock_alpha = 0;
    int cinematic = 0, window_full = 0, activated = 0, pad_mode = 0, exit_fade = 0, reopen = 0;
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    int pad_prev[5] = {0}; /* left, right, A, B, START of the last frame */
    int pad_primed = 0, slot_picked = 0, last_sel = dock_sel;
    /* OPTIONS: open or not, its row, the theme and the light colour (launcher.ini) */
    int options_arrow = 0; /* the value arrow under the mouse: -1, 1, or 0 */
    int options_open = 0, options_row = 0, options_tab = 0, options_ids[24],
        options_rows = studio_tab_rows(0, options_ids);
    float options_alpha = 0;
    int mods_open = 0, mods_row = 0, mods_arrow = 0; /* MODS: open or not, its row, the arrow */
    float mods_alpha = 0;
    if (getenv("MOONSTUDIO_MODS")) { /* dev: capture the MODS page (2: the first one switched) */
        studio_mods_scan();
        if (atoi(getenv("MOONSTUDIO_MODS")) == 2 && studio_mods.n) {
            studio_mods.on[0] ^= 1;
            studio_mods_save();
        }
        mods_open = 1;
        mods_alpha = 1;
        dock_sel = DOCK_MODS;
    }
    int theme = studio_prefs ? studio_prefs->theme : 0,
        mood = studio_prefs ? studio_prefs->mood % STUDIO_MOODS : 0;
    if (getenv("MOONSTUDIO_THEME")) /* dev: capture a theme, a light colour, the panel */
        theme = atoi(getenv("MOONSTUDIO_THEME")) != 0;
    if (getenv("MOONSTUDIO_MOOD"))
        mood = abs(atoi(getenv("MOONSTUDIO_MOOD"))) % STUDIO_MOODS;
    if (getenv("MOONSTUDIO_OPTIONS")) {
        dock_sel = DOCK_OPTIONS;
        options_open = 1;
        options_alpha = 1;
        options_tab = atoi(getenv("MOONSTUDIO_OPTIONS")) % STUDIO_TABS;
        options_rows = studio_tab_rows(options_tab, options_ids);
        if (getenv("MOONSTUDIO_OPTIONS_ROW"))
            options_row = abs(atoi(getenv("MOONSTUDIO_OPTIONS_ROW"))) % options_rows;
    }
    if(options_open && getenv("MOONSTUDIO_BINDING_PICKER")) {
        studio_game_options_load();
        int action=abs(atoi(getenv("MOONSTUDIO_BINDING_PICKER")))%11;
        studio_binding_begin(&studio_binding_picker,&studio_game_options.pad[studio_pad_port],action,studio_pad_port);
        if(getenv("MOONSTUDIO_BINDING_CHECK")) {
            int ok=studio_binding_input_check(&dock,atoi(getenv("MOONSTUDIO_BINDING_CHECK"))==2);
            printf("STUDIO_BINDING_INPUT %s\n",ok?"PASS":"FAIL");
            if(!ok)exit(3);
        }
    }
    studio_dark = theme;
    showroom_set_room_mood(cars, theme, studio_moods[mood].rgb);
    launcher_scene_pick(cars, launcher_scene_find(getenv("MOONSTUDIO_SCENE")   ? getenv("MOONSTUDIO_SCENE")
                                                  : studio_prefs ? studio_prefs->scene
                                                                 : ""));
#define STUDIO_OPTION_CHANGE(row_, delta_)                                                         \
    do {                                                                                           \
        int id_ = options_ids[(row_)];                                                             \
        if (id_ >= 0 || id_ == -4 || id_ == -5) {                                                                            \
            int was_group_ = id_ == video_setting_tweak_group();                                   \
            studio_row_change(id_, (delta_));                                                      \
            if (was_group_) {                                                                       \
                options_rows = studio_tab_rows(options_tab, options_ids);                           \
                if (options_row >= options_rows)                                                    \
                    options_row = options_rows > 0 ? options_rows - 1 : 0;                          \
            }                                                                                       \
        } else {                                                                                   \
            if (id_ == -1)                                                                         \
                theme = !theme;                                                                    \
            else if (id_ == -2)                                                                    \
                mood = (mood + (delta_) + STUDIO_MOODS) % STUDIO_MOODS;                            \
            else                                                                                   \
                launcher_scene_pick(cars, launcher_scene.index + (delta_));                        \
            studio_dark = theme;                                                                   \
            showroom_set_room_mood(cars, theme, studio_moods[mood].rgb);                           \
            if (studio_prefs) {                                                                    \
                studio_prefs->theme = theme;                                                       \
                studio_prefs->mood = mood;                                                         \
                snprintf(studio_prefs->scene, sizeof studio_prefs->scene, "%s",                    \
                         launcher_scenes[launcher_scene.index].code                                \
                             ? launcher_scenes[launcher_scene.index].code                          \
                             : "");                                                                \
                launcher_preferences_save(studio_prefs);                                           \
            }                                                                                      \
        }                                                                                          \
        STUDIO_SOUND("cursor");                                                                    \
    } while (0)
#define STUDIO_MOD_PICK(row_)                                                                      \
    do {                                                                                           \
        if ((row_) < studio_mods.n) {                                                              \
            studio_mods.on[(row_)] ^= 1;                                                           \
            studio_mods_save();                                                                    \
            STUDIO_SOUND("cursor");                                                                \
        } else {                                                                                   \
            studio_mods_open_folder();                                                             \
            STUDIO_SOUND("ok");                                                                    \
        }                                                                                          \
    } while (0)
#define STUDIO_OPTION_TAB(tab_)                                                                    \
    do {                                                                                           \
        options_tab = ((tab_) + STUDIO_TABS) % STUDIO_TABS;                                        \
        options_rows = studio_tab_rows(options_tab, options_ids);                                  \
        options_row = 0;                                                                           \
        STUDIO_SOUND("cursor");                                                                    \
    } while (0)
    int mouse_moved = 0, mouse_x0 = -1, mouse_y0 = -1;
    int mouse_seen = 0;
    SDL_ShowCursor(SDL_DISABLE); /* the game's pointer stands in for the system cursor */
    lcam_place(car, angle, LCAM_HOME_EYE, dock_eye);
    lcam_place(car, angle, LCAM_HOME_TARGET, dock_target);
    memcpy(cam_eye, dock_eye, sizeof cam_eye);
    memcpy(cam_target, dock_target, sizeof cam_target);
    cam_fov = dock_fov;
    const char *cine_at = getenv("MOONSTUDIO_CINE_AT"); /* dev: open on the idle orbit */
    if (cine_at) {
        cinematic = 1;
        cine_time = strtof(cine_at, NULL);
        orbit_time = cine_time;
        orbit_mix = 1;
        float dx = dock_eye[0] - car[0], dz = dock_eye[2] - car[2];
        float cs = cosf(angle), sn = sinf(angle);
        orbit_yaw0 = atan2f(cs * dx - sn * dz, sn * dx + cs * dz);
    }
    const char *slot_at = getenv("MOONSTUDIO_SLOT"); /* dev: capture a dock slot's framing */
#ifndef __SWITCH__
    /* the earlier launcher's music and menu sounds, on their own device, front-end only */
    const char *studio_skip_audio = getenv("MOON_STUDIO_SKIP");
    launcher_audio *sound = fade_in && studio_disc &&
                            !(studio_skip_audio && strstr(studio_skip_audio, "audio"))
                                ? launcher_audio_open(studio_disc, 1) : NULL;
    int sound_told = 0;
    launcher_mixer mixer;
    moon_options mixer_fallback = {0};
    moon_options *mixer_store = app_video_settings ? &app_video_settings->store : &mixer_fallback;
    launcher_mixer_load(&mixer, mixer_store);
    if (getenv("MOONSTUDIO_AUDIO")) mixer.open = 1;
    uint64_t mixer_changed = 0;
    int mixer_music = -1, mixer_effects = -1;

    /* the menu sounds sit over the music, as in the game's own menus */
    if (sound)
        launcher_audio_volume(sound, getenv("MOONSTUDIO_MUTE") ? 0 : launcher_mixer_music(&mixer),
                              getenv("MOONSTUDIO_MUTE") ? 0 : mixer.effects);
    fprintf(stderr, "studio: launcher audio %s (disc %s)\n", sound ? "opened" : "NOT opened",
            studio_disc ? studio_disc : "-");
#define STUDIO_SOUND(name)                                                                         \
    do {                                                                                           \
        if (sound)                                                                                 \
            launcher_audio_play(sound, name);                                                      \
    } while (0)
#else
#define STUDIO_SOUND(name) ((void)0)
#endif
    float lx = 0, ly = 0;
    int cur_rw = 0, pend_rw = -1;
    uint64_t pend_at = 0; /* raster width, rebuilt on a debounced resize */
    if (start_shot_prefix || start_launch_env)
        dock_sel = DOCK_START;
    for (int i = 0; frames == 0 || i < frames; i++) {
        app_music_tick();
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (music_download_active() && music_download_sdl_event(&ev))
                continue;
            if (ev.type == SDL_QUIT)
                goto done;
            if (ev.type == SDL_DROPFILE) {
                char found[LAUNCHER_PATH], msg[256];
                if (ev.drop.file && studio_prefs && start_fade == 0 && exit_fade == 0) {
                    if (wiz_dropped_disc(ev.drop.file, found, sizeof found) &&
                        launcher_validate_disc(found, msg, sizeof msg)) {
                        if (strcmp(found, studio_prefs->disc)) {
                            snprintf(studio_prefs->disc, sizeof studio_prefs->disc, "%s", found);
                            launcher_preferences_save(studio_prefs);
                            snprintf(studio_status.disc, sizeof studio_status.disc,
                                     "New disc saved - reopening");
                            studio_status.disc_ok = 1;
                            reopen = 1;
                            exit_fade = 1;
                        }
                    } else {
                        snprintf(studio_status.disc, sizeof studio_status.disc, "%s",
                                 wiz_dropped_disc(ev.drop.file, found, sizeof found)
                                     ? (launcher_validate_disc(found, msg, sizeof msg), msg)
                                     : "No Gran Turismo 4 disc in that folder.");
                        studio_status.disc_ok = 0;
                        STUDIO_SOUND("cancel");
                    }
                }
                SDL_free(ev.drop.file);
                continue;
            }
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_MOUSEMOTION ||
                ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEWHEEL ||
                ev.type == SDL_CONTROLLERBUTTONDOWN) {
                idle = 0;
                cinematic = 0; /* the camera eases back from wherever the shot left it */
            }
            if (start_fade > 0 || exit_fade > 0)
                continue; /* the dock is inert while the launcher fades out */
            if (ev.type == SDL_KEYDOWN)
                pad_mode = 1;
            else if (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN) {
                pad_mode = 0;
                mouse_seen = 1;
                dock_window_to_pixels(ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x,
                                      ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y,
                                      &dock.mx, &dock.my);
            }
#ifndef __SWITCH__
            if (!options_open && !mods_open) { /* the panels cover the audio strip */
                float mx = dock.mx, my = dock.my;
                if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP)
                    dock_window_to_pixels(ev.button.x, ev.button.y, &mx, &my);
                float scale = dock.scale > 0 ? dock.scale : DOCK_SCALE;
                if (launcher_mixer_event(&mixer, &ev, mx / scale, my / scale)) {
                    mixer_changed = plat_time_ns();
                    continue;
                }
            }
#endif
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_F7 && !ev.key.repeat) {
                guide_request = 1; continue;
            }
#ifndef __SWITCH__
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT &&
                !mixer.open && !options_open && !mods_open && /* the panels cover the link */
                dock.mx >= 16*dock.scale && dock.mx <= 220*dock.scale &&
                dock.my >= 90*dock.scale && dock.my <= 112*dock.scale) {
                guide_request = 1; continue;
            }
#endif
            if (options_open && studio_binding_picker.open && studio_binding_event(&dock,&ev))
                continue;
            if (options_open && ev.type == SDL_KEYDOWN) {
                int k = ev.key.keysym.sym;
                if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) {
                    options_open = 0;
                    STUDIO_SOUND("cancel");
                } else if (k == SDLK_UP) {
                    options_row = (options_row + options_rows - 1) % options_rows;
                    STUDIO_SOUND("cursor");
                } else if (k == SDLK_DOWN) {
                    options_row = (options_row + 1) % options_rows;
                    STUDIO_SOUND("cursor");
                } else if (k == SDLK_TAB || k == SDLK_e || k == SDLK_PAGEDOWN)
                    STUDIO_OPTION_TAB(options_tab + ((SDL_GetModState() & KMOD_SHIFT) ? -1 : 1));
                else if (k == SDLK_q || k == SDLK_PAGEUP)
                    STUDIO_OPTION_TAB(options_tab - 1);
                else if (k == SDLK_LEFT && !studio_row_button(options_ids[options_row]))
                    STUDIO_OPTION_CHANGE(options_row, -1);
                else if ((k == SDLK_RIGHT && !studio_row_button(options_ids[options_row])) ||
                         k == SDLK_RETURN || k == SDLK_KP_ENTER ||
                         (k == SDLK_SPACE && options_ids[options_row] == -5))
                    STUDIO_OPTION_CHANGE(options_row, 1);
                continue;
            }
            if (mods_open && ev.type == SDL_KEYDOWN) {
                int k = ev.key.keysym.sym, n = studio_mods_rows();
                if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) {
                    mods_open = 0;
                    STUDIO_SOUND("cancel");
                } else if (k == SDLK_UP) {
                    mods_row = (mods_row + n - 1) % n;
                    STUDIO_SOUND("cursor");
                } else if (k == SDLK_DOWN) {
                    mods_row = (mods_row + 1) % n;
                    STUDIO_SOUND("cursor");
                } else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_RETURN ||
                           k == SDLK_KP_ENTER || k == SDLK_SPACE)
                    STUDIO_MOD_PICK(mods_row);
                continue;
            }
            if (mods_open && (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN)) {
                float px, py;
                dock_window_to_pixels(ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x,
                                      ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y, &px,
                                      &py);
                int r = studio_mods_hit(&dock, mods_row, px, py);
                if (r >= 0 && r != mods_row && ev.type == SDL_MOUSEMOTION) {
                    mods_row = r;
                    STUDIO_SOUND("cursor");
                }
                if (r >= 0) {
                    studio_panel pp = studio_panel_at(&dock, 12);
                    const char *v = r == studio_mods.n  ? "Open"
                                    : studio_mods.on[r] ? "<  On  >"
                                                        : "<  Off  >";
                    mods_arrow = studio_value_side(&dock, &pp, v, px);
                } else
                    mods_arrow = 0;
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    if (r >= 0)
                        STUDIO_MOD_PICK(r);
                    else if (r == -1 && !dock_hover(&dock, &dock_sel, ev.button.x, ev.button.y)) {
                        mods_open = 0; /* a click on the room closes it */
                        STUDIO_SOUND("cancel");
                    }
                }
                if (ev.type == SDL_MOUSEMOTION || r != -1)
                    continue;
            }
            if (options_open && ev.type == SDL_MOUSEWHEEL && ev.wheel.y) { /* long tabs scroll */
                int to = options_row + (ev.wheel.y > 0 ? -1 : 1);
                if (to >= 0 && to < options_rows) {
                    options_row = to;
                    STUDIO_SOUND("cursor");
                }
                continue;
            }
            if (options_open && (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN ||
                                 ev.type == SDL_MOUSEBUTTONUP)) { /* the comparison's divider */
                float px, py;
                dock_window_to_pixels(ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x,
                                      ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y, &px,
                                      &py);
                if (guide_options_mouse(&ev, px, py))
                    continue;
            }
            if (options_open && (ev.type == SDL_MOUSEMOTION || ev.type == SDL_MOUSEBUTTONDOWN)) {
                float px, py;
                dock_window_to_pixels(ev.type == SDL_MOUSEMOTION ? ev.motion.x : ev.button.x,
                                      ev.type == SDL_MOUSEMOTION ? ev.motion.y : ev.button.y, &px,
                                      &py);
                int r = studio_options_hit(&dock, options_tab, px, py);
                if (ev.type == SDL_MOUSEMOTION) {
                    int hot = r <= -10 ? -10 - r : -1;
                    if (hot != studio_tab_hover && hot >= 0 && hot != options_tab)
                        STUDIO_SOUND("cursor");
                    studio_tab_hover = hot;
                }
                if (r >= 0 && r != options_row && ev.type == SDL_MOUSEMOTION) {
                    options_row = r;
                    STUDIO_SOUND("cursor");
                }
                {
                    char v[80];
                    studio_panel pp = studio_options_panel(&dock, options_tab);
                    snprintf(v, sizeof v, "<  %s  >",
                             r >= 0 ? studio_row_value(options_ids[r], theme, mood) : "");
                    options_arrow = r >= 0 && !studio_row_button(options_ids[r])
                                        ? studio_value_side(&dock, &pp, v, px)
                                        : 0;
                }
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    if (r <= -10)
                        STUDIO_OPTION_TAB(-10 - r);
                    else if (r >= 0) {
                        /* on the value: its left half (the "<") goes back, its right half
                           (the ">") forward; on the name, forward */
                        STUDIO_OPTION_CHANGE(r, options_arrow < 0 ? -1 : 1);
                    }
                    else if (r == -1 && !dock_hover(&dock, &dock_sel, ev.button.x, ev.button.y)) {
                        options_open = 0; /* a click on the room closes it */
                        STUDIO_SOUND("cancel");
                    }
                }
                if (ev.type == SDL_MOUSEMOTION)
                    continue;
            }
            if (ev.type == SDL_KEYDOWN && !ev.key.repeat &&
                (ev.key.keysym.sym == SDLK_F11 ||
                 ((ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) &&
                  (ev.key.keysym.mod & KMOD_ALT)))) {
                window_full = !(SDL_GetWindowFlags(plat_sdl_window()) & SDL_WINDOW_FULLSCREEN);
                SDL_SetWindowFullscreen(plat_sdl_window(),
                                        window_full ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                continue;
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_l) {
                    locked = !locked;
                    dirty = 1;
                    printf("studio: camera %s\n", locked ? "locked" : "free");
                } else if (locked && ev.key.keysym.sym == SDLK_ESCAPE) {
                    dock_sel = DOCK_EXIT; /* Esc is EXIT */
                    dock_cursor_target(&dock, dock_sel);
                    activated = 1;
                } else if (locked && ev.key.keysym.sym == SDLK_LEFT) {
                    STUDIO_SOUND("cursor");
                    dock_sel = (dock_sel + DOCK_COUNT - 1) % DOCK_COUNT;
                    dock_dirty = 1;
                    dock_cursor_target(&dock, dock_sel);
                    printf("studio: dock %s\n", dock_names[dock_sel]);
                } else if (locked && ev.key.keysym.sym == SDLK_RIGHT) {
                    STUDIO_SOUND("cursor");
                    dock_sel = (dock_sel + 1) % DOCK_COUNT;
                    dock_dirty = 1;
                    dock_cursor_target(&dock, dock_sel);
                    printf("studio: dock %s\n", dock_names[dock_sel]);
                } else if (locked && (ev.key.keysym.sym == SDLK_RETURN ||
                                      ev.key.keysym.sym == SDLK_KP_ENTER)) {
                    activated = 1;
                }
            } else if (ev.type == SDL_MOUSEMOTION &&
                       locked) { /* hover moves the pointer like the arrow keys */
                /* the window opening under a resting mouse reports its place: that is not a
                   hover; the hover starts once the hand actually moves */
                if (!mouse_moved) {
                    if (mouse_x0 < 0) {
                        mouse_x0 = ev.motion.x;
                        mouse_y0 = ev.motion.y;
                    } else if (abs(ev.motion.x - mouse_x0) + abs(ev.motion.y - mouse_y0) > 3)
                        mouse_moved = 1;
                }
                int was = dock_sel;
                if (mouse_moved)
                    dock_hover(&dock, &dock_sel, ev.motion.x, ev.motion.y);
                if (dock_sel != was)
                    STUDIO_SOUND("cursor");
            } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT &&
                       locked) {
                if (dock_hover(&dock, &dock_sel, ev.button.x, ev.button.y))
                    activated = 1; /* a click on a slot is Enter on it */
            } else if (locked)
                continue; /* a held camera ignores everything but L */
            else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                dragging = 1;
                lx = (float)ev.button.x;
                ly = (float)ev.button.y;
            } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT)
                dragging = 0;
            else if (ev.type == SDL_MOUSEMOTION && dragging) {
                float x = (float)ev.motion.x, y = (float)ev.motion.y;
                rig_orbit(eye, target, (lx - x) * .008f, (ly - y) * .006f);
                lx = x;
                ly = y;
                dirty = 1;
            } else if (ev.type == SDL_MOUSEWHEEL && ev.wheel.y) {
                float k = ev.wheel.y > 0 ? .92f : 1.087f;
                for (int j = 0; j < 3; j++)
                    eye[j] = target[j] + (eye[j] - target[j]) * k;
                float d[3] = {eye[0] - target[0], eye[1] - target[1], eye[2] - target[2]};
                if (sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) < 1.2f)
                    for (int j = 0; j < 3; j++)
                        eye[j] = target[j] +
                                 d[j] * 1.2f / sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                dirty = 1;
            }
        }
        {
            SDL_GameController *pad = locked && start_fade == 0 && exit_fade == 0 ? plat_sdl_pad(0) : NULL;
            if (pad) {
                int x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
                int now[5] = {
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -16000,
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > 16000,
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A),
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B),
                    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)};
                int pressed = 0;
                for (int b = 0; b < 5 && pad_primed; b++)
                    pressed |= (now[b] && !pad_prev[b]) << b;
                memcpy(pad_prev, now, sizeof now);
                pad_primed = 1; /* the first read only learns what is already held */
                if (pressed)
                    printf("studio: pad %s presses %x (left x %d)\n", SDL_GameControllerName(pad),
                           pressed, x);
                if (pressed) {
                    idle = 0;
                    cinematic = 0;
                    pad_mode = 1;
                }
                if (options_open) {
                    int up = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP),
                        down = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                    static int up_prev, down_prev;
                    int lb = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
                        rb = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                    static int lb_prev, rb_prev;
                    if(studio_binding_picker.open) {
                        if(up && !up_prev)studio_binding_move(&studio_binding_picker,-1);
                        if(down && !down_prev)studio_binding_move(&studio_binding_picker,1);
                        if(pressed&1)studio_binding_move(&studio_binding_picker,-11);
                        if(pressed&2)studio_binding_move(&studio_binding_picker,11);
                        if(pressed&4)studio_binding_accept();
                        if(pressed&8)studio_binding_picker.open=0;
                        pressed=0;
                    } else {
                    if (up && !up_prev)
                        options_row = (options_row + options_rows - 1) % options_rows;
                    if (down && !down_prev)
                        options_row = (options_row + 1) % options_rows;
                    if (lb && !lb_prev)
                        STUDIO_OPTION_TAB(options_tab - 1);
                    if (rb && !rb_prev)
                        STUDIO_OPTION_TAB(options_tab + 1);
                    int button = studio_row_button(options_ids[options_row]); /* A only */
                    if ((pressed & 1) && !button)
                        STUDIO_OPTION_CHANGE(options_row, -1);
                    if (pressed & (button ? 4 : 6))
                        STUDIO_OPTION_CHANGE(options_row, 1);
                    if (pressed & 8) {
                        options_open = 0;
                        STUDIO_SOUND("cancel");
                    }
                    pressed = 0;
                    }
                    up_prev=up;down_prev=down;lb_prev=lb;rb_prev=rb;
                }
                if (mods_open) {
                    int up = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP),
                        down = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN),
                        n = studio_mods_rows();
                    static int mup_prev, mdown_prev;
                    if (up && !mup_prev) {
                        mods_row = (mods_row + n - 1) % n;
                        STUDIO_SOUND("cursor");
                    }
                    if (down && !mdown_prev) {
                        mods_row = (mods_row + 1) % n;
                        STUDIO_SOUND("cursor");
                    }
                    mup_prev = up;
                    mdown_prev = down;
                    if (pressed & 7)
                        STUDIO_MOD_PICK(mods_row);
                    if (pressed & 8) {
                        mods_open = 0;
                        STUDIO_SOUND("cancel");
                    }
                    pressed = 0;
                }
                if (pressed & 3) {
                    STUDIO_SOUND("cursor");
                    dock_sel = (dock_sel + (pressed & 1 ? DOCK_COUNT - 1 : 1)) % DOCK_COUNT;
                    dock_dirty = 1;
                    dock_cursor_target(&dock, dock_sel);
                }
                if (pressed & 8) { /* B: to EXIT, as the game's cancel */
                    STUDIO_SOUND("cancel");
                    dock_sel = DOCK_EXIT;
                    dock_cursor_target(&dock, dock_sel);
                }
                if (pressed & 16) { /* START: START */
                    dock_sel = DOCK_START;
                    dock_cursor_target(&dock, dock_sel);
                    activated = 1;
                }
                if (pressed & 4)
                    activated = 1;
            }
        }
        if (activated) {
            activated = 0;
            if (dock_sel == DOCK_START) {
                STUDIO_SOUND("ok");
                start_fade = 1; /* START: fade the launcher to black, then the cinematic */
            } else if (dock_sel == DOCK_EXIT) {
                STUDIO_SOUND("cancel");
                exit_fade = 1; /* EXIT: the room fades to white, then the window closes */
            } else if (dock_sel == DOCK_OPTIONS) {
                STUDIO_SOUND("ok");
                options_open = !options_open;
                options_row = 0;
            } else if (dock_sel == DOCK_MODS) {
                STUDIO_SOUND("ok");
                mods_open = !mods_open;
                mods_row = 0;
                if (mods_open)
                    studio_mods_scan(); /* what is in the folder now */
            } else {
                STUDIO_SOUND("ok");
                printf("studio: %s pressed (nothing behind it yet)\n", dock_names[dock_sel]);
            }
        }
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        float dt = 1.f / 60.f, step = 2.2f * dt;
        if (getenv("MOONSTUDIO_ARROW_TEST") && (i == 20 || i == 40)) {
            /* dev: the mouse on the first row's value, its "<" (left of the value) */
            int ww = 0, wh = 0;
            SDL_GetWindowSize(plat_sdl_window(), &ww, &wh);
            studio_panel pp = studio_panel_at(&dock, options_rows);
            char v[80];
            snprintf(v, sizeof v, "<  %s  >", studio_row_value(options_ids[0], theme, mood));
            float w = dock_text_width(&dock.font, v) * DOCK_NAME_SCALE * .8f * dock.scale;
            float x = pp.x + pp.w - 14.f * dock.scale - w + 4.f * dock.scale + (i == 40 ? 2 : 0);
            SDL_WarpMouseInWindow(plat_sdl_window(), (int)(x * ww / dock.fw),
                                  (int)((pp.row0 - 5.f * dock.scale) * wh / dock.fh));
        }
        if (getenv("MOONSTUDIO_HOVER_TEST") && (i == 20 || i == 30 || i == 40 || i == 50)) {
            /* dev: the hand moves over the slots, as a player's would */
            int ww = 0, wh = 0;
            SDL_GetWindowSize(plat_sdl_window(), &ww, &wh);
            int slot = (i - 20) / 10;
            SDL_WarpMouseInWindow(plat_sdl_window(),
                                  (int)((dock.ox + dock_slot_cx(slot) * dock.scale) * ww /
                                        (dock.fw > 0 ? dock.fw : ww)),
                                  (int)((dock.oy + (DOCK_BAND_TOP + 10.f) * dock.scale) * wh /
                                        (dock.fh > 0 ? dock.fh : wh)));
        }
        if (slot_at && i == 0) {
            if (atoi(slot_at) >= 0) { /* -1: the room as it opens, no slot picked */
                dock_sel = atoi(slot_at) % DOCK_COUNT;
                dock_cursor_target(&dock, dock_sel);
                slot_picked = 1;
            }
            last_sel = dock_sel;
        }
        if (locked && (!shot || cine_at || slot_at)) {
            float goal_eye[3], goal_target[3], goal_fov = 0;
            idle += dt;
            if (!cinematic && idle >= LCAM_IDLE_SECONDS && start_fade == 0) {
                cinematic = 1;
                cine_time = 0;
                orbit_time = 0;
                /* begin where the dock's camera is pointing now: no snap into the orbit */
                float dx = cam_eye[0] - car[0], dz = cam_eye[2] - car[2];
                float cs = cosf(angle), sn = sinf(angle);
                orbit_yaw0 = atan2f(cs * dx - sn * dz, sn * dx + cs * dz);
                orbit_mix = 0;
            }
            /* ease the orbit in, and back out again when input clears `cinematic` */
            float orbit_goal = cinematic ? 1.f : 0.f;
            float orbit_step = dt / LCAM_ORBIT_EASE;
            if (orbit_mix < orbit_goal)
                orbit_mix = fminf(orbit_goal, orbit_mix + orbit_step);
            else if (orbit_mix > orbit_goal)
                orbit_mix = fmaxf(orbit_goal, orbit_mix - orbit_step);

            /* the dock's framing, eased: a critically damped follow toward the slot. The
               room's own framing is the front three-quarter (lens 32) on whatever car is
               parked, so any car and any window frame alike. */
            float home_eye[3], home_target[3];
            lcam_place(car, angle, LCAM_HOME_EYE, home_eye);
            lcam_place(car, angle, LCAM_HOME_TARGET, home_target);
            lcam_slot(slot_picked ? dock_sel : -1, home_eye, home_target, car, goal_eye,
                      goal_target);
            goal_fov = LCAM_HOME_FOV;
            if (slot_picked && dock_sel == DOCK_START) {
                lcam_place(car, angle, LCAM_PLAY_EYE, goal_eye);
                lcam_place(car, angle, LCAM_PLAY_TARGET, goal_target);
            }
            if (slot_picked && dock_sel == DOCK_EXIT) {
                /* EXIT: from the profile the camera only turns aside, so the car slides
                   out of frame to the right - no swing round the car */
                const float aside[3] = {LCAM_HOME_TARGET[0], LCAM_HOME_TARGET[1],
                                        LCAM_HOME_TARGET[2] - 2.6f};
                memcpy(goal_eye, home_eye, sizeof goal_eye);
                lcam_place(car, angle, aside, goal_target);
            }
            float k = 1 - expf(-dt * 3.5f);
            for (int j = 0; j < 3; j++) {
                dock_eye[j] += (goal_eye[j] - dock_eye[j]) * k;
                dock_target[j] += (goal_target[j] - dock_target[j]) * k;
            }
            dock_fov += (goal_fov - dock_fov) * k;

            if (orbit_mix > .0001f) {
                float e[3], a[3], lens;
                lcam_orbit(orbit_yaw0 + 6.28318530718f * orbit_time / LCAM_ORBIT_PERIOD, e, a,
                           &lens);
                float orbit_eye[3], orbit_aim[3];
                lcam_place(car, angle, e, orbit_eye);
                lcam_place(car, angle, a, orbit_aim);
                float m = orbit_mix * orbit_mix * (3.f - 2.f * orbit_mix);
                for (int j = 0; j < 3; j++) {
                    cam_eye[j] = dock_eye[j] + (orbit_eye[j] - dock_eye[j]) * m;
                    cam_target[j] = dock_target[j] + (orbit_aim[j] - dock_target[j]) * m;
                }
                cam_fov = dock_fov + (lens - dock_fov) * m;
                if (cinematic)
                    orbit_time += dt;
                cine_time += dt;
            } else {
                memcpy(cam_eye, dock_eye, sizeof cam_eye);
                memcpy(cam_target, dock_target, sizeof cam_target);
                cam_fov = dock_fov;
            }
            showroom_set_rig(cars, cam_eye, cam_target, car, angle);
            showroom_set_rig_fov(cars, cam_fov);
        }
        /* The clock lives only with the cinematic; a .4 s fade both ways, so it
           never pops in or out between states. */
        clock_alpha += (cinematic ? 1.f : -1.f) * (float)dt / .4f;
        if (clock_alpha < 0)
            clock_alpha = 0;
        if (clock_alpha > 1)
            clock_alpha = 1;
        if (!locked && (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_A] ||
                        keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_E] ||
                        keys[SDL_SCANCODE_R] || keys[SDL_SCANCODE_F])) {
            /* pan along the camera's own floor plane, so W always walks toward what is on screen */
            float f[3] = {target[0] - eye[0], 0, target[2] - eye[2]};
            float fl = sqrtf(f[0] * f[0] + f[2] * f[2]);
            if (fl < 1e-4f) {
                f[0] = 0;
                f[2] = -1;
                fl = 1;
            }
            f[0] /= fl;
            f[2] /= fl;
            float r[3] = {-f[2], 0, f[0]};
            float dx = 0, dz = 0, dy = 0;
            if (keys[SDL_SCANCODE_W]) {
                dx += f[0] * step;
                dz += f[2] * step;
            }
            if (keys[SDL_SCANCODE_S]) {
                dx -= f[0] * step;
                dz -= f[2] * step;
            }
            if (keys[SDL_SCANCODE_D]) {
                dx += r[0] * step;
                dz += r[2] * step;
            }
            if (keys[SDL_SCANCODE_A]) {
                dx -= r[0] * step;
                dz -= r[2] * step;
            }
            eye[0] += dx;
            eye[2] += dz;
            target[0] += dx;
            target[2] += dz;
            /* Two different "up", because they feel nothing alike:
               Q/E CRANE the camera - a pure up/down translation with the view direction kept,
               so it is the camera that rises and the car slides down the frame.
               R/F SWING it around the car instead (the view stays on the car). */
            if (keys[SDL_SCANCODE_E]) {
                eye[1] += step * 2.5f;
                target[1] += step * 2.5f;
            }
            if (keys[SDL_SCANCODE_Q]) {
                eye[1] -= step * 2.5f;
                target[1] -= step * 2.5f;
            }
            if (keys[SDL_SCANCODE_F])
                rig_orbit(eye, target, 0, -step * .55f);
            if (keys[SDL_SCANCODE_R])
                rig_orbit(eye, target, 0, step * .55f);
            if (eye[1] < .05f)
                eye[1] = .05f;
            dirty = 1;
        }
        if (!locked &&
            (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_UP] ||
             keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_COMMA] || keys[SDL_SCANCODE_PERIOD])) {
            if (keys[SDL_SCANCODE_LEFT])
                car[0] -= step;
            if (keys[SDL_SCANCODE_RIGHT])
                car[0] += step;
            if (keys[SDL_SCANCODE_UP])
                car[2] += step;
            if (keys[SDL_SCANCODE_DOWN])
                car[2] -= step;
            if (keys[SDL_SCANCODE_COMMA])
                angle -= .9f * dt;
            if (keys[SDL_SCANCODE_PERIOD])
                angle += .9f * dt;
            dirty = 1;
        }
        if (dirty) {
            showroom_set_rig(cars, eye, target, car, angle);
            uint64_t now = plat_time_ns();
            if (rig_path[0] && (saved_at == 0 || now - saved_at > UINT64_C(400000000))) {
                rig_save(rig_path, eye, target, car, angle, chosen, locked);
                saved_at = now;
            }
            dirty = 0;
        }
        int fw = 0, fh = 0;
        SDL_GL_GetDrawableSize(plat_sdl_window(), &fw, &fh);
        if (fw < 1)
            fw = 640;
        if (fh < 1)
            fh = 448;
        /* the raster width the window aspect asks for; rebuilt only after the drag settles */
        int want = (int)(448.f * (float)fw / (float)fh + .5f);
        if (want < 320)
            want = 320;
        if (want > 1024)
            want = 1024;
        uint64_t resize_now = plat_time_ns();
        if (want != pend_rw) {
            pend_rw = want;
            pend_at = resize_now;
        }
        if (want != cur_rw && (cur_rw == 0 || resize_now - pend_at > UINT64_C(150000000))) {
            showroom_set_width(cars, want);
            cur_rw = want;
            printf("studio: scene raster %dx448 for a %dx%d window\n", cur_rw, fw, fh);
        }
        dock_metrics(&dock, fw, fh);
        if ((start_shot_prefix || start_launch_env) && i >= 5 && start_fade == 0)
            start_fade = 1; /* dev: press START */
        if (exit_fade > 0 && ++exit_fade > FADE_FRAMES + 6)
            goto done; /* fully white: close */
        if (start_fade > 0) {
            start_fade++;
            if (start_fade > FADE_FRAMES)
                start_fade = FADE_FRAMES;
            if (fade_prints < 15) {
                fade_prints++;
                printf("studio: start fade %.3f\n", fade_curve((float)start_fade / FADE_FRAMES));
            }
        }
        if (options_open && studio_tab_guided(options_tab))
            guide_options_step(cars, chosen, cur_rw, options_ids[options_row], fw, fh);
        else if (!options_open && options_alpha < .01f) {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "guide")))
                guide_options_close(); /* the course is read again when the panel needs it */
        }
        if (options_open) /* the game's prints start the first time OPTIONS opens */
            launcher_prints_step(options_ids[options_row]);
        if (options_open && options_tab == 0)
            studio_renders_step(cars, chosen, cur_rw, options_ids[options_row], theme, mood, car,
                                angle, cam_eye, cam_target, cam_fov);
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "scene")))
                launcher_scene_step(cars, chosen);
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "scenetarget")))
                video_begin_scene();
        }
        /* the scene raster follows the window aspect (W = 448*window); the dock then draws in
           framebuffer pixels. Both map to the window at one uniform scale - nothing stretches. */
        plat_video_set_view((float)cur_rw, 448.f);
        if (!(options_open && guide_benchmark_enabled() &&
              guide_benchmark_scene(cars, chosen, cur_rw, 1.f / 60.f))) {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "showroom"))) {
                if (launcher_scene_ready()) /* the car in the chosen place; the room's car keeps loading */
                    launcher_scene_draw(locked ? cam_eye : eye, locked ? cam_target : target, car, angle,
                                        locked ? cam_fov : LCAM_RIG_FOV, cur_rw);
                else
                    showroom_draw(cars, chosen, 1.f / 60.f);
            }
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "scenetarget")))
                video_end_scene();
        }
        if (dock_dirty) {
            dock_font_load(&dock, vol);
            dock_dirty = 0;
        }
        dock_cursor_step(&dock, dt);
        plat_video_set_view((float)fw, (float)fh);
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "clock")) && clock_alpha > .004f)
                studio_clock_draw(&dock, clock_alpha, cine_time);
        }
        if (dock_sel != last_sel || activated)
            slot_picked = 1;
        if (dock_sel != DOCK_OPTIONS)
            options_open = 0;
        if (!options_open)
            studio_game_options_save(); /* sound and pads, once the panel closes */
        /* Dev: exercise closing a real panel before the headless capture. */
        if (shot && getenv("MOONSTUDIO_OPTIONS_CLOSE_AT") &&
            i >= atoi(getenv("MOONSTUDIO_OPTIONS_CLOSE_AT"))) {
            options_open = 0;
            dock_sel = DOCK_START;
        }
        options_alpha += ((options_open ? 1.f : 0.f) - options_alpha) * (1 - expf(-dt * 14.f));
        if (dock_sel != DOCK_MODS)
            mods_open = 0;
        mods_alpha += ((mods_open ? 1.f : 0.f) - mods_alpha) * (1 - expf(-dt * 14.f));
        last_sel = dock_sel;
#ifndef __SWITCH__
        int music_level = getenv("MOONSTUDIO_MUTE") ? 0 : launcher_mixer_music(&mixer);
        int effects_level = getenv("MOONSTUDIO_MUTE") ? 0 : mixer.effects;
        if (music_level != mixer_music || effects_level != mixer_effects) {
            launcher_audio_volume(sound, music_level, effects_level);
            mixer_music = music_level;
            mixer_effects = effects_level;
        }
        if (mixer.dirty && mixer.dragging < 0 && plat_time_ns() - mixer_changed > UINT64_C(250000000)) {
            if (!launcher_mixer_save(&mixer, mixer_store))
                fprintf(stderr, "studio: audio preferences could not be saved\n");
            mixer_changed = plat_time_ns();
        }
        if (sound && !sound_told && launcher_audio_ready(sound)) {
            sound_told = 1;
            fprintf(stderr, "studio: launcher music ready after %d frames\n", i);
        }
#endif
        dock_hover_step(&dock, dock_sel, dt);
        dock.mouse = !pad_mode;
        if (pad_mode) {
            options_arrow = 0; /* the pad and the keys show no mouse arrow */
            mods_arrow = 0;
            studio_tab_hover = -1;
        }
        if (!cinematic && !studio_skip_dock()) { /* the AFK cinematic shows only the room, the car and the clock */
            studio_info_draw(&dock);
#ifndef __SWITCH__
            if (options_open || mods_open)
                mixer.open = 0; /* SOUND in OPTIONS has the volumes */
            studio_mixer_draw(&dock, &mixer, 1.f - fmaxf(options_alpha, mods_alpha));
            if (!mixer.open && !options_open && !mods_open) {
                const uint8_t hint[4] = {STUDIO_LIGHT_INK ? 180 : 96,STUDIO_LIGHT_INK ? 187 : 99,STUDIO_LIGHT_INK ? 190 : 106,255};
                dock_text(&dock,"Graphics comparison / F7",16*dock.scale,105*dock.scale,.65f,0,hint);
            }
#endif
            if (shot && i == 1 && getenv("MOONSTUDIO_PANEL_INK_AUDIT"))
                studio_options_ink_check(&dock, theme, mood);
            studio_options_paint(&dock, options_tab, options_row, theme, mood, options_alpha,
                                 options_arrow);
            studio_mods_draw(&dock, mods_row, mods_alpha, mods_arrow);
            if(options_open && studio_binding_picker.open)studio_binding_draw(&dock);
            dock_caption_override = options_open ? studio_tab_guided(options_tab)
                                                       ? guide_options_help(options_ids[options_row])
                                                       : studio_row_help(options_ids[options_row])
                                    : mods_open  ? studio_mods_help(mods_row)
                                                 : NULL;
            dock_draw(&dock, dock_sel, dock.mouse ? mouse_seen : 1);
            music_download_draw();
        }
        /* one direction, eased: the wizard's white screen rises off the room ... */
        if (fade_in && i < FADE_FRAMES) {
            uint8_t veil[4] = {255, 255, 255,
                               (uint8_t)(255 * (1.f - fade_curve((float)i / FADE_FRAMES)))};
            plat_draw_quad(NULL, 0, 0, (float)fw, (float)fh, 0, 0, 1, 1, veil);
        }
        if (exit_fade > 0) {
            uint8_t veil[4] = {255, 255, 255,
                               (uint8_t)(255 * fade_curve((float)exit_fade / FADE_FRAMES))};
            plat_draw_quad(NULL, 0, 0, (float)fw, (float)fh, 0, 0, 1, 1, veil);
        }
        /* after the setup, its drawn bar sits over the disc's and fades: the graphite band
           becomes the dealers' metal, the words become the icons */
        if (fade_in && wiz_came && i < FADE_FRAMES * 2 && wiz_font_init()) {
            wiz_layout wl = {0};
            wiz_metrics(&wl, (float)fw, (float)fh);
            static const char *const words[4] = {"START", "OPTIONS", "MODS", "EXIT"};
            static const int on[4] = {1, 1, 1, 1};
            wiz_bar(&wl, words, on, -1, dock_captions[dock_sel],
                    1.f - fade_curve((float)i / (FADE_FRAMES * 2)));
        }
        /* ... and START falls the whole launcher to black, dock and cursor included */
        if (start_fade > 0) {
            uint8_t blk[4] = {0, 0, 0,
                              (uint8_t)(255 * fade_curve((float)start_fade / FADE_FRAMES))};
            plat_draw_quad(NULL, 0, 0, (float)fw, (float)fh, 0, 0, 1, 1, blk);
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "overlay")))
                moon_debug_overlay_draw(); /* G6.40: over the studio room too */
        }
        if (start_shot_prefix && start_fade >= FADE_FRAMES / 2 && !start_shot_mid) {
            char p[1200];
            snprintf(p, sizeof p, "%s-mid.png", start_shot_prefix);
            screenshot(p);
            printf("studio: start fade mid -> %s\n", p);
            start_shot_mid = 1;
        }
        if (start_shot_prefix && start_fade >= FADE_FRAMES) {
            char p[1200];
            snprintf(p, sizeof p, "%s-black.png", start_shot_prefix);
            screenshot(p);
            printf("studio: start fade black -> %s\n", p);
        }
        if (shot && i == frames - 2 &&
            (guide_options_busy() || launcher_scene_busy() ||
             (options_open && launcher_prints_busy(options_ids[options_row])) ||
             (options_open && options_tab == 0 && options_ids[options_row] >= -2 &&
              options_ids[options_row] <= -1 &&
              !studio_renders_ready(options_ids[options_row] == -1 ? 1 : theme,
                                    options_ids[options_row] == -1 ? mood : mood + 1))) &&
            ++shot_hold < 1500)
            i--; /* the shot waits for the comparison (the course loads from the disc) */
        if (shot && i == frames - 1) {
            screenshot(shot);
            printf("studio: %s painted '%s'\n", chosen, showroom_caption(cars));
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
        app_presence_poll(guide_options_busy() ? "Settings" : "Launcher", NULL, NULL,
                          guide_options_busy() ? NULL : chosen, -1, -1);

        if (guide_request && (i >= FADE_FRAMES*2 || (getenv("MOONSTUDIO_GUIDE_SHOT") && i>=60)) && !start_fade && !exit_fade) {
            guide_options_close(); /* one course at a time */
            int guide_ok=studio_guide(cars,vol,chosen,cur_rw,&dock,guide_intro);
            guide_request=0;
            if(guide_intro && !getenv("MOON_WINDOW_HIDDEN")) {
                int modified=app_video_settings->store.modified;
                moon_options_set(&app_video_settings->store,"launcher.guide_seen","1");
                app_video_settings->store.modified=modified; /* viewing help is not a preset edit */
                moon_options_save(&app_video_settings->store);
            }
            guide_intro=0; idle=0; cinematic=0;
            if(guide_ok==2) {
                dock_sel=DOCK_OPTIONS; options_open=1; options_tab=0; options_row=0;
                options_rows=studio_tab_rows(options_tab,options_ids);
            }
            if(!guide_ok) goto done;
        }
        if (start_fade >= FADE_FRAMES) {
            if (!start_shot_prefix)
                start_launch = 1;
            goto done;
        } /* fully black: the cinematic starts */
        app_window_geometry_tick(0);
        plat_sleep_ms(4);
    }
    goto done;
done:
    app_window_geometry_tick(1);
    guide_options_close();
    launcher_scene_close(cars); /* the game's showroom draws its own room */
    launcher_prints_close();
    studio_renders_close();
#ifndef __SWITCH__
    if (mixer.dirty && !launcher_mixer_save(&mixer, mixer_store))
        fprintf(stderr, "studio: audio preferences could not be saved\n");
    launcher_audio_close(sound);
#endif
#undef STUDIO_SOUND
#undef STUDIO_OPTION_CHANGE
#undef STUDIO_OPTION_TAB
#undef STUDIO_MOD_PICK
    showroom_set_rig_fov(cars, 0);
    SDL_ShowCursor(SDL_ENABLE); /* the game decides its own cursor from here */
#ifdef _WIN32
    if (reopen)
        wiz_restart(); /* the dropped disc opens in a fresh process */
#endif
    if (rig_path[0])
        rig_save(rig_path, eye, target, car, angle, chosen, locked);
    printf("studio: white room, %s\n", chosen);
    launcher_touch_mouse(0);
    return start_launch ? 2 : 0;
}

/* The SDK preview (--preview <car>): the SAME showroom renderer and the SAME
   studio scene -- the white room, the lights, the car -- and NOTHING else. No
   dock, no names, no icons, no menu, no keyboard. Mouse only: LEFT-DRAG orbits,
   WHEEL dollies, MIDDLE-DRAG pans (the studio's own camera math, not a second
   one). The camera starts from the rig file if it exists, else the studio's
   default framing; the requested car is always the one shown. One machine-
   readable line is printed once the first frame is drawn. */
static int preview_run(showroom *cars,const char *code,const char *shot,int color)
{
    float eye[3],target[3],car[3],angle;
    int locked=1;char rig_car[48]="";   /* the rig supplies the CAMERA only */
    char rig_path[1200]="",chosen[48]="";
    snprintf(chosen,sizeof chosen,"%s",code);
    video_set_fill_window(1);
    {
        char *pref=moon_pref_path();
        if(pref){snprintf(rig_path,sizeof rig_path,"%sstudio-rig.txt",pref);SDL_free(pref);}
    }
    rig_defaults(eye,target,car,&angle);
    /* P: MOON_PICTURE=<prefix> -- a mod's dealer picture: the car alone, front three-quarter
       like the disc's car_pict, once over black (<prefix>-black.png) and once over white
       (<prefix>-white.png), for moonmod_img --matte */
    const char *picture=getenv("MOON_PICTURE");
    if(picture&&*picture)
        {shot=NULL; /* the camera is aimed once the car stands (below) */
         car_model_picture_steer=getenv("MOON_PICTURE_STEER")?(float)atof(getenv("MOON_PICTURE_STEER")):-18.f;}
    else if(rig_path[0])rig_load(rig_path,eye,target,car,&angle,rig_car,sizeof rig_car,&locked);
    showroom_set_rig(cars,eye,target,car,angle);
    uint64_t start=plat_time_ns();
    while(showroom_prepare(cars,chosen)==0){
        SDL_Event ev;while(SDL_PollEvent(&ev))if(ev.type==SDL_QUIT)return 0;
        if(plat_time_ns()-start>UINT64_C(60000000000)){fprintf(stderr,"preview: assets did not arrive for %s\n",chosen);return 1;}
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }plat_video_end_frame();
        app_presence_poll("Launcher", NULL, NULL, chosen, -1, -1);
        plat_sleep_ms(10);
    }
    if(color>=0)showroom_set_color(cars,color);
    if(picture&&*picture){ /* front three-quarter (the car faces -Z), aimed at where it stands */
        float yaw=getenv("MOON_PICTURE_YAW")?(float)atof(getenv("MOON_PICTURE_YAW")):20.f,
              pitch=getenv("MOON_PICTURE_PITCH")?(float)atof(getenv("MOON_PICTURE_PITCH")):8.f,
              dist=8.1f,at[3];
        showroom_car_position(cars,at);
        target[0]=at[0];target[1]=.6f;target[2]=at[2];
        eye[0]=target[0]-dist*cosf(pitch*.0174533f)*sinf(yaw*.0174533f);
        eye[1]=target[1]+dist*sinf(pitch*.0174533f);
        eye[2]=target[2]-dist*cosf(pitch*.0174533f)*cosf(yaw*.0174533f);
        showroom_set_rig(cars,eye,target,car,angle);
    }
    int dragging=0,panning=0,dirty=0,ready=0;float lx=0,ly=0;
    int cur_rw=0,pend_rw=-1;uint64_t pend_at=0;
    const int frames=shot?60:picture&&*picture?80:0;   /* the entrance and the light timeline settle first */
    for(int i=0;frames==0||i<frames;i++){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT)goto done;
            if(ev.type==SDL_MOUSEBUTTONDOWN){
                if(ev.button.button==SDL_BUTTON_LEFT){dragging=1;lx=(float)ev.button.x;ly=(float)ev.button.y;}
                else if(ev.button.button==SDL_BUTTON_MIDDLE){panning=1;lx=(float)ev.button.x;ly=(float)ev.button.y;}
            } else if(ev.type==SDL_MOUSEBUTTONUP){
                if(ev.button.button==SDL_BUTTON_LEFT)dragging=0;
                else if(ev.button.button==SDL_BUTTON_MIDDLE)panning=0;
            } else if(ev.type==SDL_MOUSEMOTION){
                float x=(float)ev.motion.x,y=(float)ev.motion.y;
                if(dragging){rig_orbit(eye,target,(lx-x)*.008f,(ly-y)*.006f);lx=x;ly=y;dirty=1;}
                else if(panning){
                    /* pan along the camera's own floor plane, as the studio's WASD does */
                    float f[3]={target[0]-eye[0],0,target[2]-eye[2]},fl=sqrtf(f[0]*f[0]+f[2]*f[2]);
                    if(fl<1e-4f){f[0]=0;f[2]=-1;fl=1;}f[0]/=fl;f[2]/=fl;
                    float r[3]={-f[2],0,f[0]},dx=(x-lx)*.006f,dz=(y-ly)*.006f;
                    eye[0]-=f[0]*dz+r[0]*dx;eye[2]-=f[2]*dz+r[2]*dx;
                    target[0]-=f[0]*dz+r[0]*dx;target[2]-=f[2]*dz+r[2]*dx;
                    lx=x;ly=y;dirty=1;
                }
            } else if(ev.type==SDL_MOUSEWHEEL && ev.wheel.y){
                float k=ev.wheel.y>0?.92f:1.087f;
                for(int j=0;j<3;j++)eye[j]=target[j]+(eye[j]-target[j])*k;
                float d[3]={eye[0]-target[0],eye[1]-target[1],eye[2]-target[2]};
                float dl=sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
                if(dl<1.2f)for(int j=0;j<3;j++)eye[j]=target[j]+d[j]*1.2f/dl;
                dirty=1;
            }
        }
        if(dirty){showroom_set_rig(cars,eye,target,car,angle);dirty=0;}
        int fw=0,fh=0;SDL_GL_GetDrawableSize(plat_sdl_window(),&fw,&fh);if(fw<1)fw=640;if(fh<1)fh=448;
        int want=(int)(448.f*(float)fw/(float)fh+.5f);if(want<320)want=320;if(want>1024)want=1024;
        uint64_t now=plat_time_ns();
        if(want!=pend_rw){pend_rw=want;pend_at=now;}
        if(want!=cur_rw && (cur_rw==0 || now-pend_at>UINT64_C(150000000))){showroom_set_width(cars,want);cur_rw=want;}
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "scenetarget")))
                video_begin_scene();
        }
        plat_video_set_view((float)cur_rw,448.f);
        showroom_draw(cars,chosen,1.f/60.f);
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "scenetarget")))
                video_end_scene();
        }
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
        app_presence_poll("Launcher", NULL, NULL, chosen, -1, -1);
        if(!ready){ready=1;printf("preview: ready car=%s window=%dx%d\n",chosen,fw,fh);fflush(stdout);}
        if(shot && i==frames-1){screenshot(shot);printf("preview: %s painted '%s'\n",chosen,showroom_caption(cars));}
        if(picture&&*picture){
            static const uint8_t black[3]={0,0,0},white[3]={255,255,255};
            char p[1200];
            if(i==60)showroom_set_picture(cars,1,black);
            if(i==66){snprintf(p,sizeof p,"%s-black.png",picture);screenshot(p);showroom_set_picture(cars,1,white);}
            if(i==72){snprintf(p,sizeof p,"%s-white.png",picture);screenshot(p);printf("preview: %s picture -> %s-{black,white}.png\n",chosen,picture);}
        }
        plat_sleep_ms(4);
    }
done:
    printf("preview: closed %s\n",chosen);
    return 0;
}

/* The user's folder must exist before anything scans or writes it. On a virgin
   machine neither subfolder is there: `mods/` is only ever scanned
   (launcher_ui.c mods_scan) and `saves/` is only made on the first save
   (gt4_save_mkdir from save.c). Create the root and both subfolders here, at the
   one entry point both front doors (the studio and the disc panel) pass through.
   Idempotent (exists = fine) and non-fatal: a failure prints one line and the
   game carries on. gt4_save_mkdir already does the Windows/POSIX dance. */
static void ensure_user_folders(void)
{
    char pref[1200];
    if (!plat_pref_dir(pref, sizeof pref)) {
        fprintf(stderr, "player data: no user folder location (APPDATA/HOME unset?)\n");
        return;
    }
    static const char *const subs[] = {"mods", "saves"};
    for (int i = 0; i < (int)(sizeof subs / sizeof *subs); i++) {
        char path[1300];
        snprintf(path, sizeof path, "%s%s", pref, subs[i]);
        if (!gt4_save_mkdir(path))
            fprintf(stderr, "player data: could not create %s (carrying on)\n", path);
    }
}

/* One line, one shape, every platform: where every piece of user state lives,
   so a single screenshot of the log answers "where are my things?". The root
   is the same tree the desktop uses; only Android has a run log and its
   Download mirror, so those read "-" elsewhere. */
static void print_user_inventory(const char *saves)
{
    char pref[1200];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    const char *log = "-", *mirror = "-";
#if defined(__ANDROID__)
    if (moon_android_log_path())
        log = moon_android_log_path();
    if (moon_android_log_mirror())
        mirror = moon_android_log_mirror();
#endif
    fprintf(stderr,
            "moonengine: user root=%s | launcher.ini=%slauncher.ini | video.ini=%svideo.ini | "
            "options.ini=%soptions.ini | saves=%s | studio-rig.txt=%sstudio-rig.txt | "
            "mods.ini=%smods.ini | mods=%smods | log=%s | mirror=%s\n",
            pref, pref, pref, pref, saves && *saves ? saves : "-", pref, pref, pref, log, mirror);
}

/* Bench wall/process time without requiring clock_gettime on MinGW. */
static double bench_wall_seconds(void)
{
    static uint64_t freq;
    if (!freq)
        freq = SDL_GetPerformanceFrequency();
    return freq ? (double)SDL_GetPerformanceCounter() / (double)freq : 0;
}

static double bench_cpu_seconds(void)
{
#ifdef _WIN32
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
        return 0;
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;
    return (double)(k.QuadPart + u.QuadPart) * 1e-7;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0)
        return 0;
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

int main(int argc, char **argv)
{
#ifdef __ANDROID__
    moon_android_init();
    /* No arguments is the launcher's own first-run wizard / studio room, exactly as on
       desktop. GameActivity never passes a disc: native reads the remembered document
       from the startup preference to pre-fill the room's disc step. */
#endif
    ensure_user_folders();
#ifndef __SWITCH__
    {
        int diagnostic = moon_renderer_diagnostic(argc, argv);
        if (diagnostic >= 0)
            return diagnostic;
    }
#endif
    adopt_saves_from_old_names();
    launcher_startup startup = {0};
    int prepared = launcher_bootstrap(&argc, &argv, &startup);
    if (prepared <= 0)
        return prepared < 0 ? 1 : 0;
    int front_end = (prepared == 2); /* no-argument launch: the studio front-end */
    fprintf(stderr, "launcher: %s (arguments %d)\n",
            front_end ? "studio front-end" : "game", argc); /* the Android no-arg proof */
    {
        char args[512];
        size_t used = 0;
        args[0] = 0;
        for (int i = 1; i < argc && used + 2 < sizeof args; i++) {
            int n = snprintf(args + used, sizeof args - used, "%s%s", i > 1 ? " " : "", argv[i]);
            if (n < 0 || (size_t)n >= sizeof args - used)
                break;
            used += (size_t)n;
        }
        app_startup_log("launcher: prepared=%d front_end=%d argc=%d args=%s", prepared, front_end,
                        argc, args);
    }
    const char *front_disc = startup.preferences.disc;
    const char *front_shot = getenv("MOONSTUDIO_FRONT_SHOT"); /* dev: capture the room headless */
    wiz_shot_prefix = getenv("MOONSTUDIO_SETUP_SHOT");        /* dev: capture the wizard headless */
    start_shot_prefix = getenv("MOONSTUDIO_START_SHOT"); /* dev: capture the START fade headless */
    start_launch_env =
        getenv("MOONSTUDIO_START_LAUNCH") != NULL; /* dev: auto-START and boot the game */
    if (argc == 4 && !strcmp(argv[2], "--launcher-gallery"))
        return launcher_capture_gallery(argv[1], argv[3]);
    const char *err = NULL;
    const char *shot = NULL;
    const char *studio_car = NULL; /* --studio <car>: the white studio room instead of the menu */
    const char *preview_car = NULL; /* --preview <car>: the studio scene with no UI, mouse only */
    double shot_delay = 0, input_interval = 0;
    const char *quick_input =
        NULL; /* --quick-input: inputs for the quick menu (tests), 1.5 s after it opens */
    int exit_code = 0;
    double music_tempo = 1.0; /* authored sequence timing, with source-rate conversion */
    int music_with_shot = 0;
    int color_preview = 0, color_applied = 0;
    int preview_courses = 0;
    int intro_override = -1, first_run_override = 0;
    int persistent_video = 1, settings_preview = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--shot") || !strcmp(argv[i], "--no-config"))
            persistent_video = 0;
    const char *config_file = NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--config-file") && i + 1 < argc) {
            config_file = argv[++i];
            persistent_video = 1;
        }
    const char *settings_config = config_file;
#ifndef __SWITCH__
    /* P: the secondary launcher still passes legacy video.ini. The game's
       canonical store must survive its next legacy preference rewrite.
       Explicit/custom fixture paths retain their isolated override. */
    settings_config = moon_options_config_path(config_file, startup.video_file);
#endif
    video_settings settings;
    video_settings_init(&settings, persistent_video, settings_config);
    custom_modes_options = &settings.store;
    menu_set_controller_tiles(settings.current.controller_tiles);
#ifndef __SWITCH__
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--renderer")) {
            int backend = i + 1 < argc ? moon_renderer_parse(argv[++i]) : -1;
            if (backend < 0) {
                fprintf(stderr, "--renderer expects opengl or vulkan\n");
                return 1;
            }
            settings.current.renderer = backend;
        }
    }
    const char *backend_error = moon_renderer_unavailable(settings.current.renderer);
    if (backend_error) {
        fprintf(stderr, "%s\n", backend_error);
        return 1;
    }
#endif
    char game_options_path[1100] = "";
    if (settings.persist && settings.path[0])
        snprintf(game_options_path, sizeof game_options_path, "%s.game_options", settings.path);
#ifndef __SWITCH__
    if (!settings_config && settings.persist) {
        char *dir = moon_pref_path();
        if (dir) {
            snprintf(game_options_path, sizeof game_options_path, "%svideo.ini.game_options", dir);
            SDL_free(dir);
        }
    }
#endif
    menu_options_storage(game_options_path[0] ? game_options_path : NULL);
    char save_directory[1100] = "";
    const char *player_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--save-dir") && i + 1 < argc)
            snprintf(save_directory, sizeof save_directory, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--player-name") && i + 1 < argc)
            player_name = argv[++i];
    }
    if (!save_directory[0] && persistent_video) {
        char *pref = moon_pref_path();
        if (pref) {
            snprintf(save_directory, sizeof save_directory, "%ssaves", pref);
            SDL_free(pref);
        }
    }
    print_user_inventory(save_directory);
    gt4_save_init_pending(save_directory[0] ? save_directory : NULL);
#ifdef __SWITCH__
    fprintf(stderr, "Switch save: directory=%s active=%d error=%s\n", gt4_save_directory(),
            gt4_save_active(), gt4_save_error());
    for (int slot = 1; slot <= GT4_SAVE_SLOTS; slot++) {
        gt4_save_info info;
        int status = gt4_save_info_get(slot, &info);
        fprintf(stderr, "Switch save slot %d: status=%d\n", slot, status);
    }
#endif
    app_video_settings = &settings;
    if (getenv("MOON_SETTINGS_PARITY") && !studio_tweaks_parity()) {
        fprintf(stderr, "settings parity: the two Tweaks pages differ\n");
        return 3;
    }
    int discord_enabled =
        moon_options_int(&settings.store, "integrations.discord_presence", 0, 0, 1);
    /* The captain's own Discord application (Developer Portal). A client id is public —
       it is not a secret and it is what lets presence work out of the box. The toggle
       above still ships OFF (LAW 12): nothing talks to Discord until it is switched on. */
    discord_presence_set_log(app_presence_log);
    discord_presence_configure(
        discord_enabled,
        moon_options_get(&settings.store, "integrations.discord_app_id",
                         "1562846084085194752"));
    char music_folder[1100];
    if (app_music_folder(settings.store.path, music_folder, sizeof music_folder))
        fprintf(stderr, "music: folder=%s\n", music_folder);
    int window_w = settings.current.video.width, window_h = settings.current.video.height;
    int window_full = settings.current.video.fullscreen, upscale = settings.current.video.upscale,
        video_vsync = settings.current.video.vsync;
    int race_preview = 0, race_active = 0, race_loading = 0, race_drive = 0, loading_preview = 0,
        bench_frames = 0, bench_started = 0, bench_count = 0, bench_frame_index = 0;
    double bench_throughput_seconds = 0;
    double bench_throughput_wall = 0, bench_throughput_cpu = 0;
    uint64_t bench_throughput_frames = 0;
    uint64_t bench_throughput_gpu = 0, bench_throughput_submits = 0;
    double *bench_times = NULL;
    double bench_gpu_first = 0, bench_gpu_last = 0;
    int race_return = 0;
    int pre_race_preview = 0, quick_options = 0, quick_options_project = -1, race_time_trial = 0;
    int logger_track = -1;     /* the Logger's Load Track waiting for labomode's best lap */
    int logger_load_asked = 0; /* its Load Ghost not yet sent: the quick menu must be on screen */
    int split_preview = 0;
    const char *split_car_option = NULL;
    const char *race_course_option = NULL, *race_car_option = NULL;
#ifdef MOON_SWITCH_RACE_DIAGNOSTICS
    const char *race_grid[5] = {0};
#endif
    char race_car[96] = {0}, race_course[96] = {0};
    const char *input_sequence = NULL;
    int selection_steps = 0, positional = 1;
    for (int i = 1; i < argc; i++) {
#ifndef __SWITCH__
        if (!strcmp(argv[i], "--renderer")) {
            i++;
            continue;
        }
#endif
        if (!strcmp(argv[i], "--loading-shot") && i + 1 < argc) {
            loading_shot = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "--loading-logo") && i + 1 < argc) {
            int logo = atoi(argv[++i]);
            if (logo < 0 || logo > 1)
                return 2;
            loading_screen_preview_variant(logo);
            continue;
        }
        if (!strcmp(argv[i], "--loading-preview")) {
            loading_preview = 1;
            continue;
        }
        if (!strcmp(argv[i], "--save-dir") || !strcmp(argv[i], "--player-name")) {
            if (++i == argc)
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--config-file")) {
            if (++i == argc || strlen(argv[i]) >= sizeof settings.path)
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--no-config"))
            continue;
        if (!strcmp(argv[i], "--video-settings")) {
            settings_preview = 1;
            continue;
        }
        if (!strcmp(argv[i], "--ui-filter") && i + 1 < argc) {
            const char *mode = argv[++i];
            if (!strcmp(mode, "smooth"))
                settings.current.filter = 0;
            else if (!strcmp(mode, "sharp"))
                settings.current.filter = 1;
            else if (!strcmp(mode, "nearest"))
                settings.current.filter = 2;
            else
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--text-sharpness") && i + 1 < argc) {
            char *end;
            long n = strtol(argv[++i], &end, 10);
            if (!*argv[i] || *end || n < 0 || n > 100)
                return 2;
            settings.current.text = (int)n;
            continue;
        }
        if (!strcmp(argv[i], "--vsync") && i + 1 < argc) {
            const char *mode = argv[++i];
            if (!strcmp(mode, "on"))
                video_vsync = 1;
            else if (!strcmp(mode, "off"))
                video_vsync = 0;
            else
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--first-run")) {
            first_run_override = 1;
            continue;
        }
        if (!strcmp(argv[i], "--intro")) {
            intro_override = 1;
            continue;
        }
        if (!strcmp(argv[i], "--skip-intro")) {
            intro_override = 0;
            continue;
        }
#ifdef MOON_SWITCH_RACE_DIAGNOSTICS
        /* Internal hardware fixture: reproduce the five opponents from a crash log. */
        if (!strcmp(argv[i], "--race-grid")) {
            for (unsigned j = 0; j < 5; j++) {
                if (++i == argc || !*argv[i] || strlen(argv[i]) > 80)
                    return 2;
                race_grid[j] = argv[i];
            }
            continue;
        }
#endif
        if (!strcmp(argv[i], "--race-preview")) {
            race_preview = 1;
            continue;
        }
        if (!strcmp(argv[i], "--split-screen")) {
            split_preview = race_preview = race_drive = 1;
            pre_race_preview = 0;
            continue;
        }
        if (!strcmp(argv[i], "--race-car-2p")) {
            if (++i == argc || !*argv[i] || strlen(argv[i]) > 80)
                return 2;
            split_car_option = argv[i];
            continue;
        }
        if (!strcmp(argv[i], "--drive")) {
            race_preview = race_drive = 1;
            continue;
        } /* the race itself, not the grid preview */
        if (!strcmp(argv[i], "--bench-frames") && i + 1 < argc) {
            char *end;
            long n = strtol(argv[++i], &end, 10);
            if (!*argv[i] || *end || n < 0 || n > 100000)
                return 2;
            bench_frames = (int)n;
            continue;
        }
        if (!strcmp(argv[i], "--bench-throughput") && i + 1 < argc) {
            char *end;
            double sec = strtod(argv[++i], &end);
            if (!*argv[i] || *end || sec <= 0 || sec > 600)
                return 2;
            bench_throughput_seconds = sec;
            continue;
        }
        if (!strcmp(argv[i], "--time-trial")) {
            race_time_trial = race_preview = race_drive = 1;
            pre_race_preview = !getenv("MOON_RACE_SNAPSHOT");
            continue;
        } /* a capture: straight to its moment */
        if (!strcmp(argv[i], "--pre-race")) {
            pre_race_preview = race_preview = race_drive = 1;
            continue;
        }
        if (!strcmp(argv[i], "--race-course") || !strcmp(argv[i], "--race-car")) {
            int is_course = !strcmp(argv[i], "--race-course");
            if (++i == argc || !*argv[i] || strlen(argv[i]) > 80) {
                fprintf(stderr, "race option needs a label (up to 80 characters)\n");
                return 2;
            }
            if (is_course)
                race_course_option = argv[i];
            else
                race_car_option = argv[i];
            race_preview = 1;
            continue;
        }
        if (!strcmp(argv[i], "--preview-courses")) {
            preview_courses = 1;
            continue;
        }
        if (!strcmp(argv[i], "--fullscreen")) {
            window_full = 1;
            continue;
        }
        if (!strcmp(argv[i], "--upscale") && i + 1 < argc) {
            i++;
            if (!strcmp(argv[i], "off"))
                upscale = 0;
            else if (!strcmp(argv[i], "ultra-quality"))
                upscale = 1;
            else if (!strcmp(argv[i], "fsr") || !strcmp(argv[i], "quality"))
                upscale = 2;
            else if (!strcmp(argv[i], "balanced"))
                upscale = 3;
            else if (!strcmp(argv[i], "performance"))
                upscale = 4;
            else
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--window") && i + 1 < argc) {
            /* window size in pixels: the GPU draws at this resolution */
            if (sscanf(argv[++i], "%dx%d", &window_w, &window_h) != 2 || window_w < 320 ||
                window_h < 240 || window_w > 7680 || window_h > 4320)
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--dialog-shot") && i + 1 < argc) {
            dialog_shot = argv[++i];
            continue;
        }
        if (!strcmp(argv[i], "--dialog-shot-scripted") && i + 1 < argc) {
            dialog_shot = argv[++i];
            dialog_shot_scripted = 1;
            continue;
        } /* taken at the input "shot" */
        if (!strcmp(argv[i], "--with-music")) {
            music_with_shot = 1;
            continue;
        }
        if (!strcmp(argv[i], "--studio") && i + 1 < argc) {
            studio_car = argv[++i];
            continue;
        } /* the white studio room, no menu */
        if (!strcmp(argv[i], "--preview") && i + 1 < argc) {
            preview_car = argv[++i];
            continue;
        } /* the studio scene, no UI, mouse only */
        if (!strcmp(argv[i], "--preview-size") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &window_w, &window_h) != 2 || window_w < 320 ||
                window_h < 240 || window_w > 7680 || window_h > 4320)
                return 2;
            continue;
        }
        if (!strcmp(argv[i], "--quick-input") || !strcmp(argv[i], "--input-interval") ||
            !strcmp(argv[i], "--color") || !strcmp(argv[i], "--shot-delay") ||
            !strcmp(argv[i], "--music-tempo") || !strcmp(argv[i], "--shot") ||
            !strcmp(argv[i], "--select") || !strcmp(argv[i], "--input") ||
            !strcmp(argv[i], "--dialog-answer")) {
            const char *option = argv[i];
            if (++i == argc) {
                fprintf(stderr, "%s needs a value\n", option);
                return 2;
            }
            if (!strcmp(option, "--quick-input")) {
                quick_input = argv[i];
                continue;
            }
            if (!strcmp(option, "--input-interval")) {
                char *end;
                input_interval = strtod(argv[i], &end);
                if (!*argv[i] || *end || !(input_interval >= 0 && input_interval <= 2))
                    return 2;
            } else if (!strcmp(option, "--color")) {
                char *end;
                long n = strtol(argv[i], &end, 10);
                if (!*argv[i] || *end || n < 0 || n > 127)
                    return 2;
                color_preview = (int)n;
            } else if (!strcmp(option, "--shot-delay")) {
                char *end;
                shot_delay = strtod(argv[i], &end);
                if (!*argv[i] || *end || !(shot_delay >= 0 && shot_delay <= 300))
                    return 2;
            } else if (!strcmp(option, "--music-tempo")) {
                char *end;
                music_tempo = strtod(argv[i], &end);
                if (!*argv[i] || *end || !(music_tempo >= 0.5 && music_tempo <= 1.5)) {
                    fprintf(stderr, "--music-tempo expects 0.5..1.5\n");
                    return 2;
                }
            } else if (!strcmp(option, "--shot"))
                shot = argv[i];
            else if (!strcmp(option, "--input"))
                input_sequence = argv[i];
            else if (!strcmp(option, "--dialog-answer")) {
                if (strcmp(argv[i], "yes") && strcmp(argv[i], "no")) {
                    fprintf(stderr, "--dialog-answer needs yes or no\n");
                    return 2;
                }
                plat_sdl_dialog_answer(!strcmp(argv[i], "yes"));
            } else {
                char *end;
                long value = strtol(argv[i], &end, 10);
                if (!*argv[i] || *end || value < 0 || value > 1000) {
                    fprintf(stderr, "--select expects 0..1000 preview steps\n");
                    return 2;
                }
                selection_steps = (int)value;
            }
        } else
            argv[positional++] = argv[i];
    }
    argc = positional;
    if (argc < 2) {
        fprintf(stderr, "usage: %s <disc.iso|GT4.VOL> [project] [screen]\n", argv[0]);
        return 2;
    }
    app_presence_enabled =
        discord_enabled && !shot && !front_shot && !wiz_shot_prefix && !start_shot_prefix;

    int startup_intro =
        !race_preview &&
        (intro_override == 1 || (intro_override < 0 && (argc == 2 || startup.preferences.file[0]) &&
                                 !shot && !input_sequence && !selection_steps && !preview_courses));
    int proj = startup_intro ? 2 : 0, root = 0;
    if (argc > 2)
        for (int k = 0; k < NPROJECTS; k++)
            if (!strcmp(argv[2], projects[k]))
                proj = k;
    int show_opening =
        intro_override == 1 || menu_option_number(NULL, "opening_after_autoload", 1) != 0;
    if (video_settings_skip_intro())
        show_opening = 0; /* Tweaks: skip the GT splash and the opening movie */
    if (startup_intro)
        proj = show_opening ? 2 : 0;
    int first_run = first_run_override || (startup_intro && !gt4_save_active() && !gt4_save_pending_slot());
    /* No save yet: the profile/import screen comes first, from the launcher too (it was
       skipped after START, so a first run never offered to make a save). */
    if (first_run) {
        proj = 7;
        startup_intro = 0;
    }
    /* Tweaks: skip the boot chain entirely. show_opening above already moved the
       start to arcade, but startup_intro stayed on, which still drew the boot
       notices and then let the boot project (LoadRoot, MovieRoot, CheckRoot)
       run. Clear it before any boot screen is opened. */
    if (video_settings_skip_intro() && startup_intro) {
        startup_intro = 0;
        show_opening = 0;
        proj = 0;
        root = 0;
    }
    if (settings_preview) {
        proj = 3;
        startup_intro = 0;
        first_run = 0;
    }
    app_startup_log("startup: skip_intro=%d intro_override=%d startup_intro=%d show_opening=%d "
                    "proj=%s argc=%d", video_settings_skip_intro(), intro_override, startup_intro,
                    show_opening, proj >= 0 && proj < NPROJECTS ? projects[proj] : "?", argc);

    plat_video_desc desc = {window_w, window_h, window_full, video_vsync, upscale};
#ifndef __SWITCH__
    desc.backend = settings.current.renderer;
#endif
    settings.current.video = desc;
#ifndef __SWITCH__
    if (front_end && !studio_car) {
        /* P: the launcher is a window of its own size; START hands the game its settings. */
        desc.width = 960;
        desc.height = 720; /* 4:3 */
        desc.fullscreen = 0;
        if (getenv("MOONSTUDIO_WINDOW")) /* dev: capture another window size, WxH */
            sscanf(getenv("MOONSTUDIO_WINDOW"), "%dx%d", &desc.width, &desc.height);
    }
    if (front_end && !studio_car)
        app_window_geometry_restore(&desc);
#endif
    if (plat_video_init(&desc) != 0) {
        fprintf(stderr, "cannot open a window: %s\n", SDL_GetError());
        return 1;
    }
    moon_debug_overlay_init(); /* F4 / volume chord; off until toggled */
#ifdef __SWITCH__
    video_switch_scene_resolution(settings.current.scene_resolution);
#endif
    video_set_aspect(settings.current.aspect);
    race_hud_set_widescreen(settings.current.aspect); /* D_006184F0 */
    plat_video_set_view(640, 480);
    video_performance_options(settings.current.low_latency, settings.current.fps_limit);
#ifndef __SWITCH__
    r3d_scene_options(settings.current.spatial_visibility, settings.current.resident_reflections);
#endif
    video_ui_quality(settings.current.filter, settings.current.text / 100.f);
#ifdef __ANDROID__
    video_set_scene_render_scale(settings.current.mobile.render_scale);
#endif
    /* Loaded values must reach the renderer even when the launcher's START
       handover is skipped (--drive / config-file runs). */
    video_set_speed_post(settings.current.speed_blur, settings.current.speed_blur_amount,
                         settings.current.speed_vignette);
    fprintf(stderr, "settings: applied at startup renderer=%s speed_post=%d:%d:%d\n",
            moon_renderer_name(settings.current.renderer), settings.current.speed_blur,
            settings.current.speed_vignette, settings.current.speed_blur_amount);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    video_scene_quality(settings.current.antialiasing, settings.current.render_scale);
    video_set_scene_scale(settings.current.scene_scale);
    r3d_texture_smoothing(settings.current.texture_smoothing);
    r3d_texture_filtering(settings.current.texture_filtering);
    r3d_car_shadows(settings.current.car_shadows);
    r3d_material_shading(settings.current.material_shading);
    r3d_reflection_quality(settings.current.reflection_quality);
    r3d_shadow_map_quality(settings.current.shadow_maps);
    r3d_shadow_filter(settings.current.shadow_filter);
    r3d_modern_lighting(settings.current.modern_lighting);
    r3d_daylight_options(settings.current.daylight,video_settings_solar_hour(&settings.current),settings.current.solar_cycle);
    video_hdr_options(settings.current.hdr,settings.current.sun_rays);
    video_ambient_occlusion(settings.current.ambient_occlusion);
#endif
    if (!video_set_upscale(upscale)) {
        settings.current.video.upscale = 0;
        fprintf(stderr, "FSR unavailable; using native resolution.\n");
    }
    if (!shot) {
        plat_audio_desc audio = {48000, 2};
        plat_audio_init(&audio, NULL, NULL);
    }

#ifndef __SWITCH__
    /* The no-argument front-end: the disc and the graphics API come from launcher.ini, and on
       the very first run from the wizard (video is up, the volume is not open yet). */
    if (front_end) {
        char msg[256];
        int configured = front_disc[0] && startup.preferences.api[0] &&
                         launcher_validate_disc(front_disc, msg, sizeof msg);
        if (wiz_shot_prefix || !configured) {
            int setup = first_run_wizard(&startup.preferences);
            wiz_came = setup == 1;
            if (setup != 1)
                return 0; /* cancelled, or reopened with the chosen graphics API */
            front_disc = startup.preferences.disc;
        } else {
            printf("setup loaded from launcher.ini\n");
        }
        argv[1] = (char *)front_disc;
    }
#endif
    /* Disc-dependent tables and the volume open now that the disc is known. */
    app_car_ratings = gt4_car_ratings_open(argv[1]);
    SDL_PumpEvents(); /* P: keep the window answering while the disc's tables load */
    app_car_factory = gt4_career_vehicle_open(argv[1]);
    SDL_PumpEvents();
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, app_car_factory);
    gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears, app_car_factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats, app_car_factory);
    gt4_game_set_service_reader(gt4_career_vehicle_service_geometry, app_car_factory);
    gt4_game_set_setting_reader(gt4_career_vehicle_setting, app_car_factory);
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    SDL_PumpEvents();
    if (vol && !gt4_game_load_disc(argv[1]))
        fprintf(stderr,
                "career: original calendar, price or oil constants unavailable for this disc\n");
    SDL_PumpEvents();
    if (vol && !present_startup_load(vol)) {
        plat_dialog("Load Game",
                    gt4_save_error()[0] ? gt4_save_error() : "The profile could not be loaded.", 0);
        if (startup_intro) {
            first_run = 1;
            proj = 7;
            startup_intro = 0;
        }
    }
    {
        /* The local HD cache of the menu images (docs/UI_HD.md), built by
           gt4hdui; MOON_UI_HD=0 turns it off, or names another folder. */
        const char *hd = getenv("MOON_UI_HD"), *base = getenv("LOCALAPPDATA");
        char dir[512] = "";
        if (hd && strcmp(hd, "0"))
            snprintf(dir, sizeof dir, "%s", hd);
        else if (!hd && base) {
            snprintf(dir, sizeof dir, "%s/MoonEngine/ui_hd", base);
            /* P: the cache of the project's former name, while no new one exists */
            char old[512];
            struct stat st;
            snprintf(old, sizeof old, "%s/OpenGT4/ui_hd", base);
            if (stat(dir, &st) && !stat(old, &st))
                snprintf(dir, sizeof dir, "%s", old);
        }
        ui_set_hd_cache(dir);
    }
    if (!vol) {
        fprintf(stderr, "cannot open the game volume: %s\n", err);
        return 1;
    }

    loading_screen_open(vol, argv[1]);
#ifdef __SWITCH__
    /* P: compile/exercise every enabled GL program behind loading rather
       than blocking the first screen, wheel, shadow or image-filter use. */
    loading_screen_begin();
    while (!loading_screen_ready() && !plat_should_quit())
        pump_loading_work(NULL);
    SWP_LOAD_BEGIN("shader-warmup");
    plat_video_begin_frame();
    int shaders_ready = r3d_warmup() && switch_gpu_warmup();
    SWP_LOAD_END();
    fprintf(stderr, "Switch GPU warmup: all_enabled=%d programs=%u\n", shaders_ready,
            gl_program_count());
    pump_loading_work(NULL);
    loading_screen_end();
#endif
#ifdef MOON_PC_VULKAN
    if (moon_vk_render_active() && !getenv("MOON_VK_SKIP_PREPARE")) {
        loading_screen_begin();
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }
        int prepared = r3d_warmup() && switch_gpu_warmup();
        moon_vk_render_drain();
        fprintf(stderr, "Vulkan startup preparation: ready=%d pipelines=%llu\n", prepared,
                (unsigned long long)moon_vk_render_statistics().pipeline_creations);
        pump_loading_work(NULL);
        loading_screen_end();
        /* The loader uses 448 lines; the menu VM starts in its 480-line view. */
        plat_video_set_view(640, 480);
    }
#endif
    presentation_disc = argv[1];
    showroom *cars = (studio_car || preview_car || front_end) ? showroom_open_studio(argv[1])
                                                              : showroom_open(argv[1]);
    presentation_cars = cars;
    SDL_PumpEvents();
    race_view *race = race_view_open(argv[1]);
    SDL_PumpEvents();
    presentation_photo = photo_travel_open(argv[1]);
    SDL_PumpEvents();
    if (preview_car)
        return preview_run(cars, preview_car, shot, color_preview);
    if (studio_car) {
        studio_disc = argv[1];
        return studio_run(cars, vol, studio_car, shot, color_preview, 0);
    }
    if (front_end) {
        static char saved_car[96];
        int fcol = startup.preferences.color;
        const char *fcar = startup.preferences.car[0] ? startup.preferences.car : FRONT_CAR;
        int saved = studio_saved_car(startup.preferences.saves, vol, saved_car, &fcol);
        if (saved)
            fcar = saved_car;
        {
            char msg[256];
            studio_status.disc_ok = launcher_validate_disc(front_disc, msg, sizeof msg);
            snprintf(studio_status.disc, sizeof studio_status.disc, "%s",
                     studio_status.disc_ok ? "Gran Turismo 4 disc ready (SCUS-97328)" : msg);
            if (!studio_status.profile[0])
                snprintf(studio_status.profile, sizeof studio_status.profile,
                         "GT Mode: no saved game yet");
            /* the car's name from the disc's own tables, as the menus name it */
            size_t ln = 0, nn = 0, sn = 0;
            uint8_t *labels = gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &ln),
                    *names = gt4_vol_read(vol, "/specdb/GT4_US2560/CAR_NAME_american.dbt", &nn),
                    *strs = gt4_vol_read(vol, "/specdb/GT4_US2560/american_StrDB.sdb", &sn);
            uint32_t id;
            uint8_t row[256];
            const char *title = NULL;
            if (labels && names && strs && gt4_specdb_label(labels, ln, fcar, &id) &&
                gt4_specdb_row(names, nn, id, row, sizeof row) == 16)
                title = gt4_specdb_string(strs, sn, (uint32_t)row[0] | (uint32_t)row[1] << 8 |
                                                         (uint32_t)row[2] << 16 |
                                                         (uint32_t)row[3] << 24);
            snprintf(studio_status.car, sizeof studio_status.car, "%s%s", title ? title : fcar,
                     saved ? "" : "   (no car in the save)");
            free(labels);
            free(names);
            free(strs);
            int mods = 0;
            for (const char *r = moon_mod_roots(); r && *r; r++)
                if (r[0] != ';' && (r == moon_mod_roots() || r[-1] == ';'))
                    mods++;
            snprintf(studio_status.mods, sizeof studio_status.mods,
                     mods ? "Mods: %d active" : "Mods: none active", mods);
#ifdef __SWITCH__
            const char *api = "Switch";
#else
            const char *api = moon_renderer_name(settings.current.renderer);
#endif
            snprintf(studio_status.renderer, sizeof studio_status.renderer, "Renderer: %s",
                     !strcmp(api, "vulkan") ? "Vulkan" : !strcmp(api, "opengl") ? "OpenGL" : api);
        }
        printf("studio: room car %s (wheel %s, wing %s)\n", fcar,
               studio_wheel[0] ? studio_wheel : "-", studio_wing[0] ? studio_wing : "-");
        studio_disc = argv[1];
        studio_prefs = &startup.preferences;
        {   /* the game's options file is "<video file>.game_options" */
            snprintf(studio_video_file, sizeof studio_video_file, "%s", game_options_path);
            char *suffix = strstr(studio_video_file, ".game_options");
            if (suffix)
                *suffix = 0;
            else
                studio_video_file[0] = 0;
        }
        int fr = studio_run(cars, vol, fcar, front_shot, fcol, 1);
        if (fr != 2) {
            music_library_install(NULL);music_library_close(app_music_library);app_music_library=NULL;
            return fr;
        } /* 2 = START: the launcher is black, run the game (its opening rises from black) */
        showroom_clear_rig(cars);
        showroom_use_disc_room(cars);
        /* the save's wheel and wing were the room car's: the game's cars bring their own */
        showroom_set_wheel(cars, NULL);
        showroom_set_wing(cars, NULL);
        fprintf(stderr, "launcher: START, save slot %d active\n", gt4_save_active());
        showroom_set_width(cars, 640); /* hand the showroom back to the game */
        /* The studio drew in its own fill view; put the game's own authentic present back
           (the resolution and ratio from the video options), so nothing is zoomed or cropped. */
        video_set_fill_window(0);
        video_settings_apply_current(&settings); /* what OPTIONS changed in the launcher */
        video_set_aspect(settings.current.aspect);
        {
            /* the game's own window mode and size, as its video settings have them */
            SDL_Window *window = plat_sdl_window();
            if (settings.current.video.fullscreen)
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            else {
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, settings.current.video.width,
                                  settings.current.video.height);
                SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
        }
        plat_video_set_view(640, 480);
    #ifdef __SWITCH__
    video_switch_scene_resolution(settings.current.scene_resolution);
#endif
    video_set_aspect(settings.current.aspect);
        {
            const char *ls = getenv("MOONSTUDIO_LAUNCH_SHOT");
            if (ls) {
                shot = ls;
                const char *d = getenv("MOONSTUDIO_LAUNCH_DELAY");
                if (d)
                    shot_delay = atof(d);
            }
        } /* dev: capture after the handover */
    }
    /* The studio OPTIONS page can change the Tweaks switch after the startup
       decision above; apply it here, before the boot project or the movie. */
    if (video_settings_skip_intro() && startup_intro && proj == 2) {
        startup_intro = 0;
        show_opening = 0;
        proj = 0;
        root = 0;
        app_startup_log("startup: launcher path forced arcade / TopRoot (skip_intro=1)");
    }
    /* the race itself (the physics with the player's controls): driving 1
       while its models load, 2 racing */
    race_game *game = race_game_open(argv[1]);
    int driving = 0;
    /* P: Graphics Swap Key - F7 in a race or replay shows Classic graphics until pressed
       again (nothing saved); the label says which, for two seconds */
    int race_graphics_swap = 0;
    uint64_t race_graphics_swap_shown = 0;
    int photo_return_project = -1;
    presentation_race = game;
    /* driving: 1 the race loading (the quick menu shown), 3 loaded, 2
       racing; its settings kept to race again from the quick menu */
    int race_start_pending = 0, race_color = 0, race_others_n = 0, race_other_colors[5] = {0};
    int theater = 0; /* 1 user replay, 2 title attract tape */
    double attract_seconds = 0;
    int attract_cancel = 0;
    char race_others[5][32];
    const char *race_other_ptr[5];
    for (int i = 0; i < 5; i++)
        race_other_ptr[i] = race_others[i];
    movie_view movie = {0};
    movie.muted = shot != NULL;
    movie.volume = gt4_vol_open_movies(argv[1], &err);
    layer_movie = &movie;
    presentation_movie_layer = movie_layer;
    movie.lock = SDL_CreateMutex();
    movie.wake = SDL_CreateCond();
    if (movie.lock && movie.wake)
        movie.thread = SDL_CreateThread(movie_worker, "movie loader", &movie);
    /* The legal notice and "SCEA Presents" come before the boot project. */
    gt4_boot_notices notices = {0};
    plat_texture *notice_tex[GT4_NOTICE_COUNT] = {0};
    int notice = GT4_NOTICE_COUNT;
    uint64_t notice_start = 0;
    if (startup_intro) {
        gt4_boot_notices_open(&notices, argv[1], vol);
        for (int i = 0; i < GT4_NOTICE_COUNT; i++) {
            const tex1_image *im = gt4_boot_notice_image(&notices, i);
            if (im)
                notice_tex[i] = plat_texture_create(im->width, im->height, im->rgba);
            else
                fprintf(stderr, "boot notice %d unavailable\n", i);
        }
        notice = 0;
    }
    /* Tracks are requested from the authored RootWindow.bgm, not project IDs.
       Each preparation worker owns its disc reader. A cached track keeps its
       playback position across keep/popups and loads without blocking input. */
    music_job music_load[32] = {0};
    int music_count = 0;
    /* RootWindow.bgm is requested by menu_enter before its reveal
       (0022F6A0 page entry; original mSound playback 004616E8).
       A: the PC backend pre-renders complete sequences. Prefetch MAP,
       the GT Mode authored group, while boot/project loading is running
       rather than beginning that expensive work after the loading overlay. */
    music_job *map_prefetch = &music_load[music_count++];
    map_prefetch->disc = argv[1];
    map_prefetch->tempo = music_tempo;
    snprintf(map_prefetch->name, sizeof map_prefetch->name, "map");
    SDL_AtomicSet(&map_prefetch->volume, 120);
    map_prefetch->thread = SDL_CreateThread(prepare_music, "menu music preparation", map_prefetch);
    if (!map_prefetch->thread)
        SDL_AtomicSet(&map_prefetch->ready, 1);
    pregrid_audio *pregrid_music = pregrid_audio_create(argv[1]);
    menu_audio_context menu_audio_state = {0};
    menu_audio_state.jobs = music_load;
    menu_audio_state.count = &music_count;
    menu_audio_state.disc = argv[1];
    menu_audio_state.tempo = music_tempo;
    menu_audio_state.pcm = pregrid_music;
    menu_audio_state.enabled = !shot || music_with_shot;
    menu_audio_state.allowed = 1;
    char *desired_music = menu_audio_state.desired;
    preparing_audio = &menu_audio_state;
    char dialog_error[256] = "";
    dialog_ui = ui_project_load(vol, "cursor", dialog_error, sizeof dialog_error);
    if (!dialog_ui)
        fprintf(stderr, "dialog: %s\n", dialog_error);
    else
        ui_prepare_dialogs(dialog_ui, NULL, NULL);
    menu *cache[NPROJECTS] = {0};
    menu *mn = cache[proj] = load(vol, proj);
    ui_project *p = mn ? menu_ui(mn) : NULL;
    if (p && startup_intro && show_opening)
        for (int i = 0; i < ui_root_count(p); i++)
            if (!strcmp(ui_root_name(p, i), "MovieRoot")) {
                root = i;
                break;
            }
    if (proj == 7) {
        root = save_screen.root;
        if (first_run)
            save_ui_first_run(&save_screen);
    }
    if (p && argc > 3 && !startup_intro)
        for (int i = 0; i < ui_root_count(p); i++)
            if (!strcmp(ui_root_name(p, i), argv[3]))
                root = i;
    if (p && settings_preview)
        for (int i = 0; i < ui_root_count(p); i++)
            if (!strcmp(ui_root_name(p, i), "ScreenRoot"))
                root = i;

    if (mn) {
        if (proj == 4 && !menu_setting_begin(mn)) {
            fprintf(stderr, "Setting: no valid garage car\n");
            exit_code = 1;
            goto done;
        }
        if (photo_project(proj)) {
            char course[32], car[96];
            int color = 0;
            const char *test_course = getenv("MOON_PHOTO_COURSE"),
                       *test_car = getenv("MOON_PHOTO_CAR");
            if (test_course && test_car)
                menu_photo_set_selection(mn, test_course, test_car, 0);
            if (!menu_photo_request(mn, course, car, &color) ||
                !photo_travel_load(presentation_photo, mn, course, car, color, pump_loading_work,
                                   NULL)) {
                fprintf(stderr, "Photo Travel: no valid destination/garage car\n");
                exit_code = 1;
                goto done;
            }
        }
        menu_preview_courses(mn, preview_courses);
        if (preview_courses)
            printf("Catalog preview: disc catalog, save unlocks not loaded.\n");
        if (getenv("MOON_TEST_CAR"))
            menu_test_car(mn, getenv("MOON_TEST_CAR")); /* tests: a new car ridden */
        if (getenv("MOON_MENU_PUSH"))
            menu_test_push(
                mn, getenv("MOON_MENU_PUSH")); /* tests: a page entered as from a dealer */
        if (!(proj == 12 && getenv("MOON_LABO_ARG")))
            menu_enter(mn, root);
        if (proj == 1)
            custom_modes_attach(mn);
        if (getenv("MOON_TEST_MONEY")) {
            menu_test_money(mn, strtoll(getenv("MOON_TEST_MONEY"), NULL, 10));
            menu_save_capture(mn);
        } /* tests: credits */
        if (proj == 12 &&
            getenv(
                "MOON_LABO_SAVE")) { /* tests: a saved replay offered to the Save Replay screen */
            static gt4_replay_file offered;
            if (gt4_replay_load(getenv("MOON_LABO_SAVE"), &offered))
                menu_set_replay_to_save(&offered);
        }
        if (proj == 12 && getenv("MOON_LABO_ARG"))
            menu_open_labo(
                mn, getenv("MOON_LABO_ARG")); /* tests: labomode at a page or a card mode */
        if (proj == 1 && getenv("MOON_MISSION"))
            menu_request_mission(
                mn, getenv("MOON_MISSION")); /* tests: a mission, its licence check passed */
        menu_tick(mn);
        if (proj == 7 && player_name)
            save_ui_name(&save_screen, player_name);
        while (selection_steps-- > 0)
            menu_send_input(mn, MENU_DOWN);
        if (input_sequence) {
            char *seq = strdup(input_sequence);
            scripted_keys = seq;
            scripted_interval = (float)input_interval;
            char *key;
            while ((key = next_scripted_key()) != NULL) {
                if (!strcmp(key, "l1") || !strcmp(key, "r1")) {
                    int symbol = !strcmp(key, "l1") ? 0xffc8 : 0xffd2;
                    menu_send_key(mn, symbol, 0);
                    menu_send_key(mn, symbol, 1);
                    menu_tick(mn);
                    printf("input %s -> %s/%s focus=%s\n", key, projects[proj],
                           ui_root_name(menu_ui(mn), menu_active_root(mn)), menu_focus_name(mn));
                    continue;
                }
                menu_input in;
                if (!strcmp(key, "profiles")) {
                    present_profile_picker(mn);
                    continue;
                }
                if (!strncmp(key, "mouse:", 6)) {
                    /* P: "mouse:fx:fy", the pointer moved to that fraction of the window: a
                       real motion event, handled by the loop as the player's (launcher prints) */
                    float fx, fy;
                    int ww = 0, wh = 0;
                    SDL_Window *win = plat_sdl_window();
                    if (win && sscanf(key + 6, "%f:%f", &fx, &fy) == 2) {
                        SDL_GetWindowSize(win, &ww, &wh);
                        SDL_Event motion = {0};
                        motion.type = SDL_MOUSEMOTION;
                        motion.motion.windowID = SDL_GetWindowID(win);
                        motion.motion.x = (int)(fx * ww);
                        motion.motion.y = (int)(fy * wh);
                        SDL_PushEvent(&motion);
                        printf("input %s -> pointer %d,%d\n", key, motion.motion.x, motion.motion.y);
                    }
                    continue;
                }
                if (!strcmp(key, "wait")) {
                    /* the menu keeps running while the test waits, as it does
                       between a player's presses (its watchers, e.g.
                       labomode.ad onCheck filling the replay list) */
                    ui_set_movie_renderer(menu_ui(mn), draw_movie, &movie);
                    uint64_t until = plat_time_ns() +
                                     (uint64_t)((input_interval > 0 ? input_interval : 1.0) * 1e9);
                    do {
                        present_wait(mn, 1.0f / 60, NULL);
                        menu_tick(mn);
                    } while (plat_time_ns() < until && !plat_should_quit());
                    continue;
                }
                /* tests: "l1+", "r1+" or "l1r1+" before an input hold those buttons through it */
                unsigned held = 0;
                const char *named = key;
                if (!strncmp(named, "l1r1+", 5)) {
                    held = MENU_PAD_L1 | MENU_PAD_R1;
                    named += 5;
                } else if (!strncmp(named, "l1+", 3)) {
                    held = MENU_PAD_L1;
                    named += 3;
                } else if (!strncmp(named, "r1+", 3)) {
                    held = MENU_PAD_R1;
                    named += 3;
                }
                if (!input_name(named, &in)) {
                    fprintf(stderr, "unknown input '%s'\n", key);
                    free(seq);
                    scripted_keys = NULL;
                    exit_code = 2;
                    goto done;
                }
                menu_set_pad_held(mn, held);
                menu_send_input(mn, in);
                { /* a tap: the key's release too (photo_shoot.ad lets a grab go on it) */
                    static const int released[] = {65362, 65364, 65361, 65363, 65293,
                                                   65307, 65421, 65307, 65379};
                    menu_send_key(mn, released[in], 1);
                }
                menu_set_pad_held(mn, 0);
                menu_tick(mn);
                switch_requested(vol, cache, &mn, &proj);
                printf("input %s -> %s/%s focus=%s\n", key, projects[proj],
                       ui_root_name(menu_ui(mn), menu_active_root(mn)), menu_focus_name(mn));
                if (input_interval > 0) {
                    ui_set_movie_renderer(menu_ui(mn), draw_movie, &movie);
                    present_wait(mn, (float)input_interval, NULL);
                }
            }
            scripted_keys = NULL;
            scripted_interval = 0;
            free(seq);
        }
        p = menu_ui(mn);
        root = menu_active_root(mn);
        if (shot)
            for (int i = 0; i < 60; i++)
                menu_animate(mn, 1.0f / 60);
    }

    int last_root = -1, last_proj = -1, was_car_page = 0;
    int movie_page_root = -1, movie_page_project = -1;
    if (race_preview) {
        const char *car = race_car_option ? race_car_option : menu_selected_car(mn);
        const char *course = race_course_option ? race_course_option : menu_selected_course(mn);
        snprintf(race_car, sizeof race_car, "%s", car && *car ? car : "ka_01");
        snprintf(race_course, sizeof race_course, "%s", course && *course ? course : "tsukuba");
        if (race_drive) {
            static const char *const others[] = {"_1100_standard_type11_49", "z_act_70",
                                                 "today_g_85", "midget2_d_type_98", "s500_63"};
            char msg[256];
            int field_size = race_field_size_request();
            int preview_others = field_size - 1;
            if (preview_others > MOON_CONDUCTOR_CARS - 1)
                preview_others = MOON_CONDUCTOR_CARS - 1;
            race_game_set_field_size(race_phantoms_on() ? field_size : 0);
            race_game_set_time_trial(game, race_time_trial);
            gt4_race_desc split = {0};
            split.tire_2p = 1;
            race_game_set_split(game, split_preview ? &split : NULL);
            const char *second[1] = {split_car_option ? split_car_option : race_car};
            if (getenv("MOON_REPLAY_PLAY")) { /* tests: a saved replay watched at once */
                gt4_replay_file r;
                if (!game || !race_game_read_replay(game, getenv("MOON_REPLAY_PLAY"), &r, msg,
                                                    sizeof msg)) {
                    fprintf(stderr, "replay: cannot read %s\n", getenv("MOON_REPLAY_PLAY"));
                    exit_code = 1;
                    goto done;
                }
                race_game_replay_import(game, &r);
                gt4_replay_free(&r);
                if (!race_game_replay(game, msg, sizeof msg)) {
                    fprintf(stderr, "replay: %s\n", msg);
                    exit_code = 1;
                    goto done;
                }
            } else if (!game || !race_game_start(game, race_course, race_car, color_preview,
#ifdef MOON_SWITCH_RACE_DIAGNOSTICS
                                                 split_preview ? second : race_grid[0] ? race_grid : others, NULL,
#else
                                                 split_preview ? second : others, NULL,
#endif
                                                 split_preview     ? 1
                                                 : race_time_trial ? 0
                                                                   : preview_others,
                                                 msg, sizeof msg)) {
                fprintf(stderr, "race: %s\n", msg);
                exit_code = 1;
                goto done;
            }
            if (split_preview)
                pre_race_preview = 0;
            { /* test hook: arm Custom Modes elimination on a scripted race.
                 1 = with the thunder strike, 2 = the silent cut. */
                const char *elim = getenv("MOON_ELIM_TEST");
                if (elim && *elim)
                    race_game_set_elimination(game, 1, atoi(elim) != 2);
            }
            driving = 1;
            race_start_pending = !pre_race_preview;
            race_color = color_preview;
            race_others_n = race_time_trial ? 0 : 5;
            for (int i = 0; i < 5; i++)
                snprintf(race_others[i], sizeof race_others[i], "%s", others[i]);
            if (pre_race_preview) {
                char quick[64];
                snprintf(quick, sizeof quick, "%s", race_quick_route(race_time_trial));
                quick_race_set(game, race_course, race_car, race_color, race_other_ptr,
                               race_other_colors, race_others_n, 0);
                if (getenv(
                        "MOON_REPLAY_IMPORT")) { /* tests: the quick menu after a race, its replay a saved one */
                    gt4_replay_file r;
                    if (gt4_replay_load(getenv("MOON_REPLAY_IMPORT"), &r)) {
                        race_game_replay_import(game, &r);
                        gt4_replay_free(&r);
                    }
                }
                switch_to(vol, cache, &mn, &proj, quick);
                p = menu_ui(mn);
                root = menu_active_root(mn);
                if (getenv("MOON_REPLAY_IMPORT")) {
                    menu_set_quick_completed(mn, 1);
                    menu_enter(mn, root);
                    menu_tick(mn);
                }
            }
        } else if (!race_view_request(race, race_course, race_car, color_preview)) {
            exit_code = 1;
            goto done;
        } else
            race_loading = 1;
    }
    if (loading_preview || driving == 1 || race_loading)
        loading_screen_begin();
    uint64_t shot_wait = 0, shot_started = 0;
    presentation_clock = plat_time_ns();
    int returning_quick_options = 0;
    app_race_flow race_flow = {.game = game,
                               .race = race,
                               .mn = &mn,
                               .cache = cache,
                               .proj = &proj,
                               .driving = &driving,
                               .race_start_pending = &race_start_pending,
                               .last_root = &last_root,
                               .race_loading = &race_loading,
                               .race_active = &race_active,
                               .race_return = &race_return,
                               .theater = &theater,
                               .attract_seconds = &attract_seconds,
                               .attract_cancel = &attract_cancel,
                               .race_course = race_course,
                               .race_car = race_car,
                               .shot = shot,
                               .exit_code = &exit_code};
    app_music_ensure(0);
    while (!plat_should_quit()) {
        app_music_tick();
        plat_key_event ev;
        if (!handle_race_retry_and_replay_return(&race_flow))
            goto done;
        if (game && race_quick_project(proj) && logger_track >= 0 && !logger_load_asked &&
            !menu_pending_command(mn) && ui_fade_remaining(menu_ui(mn)) <= 0 &&
            menu_active_root(mn) >= 0 && !loading_screen_active()) {
            /* 0010FEE8: replace only the selected bank from the loaded lap;
               cancel retains both banks and returns to the Logger. */
            char name[64], msg[160];
            int track = logger_track;
            logger_track = -1;
            if (menu_take_loaded_ghost(name, sizeof name)) {
                gt4_replay_file r;
                int ok = gt4_replay_load(name, &r);
                if (ok) {
                    loading_screen_begin();
                    ok = race_game_logger_load(game, &r, track, msg, sizeof msg);
                    if (ok) {
                        menu_logger_source src;
                        if (race_game_logger_source(game, &src))
                            menu_set_logger_source(&src);
                    }
                    loading_screen_end();
                }
                gt4_replay_free(&r);
                if (!ok)
                    present_dialog(
                        mn, "Logger",
                        "Cannot load this recording. Choose a best lap from the same course.", 0,
                        NULL, NULL);
            }
            char logger[] = "logger:LoggerRoot";
            switch_to(vol, cache, &mn, &proj, logger);
            p = menu_ui(mn);
            root = menu_active_root(mn);
        }
        if (game &&
            race_quick_project(
                proj)) { /* a best lap the memory card's screen loaded: the Time Trial's ghost */
            char name[64];
            if (menu_take_loaded_ghost(name, sizeof name)) {
                gt4_replay_file r;
                int ok = gt4_replay_load(name, &r);
                if (ok)
                    ok = race_game_ghost_import(game, &r);
                if (ok) {
                    menu_set_quick_best(r.best_lap < session_best ? (session_best = r.best_lap)
                                                                  : session_best);
                    menu_set_quick_ghost(1);
                    fprintf(stderr, "ghost: %s loaded (%zu steps)\n", name, r.steps);
                }
                gt4_replay_free(&r);
                if (!ok)
                    present_dialog(mn, "Load Ghost",
                                   "This ghost was driven on another course or with another car.",
                                   0, NULL, NULL);
            }
        }
        if (!driving && game &&
            (proj != 12 ||
             menu_in_replay_theater(
                 mn))) { /* a replay the memory card's screen or the Replay Theater loaded */
            char name[64];
            int attract = menu_take_attract_replay(name, sizeof name);
            if (attract || menu_take_loaded_replay(name, sizeof name)) {
                gt4_replay_file r;
                char msg[256];
                if (race_game_read_replay(game, name, &r, msg, sizeof msg) &&
                    race_game_replay_import(game, &r)) {
                    snprintf(race_course, sizeof race_course, "%s", r.course);
                    snprintf(race_car, sizeof race_car, "%s", r.car);
                    {
                        const char *rm = menu_option_text(mn, "replay_mode", "NORMAL");
                        race_game_set_replay_options(
                            game, menu_option_number(mn, "replay_display_enable", 0) != 0,
                            rm && !strcmp(rm, "DIVE"));
                    }
                    if (race_game_replay(game, msg, sizeof msg)) {
                        ui_fade(menu_ui(mn), 0, 0, 0);
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 1;
                        theater = attract ? 2 : 1;
                        attract_seconds = 0;
                        attract_cancel = 0;
                        fprintf(stderr, "%s: %s loaded\n",
                                attract ? "attract replay" : "theater replay", name);
                    } else {
                        fprintf(stderr, "replay: %s\n", msg);
                        if (attract)
                            menu_attract_finish(mn, 1);
                        else
                            present_dialog(mn, "Replay Theater", "This replay could not be played.",
                                           0, NULL, NULL);
                    }
                } else {
                    if (attract)
                        menu_attract_finish(mn, 1);
                    else
                        present_dialog(mn, "Replay Theater", "This replay could not be read.", 0,
                                       NULL, NULL);
                }
                gt4_replay_free(&r);
            }
        }
        if (!return_race_to_menu(&race_flow))
            goto done;
        if (mn) { /* the pad's L1 and R1 for the menus: Q and E held (P: a PC key choice) */
            const Uint8 *k = SDL_GetKeyboardState(NULL);
            menu_set_pad_held(mn, plat_sdl_menu_buttons() |
                                      plat_sdl_menu_directions(settings.current.desktop) |
                                      (k[SDL_SCANCODE_Q] ? MENU_PAD_L1 : 0) |
                                      (k[SDL_SCANCODE_E] ? MENU_PAD_R1 : 0));
        }
#ifndef __SWITCH__
        menu_set_controller_tiles(settings.current.controller_tiles);
        ui_main_menu_layout(menu_ui(mn),settings.current.main_menu);
        ui_pointer_smoothing(menu_ui(mn),settings.current.smooth_cursor);
        ui_pointer_feedback(menu_ui(mn),settings.current.hover_feedback);
        ui_menu_readability(menu_ui(mn),settings.current.compact_cars);
        SDL_ShowCursor(settings.current.desktop ? SDL_DISABLE : SDL_ENABLE);
        if(!settings.current.desktop && mn) ui_pointer(menu_ui(mn),0,0,0);
#endif
#ifdef __ANDROID__
        moon_android_configure(&settings.current.mobile,
                               driving == 2 && !race_game_is_paused(game) &&
                                   !race_game_replaying(game) && theater != 2);
#endif
        uint64_t event_start = plat_time_ns();
        while (plat_sdl_poll(&ev)) {
            if (mlj_rename_active() && mlj_rename_event(&ev))
                continue;
            if (music_download_active() && music_download_event(&ev))
                continue;
#ifdef __ANDROID__
            if (ev.lifecycle) {
                if (driving == 2 && !race_game_is_paused(game))
                    race_game_key(game, SDLK_ESCAPE);
                presentation_clock = plat_time_ns();
            }
            if (moon_android_event(&ev))
                continue;
#endif
#if defined(__SWITCH__) || defined(__ANDROID__)
            if (ev.pad || ev.key) {
                if (mn)
                    ui_touch_mode(menu_ui(mn), 0);
                ui_touch_mode(dialog_ui, 0);
                settings.touch_back.tap = (touch_dialog_state){0};
            }
#endif
            if (ev.touch) {
#if defined(__SWITCH__) || defined(__ANDROID__)
                /* Touch is activity too: use the same exit path as a pad press
                   in the title attract cycle (0010BA28/0010BE20). */
                if (settings.current.touch_menu && ev.touch == 1 && driving == 2 && theater == 2) {
                    attract_cancel = 1;
                    race_game_key(game, SDLK_ESCAPE);
                    continue;
                }
                if (notice >= GT4_NOTICE_COUNT && driving != 2 && !race_loading &&
                    !loading_screen_active())
                    desktop_menu_event(mn, &ev);
                else {
                    menu_touch(mn, 0, 4, 0, 0, 0);
                    if (driving == 2 && settings.current.touch_menu && race_game_is_paused(game)) {
                        int back = 0;
                        int back_contact = touch_back_input(dialog_ui, 1, game, -1, &ev,
                                                            &settings.touch_back, &back);
#ifdef __ANDROID__
                        if (!back_contact)
                            race_game_pause_touch(game, ev.touch, ev.touch_id, ev.touch_x * 640,
                                                  ev.touch_y * 480);
#else
                        (void)back_contact;
#endif
                        if (ev.touch == 1)
                            ui_touch_mode(dialog_ui, 1);
                        if (back)
                            race_game_pause_back(game);
                    }
                }
#endif
                continue;
            }
            if (ev.mouse && ev.mouse_absolute) {
#ifndef __SWITCH__
                if(driving==2 && settings.current.desktop) {
                    float x,y;
                    int inside=video_window_to_ui(ev.mouse_x,ev.mouse_y,&x,&y);
                    race_game_pause_pointer(game,inside,x,y*480.f/video_view_height(),ev.mouse_button);
                }
#endif
                if (notice >= GT4_NOTICE_COUNT && driving != 2 && !race_loading && !loading_screen_active())
                    desktop_menu_event(mn, &ev);
                continue;
            }
            if (!ev.mouse && driving != 2 && proj != 7)
                desktop_menu_event(mn, &ev);
            if (ev.key == SDLK_F10)
                goto done;

            if (ev.mouse) {
                if (driving == 2)
                    race_game_cinematic_mouse(game, ev.mouse_x, ev.mouse_y, ev.wheel);
                continue;
            }
            if (notice < GT4_NOTICE_COUNT)
                continue; /* the original screens take no input */
            if (driving == 2) {
                if (race_game_fail_active(game)) {
                    if (!ev.pad)
                        race_game_fail_key(game, ev.key);
                    continue;
                }
                if (!ev.pad && ev.key == SDLK_F9) {
                    race_game_cinematic_toggle(game);
                    continue;
                }
                /* Custom Modes: L fires one real strike, through the same
                   elimination cut the lap line uses. */
                if (!ev.pad && ev.key == SDLK_l && race_game_elimination(game)) {
                    race_game_strike_test(game);
                    continue;
                }
                if (!ev.pad && ev.key == SDLK_k) {
                    race_game_fail_test(game);
                    continue;
                }
#if !defined(__SWITCH__) && !defined(__ANDROID__)
                if (!ev.pad && ev.key == SDLK_F7 && settings.current.graphics_swap) {
                    race_graphics_swap ^= 1;
                    video_settings_preview_graphics(&settings, race_graphics_swap);
                    race_graphics_swap_shown = plat_time_ns();
                    printf("graphics swap: %s\n", race_graphics_swap ? "classic" : "yours");
                    continue;
                }
#endif
                if (!ev.pad && race_game_cinematic(game) && ev.key != SDLK_F5 &&
                    ev.key != SDLK_F3) {
                    race_game_cinematic_key(game, ev.key);
                    continue;
                }
                /* 0010BA28/0010BE20: a pad press leaves the attract cycle. */
                if (theater == 2) {
                    attract_cancel = 1;
                    race_game_key(game, SDLK_ESCAPE);
                    continue;
                }
                /* P: F5 opens Photo Drive from its live session or a replay. Rendering borrows
                   the resident race; driving=5 prevents any solver frame. */
                if ((ev.key == SDLK_F5 || race_game_photo_select(game, ev.ps2key, ev.release)) &&
                    race_game_photo_view(game)) {
                    if (race_game_cinematic(game))
                        race_game_cinematic_toggle(game);
                    if (!cache[14])
                        cache[14] = load(vol, 14);
                    photo_travel_drive_collision(presentation_photo,game,race_game_photo_ground,race_game_photo_wall);
                    float camera_size[4], camera_distance;
                    if (race_game_photo_camera_data(game, camera_size, &camera_distance))
                        photo_travel_drive_setup(presentation_photo, camera_size, camera_distance);
                    if (cache[14] && photo_travel_drive(presentation_photo, cache[14],
                                                        race_game_photo_view(game))) {
                        photo_return_project = proj;
                        mn = cache[14];
                        proj = 14;
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                        driving = 5;
                        race_game_photo_pause(game, 1);
                        plat_video_set_view(640, 480);
                    }
                    continue;
                }
                /* P: F6 stores the complete native 2P recording. The same
                   file is selectable in Replay Theater. */
                if (ev.key == SDLK_F6 && race_game_split(game) && race_game_can_replay(game)) {
                    gt4_replay_file r;
                    char name[64];
                    if (race_game_replay_export(game, &r, GT4_REPLAY_FULL) &&
                        gt4_replay_store(&r, name, sizeof name))
                        fprintf(stderr, "2P replay saved: %s\n", name);
                    continue;
                }
                if (ev.pad)
                    continue; /* race_game owns controller edges */
                if (ev.key == SDLK_F3)
                    race_game_inspect_toggle(game);
                /* Escape, the arrows and Enter go to the pause menu (RacePause) */
                race_game_key(game, ev.key);
                continue;
            }
            if (theater == 2 && driving == 1) {
                menu_attract_finish(mn, 1);
                race_game_stop(game);
                driving = 0;
                race_start_pending = 0;
                theater = 0;
                loading_screen_end();
                plat_video_set_view(640, 480);
                presentation_clock = plat_time_ns();
                continue;
            }
            if (driving == 1 && loading_screen_active()) {
                if (ev.key == SDLK_ESCAPE) {
                    if (race_game_demonstrating(game)) {
                        char msg[256];
                        if (!race_game_end_demo(game, msg, sizeof msg)) {
                            fprintf(stderr, "demonstration return: %s\n", msg);
                            exit_code = 1;
                            goto done;
                        }
                        driving = 1;
                        race_start_pending = 0;
                        menu_enter(mn, menu_active_root(mn) < 0 ? 0 : menu_active_root(mn));
                        menu_tick(mn);
                    } else {
                        race_game_stop(game);
                        driving = 0;
                        race_start_pending = 0;
                        loading_screen_end();
                    }
                    plat_video_set_view(640, 480);
                    presentation_clock = plat_time_ns();
                }
                continue;
            }
            if (race_loading) {
                if (ev.key == SDLK_ESCAPE)
                    race_loading = -1;
                continue;
            }
            if (race_active) {
                if (ev.key == SDLK_ESCAPE) {
                    loading_screen_begin();
                    present_wait(mn, menu_loading_duration(mn), NULL);
                    race_view_clear(race);
                    loading_screen_end();
                    race_active = 0;
                    last_root = -1;
                    plat_video_set_view(640, 480);
                    presentation_clock = plat_time_ns();
                }
                continue;
            }
            if (driving == 5 && (ev.key == SDLK_ESCAPE || (ev.ps2key == 0xffff && !ev.release)) &&
                !strcmp(ui_root_name(menu_ui(mn), menu_active_root(mn)), "ShootRoot")) {
                photo_travel_drive_end(presentation_photo);
                proj = photo_return_project;
                photo_return_project = -1;
                mn = cache[proj];
                p = menu_ui(mn);
                root = menu_active_root(mn);
                driving = 2;
                race_game_photo_pause(game, 0);
                plat_video_set_view(640, 448);
                continue;
            }
            if (driving == 5 && proj == 14 && (ev.key == SDLK_F5 || ev.key == SDLK_RETURN)) {
                if (!cache[21])
                    cache[21] = load(vol, 21);
                if (cache[21] && photo_travel_drive_preview(presentation_photo, cache[21])) {
                    proj = 21;
                    mn = cache[21];
                    p = menu_ui(mn);
                    root = menu_active_root(mn);
                }
                continue;
            }
            if (driving == 5 && proj == 21 && ev.key == SDLK_ESCAPE) {
                photo_travel_drive_editor(presentation_photo);
                proj = 14;
                mn = cache[14];
                p = menu_ui(mn);
                root = menu_active_root(mn);
                continue;
            }
            /* The PhotoPause editor polls its own controls. Hidden PhotoTravel
               widgets must not receive directional or tool events. */
            if (driving == 5 && proj == 14)
                continue;
            if (proj == 7 && save_ui_event(&save_screen, ev.key, ev.text))
                continue;
            if (photo_project(proj) && ev.key == SDLK_F5) {
                photo_travel_shoot(presentation_photo);
                continue;
            }
            if (mn) {
                int car_page = !strcmp(ui_root_name(menu_ui(mn), menu_active_root(mn)), "CarRoot");
                if (car_page && ev.key == SDLK_F5) {
                    const char *car = menu_selected_car(mn), *course = menu_selected_course(mn);
                    if (car && *car && course && *course) {
                        snprintf(race_car, sizeof race_car, "%s", car);
                        snprintf(race_course, sizeof race_course, "%s", course);
                        if (race_view_request(race, race_course, race_car,
                                              showroom_color_index(cars))) {
                            loading_screen_begin();
                            race_loading = 1;
                        }
                    } else
                        present_dialog(mn, "Info", "Escolha uma pista e um carro primeiro.", 0,
                                       NULL, NULL);
                    continue;
                }
                if (ev.key == SDLK_UP)
                    menu_send_input(mn, MENU_UP);
                if (ev.key == SDLK_DOWN)
                    menu_send_input(mn, MENU_DOWN);
                if (ev.key == SDLK_LEFT)
                    menu_send_input(mn, MENU_LEFT);
                if (ev.key == SDLK_RIGHT)
                    menu_send_input(mn, MENU_RIGHT);
                if (ev.key == SDLK_RETURN || ev.key == SDLK_KP_ENTER)
                    menu_send_input(mn, MENU_ACCEPT);
                if (ev.key == SDLK_ESCAPE) {
                    int at_top = menu_active_root(mn) >= 0 &&
                                 !strcmp(ui_root_name(menu_ui(mn), menu_active_root(mn)),
                                         "TopRoot");
                    if (proj == 0 && at_top)
                        menu_test_activate(mn, "QuitGame");
                    else if (proj == 1 && at_top) {
                        int answer = present_dialog(mn, "Exit Gran Turismo Mode?",
                                                    "Exit Gran Turismo Mode?", 1, "Yes", "No");
                        fprintf(stderr, "gtmode: exit confirm result=%d\n", answer);
                        if (answer == 1) {
                            char route[] = "boot";
                            switch_to(vol, cache, &mn, &proj, route);
                            p = menu_ui(mn);
                            root = menu_active_root(mn);
                        }
                        continue;
                    } else
                        menu_send_input(mn, ev.pad ? MENU_CIRCLE : MENU_BACK);
                }
                if (ev.pad && ev.key == SDLK_INSERT)
                    menu_send_input(mn, MENU_SQUARE);
                /* P: F4 maps the original pad Triangle event for Home/Setting. */
                if (ev.key == SDLK_F4)
                    menu_send_input(mn, MENU_TRIANGLE);
                /* L1, L2, R1, R2, Square, Select: the scripts' own keys */
                if (ev.ps2key)
                    menu_send_key(mn, ev.ps2key, ev.release);
            }
            if (p && (ev.key == SDLK_F1 || ev.key == SDLK_F2)) {
                root = (root + (ev.key == SDLK_F2 ? 1 : ui_root_count(p) - 1)) % ui_root_count(p);
                menu_enter(mn, root);
            }
            if (ev.key == SDLK_PAGEDOWN || ev.key == SDLK_PAGEUP) {
                proj = (proj + (ev.key == SDLK_PAGEDOWN ? 1 : NPROJECTS - 1)) % NPROJECTS;
                if (!cache[proj])
                    cache[proj] = load(vol, proj);
                mn = cache[proj];
                p = mn ? menu_ui(mn) : NULL;
                root = 0;
                if (mn)
                    menu_enter(mn, root);
            }
        }
        video_profile_bucket(VIDEO_BUCKET_EVENTS, plat_time_ns() - event_start);
        if (mn && menu_take_quit(mn))
            goto done;
        /* The driving options' OK (doGame -> finish, single race / time
           trial): the race on the chosen course with the chosen car and
           paint, against the AI. */
        if (mn && !race_loading && !race_active && !driving && menu_take_race_request(mn)) {
            if (proj == 1 && menu_race_is_mission(mn)) {
                /* 0011BB60: the mission from RACE/RACES (0011BEC0), RaceMission,
                   then quick-mission (0011BF60) */
                gt4_career_license_exam mission;
                char code[32], msg[256] = "Cannot read mission request.";
                int ok = menu_get_mission_request(mn, code) &&
                         gt4_career_mission_prepare(&mission, argv[1], gt4_game_race_lookup_data(),
                                                    code, msg, sizeof msg);
                if (ok) {
                    race_time_trial = 0;
                    free_mode = 0;
                    race_game_set_time_trial(game, 0);
                    mission.desc.manual = menu_driving_manual(mn);
                    ok = race_game_start_prepared(game, &mission.desc, msg, sizeof msg);
                }
                if (ok) {
                    menu_quick_race q;
                    ok = race_game_quick_grid(game, &q);
                    if (ok) {
                        career_license.active = 0;
                        career_race.active = 1;
                        career_race.mission = 1;
                        career_race.championship = 0;
                        career_race.result_applied = 0;
                        career_race.has_distances = 0;
                        career_race.retired = 0;
                        career_race.round = 0;
                        career_race.rounds = 0;
                        career_race.best = 15;
                        career_race.award = 0;
                        memcpy(career_race.row, mission.row, sizeof career_race.row);
                        snprintf(career_race.race, sizeof career_race.race, "%s", code);
                        snprintf(q.race, sizeof q.race, "%s", code);
                        snprintf(race_course, sizeof race_course, "%s", q.course);
                        snprintf(race_car, sizeof race_car, "%s", q.grid[q.player].car);
                        race_color = q.grid[q.player].color;
                        race_others_n = q.entries - 1;
                        menu_save_capture(mn);
                        menu_set_quick_race(&q);
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 0;
                        char route[] = "quick-mission:MissionRoot";
                        switch_to(vol, cache, &mn, &proj, route);
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                    } else
                        snprintf(msg, sizeof msg, "Cannot read the mission's grid.");
                }
                if (!ok) {
                    fprintf(stderr, "mission: %s\n", msg);
                    present_dialog(mn, "Info", msg, 0, NULL, NULL);
                }
            } else if (proj == 1 && menu_race_is_license(mn)) {
                gt4_career_license_exam exam;
                char code[32], msg[256] = "Cannot read licence request.";
                int ok = menu_get_license_request(mn, code) &&
                         gt4_career_license_prepare(&exam, argv[1], gt4_game_race_lookup_data(),
                                                    code, msg, sizeof msg);
                if (ok) {
                    race_time_trial = 0;
                    free_mode = 0;
                    race_game_set_time_trial(game, 0);
                    exam.desc.manual = menu_driving_manual(mn);
                    ok = race_game_start_prepared(game, &exam.desc, msg, sizeof msg);
                }
                if (ok) {
                    menu_quick_race q;
                    ok = race_game_quick_grid(game, &q) &&
                         menu_license_begin(mn, code, &career_license.visit);
                    if (ok) {
                        career_race.active = 0;
                        career_license.active = 1;
                        career_license.result_applied = 0;
                        snprintf(career_license.code, sizeof career_license.code, "%s", code);
                        career_license.best_time = menu_license_best(mn, code);
                        snprintf(q.race, sizeof q.race, "%s", code);
                        menu_set_quick_race(&q);
                        menu_set_quick_best(career_license.best_time);
                        snprintf(race_course, sizeof race_course, "%s", q.course);
                        snprintf(race_car, sizeof race_car, "%s", q.grid[0].car);
                        race_color = q.grid[0].color;
                        race_others_n = 0;
                        menu_save_capture(mn);
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 0;
                        char route[] = "quick-license:LicenseRoot";
                        switch_to(vol, cache, &mn, &proj, route);
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                    }
                }
                if (!ok) {
                    /* PC integration errors after race construction may not
                       supply text; never open an empty information dialog. */
                    if (!*msg)
                        snprintf(msg, sizeof msg,
                                 "Cannot read the licence's prepared car or record.");
                    fprintf(stderr, "licence: %s\n", msg);
                    present_dialog(mn, "Info", msg, 0, NULL, NULL);
                }
            } else if (proj == 1 && !menu_race_is_free(mn)) {
                menu_career_request request;
                gt4_career_event event;
                char msg[256];
                int ok = menu_get_career_request(mn, &request, msg, sizeof msg);
                char first_round[32];
                const char *event_label = request.race;
                unsigned rounds = 0, restored_round = 0;
                uint8_t suspended[0x90] = {0};
                int resume = 0;
                if (ok && request.championship) {
                    ok = menu_championship_context_get(mn, suspended);
                    resume = ok && suspended[0x28] != 0;
                    if (resume) {
                        memcpy(request.race, suspended + 8, 32);
                        request.race[31] = 0;
                        restored_round = suspended[0x29];
                    }
                    if (ok)
                        ok = gt4_career_championship_label(request.race, restored_round,
                                                           first_round, &rounds);
                    event_label = first_round;
                    if (!ok)
                        snprintf(msg, sizeof msg, "Invalid original championship label.");
                }
                if (ok && !career_race.rng_ready) {
                    career_race.rng_ready = gt4_career_event_rng_load(argv[1], &career_race.rng);
                    ok = career_race.rng_ready;
                    if (!ok)
                        snprintf(msg, sizeof msg, "Cannot read original event RNG state.");
                }
                if (ok) {
                    if (resume)
                        ok = gt4_career_championship_restore(
                            &event, argv[1], gt4_game_race_lookup_data(), suspended,
                            request.player_body, &career_race.rng, msg, sizeof msg);
                    else
                        ok = gt4_career_event_prepare(&event, argv[1], gt4_game_race_lookup_data(),
                                                      event_label, request.player_body,
                                                      request.championship, &career_race.rng, msg,
                                                      sizeof msg);
                }
                if (ok) {
                    race_time_trial = 0;
                    free_mode = 0;
                    race_game_set_time_trial(game, 0);
                    event.desc.manual = menu_driving_manual(mn);
                    race_game_set_career(game, mn);
                    ok = race_game_start_prepared(game, &event.desc, msg, sizeof msg);
                    /* 00114250 +F4 / 00119918 +174: licence availability
                       becomes options+970, read by the pit menu 003C12A8. */
                    if (ok)
                        race_game_set_pit_mode_change(game,
                                                      menu_career_aspec_available(mn, event_label));
                }
                if (ok) {
                    menu_quick_race q;
                    ok = race_game_quick_grid(game, &q);
                    if (ok) {
                        career_race.active = 1;
                        career_race.mission = 0;
                        career_race.result_applied = 0;
                        career_race.has_distances = 0;
                        career_race.championship = request.championship;
                        career_race.retired = 0;
                        career_race.round = restored_round;
                        career_race.rounds = rounds;
                        career_race.grand_prize = 0;
                        memset(career_race.points, 0, sizeof career_race.points);
                        snprintf(career_race.series, sizeof career_race.series, "%s", request.race);
                        if (request.championship) {
                            if (!race_game_championship_grid(game, &career_race.championship_grid,
                                                             career_race.bodies)) {
                                exit_code = 1;
                                goto done;
                            }
                            career_race.championship_grid.event_rng = &career_race.rng;
                            q.championship = 1;
                            q.session_number = (int)restored_round;
                            q.final_session = restored_round + 1 == rounds;
                            if (resume)
                                for (unsigned j = 0; j < 6; j++)
                                    memcpy(career_race.points + j, suspended + 0x3c + j * 16, 4);
                            if (!race_game_quick_points(game, &q, career_race.points)) {
                                exit_code = 1;
                                goto done;
                            }
                        }
                        /* 00113CB4..13CB8: best position belongs to the
                           event sequence, persists over its retries. */
                        career_race.best = 15;
                        career_race.award = 0;
                        memcpy(career_race.row, event.race_row, sizeof career_race.row);
                        snprintf(career_race.race, sizeof career_race.race, "%s", event_label);
                        snprintf(q.race, sizeof q.race, "%s", event_label);
                        snprintf(race_course, sizeof race_course, "%s", q.course);
                        snprintf(race_car, sizeof race_car, "%s", q.grid[q.player].car);
                        race_color = q.grid[q.player].color;
                        race_others_n = q.entries - 1;
                        menu_save_capture(mn);
                        menu_set_quick_race(&q);
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 0;
                        char quick[64];
                        snprintf(quick, sizeof quick, "%s",
                                 request.championship ? "quick-championship:ChampionshipRoot"
                                                      : "quick-event:RaceEventRoot");
                        switch_to(vol, cache, &mn, &proj, quick);
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                    } else
                        snprintf(msg, sizeof msg, "Cannot read the prepared grid.");
                }
                if (!ok) {
                    fprintf(stderr, "career event: %s\n", msg);
                    present_dialog(mn, "Info", msg, 0, NULL, NULL);
                }
            } else {
                career_race.active = 0;
                career_license.active = 0;
                /* GT Mode's Free Run (00117668: world "freerun", quick-freerun)
               and Practice (00118670: quick-practice): the riding car on the
               course chosen, through the Arcade's own build */
                int free = proj == 1 ? menu_race_is_free(mn) : 0;
                int split = free ? 0 : menu_race_is_split(mn);
                const char *car = menu_split_car(mn, 0), *course = menu_selected_course(mn);
                uint8_t split_bodies[2][0x4b8];
                int split_owned[2] = {0}, split_colors[2] = {0};
                char free_course[32], free_car[96];
                int free_laps = 0;
                if (free) {
                    if (!menu_free_request(mn, free_course, free_car, &split_colors[0],
                                           split_bodies[0], &free_laps)) {
                        present_dialog(mn, "Info", "Cannot read the riding car.", 0, NULL, NULL);
                        continue;
                    }
                    split_owned[0] = 1;
                    car = free_car;
                    course = free_course;
                } else {
                    for (int i = 0; i < (split ? 2 : 1); i++)
                        split_owned[i] = menu_split_owned(mn, i, split_bodies[i], &split_colors[i]);
                    if (split_owned[0] < 0 || split_owned[1] < 0) {
                        present_dialog(mn, "Info", "Cannot read the selected garage car.", 0, NULL,
                                       NULL);
                        continue;
                    }
                }
                /* the opponents the scripts chose (set_enemy_cars:
               MCarData.PickUpEnemyCars, game.setEntryCarCode) */
                const char *others[5];
                int other_colors[5];
                int field_size = race_field_size_request();
                /* Ask for the whole field's opponents. The menu's own six entries
                   yield at most five official opponents; the rest are phantoms. */
                int max_others = field_size - 1;
                int nothers = menu_race_entries(mn, others, other_colors, max_others);
                if (split) {
                    others[0] = menu_split_car(mn, 1);
                    other_colors[0] = split_colors[1];
                    nothers = 1;
                }
                if (split && (!others[0] || !*others[0])) {
                    fprintf(stderr, "split battle: missing second car selection\n");
                    continue;
                }
                fprintf(stderr, "race: %s on %s against", car ? car : "-", course ? course : "-");
                for (int i = 0; i < nothers; i++)
                    fprintf(stderr, " %s/%d", others[i], other_colors[i]);
                fputc('\n', stderr);
                if (car && *car && course && *course && game) {
                    char msg[256], quick[64];
                    race_time_trial = free ? free == 1 : menu_race_is_time_trial(mn);
                    free_mode = free;
                    if (race_time_trial || free == 3 || free == 4)
                        nothers = 0; /* quick-tt.ad L103; Photo Drive 0011B1CC: one entry. */
                    snprintf(quick, sizeof quick, "%s", race_quick_route(race_time_trial));
                    race_game_set_world_mode(game, free == 1   ? 11
                                                   : free == 2 ? 12
                                                   : free == 4 ? 15
                                                               : 0);
                    if (free == 4)
                        snprintf(
                            machine_race, sizeof machine_race, "%s",
                            menu_game_text(
                                mn,
                                "race_code")); /* 00115A90 -> 003BDD60: Power & Speed, mode 15 */
                    int tire = (int)menu_game_number(mn, "tire_idx", 0);
                    snprintf(race_car, sizeof race_car, "%s", car);
                    snprintf(race_course, sizeof race_course, "%s", course);
                    race_color =
                        (split || split_owned[0]) ? split_colors[0] : showroom_color_index(cars);
                    race_others_n = nothers;
                    for (int i = 0; i < nothers; i++) {
                        snprintf(race_others[i], sizeof race_others[i], "%s", others[i]);
                        race_other_colors[i] = other_colors[i];
                    }
                    gt4_race_desc split_options = {0};
                    if (split) {
                        for (int i = 0; i < 2; i++)
                            if (split_owned[i])
                                split_options.owned_car[i] = split_bodies[i];
                        split_options.tire_2p = (int)menu_game_number(mn, "tire_idx_2p", 0) + 1;
                        split_options.manual_2p = (int)menu_game_number(mn, "manual_2p", 0);
                        split_options.assist_asm_2p = (int)menu_game_number(mn, "assist_asm_2p", 0);
                        split_options.assist_tcs_2p = (int)menu_game_number(mn, "assist_tcs_2p", 0);
                        split_options.split_boost =
                            (int)menu_option_number(mn, "split_boost_level", 0);
                        split_options.split_handicap =
                            (int)menu_option_number(mn, "split_handicap", 0);
                        split_options.split_tire_damage =
                            (int)menu_option_number(mn, "split_tire_damage", 0);
                    }
                    race_game_set_split(game, split ? &split_options : NULL);
                    race_game_set_arcade_owned(game,
                                               !split && split_owned[0] ? split_bodies[0] : NULL);
                    race_game_set_laps(game,
                                       free == 2 || free == 4
                                           ? free_laps
                                           : (int)menu_option_number(
                                                 mn, split ? "split_race_laps" : "race_laps", 2));
                    race_game_set_time_trial(game, race_time_trial);
                    race_game_apply_units(game, mn);
                    /* the driving options' apply_setting: game.manual, assist_asm, assist_tcs, tire_idx */
                    race_game_set_driving(
                        game, tire,
                        proj == 1 ? menu_driving_manual(mn)
                                  : (int)menu_game_number(mn, "manual", 0),
                        (int)menu_game_number(mn, "assist_asm",
                                              menu_option_number(mn, "assist_asm", 1)),
                        (int)menu_game_number(mn, "assist_tcs",
                                              menu_option_number(mn, "assist_tcs", 1)));
                    race_game_quick_clear(game); /* 00111B50: a new race, no QuickTune */
                    int started;
                    if (free == 3) {
                        /* 0011AC54/0011ACB4: Photo Drive has the GT world/controller
                       (003EDE50), its own RACE row and one equipped garage car.
                       0011B288: its authored quick page is quick-freerun. */
                        gt4_career_event photo = {0};
                        uint8_t active[0x178];
                        started = menu_photo_drive_rules(mn, photo.race_row) &&
                                  gt4_career_active_setup(split_bodies[0], active);
                        if (!career_race.rng_ready)
                            career_race.rng_ready =
                                gt4_career_event_rng_load(argv[1], &career_race.rng);
                        started = started && career_race.rng_ready;
                        if (started) {
                            memcpy(photo.player_body, split_bodies[0], sizeof photo.player_body);
                            photo.desc.cars = 1;
                            memcpy(&photo.desc.car[0], active, 4);
                            snprintf(photo.desc.course, sizeof photo.desc.course, "%s",
                                     race_course);
                            photo.desc.owned_car[0] = photo.player_body;
                            photo.desc.event_row = photo.race_row;
                            photo.desc.event_rng = &career_race.rng;
                            photo.desc.manual = menu_driving_manual(mn);
                            photo.desc.assist_asm = (int)menu_game_number(mn, "assist_asm", 0);
                            photo.desc.assist_tcs = (int)menu_game_number(mn, "assist_tcs", 0);
                            started = race_game_start_prepared(game, &photo.desc, msg, sizeof msg);
                        } else
                            snprintf(msg, sizeof msg,
                                     "Cannot read Photo Drive rules or equipped car.");
                    } else {
                        race_game_set_field_size(race_phantoms_on() ? field_size : 0);
                        started =
                            race_game_start(game, race_course, race_car, race_color, race_other_ptr,
                                            race_other_colors, nothers, msg, sizeof msg);
                    }
                    if (started) {
                        /* the race sequence: its quick menu first (the grid), the race on its Start */
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = split;
                        if (!split) {
                            quick_race_set(game, race_course, race_car, race_color, race_other_ptr,
                                           race_other_colors, nothers, tire);
                            switch_to(vol, cache, &mn, &proj, quick);
                            p = menu_ui(mn);
                            root = menu_active_root(mn);
                        }
                    } else {
                        fprintf(stderr, "race: %s\n", msg);
                        present_dialog(mn, "Info",
                                       msg[0] ? msg : "Nao foi possivel montar esta corrida. Veja o log.",
                                       0, NULL, NULL);
                    }
                }
            }
        }
        /* --quick-input: one input a second, including its Options round trip. */
        { /* tests: MOON_LOGGER_INPUT=<keys> pressed in the Logger, one a second and a half */
            static const char *keys;
            static uint64_t since, last;
            if (!keys)
                keys = getenv("MOON_LOGGER_INPUT");
            if (keys && *keys && mn && (proj == 16 || (proj == 12 && logger_track >= 0))) {
                uint64_t t = plat_time_ns();
                if (!since)
                    since = t;
                if (t - since > 3000000000ull && t - last > 1500000000ull) {
                    char key[16];
                    size_t n = strcspn(keys, ",");
                    menu_input in;
                    snprintf(key, sizeof key, "%.*s", (int)(n < 15 ? n : 15), keys);
                    keys += n;
                    if (*keys == ',')
                        keys++;
                    if (input_name(key, &in)) {
                        menu_send_input(mn, in);
                        menu_tick(mn);
                        printf("logger input %s -> %s\n", key, menu_focus_name(mn));
                    }
                    last = t;
                }
            }
        }
        {
            static uint64_t quick_since, quick_last;
            if (quick_input && *quick_input && mn &&
                (race_quick_project(proj) || quick_options || proj == 12) && driving != 2) {
                uint64_t t = plat_time_ns();
                if (!quick_since)
                    quick_since = t;
                if (t - quick_since > 1500000000ull && t - quick_last > 1000000000ull) {
                    char key[16];
                    size_t n = strcspn(quick_input, ",");
                    menu_input in;
                    snprintf(key, sizeof key, "%.*s", (int)(n < 15 ? n : 15), quick_input);
                    quick_input += n;
                    if (*quick_input == ',')
                        quick_input++;
                    if (input_name(key, &in)) {
                        menu_send_input(mn, in);
                        menu_tick(mn);
                        printf("quick input %s -> %s\n", key, menu_focus_name(mn));
                    }
                    quick_last = t;
                }
            }
        }
        if (logger_load_asked && mn && race_quick_project(proj) &&
            ui_fade_remaining(menu_ui(mn)) <= 0 && menu_active_root(mn) >= 0) {
            logger_load_asked = 0;
            menu_test_quick_command(mn, "COMMAND_LOAD_GHOST");
        } /* the quick menu's own Load Ghost (0010FEE8: labomode's best laps) */
        { /* tests: MOON_ACTIVATE=<widget>[,<widget>...] chosen in turn, 4 s apart once a page is up */
            static uint64_t since;
            static const char *next;
            if (!next)
                next = getenv("MOON_ACTIVATE");
            if (next && *next && mn && menu_active_root(mn) >= 0 && !loading_screen_active()) {
                uint64_t t = plat_time_ns();
                if (!since)
                    since = t;
                if (t - since > 4000000000ull) {
                    char name[64];
                    size_t n = strcspn(next, ",");
                    snprintf(name, sizeof name, "%.*s", (int)(n < 63 ? n : 63), next);
                    next += n;
                    if (*next == ',')
                        next++;
                    since = t;
                    if (!strcmp(name, "BACK")) {
                        menu_send_input(mn, MENU_BACK);
                        fprintf(stderr, "activate BACK: ok\n");
                    } else
                        fprintf(stderr, "activate %s: %s\n", name,
                                menu_test_activate(mn, name) ? "ok" : "not found");
                }
            }
        }
        { /* tests: a quick menu command once the race is over and its menu back */
            static int sent;
            if (!sent && mn && race_quick_project(proj) && driving == 4 &&
                getenv("MOON_QUICK_COMMAND") && ui_fade_remaining(menu_ui(mn)) <= 0) {
                sent = 1;
                menu_test_quick_command(mn, getenv("MOON_QUICK_COMMAND"));
            }
        }
        /* The quick menu's command: Start races (the race loaded, or again) */
        {
            char command[32];
            if (mn && menu_take_quick_command(mn, command, sizeof command)) {
                fprintf(stderr, "quick menu: %s\n", command);
                int spectator = !strcmp(command, "COMMAND_SPECTATOR") && career_race.active &&
                                !career_race.mission && !career_license.active;
                if (!strcmp(command, "COMMAND_START") || spectator) {
                    /* quick-event.ad L363: Spectator is COMMAND_SPECTATOR (14 of
                       D_006187B0), the race B-Spec; Start is A-Spec. The race
                       built for the menu is rebuilt when that changes. */
                    int rebuild = game && race_game_set_bspec(game, spectator);
                    if (career_race.active && !career_race.mission && career_race.result_applied &&
                        !career_race.championship) {
                        if (!menu_commit_career_distances(cache[1], mn, career_race.a_distance,
                                                          career_race.b_distance,
                                                          &career_race.has_distances)) {
                            exit_code = 1;
                            goto done;
                        }
                        /* 00113DC8/00113EC8 save the live wear before another
                           attempt; the native restart constructs a new race
                           and must receive this updated owned body. */
                        uint8_t body[0x4b8];
                        if (!menu_riding_car_body(cache[1], body) ||
                            !race_game_setting(game, body)) {
                            exit_code = 1;
                            goto done;
                        }
                    }
                    if (career_license.active)
                        career_license.result_applied = 0;
                    if (career_race.active) {
                        if (career_race.championship && career_race.result_applied)
                            continue;
                        career_race.result_applied = 0;
                        career_race.award = 0;
                        menu_quick_event_result(mn, 0, 0);
                    }
                    /* 00111498 -> 00111DC0: the Arcade's QuickTune into the
                       player's entry; the race built for the grid is rebuilt
                       to take it (the PS2 writes the live entry before its
                       cars start) */
                    /* 003D6350 reads the current option on GT Start;
                       Options may have changed it after the grid loaded. */
                    if ((career_race.active || career_license.active || free_mode) &&
                        race_game_set_transmission(game, menu_driving_manual(mn)))
                        rebuild = 1;
                    menu_quick_tune_values tune;
                    if (!career_race.active && !career_license.active && game && race_car[0] &&
                        menu_quick_tune(mn, &tune) &&
                        race_game_quick_tune(game, tune.tuned, tune.power, tune.weight,
                                             tune.front_tire, tune.rear_tire, tune.gear,
                                             tune.transmission, tune.assist) &&
                        (driving == 3 || driving == 1)) {
                        char msg[256];
                        fprintf(
                            stderr,
                            "quick tune: tuned %d power %+d%% weight %+d%% tyres %d/%d gear %d manual %d aids %d\n",
                            tune.tuned, tune.power, tune.weight, tune.front_tire, tune.rear_tire,
                            tune.gear, tune.transmission, tune.assist);
                        if (race_game_restart(game, msg, sizeof msg)) {
                            loading_screen_begin();
                            driving = 1;
                            race_start_pending = 1;
                        } else
                            fprintf(stderr, "race: %s\n", msg);
                        continue;
                    }
                    if (driving == 3 && race_game_license_active(game)) {
                        char msg[256];
                        if (race_game_restart(game, msg, sizeof msg)) {
                            loading_screen_begin();
                            driving = 1;
                            race_start_pending = 1;
                        } else
                            fprintf(stderr, "race: %s\n", msg);
                    } else if (driving == 3 && !rebuild) {
                        driving = 2;
                        plat_video_set_view(640, 448);
                        presentation_clock = plat_time_ns();
                    } else if (driving == 1 && !rebuild)
                        race_start_pending = 1;
                    else if ((!driving || driving == 4 || rebuild) && game && race_car[0]) {
                        char msg[256];
                        if (race_game_restart(game, msg, sizeof msg)) {
                            loading_screen_begin();
                            driving = 1;
                            race_start_pending = 1;
                        } else
                            fprintf(stderr, "race: %s\n", msg);
                    }
                } else if (!strcmp(command, "COMMAND_NEXT_RACE") && career_license.active) {
                    char next[32], msg[256];
                    gt4_career_license_exam exam;
                    if (!gt4_career_license_next(career_license.code, next)) {
                        char destination[] = "gtmode";
                        switch_to(vol, cache, &mn, &proj, destination);
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                    } else {
                        /* 0011D020 -> 00110800 -> 0010FCD0: leave the old
                           visit before constructing the next examination. */
                        menu_save_capture(mn);
                        menu_save_refresh(cache[1]);
                        int ok = gt4_career_license_prepare(
                            &exam, argv[1], gt4_game_race_lookup_data(), next, msg, sizeof msg);
                        if (ok) {
                            exam.desc.manual = menu_driving_manual(cache[1]);
                            ok = race_game_start_prepared(game, &exam.desc, msg, sizeof msg);
                        }
                        menu_quick_race q;
                        if (ok)
                            ok = race_game_quick_grid(game, &q);
                        if (ok)
                            ok = menu_license_leave(cache[1], career_license.code,
                                                    &career_license.visit);
                        if (ok) {
                            career_license.active = 0;
                            ok = menu_license_begin(cache[1], next, &career_license.visit);
                        }
                        if (ok)
                            career_license.active = 1;
                        if (ok) {
                            career_license.result_applied = 0;
                            snprintf(career_license.code, sizeof career_license.code, "%s", next);
                            career_license.best_time = menu_license_best(cache[1], next);
                            snprintf(q.race, sizeof q.race, "%s", next);
                            menu_set_quick_race(&q);
                            menu_set_quick_best(career_license.best_time);
                            snprintf(race_course, sizeof race_course, "%s", q.course);
                            snprintf(race_car, sizeof race_car, "%s", q.grid[0].car);
                            race_color = q.grid[0].color;
                            menu_save_refresh(mn);
                            menu_quick_event_context(mn, next);
                            menu_enter_quick_start(mn);
                            loading_screen_begin();
                            driving = 1;
                            race_start_pending = 0;
                        } else {
                            fprintf(stderr, "licence next exam failed: %s\n", msg);
                            present_dialog(mn, "Info", "Cannot load the next licence examination.",
                                           0, NULL, NULL);
                        }
                    }
                } else if (!strcmp(command, "COMMAND_NEXT_RACE") && career_race.active &&
                           career_race.championship) {
                    char msg[256], label[32];
                    gt4_career_event next;
                    unsigned rounds;
                    if (!menu_commit_career_distances(cache[1], mn, career_race.a_distance,
                                                      career_race.b_distance,
                                                      &career_race.has_distances)) {
                        exit_code = 1;
                        goto done;
                    }
                    int ok = career_race.result_applied &&
                             gt4_career_championship_label(career_race.series,
                                                           career_race.round + 1, label, &rounds);
                    menu_save_capture(mn);
                    menu_save_refresh(cache[1]);
                    /* 003EF088 keeps entrants; only the player's owned body
                       receives the wear saved after the preceding attempt. */
                    menu_career_request equipped;
                    if (ok)
                        ok = menu_get_career_request(cache[1], &equipped, msg, sizeof msg);
                    if (ok) {
                        memcpy(career_race.bodies[0], equipped.player_body, 0x4b8);
                        ok = gt4_career_championship_round(
                            &next, argv[1], gt4_game_race_lookup_data(), label,
                            &career_race.championship_grid, msg, sizeof msg);
                    }
                    if (ok) {
                        next.desc.manual = menu_driving_manual(mn);
                        race_game_set_career(game, cache[1]);
                        ok = race_game_start_prepared(game, &next.desc, msg, sizeof msg);
                    }
                    if (ok)
                        race_game_set_pit_mode_change(game,
                                                      menu_career_aspec_available(cache[1], label));
                    if (ok) {
                        menu_quick_race q;
                        ok = race_game_quick_grid(game, &q);
                        if (ok) {
                            career_race.round++;
                            career_race.result_applied = 0;
                            career_race.award = 0;
                            career_race.grand_prize = 0;
                            memcpy(career_race.row, next.race_row, 151);
                            snprintf(career_race.race, sizeof career_race.race, "%s", label);
                            q.championship = 1;
                            q.session_number = (int)career_race.round;
                            q.final_session = career_race.round + 1 == career_race.rounds;
                            if (!race_game_quick_points(game, &q, career_race.points)) {
                                exit_code = 1;
                                goto done;
                            }
                            snprintf(q.race, sizeof q.race, "%s", label);
                            snprintf(race_course, sizeof race_course, "%s", q.course);
                            menu_set_quick_race(&q);
                            menu_quick_event_context(mn, label);
                            menu_quick_event_result(mn, 0, 0);
                            menu_enter_quick_start(mn);
                            loading_screen_begin();
                            driving = 1;
                            race_start_pending = 0;
                        }
                    }
                    if (!ok) {
                        fprintf(stderr, "championship next round failed\n");
                        present_dialog(mn, "Info", "Cannot load the next championship round.", 0,
                                       NULL, NULL);
                    }
                } else if (!strcmp(command, "COMMAND_RETIRE_CHAMPIONSHIP") && career_race.active &&
                           career_race.championship) {
                    career_race.retired = 1;
                    char destination[] = "gtmode";
                    switch_to(vol, cache, &mn, &proj, destination);
                    p = menu_ui(mn);
                    root = menu_active_root(mn);
                } else if (!strcmp(command, "COMMAND_DEMONSTRATION") &&
                           race_game_license_active(game) &&
                           (driving == 3 || driving == 4 || !driving)) {
                    char msg[256];
                    if (race_game_license_demo(game, NULL, msg, sizeof msg)) {
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 1;
                    } else {
                        fprintf(stderr, "demonstration: %s\n", msg);
                        present_dialog(mn, "Demonstration",
                                       "Cannot read the licence demonstration from the disc.", 0,
                                       NULL, NULL);
                    }
                } else if (!strcmp(command, "COMMAND_REPLAY") && (!driving || driving == 4) &&
                           game && race_game_can_replay(game)) {
                    /* the last race again from its commands (race_game_replay) */
                    char msg[256];
                    {
                        const char *rm = menu_option_text(mn, "replay_mode", "NORMAL");
                        race_game_set_replay_options(
                            game, menu_option_number(mn, "replay_display_enable", 0) != 0,
                            rm && !strcmp(rm, "DIVE"));
                    }
                    if (race_game_replay(game, msg, sizeof msg)) {
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 1;
                    } else
                        fprintf(stderr, "replay: %s\n", msg);
                } else if (!strcmp(command, "COMMAND_SAVE_REPLAY") && game &&
                           race_game_can_replay(game)) {
                    /* the race lent to the memory card's save screen (its files
                       the save folder's, replay_file.h, the PC layer) */
                    gt4_replay_file r;
                    char msg[160];
                    int ok = race_game_replay_export(game, &r, GT4_REPLAY_FULL);
                    if (ok) { /* the names a list shows, the quick menu's own (CourseName, CAR_NAME) */
                        const char *cn = ui_translate(
                                       menu_ui(mn),
                                       menu_active_root(mn) < 0 ? 0 : menu_active_root(mn),
                                       "CourseName", r.course),
                                   *car = menu_car_name(mn, r.car);
                        snprintf(r.course_name, sizeof r.course_name, "%s",
                                 cn && *cn && strcmp(cn, r.course) ? cn : "");
                        snprintf(r.car_name, sizeof r.car_name, "%s", car ? car : "");
                    }
                    if (ok) { /* labomode's SaveReplayRoot (MODE_SAVE_FULL_REPLAY), back to this menu */
                        replay_to_save = r;
                        menu_set_replay_to_save(&replay_to_save);
                        menu_memcard_return(projects[proj]);
                        char labo[] = "labomode";
                        switch_to(vol, cache, &mn, &proj, labo);
                        if (proj == 12 && !menu_open_labo(mn, "MODE_SAVE_FULL_REPLAY"))
                            fprintf(stderr, "labomode: save screen failed\n");
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                    } else {
                        if (!race_game_replay_refusal(game, msg, sizeof msg)) {
                            snprintf(msg, sizeof msg, "The replay could not be saved.");
                            fprintf(stderr, "replay: %s\n", msg);
                        }
                        present_dialog(mn, "Save Replay", msg, 0, NULL, NULL);
                    }
                } else if ((!strcmp(command, "COMMAND_SAVE_GHOST") ||
                            !strcmp(command, "COMMAND_LOAD_GHOST")) &&
                           game) {
                    /* quick-tt.ad SaveGhost / LoadGhost: labomode's SaveReplayRoot /
                       LoadReplayRoot in the best lap's modes, back to this menu;
                       a loaded one is taken as the ghost below */
                    int save = !strcmp(command, "COMMAND_SAVE_GHOST"), ready = 1;
                    if (save) {
                        gt4_replay_file r;
                        ready = race_game_ghost_export(game, &r);
                        if (ready) {
                            const char *cn = ui_translate(
                                           menu_ui(mn),
                                           menu_active_root(mn) < 0 ? 0 : menu_active_root(mn),
                                           "CourseName", r.course),
                                       *car = menu_car_name(mn, r.car);
                            snprintf(r.course_name, sizeof r.course_name, "%s",
                                     cn && *cn && strcmp(cn, r.course) ? cn : "");
                            snprintf(r.car_name, sizeof r.car_name, "%s", car ? car : "");
                            replay_to_save = r;
                            menu_set_replay_to_save(&replay_to_save);
                        } else
                            present_dialog(mn, "Save Ghost", "There is no lap to save.", 0, NULL,
                                           NULL);
                    }
                    if (ready) {
                        menu_memcard_return(projects[proj]);
                        char labo[] = "labomode";
                        switch_to(vol, cache, &mn, &proj, labo);
                        if (proj == 12 && !menu_open_labo(mn, save ? "MODE_SAVE_BESTLAP_REPLAY"
                                                                   : "MODE_LOAD_BESTLAP_REPLAY"))
                            fprintf(stderr, "labomode: ghost screen failed\n");
                        p = menu_ui(mn);
                        root = menu_active_root(mn);
                    }
                } else if (!strcmp(command, "COMMAND_SETTINGS")) {
                    quick_options_project = proj;
                    char setting[] = "setting";
                    quick_options = 2;
                    switch_to(vol, cache, &mn, &proj, setting);
                    p = menu_ui(mn);
                    root = menu_active_root(mn);
                } else if (!strcmp(command, "COMMAND_LOGGER") && game) {
                    /* quick-tt.ad L483 / quick-license.ad: the Logger
                       project over the last race; its exit returns here
                       (logger.ad exit: setReturnToLastMenuProject) */
                    menu_logger_source src;
                    char amsg[160];
                    if (getenv("MOON_LOGGER_ANALYZE") &&
                        !race_game_logger_analyze(game, 1, amsg, sizeof amsg))
                        fprintf(stderr, "logger: %s\n", amsg); /* tests: the analysis by replay */
                    if (race_game_logger_source(game, &src)) {
                        if (career_license.active && career_license.result_applied &&
                            (src.time[0] == 0 || src.time[0] == 0x157529ffu))
                            src.time[0] = career_license.last_time;
                        menu_set_logger_source(&src);
                    } else
                        menu_set_logger_source(NULL);
                    logger_game = game;
                    menu_set_logger_analyzer(NULL, logger_analysis_begin, logger_analysis_step,
                                             logger_analysis_end);
                    char logger[] = "logger:LoggerRoot";
                    switch_to(vol, cache, &mn, &proj, logger);
                    p = menu_ui(mn);
                    root = menu_active_root(mn);
                } else if (!strcmp(command, "COMMAND_OPTION")) {
                    /* quick-arcade.ad L472 returns to the native race caller;
                       the option project must finish back at this quick menu. */
                    quick_options_project = proj;
                    char option[] = "option";
                    quick_options = 1;
                    switch_to(vol, cache, &mn, &proj, option);
                    p = menu_ui(mn);
                    root = menu_active_root(mn);
                }
            }
        }
        if (quick_options && (returning_quick_options || (proj != 3 && proj != 4))) {
            returning_quick_options = 0;
            race_game_apply_units(game, mn);
            if (quick_options == 2 && !race_game_license_active(game)) {
                uint8_t body[0x4b8];
                char msg[256];
                if (menu_riding_car_body(mn, body) && race_game_setting(game, body)) {
                    /* Preserve the completed attempt/replay; the next Start
                       rebuilds it. A pre-grid scene must use the new sheet. */
                    if (race_game_finished(game) < 0) {
                        if (!race_game_restart(game, msg, sizeof msg)) {
                            fprintf(stderr, "cannot apply garage setting: %s\n", msg);
                            exit_code = 1;
                            goto done;
                        }
                        int32_t stats[3];
                        if (gt4_career_vehicle_stats(app_car_factory, body, stats))
                            menu_update_quick_player((int32_t)((float)stats[0] / 100.f + .5f),
                                                     stats[2]);
                        loading_screen_begin();
                        driving = 1;
                        race_start_pending = 0;
                    }
                }
            }
            /* quick-championship.ad getStartPage L22..39 and quick-event:
               Option returns through the selector (AfterRoot if finished),
               keeping the cursorPosition saved by the original button. */
            char quick[64];
            snprintf(quick, sizeof quick, "%s",
                     quick_options_project >= 0 ? projects[quick_options_project]
                     : career_license.active    ? "quick-license"
                     : career_race.active       ? (career_race.mission        ? "quick-mission"
                                                   : career_race.championship ? "quick-championship"
                                                                              : "quick-event")
                                                : race_quick_route(race_time_trial));
            quick_options = 0;
            quick_options_project = -1;
            pregrid_clock = 0;
            switch_to(vol, cache, &mn, &proj, quick);
            p = menu_ui(mn);
            root = menu_active_root(mn);
        }
        if (driving && !race_quick_project(proj) && proj != 12 && proj != 16 && !quick_options &&
            !race_start_pending && driving != 2 &&
            driving != 5) { /* the quick menu left (Exit): no race */
            if (career_race.active && career_race.has_distances) {
                if (!menu_commit_career_distances(cache[1], mn, career_race.a_distance,
                                                  career_race.b_distance,
                                                  &career_race.has_distances)) {
                    exit_code = 1;
                    goto done;
                }
            }
            career_race.active = 0;
            race_game_stop(game);
            driving = 0;
            loading_screen_end();
        }
        uint64_t now = plat_time_ns();
        double elapsed = (now - presentation_clock) / 1e9;
        presentation_clock = now;
        /* P: opt-in offline film export. Every rendered frame advances one
           existing 60 Hz step; no physics formulas, parameters or tick rate
           change. Export can take longer than its playback on a busy GPU. */
        if (video_export_fixed_step())
            elapsed = 1.0 / 60.0;
        float notice_level = 0;
        static int logged_notice = -2;
        if (notice < GT4_NOTICE_COUNT) {
            if (notice != logged_notice) {
                app_startup_log("screen: boot notice %s",
                                notice == GT4_NOTICE_LEGAL ? "legal" : "presents");
                logged_notice = notice;
            }
            if (!notice_start)
                notice_start = now;
            notice_level = gt4_boot_notice_level(notice, (now - notice_start) / 1e9);
            while (notice < GT4_NOTICE_COUNT && (notice_level < 0 || !notice_tex[notice])) {
                if (++notice < GT4_NOTICE_COUNT) {
                    notice_start = now;
                    notice_level = gt4_boot_notice_level(notice, 0);
                }
            }
            if (notice == GT4_NOTICE_COUNT) {
                plat_video_set_view(640, 480);
                elapsed = 0;
                last_root = -1;
            }
        }
        int notice_active = notice < GT4_NOTICE_COUNT;
        if (!poll_race_loading(&race_flow, &elapsed))
            goto done;
        custom_modes_tick(mn);
        { /* Custom Modes: the tile's Elimination entry starts the 12-car race. */
            custom_modes_race_request cm;
            if (custom_modes_take_race_request(&cm)) {
                static const char *const cm_others[11] = {
                    "_1100_standard_type11_49", "z_act_70", "today_g_85", "midget2_d_type_98",
                    "s500_63", "_1100_standard_type11_49", "z_act_70", "today_g_85",
                    "midget2_d_type_98", "s500_63", "_1100_standard_type11_49"};
                char msg[256];
                int nothers = cm.field_size > 1 ? cm.field_size - 1 : 1;
                if (nothers > 11)
                    nothers = 11;
                race_game_set_field_size(cm.field_size);
                if (game && cm.car[0] &&
                    race_game_start(game, cm.course, cm.car, cm.color, cm_others, NULL, nothers,
                                    msg, sizeof msg)) {
                    race_game_set_elimination(game, 1, cm.thunder);
                    snprintf(race_course, sizeof race_course, "%s", cm.course);
                    snprintf(race_car, sizeof race_car, "%s", cm.car);
                    loading_screen_begin();
                    driving = 1;
                    race_start_pending = 1;
                    race_active = 0;
                    race_loading = 0;
                    fprintf(stderr, "custom modes: race starting course=%s car=%s\n", cm.course,
                            cm.car);
                } else {
                    fprintf(stderr, "custom modes: race start failed: %s\n", msg);
                    present_dialog(mn, "Info", msg[0] ? msg : "Cannot start the custom race.", 0,
                                   NULL, NULL);
                }
            }
        }
        if (mn && loading_screen_revealing() && !race_active && !race_loading && driving != 2 &&
            !notice_active)
            animate_loading_destination(mn, (float)elapsed);
        if (mn && !loading_screen_active() && !race_active && !race_loading && driving != 2 &&
            !notice_active) {
            if (proj == 16) {
                /* the Logger's graph reads the sticks (001206A0: left x/y the
                   cursor and the scroll, right x/y the zooms). P: the arrows
                   are the left stick, Page Up/Down and Home/End the right */
                float axes[4] = {0};
                plat_pad_state pad;
                if (plat_input_poll(0, &pad))
                    for (int k = 0; k < 4; k++)
                        axes[k] = pad.axes[k];
                const Uint8 *k = SDL_GetKeyboardState(NULL);
                axes[0] += (float)(k[SDL_SCANCODE_RIGHT] - k[SDL_SCANCODE_LEFT]);
                axes[1] += (float)(k[SDL_SCANCODE_DOWN] - k[SDL_SCANCODE_UP]);
                axes[2] += (float)(k[SDL_SCANCODE_PAGEUP] - k[SDL_SCANCODE_PAGEDOWN]);
                axes[3] += (float)(k[SDL_SCANCODE_HOME] - k[SDL_SCANCODE_END]);
                menu_set_sticks(mn, axes);
            }
            /* Asset/device stalls must not spend six native cursor steps in
               one displayed frame. The 60 Hz motion itself is unchanged. */
#ifndef __SWITCH__
            if(settings.current.desktop)
                ui_pointer_edge_scroll(menu_ui(mn),menu_active_root(mn),(float)fmin(elapsed,1.0/30.0));
            menu_edge_pan(mn, settings.current.desktop && settings.current.edge_pan,
                          (float)fmin(elapsed,1.0/30.0));
            ui_main_menu_layout(menu_ui(mn),settings.current.main_menu);
            ui_pointer_smoothing(menu_ui(mn),settings.current.smooth_cursor);
            ui_pointer_feedback(menu_ui(mn),settings.current.hover_feedback);
            ui_menu_readability(menu_ui(mn),settings.current.compact_cars);
#endif
            SWP_VOID(SWP_LOGIC, menu_advance(mn, fmin(elapsed, 1.0 / 30.0)));
            if (proj == 7) {
                save_ui_transfer_tools(&save_screen,settings.current.save_tools);
                save_ui_update(&save_screen);
            }
            char film_path[1200];
            if (proj == 12 && menu_take_photo_film(mn, film_path, sizeof film_path)) {
                int preview = 0;
                while (preview < NPROJECTS && strcmp(projects[preview], "photo_save"))
                    preview++;
                if (preview < NPROJECTS && !cache[preview])
                    cache[preview] = load(vol, preview);
                if (preview < NPROJECTS && cache[preview] &&
                    photo_travel_develop(presentation_photo, cache[preview], film_path,
                                         pump_loading_work, NULL)) {
                    photo_return_project = 12;
                    proj = preview;
                    mn = cache[preview];
                    driving = 6;
                    plat_video_set_view(640, 480);
                } else {
                    fprintf(stderr, "Photo film: cannot develop %s\n", film_path);
                    menu_open_labo(mn, "PhotoRoot");
                }
            } else if (driving == 6) {
                char destination[64];
                if (menu_take_project(mn, destination, sizeof destination)) {
                    photo_travel_drive_end(presentation_photo);
                    proj = photo_return_project;
                    photo_return_project = -1;
                    mn = cache[proj];
                    driving = 0;
                    menu_open_labo(mn, "PhotoRoot");
                }
            } else if (driving == 5) {
                /* PhotoShoot Exit uses the authored confirmation/sequence.
                   Restore the held replay instead of entering its major menu. */
                char destination[64];
                if (menu_take_project(mn, destination, sizeof destination)) {
                    if (proj == 21) {
                        photo_travel_drive_editor(presentation_photo);
                        proj = 14;
                        mn = cache[14];
                    } else {
                        photo_travel_drive_end(presentation_photo);
                        proj = photo_return_project;
                        photo_return_project = -1;
                        mn = cache[proj];
                        driving = 2;
                        race_game_photo_pause(game, 0);
                        plat_video_set_view(640, 448);
                        presentation_clock = plat_time_ns();
                    }
                }
            } else if (quick_options && (proj == 3 || proj == 4)) {
                /* 0010FE40 quick loop: Option returns to its native caller.
                   Consume the authored major-menu route without opening it. */
                char destination[64];
                if (menu_take_project(mn, destination, sizeof destination))
                    returning_quick_options = 1;
            } else
                switch_requested(vol, cache, &mn, &proj);
            p = menu_ui(mn);
            root = menu_active_root(mn);
            if (race_quick_project(proj) && game) {
                /* 0010FEE8: the Logger left with a track to load: labomode's
                   best laps for this course, then the Logger again */
                int track = menu_take_logger_load();
                if (track >= 0) {
                    /* the quick menu's own Load Ghost, once it is back */
                    logger_track = track;
                    logger_load_asked = 1;
                }
            }
        }
        /* Music requests belong to the destination page; MAP belongs to GT Mode. */
        const char *page = p ? ui_root_name(p, root) : "";
        SWP_CONTEXT(projects[proj], page, menu_selected_course(mn), menu_selected_car(mn), driving);
        int pcm_volume = 0;
        const char *pcm_group = menu_pcm_request(mn, &pcm_volume);
        int wants_pcm = (!shot || music_with_shot) && !notice_active && !race_active &&
                        driving != 2 && !loading_screen_active() && pcm_group && *pcm_group &&
                        strcmp(page, "MovieRoot") && !(music_faded && plat_sdl_music_fading()) &&
                        !app_strike_state.active;
        int prepare_pcm = (!shot || music_with_shot) && !notice_active && !race_active &&
                          driving != 2 && pcm_group && *pcm_group && strcmp(page, "MovieRoot") &&
                          !app_strike_state.active;
        if (!prepare_pcm)
            pregrid_audio_update(pregrid_music, NULL, 0, 0);
        if (!strcmp(page, "MovieRoot") && (root != movie_page_root || proj != movie_page_project)) {
            if (movie.lock) {
                SDL_LockMutex(movie.lock);
                movie.name[0] = 0;
                SDL_UnlockMutex(movie.lock);
            }
        }
        movie_page_root = root;
        movie_page_project = proj;
        {
            int car_page = !race_active && driving != 2 && !strcmp(page, "CarRoot");
            if (car_page && !was_car_page)
                showroom_hide(cars);
            was_car_page = car_page;
        }
        /* A project's mSound stops its music when the project goes
           (destructor 0023E078 -> 0023F890 -> 002C3BA0/00460B90): the
           option project, with no bgm on its pages, is silent (PCSX2). */
        {
            static int music_project = -1;
            if (proj != music_project) {
                if (music_project >= 0)
                    desired_music[0] = 0;
                music_project = proj;
            }
        }
        const char *request = mn ? menu_music_name(mn) : NULL;
        if (request)
            snprintf(desired_music, sizeof desired_music, "%s", request);
        int music_top = proj == 0 && !strcmp(page, "TopRoot");
        /* TopRoot authors bgm="keep", not a named title playlist. MAP must
           not leak across GT Mode exit; a title-specific source is unverified. */
        if (music_top)
            desired_music[0] = 0;
        menu_audio_state.owner = mn;
        menu_audio_state.allowed = !notice_active && !race_active && driving != 2 &&
                                   !race_quick_project(proj) && !app_strike_state.active;
        update_present_midi(mn);
        if (app_jukebox_pcm(mn, wants_pcm)) {
            pregrid_audio_update(pregrid_music, NULL, 0, 0);
        } else {
        if (prepare_pcm) {
            int sequence, repeat;
            unsigned serial;
            menu_pcm_parameters(mn, &sequence, &repeat, &serial);
            pregrid_audio_update_gated(pregrid_music, pcm_group, pcm_volume,
                                       (uint32_t)plat_time_ns(), sequence, repeat, serial,
                                       wants_pcm);
        }
        pregrid_audio_pause(pregrid_music, menu_pcm_paused(mn));
        menu_pcm_state(mn, wants_pcm && pregrid_audio_playing(pregrid_music));
        }
        /* ResultLicense/ResultArcade advance per VBlank (003DF978 +B0),
           including failure colour decay. The mission shares this clock. */
        video_set_menu_pacing(
            (!race_active && driving != 2) ||
            (race_active && (race_game_license_active(game) || race_game_mission_active(game))));
        video_set_menu_projection(!race_active && driving != 2);
        if ((bench_frames > 0 || bench_throughput_seconds > 0) && !bench_started && driving == 2) {
            bench_started = 1;
            bench_freeze = 1;
            if (bench_frames > 0)
                bench_times = malloc((size_t)bench_frames * sizeof *bench_times);
            video_set_bench_finish(bench_frames > 0 ? 1 : 0);
            video_bench_reset();
            if (bench_throughput_seconds > 0) {
                bench_throughput_cpu = bench_cpu_seconds();
                bench_throughput_wall = bench_wall_seconds();
#ifdef MOON_PC_VULKAN
                moon_vk_render_stats vk = moon_vk_render_statistics();
                bench_throughput_gpu = vk.gpu_ns;
                bench_throughput_submits = vk.submissions;
#endif
                fprintf(stderr, "bench: start throughput seconds=%.2f\n", bench_throughput_seconds);
            } else
                fprintf(stderr, "bench: start frames=%d\n", bench_frames);
        }
        presentation_pregrid = (driving == 3 || driving == 4) && race_quick_project(proj);
        if (!presentation_pregrid)
            pregrid_clock = 0;
        uint64_t bench_frame_start = bench_started ? plat_time_ns() : 0;
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_begin_frame();
        }
        SWP_PUSH(SWP_SCENE);
        int car_ready = 1;
        int scene_layer = race_active || driving == 2 ||
                          (!notice_active && mn && page_below_popup(mn, "CarRoot"));
        if (scene_layer)
            video_begin_scene();
        if (driving == 2) {
            if (!update_and_draw_race(&race_flow, elapsed, &car_ready))
                goto done;
        } else if (race_active) {
            const uint8_t *keys = SDL_GetKeyboardState(NULL);
            float dt = (float)fmin(elapsed, .05);
            race_view_camera(race, (keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT]) * dt,
                             (keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP]) * dt * 3);
            car_ready = race_view_draw(race);
        } else if (notice_active) {
            /* Centered in the 640x448 frame and faded from black, as the
               boot executable (0x01000A48) and CORE (0x001091B8) draw them. */
            const tex1_image *im = gt4_boot_notice_image(&notices, notice);
            uint8_t v = (uint8_t)(255 * notice_level + .5f), tint[4] = {v, v, v, 255};
            plat_video_set_view(640, 448);
            plat_draw_quad(notice_tex[notice], (float)((640 - im->width) >> 1),
                           (float)((448 - im->height) >> 1), (float)im->width, (float)im->height, 0,
                           0, 1, 1, tint);
        } else if (mn && page_below_popup(mn, "CarRoot")) {
            /* Arcade's cars are stock: no wheel or wing left by a GT Mode car or the launcher */
            showroom_set_wheel(cars, NULL);
            showroom_set_wing(cars, NULL);
            car_ready = showroom_draw(cars, menu_selected_car(mn), (float)elapsed);
            if (!color_preview)
                showroom_set_color(cars, menu_car_color(mn)); /* the script's colour chips */
            if (car_ready && color_preview && !color_applied) {
                showroom_color(cars, color_preview);
                color_applied = 1;
                car_ready = 0;
            }
        }
        if (scene_layer)
            {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "scenetarget")))
                video_end_scene();
        }
        app_strike_poll(game, &mn, elapsed, &driving, &race_start_pending, &last_root);
        app_fail_poll(game, &mn, &driving, &race_start_pending, &last_root);
        if (driving == 2 && race_game_wants_thumbnail(
                                game)) { /* the race's picture for a saved replay, before its HUD */
            int tw, th;
            unsigned char *px = plat_sdl_read_view(&tw, &th);
            if (px) {
                race_game_set_thumbnail(game, px, tw, th);
                free(px);
            }
        }
        if (driving == 2) {
            if (race_game_fail_active(game))
                race_game_draw_fail(game, elapsed);
            else
                race_game_draw_hud(game, elapsed);
        }
#if !defined(__SWITCH__) && !defined(__ANDROID__)
        if (race_graphics_swap && driving != 2) { /* the race is over: back to the player's */
            race_graphics_swap = 0;
            race_graphics_swap_shown = 0;
            video_settings_preview_graphics(&settings, 0);
        }
        { /* test hook: one real F7 press after this many seconds of race */
            static uint64_t swap_race_start;
            static int swap_injected;
            const char *after = getenv("MOON_TEST_GRAPHICS_SWAP_AFTER");
            if (driving != 2)
                swap_race_start = 0;
            else if (after && !swap_injected) {
                if (!swap_race_start)
                    swap_race_start = plat_time_ns();
                if ((plat_time_ns() - swap_race_start) / 1e9 >= atof(after)) {
                    SDL_Event key = {0};
                    key.type = SDL_KEYDOWN;
                    key.key.keysym.sym = SDLK_F7;
                    SDL_PushEvent(&key);
                    swap_injected = 1;
                }
            }
        }
        { /* test hook: a real L press after this many seconds of the race,
             repeated once a second until the cut actually happens */
            static uint64_t strike_race_start;
            static int strike_injected;
            const char *after = getenv("MOON_STRIKE_TEST_AFTER");
            if (driving != 2)
                strike_race_start = 0;
            else if (after && !strike_injected && !app_strike_state.active) {
                if (!strike_race_start)
                    strike_race_start = plat_time_ns();
                if ((plat_time_ns() - strike_race_start) / 1e9 >= atof(after)) {
                    SDL_Event key = {0};
                    key.type = SDL_KEYDOWN;
                    key.key.keysym.sym = SDLK_l;
                    SDL_PushEvent(&key);
                    strike_race_start = plat_time_ns(); /* try again next second */
                    fprintf(stderr, "custom modes: strike test key injected\n");
                }
            }
        }
        if (driving == 2 && race_graphics_swap_shown) {
            double age = (plat_time_ns() - race_graphics_swap_shown) / 1e9;
            if (age < 2.0 && wiz_font_init()) {
                float a = age < 1.5 ? 1.f : (float)((2.0 - age) / .5);
                const char *t = race_graphics_swap ? "CLASSIC GRAPHICS" : "YOUR GRAPHICS";
                const float sc = .36f, tw = wiz_text_width(t, sc) + wiz_text_width("   F7", .3f);
                const uint8_t box[4] = {0, 0, 0, (uint8_t)(170 * a)},
                              ink[4] = {255, 255, 255, (uint8_t)(255 * a)},
                              soft[4] = {190, 193, 199, (uint8_t)(230 * a)};
                plat_video_set_view(640, 480);
                plat_draw_quad(NULL, 320 - tw / 2 - 14, 28, tw + 28, 34, 0, 0, 1, 1, box);
                wiz_text(t, 320 - tw / 2, 51, sc, ink);
                wiz_text("   F7", 320 - tw / 2 + wiz_text_width(t, sc), 51, .3f, soft);
            } else if (age >= 2.0)
                race_graphics_swap_shown = 0;
        }
#endif
#if defined(__SWITCH__) || defined(__ANDROID__)
        if (driving == 2 && settings.current.touch_menu && race_game_is_paused(game) &&
            !touch_overlay_hidden(mn, NULL)) {
            plat_video_set_view(640, 480);
            ui_draw_touch_back(dialog_ui, ui_touch_active(dialog_ui),
                               settings.touch_back.tap.selected != NULL, (float)elapsed);
        }
#endif
#ifdef __ANDROID__
        if (driving == 2 && !race_game_is_paused(game) && !touch_overlay_hidden(mn, NULL))
            moon_android_overlay(menu_ui(mn));
#endif
        int drawn = 0, skipped = 0;
        movie.seen = movie.drawn = 0;
#if !defined(__SWITCH__) && !defined(__ANDROID__)
        video_settings_scroll_tick();
#endif
        if (p && !race_active && driving != 2 && !notice_active) {
            ui_set_movie_renderer(p, draw_movie, &movie);
            if (driving == 5 && proj == 14)
                photo_travel_editor_draw(presentation_photo);
            else
                draw_pages(mn, &drawn, &skipped);
        }
        music_download_draw();
        if ((photo_project(proj) || driving == 6 || (driving == 5 && proj == 21)) && !notice_active)
            photo_travel_feedback(presentation_photo);
        { /* P: opt-in filming input, the same shutter path as F5. A real
               Photo Travel render/preview; no replacement image or UI. */
            static uint64_t photo_film_started;
            static int photo_film_shot, photo_film_focused;
            const char *after = getenv("MOON_PHOTO_SHOOT_AFTER");
            if (after && photo_project(proj) && !loading_screen_active() && !photo_film_shot) {
                uint64_t now = plat_time_ns();
                if (!photo_film_started)
                    photo_film_started = now;
                /* P: photographer's framing for this recording only. Aim at
                   the body, then let the ordinary centre-depth AF sample it
                   on the next frame before pressing the shutter. */
                if (!photo_film_focused && (now - photo_film_started) / 1e9 >= atof(after) - 1) {
                    gt4_photo_mode *s = menu_photo_state(mn);
                    float dx = s->car[0] - s->eye[0], dy = s->car[1] + .65f - s->eye[1],
                          dz = s->car[2] - s->eye[2];
                    s->yaw = atan2f(dx, -dz) * 57.2957763671875f;
                    s->pitch = -atan2f(dy, sqrtf(dx * dx + dz * dz)) * 57.2957763671875f;
                    s->manual_focus = 0;
                    s->focus_requested = 1;
                    photo_film_focused = 1;
                }
                if ((now - photo_film_started) / 1e9 >= atof(after)) {
                    photo_travel_shoot(presentation_photo);
                    photo_film_shot = 1;
                }
            }
        }
        if (presentation_pregrid && p && !race_active && driving != 2 && !notice_active)
            race_game_draw_pregrid_overlay(presentation_race);
        /* A: cached PC decoder outlives its page. Original project teardown
           stops sound (0023E078 -> 0023F890); never leak the movie bus into
           race/replay. Resume is handled by draw_movie above. */
        if (!movie.seen && movie.lock) {
            SDL_LockMutex(movie.lock);
            sdl_movie_audio(movie.decoder, 0);
            sdl_movie_audio(movie.previous, 0);
            /* Request a fresh decoder on re-entry; the stopped decoder's
               audio_attempted flag cannot start its sound a second time. */
            movie.name[0] = 0;
            SDL_UnlockMutex(movie.lock);
        }
        if (loading_screen_active()) {
            loading_screen_draw();
            if (loading_shot && loading_screen_ready()) {
                screenshot(loading_shot);
                loading_shot = NULL;
            }
        }
        if (loading_screen_revealing() && getenv("MOON_LOADING_REVEAL_SHOT")) {
            static unsigned reveal_frames;
            if (++reveal_frames == 5)
                screenshot(getenv("MOON_LOADING_REVEAL_SHOT"));
        }
        if (p && !race_active && driving != 2 && movie.seen &&
            movie.finish_notified != movie.generation &&
            (movie.unavailable || (movie.decoder && movie.displayed == movie.generation &&
                                   sdl_movie_finished(movie.decoder)))) {
            movie.finish_notified = movie.generation;
            printf("movie: %s finished after %llu frames\n", movie.name,
                   (unsigned long long)sdl_movie_frame_count(movie.decoder));
            sdl_movie_audio(movie.decoder, 0);
            if (menu_movie_finished(mn)) {
                menu_tick(mn);
                switch_requested(vol, cache, &mn, &proj);
            }
        }
        /* Retain the last frame/decoder while changing pages; worker replaces it. */
        if (shot &&
            ((!loading_preview && loading_screen_active()) ||
             (loading_preview && !loading_screen_ready()) || race_loading ||
             (driving == 1 && race_start_pending) || (movie.seen && !movie.drawn) || !car_ready)) {
            if (!shot_wait)
                shot_wait = plat_time_ns();
            if (plat_time_ns() - shot_wait < ((race_preview || !strncmp(movie.name, "opening", 7))
                                                  ? 30000000000ull
                                                  : 5000000000ull)) {
                SWP_POP();
                {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
                SDL_Delay(10);
                continue;
            }
            fprintf(stderr, "presentation: timed out waiting for requested asset\n");
            exit_code = 1;
        } else if (shot)
            shot_wait = 0; /* each movie transition gets a fresh loading deadline */
        /* G6.40: the frame's last drawing — the debug overlay, over launcher,
           menus and race alike. Its one hook in this loop. */
        if (driving != 2)
            moon_debug_overlay_set_race(-1, 0, 0);
        moon_debug_overlay_draw();
        app_strike_draw();
        { /* dev: the strike sequence's own pictures, taken from its frames */
            const char *prefix = getenv("MOON_STRIKE_SHOT");
            static int shot_impact, shot_peak, shot_black, shot_fade, shot_menu;
            if (prefix) {
                char path[1200];
                int take = 0;
                const char *stage = NULL;
                double t = app_strike_state.t;
                if (app_strike_state.active && !shot_impact && !app_strike_state.cut_done &&
                    t > .10 && t < .34) {
                    stage = "thunder";
                    shot_impact = take = 1;
                } else if (app_strike_state.active && !shot_peak && !app_strike_state.cut_done &&
                           t >= .20 && t < .30) {
                    stage = "peak";
                    shot_peak = take = 1;
                } else if (app_strike_state.active && !shot_black && app_strike_state.cut_done &&
                           t < STRIKE_THUNDER_S + STRIKE_HOLD_S) {
                    stage = "black";
                    shot_black = take = 1;
                } else if (app_strike_state.active && !shot_fade &&
                           t >= STRIKE_THUNDER_S + STRIKE_HOLD_S + .55 && t < STRIKE_TOTAL_S) {
                    stage = "fade";
                    shot_fade = take = 1;
                } else if (!app_strike_state.active && shot_fade && !shot_menu &&
                           app_strike_state.serial) {
                    stage = "menu";
                    shot_menu = take = 1;
                }
                if (take && stage) {
                    snprintf(path, sizeof path, "%s-%s.png", prefix, stage);
                    screenshot(path);
                    printf("strike shot: %s\n", path);
                }
            }
        }
        int shot_due = shot != NULL && !app_jukebox_fixture(mn);
        if (shot && shot_delay > 0) {
            if (!shot_started)
                shot_started = plat_time_ns();
            /* Capture delay must not skip the result/retry/menu transitions
               below. Only the screenshot waits, not the application's flow. */
            shot_due = (plat_time_ns() - shot_started) / 1e9 >= shot_delay;
        }
        if (shot_due) {
            printf(
                "video settings: filter=%d text=%d vsync=%d fullscreen=%d size=%dx%d upscaling=%d aspect=%d\n",
                settings.current.filter, settings.current.text, settings.current.video.vsync,
                settings.current.video.fullscreen, settings.current.video.width,
                settings.current.video.height, settings.current.video.upscale,
                settings.current.aspect);
            if (race_active)
                printf("race preview: course=%s car=%s\n", race_course, race_car);
            if (!strcmp(page, "CarRoot"))
                printf("car color=%s\n", showroom_caption(cars));
            if (movie.decoder)
                printf("movie frames=%llu\n",
                       (unsigned long long)sdl_movie_frame_count(movie.decoder));
            printf("%s / %s: drawn %d, not yet drawn %d\n", projects[proj],
                   p ? ui_root_name(p, root) : "?", drawn, skipped);
            const char *car = menu_selected_car(mn);
            if (car)
                printf("selected car=%s\n", car);
            const char *course = menu_selected_course(mn);
            if (course)
                printf("selected course=%s\n", course);
            if (p)
                report_choices(ui_root_widget(p, root));
            if (p && getenv("MOON_UI_TREE"))
                report_tree(ui_root_widget(p, root), 0);
            screenshot(shot);
            goto done;
        }
        SWP_READY(car_ready && !loading_screen_active() && !race_loading);
        SWP_POP();
        uint64_t bench_gpu_before = bench_started ? video_bench_finish_ns() : 0;
        {
            const char *sk = getenv("MOON_STUDIO_SKIP");
            if (!(sk && strstr(sk, "frame")))
                plat_video_end_frame();
        }
        if (bench_started && bench_frame_start && bench_frames > 0) {
            bench_frame_index++;
            double gpu_frame = (video_bench_finish_ns() - bench_gpu_before) / 1e6;
            if (bench_frame_index > 30) {
                if (bench_count == 0)
                    bench_gpu_first = gpu_frame;
                bench_gpu_last = gpu_frame;
                bench_times[bench_count++] = (plat_time_ns() - bench_frame_start) / 1e6;
            }
            if (bench_count >= bench_frames) {
                double sum = 0, min = bench_times[0], max = bench_times[0];
                for (int bi = 0; bi < bench_count; bi++) {
                    sum += bench_times[bi];
                    if (bench_times[bi] < min) min = bench_times[bi];
                    if (bench_times[bi] > max) max = bench_times[bi];
                }
                for (int bi = 1; bi < bench_count; bi++) {
                    double key = bench_times[bi];
                    int bj = bi - 1;
                    while (bj >= 0 && bench_times[bj] > key) {
                        bench_times[bj + 1] = bench_times[bj];
                        bj--;
                    }
                    bench_times[bj + 1] = key;
                }
                int p10i = (bench_count - 1) * 10 / 100, p90i = (bench_count - 1) * 90 / 100;
                double median = bench_count & 1 ? bench_times[bench_count / 2]
                                                : .5 * (bench_times[bench_count / 2 - 1] +
                                                        bench_times[bench_count / 2]);
                fprintf(stderr,
                        "bench: frames=%d p10=%.3f median=%.3f p90=%.3f min=%.3f max=%.3f "
                        "draws=%u gpu_finish=%.3f gpu_first=%.3f gpu_last=%.3f\n",
                        bench_count, bench_times[p10i], median, bench_times[p90i], min, max,
                        r3d_frame_stats().draws,
                        (double)video_bench_finish_ns() / 1e6 / bench_frame_index,
                        bench_gpu_first, bench_gpu_last);
                if (median > 0 && (bench_times[p90i] - bench_times[p10i]) / median > .05)
                    fprintf(stderr, "bench: spread=%.1f%% note=clock/swapchain/caches still vary\n",
                            100.0 * (bench_times[p90i] - bench_times[p10i]) / median);
                goto done;
            }
        }
        if (bench_throughput_seconds > 0 && bench_started) {
            bench_throughput_frames++;
            double wall_now = bench_wall_seconds();
            double elapsed = wall_now - bench_throughput_wall;
            if (elapsed >= bench_throughput_seconds) {
                double cpu_now = bench_cpu_seconds();
                double cpu_ms = (cpu_now - bench_throughput_cpu) * 1000.0 / bench_throughput_frames;
                double gpu_ms = 0;
                uint64_t submits = bench_throughput_submits;
#ifdef MOON_PC_VULKAN
                moon_vk_render_stats vk = moon_vk_render_statistics();
                gpu_ms = (vk.gpu_ns - bench_throughput_gpu) / 1e6;
                submits = vk.submissions - bench_throughput_submits;
#endif
                fprintf(stderr,
                        "throughput: frames=%llu seconds=%.3f fps=%.1f cpu_per_frame=%.3f "
                        "gpu_total=%.3f submits=%llu waits=0\n",
                        (unsigned long long)bench_throughput_frames, elapsed,
                        bench_throughput_frames / elapsed, cpu_ms, gpu_ms,
                        (unsigned long long)submits);
                goto done;
            }
        }
        /* Discord presence: one cheap check a second, never in the race step. */
        if ((driving == 2 || driving == 5) && race_course[0]) {
            const char *mode = race_game_replaying(game)      ? "Replay"
                               : race_game_license_active(game) ? "Licence"
                               : race_game_mission_active(game) ? "Mission"
                               : career_race.active             ? "GT Mode"
                               : race_time_trial                ? "Time Trial"
                                                                : "Arcade";
            app_presence_poll(mode, race_course, mn, race_car, driving, proj);
        } else {
            app_presence_poll("In the menus", NULL, mn, race_car[0] ? race_car : NULL, driving, proj);
        }
        /* the results over: the race's replay plays by itself from its
           opening, then the quick menu (as the game goes on) */
        if (race_return && career_license.active && race_game_license_retry(game)) {
            char msg[256];
            race_return = 0;
            career_license.result_applied = 0;
            if (race_game_restart(game, msg, sizeof msg)) {
                loading_screen_begin();
                driving = 1;
                race_start_pending = 1;
            } else {
                fprintf(stderr, "licence retry: %s\n", msg);
                exit_code = 1;
                goto done;
            }
        }
        /* 0011BB60 returns RaceMission to 0011BF60's QuickWork loop;
           replay is an explicit menu command, not Arcade's automatic replay. */
        if (race_return && !career_license.active && !career_race.mission &&
            !race_game_replaying(game) && race_game_can_replay(game)) {
            char msg[256];
            race_return = 0;
            {
                const char *rm = menu_option_text(mn, "replay_mode", "NORMAL");
                race_game_set_replay_options(
                    game, menu_option_number(mn, "replay_display_enable", 0) != 0,
                    rm && !strcmp(rm, "DIVE"));
            }
            if (race_game_replay(game, msg, sizeof msg)) {
                loading_screen_begin();
                driving = 1;
                race_start_pending = 1;
            } else {
                fprintf(stderr, "replay: %s\n", msg);
                race_return = 1;
            }
        }
        if (race_return) {
            loading_screen_begin();
            present_wait(mn, menu_loading_duration(mn), NULL);
            race_game_return_pregrid(game);
            driving = 4;
            pregrid_clock = 0;
            race_return = 0;
            last_root = -1;
            menu_set_quick_completed(mn, 1);
            if (career_license.active) {
                /* 0010FBD8 rebuilds quick-license after the examination.
                   Re-run its authored BestTime/ranking/button initialization. */
                menu_quick_event_context(mn, career_license.code);
                menu_enter_quick_start(mn);
            } else
                menu_enter_quick_start(mn);
            /* quick-event.ad L899 queues doAction: dispatch only after the
               loading overlay has finished, from the normal menu loop. */
            ui_prepare_root(menu_ui(mn), menu_active_root(mn), pump_loading_work, NULL);
            loading_screen_end();
            plat_video_set_view(640, 480);
            presentation_clock = plat_time_ns();
            ui_fade(menu_ui(mn), 1, 0, .2f);
        }
        /* The window keeps its product title; the page goes to the log. */
        if (!(driving == 2 || (driving == 1 && race_start_pending)) && !race_active &&
            !race_loading && (root != last_root || proj != last_proj)) {
            char title[256];
            snprintf(title, sizeof title, "MOON ENGINE%s - %s / %s (%d/%d)  drawn %d, not yet drawn %d",
                     !strcmp(page, "CarRoot") ? " [F5: carro na pista]"
                     : preview_courses        ? " [Catalog preview]"
                                              : "",
                     projects[proj], p ? ui_root_name(p, root) : "?", root + 1,
                     p ? ui_root_count(p) : 0, drawn, skipped);
            printf("%s\n", title);
            app_startup_log("menu: %s / %s", projects[proj],
                            p ? ui_root_name(p, root) : "?");
            last_root = root;
            last_proj = proj;
        }
    }
done:
    discord_presence_shutdown();
    photo_travel_close(presentation_photo);
    presentation_photo = NULL;
    race_game_close(game);
    game = NULL; /* audio writes its jukebox back before the final profile save */
    if (mn) {
        menu_save_capture(mn);
        if (gt4_save_dirty() && gt4_save_auto_enabled() && gt4_save_active() &&
            !present_profile_access_screen(vol, 0))
            fprintf(stderr, "Auto Save: %s\n", gt4_save_error());
    }
    gt4_save_shutdown();
    gt4_game_set_stats_reader(NULL, NULL);
    gt4_game_set_service_reader(NULL, NULL);
    gt4_game_set_setting_reader(NULL, NULL);
    for (int i = 0; i < 2; i++) {
        showroom_close(split_selection_cars[i]);
        split_selection_cars[i] = NULL;
    }
    showroom_close(cars);
    race_view_close(race);
    gt4_car_ratings_close(app_car_ratings);
    gt4_game_set_car_factory(NULL, NULL);
    gt4_game_set_auto_gears(NULL, NULL);
    gt4_career_vehicle_close(app_car_factory);
    for (int i = 0; i < GT4_NOTICE_COUNT; i++)
        plat_texture_destroy(notice_tex[i]);
    gt4_boot_notices_close(&notices);
    if (movie.thread) {
        SDL_LockMutex(movie.lock);
        movie.stop = 1;
        SDL_CondSignal(movie.wake);
        SDL_UnlockMutex(movie.lock);
        SDL_WaitThread(movie.thread, NULL);
    }
    plat_texture_destroy(movie.poster);
    free(movie.ipic_pixels);
    free(movie.ipic_data);
    sdl_movie_close(movie.previous);
    sdl_movie_close(movie.decoder);
    sdl_movie_close(movie.pending);
    sdl_movie_close(movie.retired);
    if (movie.wake)
        SDL_DestroyCond(movie.wake);
    if (movie.lock)
        SDL_DestroyMutex(movie.lock);
    gt4_vol_close(movie.volume);
    preparing_audio = NULL;
    app_jukebox_leave();
    pregrid_audio_close(pregrid_music);
    plat_audio_shutdown();
    music_library_install(NULL);music_library_close(app_music_library);app_music_library=NULL;
    for (int i = 0; i < music_count; i++) {
        if (music_load[i].thread)
            SDL_WaitThread(music_load[i].thread, NULL);
        if (music_load[i].playlist) {
            for (unsigned j = 0; j < music_load[i].count; j++)
                gt4_music_close(music_load[i].playlist[j]);
            free(music_load[i].playlist);
            free(music_load[i].track_ready);
            free(music_load[i].track_frames);
        } else
            gt4_music_close(music_load[i].music);
    }
    for (int i = 0; i < NPROJECTS; i++)
        menu_close(cache[i]);
    ui_project_free(dialog_ui);
    dialog_ui = NULL;
    loading_screen_close();
    plat_video_shutdown();
    gt4_vol_close(vol);
    return exit_code;
}
