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
#include <sys/stat.h>
#include "platform/switch_profile.h"
#include "platform/platform.h"
#include "plat_sdl.h"
#include "movie.h"
#include "showroom.h"
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
#ifdef __SWITCH__
#include "platform/switch_scene.h"
#endif
#include "desktop_input.h"
#include "launcher.h"
#ifndef __SWITCH__
#include "renderer_startup.h"
#include "platform/renderer_backend.h"
#endif
#include "save_ui.h"
#include "save.h"
#include "replay_file.h"
#include "career_event.h"
#include "career_license_disc.h"
#include "video_gl.h"
#include "rtext.h"
#include "font.h"
#include "gpb.h"
#include "tex1.h"
#include "glyph_sdf.h"
#include "launcher_fonts_data.h"
#include "../../third_party/stb/stb_truetype.h"

#include "miniz.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "main_support.inc"
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

/* ---- the dock: the launcher's buttons, drawn over the room ----
   Four of them, and their icons are the captain's picks from the disc's own UI art - the
   assets are read out of his own image at run time (never shipped, never in the repo), the
   names under them are drawn in the disc's own face, and the selection is the game's own
   pointer. No panel and no scrim: the icons and names sit straight on the room. */
#define DOCK_COUNT 4
#define DOCK_START 2 /* the START slot (dock_names[2]) */
static const char *const dock_names[DOCK_COUNT] = {"MODS", "OPTIONS", "START", "EXIT"};
typedef struct {
    const char *pkg;
    const char *asset;
    plat_texture *tex;
    int w, h;
} dock_icon;
static dock_icon dock_icons[DOCK_COUNT] = {
    {"/projects/gtmode/US/LicenseDetailRoot.gpb", "image/gtmode/license/coffee_gold.png", NULL, 0,
     0},
    {"/projects/gtmode/US/CarViewRoot.gpb", "image/gtmode/home/assessment.png", NULL, 0, 0},
    {"/projects/gtmode/US/CourseEntryRoot.gpb", "image/gtmode/course/icon_freerun.png", NULL, 0, 0},
    {"/projects/quick-arcade/US/ArcadeRoot.gpb", "image/quick/JP/quick_exit.png", NULL, 0, 0},
};
/* The selection: the game's own menu pointer - CursorRoot's cursor, the sprite the menus
   turn about its tip at (16,0) of the 32x32 image (ui.c ui_draw). Read off the disc like
   the icons. */
static dock_icon dock_arrow = {"/projects/cursor/US/CursorRoot.gpb", "image/cursor/cursor.png",
                               NULL, 0, 0};
/* One icon out of a package on the disc: the same decode gt4img uses. */
static plat_texture *dock_icon_load(gt4_vol *vol, const char *pkg_path, const char *asset,
                                    int *out_w, int *out_h)
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
    }
    tex1_free(im, count);
    return t;
}

/* The disc face the names are drawn in, and how big a name runs. */
#define DOCK_NAME_FACE "hm_16"
#define DOCK_NAME_SCALE 0.95f
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
} studio_dock;

/* One name in the disc's own face, white over a soft dark shadow so it reads on the bright
   room. The glyph quad is ui.c's own (gt4_font_glyph + plat_draw_glyph). */
static void dock_name_pass(const studio_dock *d, const char *text, float cx, float base, float dx,
                           float dy, const uint8_t rgba[4])
{
    const gt4_font *f = &d->font;
    const tex1_image *a = d->font_atlas;
    const float s = DOCK_NAME_SCALE * (d->scale > 0 ? d->scale : DOCK_SCALE);
    float x = cx - gt4_font_line_width(f, text, 0) * s / 2.f;
    for (const char *c = text; *c;) {
        unsigned code = gt4_text_next(&c);
        if (code < 32)
            continue;
        const gt4_glyph *g = gt4_font_glyph(f, code);
        if (!g)
            g = gt4_font_glyph(f, '?');
        if (!g || !(g->w && g->h))
            continue;
        plat_draw_glyph(d->font_tex, x + g->x * s + dx, base + g->y * s + dy, g->w * s, g->h * s,
                        base, 0, (float)g->u / a->width, (float)g->v / a->height,
                        (float)(g->u + g->w) / a->width, (float)(g->v + g->h) / a->height, rgba);
        x += g->advance * s;
    }
}
static void dock_name(const studio_dock *d, const char *text, float cx, float base,
                      const uint8_t col[4])
{
    if (!d->font_ok)
        return;
    static const uint8_t s1[4] = {6, 7, 9, 120}, s2[4] = {6, 7, 9, 70};
    float s = d->scale > 0 ? d->scale : DOCK_SCALE;
    dock_name_pass(d, text, cx, base, s, s, s1);
    dock_name_pass(d, text, cx, base, 2.f * s, 2.f * s, s2);
    dock_name_pass(d, text, cx, base, 0.f, 0.f, col);
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
#define DOCK_PAD 26.f
#define DOCK_BOX 76.f
#define DOCK_CURSOR_Y 416.f
#define DOCK_BOTTOM_MARGIN 28.f
#define DOCK_WIDTH 640.f
static const float dock_tilt[DOCK_COUNT] = {22.5f, 0.f, -22.5f, -45.f};
static float dock_slot(void)
{
    return (DOCK_WIDTH - DOCK_PAD * 2.f) / DOCK_COUNT;
}
/* The dock's pixel placement for the current framebuffer: centred, bottom-anchored. Its size
   follows the window height (anchored at the 1280x960 default, where it is DOCK_SCALE), so the
   buttons look the same on a 1080p and a 1440p window; it never overflows a narrow one. */
static void dock_metrics(studio_dock *d, int fw, int fh)
{
    float s = DOCK_SCALE * (float)fh / 960.f;
    float maxw = (float)fw - 2.f * 24.f;
    if (maxw < DOCK_WIDTH * s)
        s = maxw / DOCK_WIDTH;
    if (s < 0.4f)
        s = 0.4f;
    d->scale = s;
    d->ox = ((float)fw - DOCK_WIDTH * s) * .5f;
    d->oy = (float)fh - DOCK_BOTTOM_MARGIN * (s / DOCK_SCALE) - 448.f * s;
}
static void dock_cursor_target(studio_dock *d, int sel)
{
    if (sel < 0)
        sel = 0;
    if (sel >= DOCK_COUNT)
        sel = DOCK_COUNT - 1;
    d->ctx = DOCK_PAD + dock_slot() * sel + dock_slot() / 2.f;
    d->cty = DOCK_CURSOR_Y;
    d->cta = dock_tilt[sel];
}
/* Hovering a slot selects it, exactly as the arrow keys do. The dock is drawn in framebuffer
   pixels (view = framebuffer, fill mode), so the window point maps straight to a pixel and the
   hit-test is in the same coordinates as the drawing. */
static void dock_hover(studio_dock *d, int *sel, int mx, int my)
{
    SDL_Window *win = plat_sdl_window();
    int ww = 0, wh = 0, fw = 0, fh = 0;
    if (!win)
        return;
    SDL_GetWindowSize(win, &ww, &wh);
    SDL_GL_GetDrawableSize(win, &fw, &fh);
    if (ww < 1 || wh < 1)
        return;
    float vx = (float)mx * (fw > 0 ? fw : ww) / (float)ww,
          vy = (float)my * (fh > 0 ? fh : wh) / (float)wh;
    float px = (vx - d->ox) / (d->scale > 0 ? d->scale : DOCK_SCALE),
          py = (vy - d->oy) / (d->scale > 0 ? d->scale : DOCK_SCALE);
    if (py < 448.f - 120.f)
        return; /* only the dock band selects */
    const float slot = dock_slot();
    for (int i = 0; i < DOCK_COUNT; i++) {
        float cx = DOCK_PAD + slot * i + slot / 2.f;
        if (px >= cx - slot / 2.f && px < cx + slot / 2.f) {
            if (i != *sel) {
                *sel = i;
                dock_cursor_target(d, i);
                printf("studio: dock %s\n", dock_names[i]);
            }
            return;
        }
    }
}
/* One frame of the game's cursor: accumulate the 60Hz ticks and step the engine's
   port once per tick, exactly as ui_animate drives it (ui.c:2398). Ratio 1 is the
   value the menus set (menu.c:5352). */
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
static void dock_draw(const studio_dock *d, int sel)
{
    static const uint8_t opaque[4] = {255, 255, 255, 255};
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE, ox = d->ox, oy = d->oy;
    const float box = DOCK_BOX * S, slot = dock_slot() * S;
    const float icon_top = (448.f - 120.f + 2.f) * S; /* the design's bottom 120px are the dock */
    const float name_base = (448.f - 120.f + 2.f + DOCK_BOX + 10.f) * S;
    for (int i = 0; i < DOCK_COUNT; i++) {
        dock_icon *ic = &dock_icons[i];
        float cx = ox + DOCK_PAD * S + slot * i + slot / 2.f;
        if (ic->tex) {
            float k = fminf(box / (float)ic->w, box / (float)ic->h);
            float w = ic->w * k, h = ic->h * k;
            plat_draw_quad(ic->tex, cx - w / 2.f, oy + icon_top + (box - h) / 2.f, w, h, 0, 0, 1, 1,
                           opaque);
        }
        uint8_t col[4] = {255, 255, 255, (uint8_t)(i == sel ? 255 : 205)};
        dock_name(d, dock_names[i], cx, oy + name_base, col);
    }
    /* the game's own pointer, 32x32 with its tip at (d->cx,d->cy), turned about that tip
       by d->ca - the same draw the menus use (ui.c:2081). */
    if (dock_arrow.tex)
        plat_draw_rotated(dock_arrow.tex, ox + d->cx * S - 16.f * S, oy + d->cy * S, 32.f * S,
                          32.f * S, ox + d->cx * S, oy + d->cy * S, d->ca, opaque);
}

/* ---- first-run setup wizard ----
   A plain white screen, shown once: point Moon Engine at the user's disc, pick the graphics
   API, save both to launcher.ini. It is our own 2D on white - quiet text in the bundled face
   (the engine cannot lend its glyphs here: they are decoded from the disc, which is exactly
   what this screen chooses), no panels, no boxes. Arrows move, Enter confirms, Escape cancels. */
#define FRONT_CAR "ford_gt_no_stripe_05" /* the room's default car before the rig overrides it */

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

#ifdef _WIN32
#include <commdlg.h>
static int wiz_pick_disc(char *out, size_t size) /* the platform's own open dialog */
{
    wchar_t wfile[LAUNCHER_PATH] = {0};
    OPENFILENAMEW o;
    memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o;
    o.hwndOwner = NULL;
    o.lpstrFilter = L"PlayStation 2 disc image (*.iso)\0*.iso\0All files\0*.*\0";
    o.lpstrFile = wfile;
    o.nMaxFile = LAUNCHER_PATH;
    o.lpstrTitle = L"MOON ENGINE - Select game disc";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&o))
        return 0;
    return WideCharToMultiByte(CP_UTF8, 0, wfile, -1, out, (int)size, NULL, NULL) > 0;
}
#else
static int wiz_pick_disc(char *out, size_t size)
{
    /* The native picker is Windows-only; other systems take the disc on the CLI (the dev path). */
    const char *env = getenv("MOONSTUDIO_ISO");
    if (env && *env) {
        snprintf(out, size, "%s", env);
        return 1;
    }
    return 0;
}
#endif
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
static const char *start_shot_prefix; /* dev: capture the START fade mid-way and at black */
static int start_launch_env; /* dev: auto-press START and let the game boot (headless check) */
/* Runs the wizard on the current window. 1 = saved, 0 = cancelled (nothing written). */
static int first_run_wizard(launcher_preferences *prefs)
{
    if (!wiz_font_init())
        return 0;
    char disc[LAUNCHER_PATH];
    snprintf(disc, sizeof disc, "%s", prefs->disc);
    char status[256] = "";
    int step = 0, sel = 0, saved = 0, frame = 0;
    video_set_fill_window(1);
    for (;;) {
        SDL_Event ev;
        int quit = 0;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (ev.type != SDL_KEYDOWN || ev.key.repeat)
                continue;
            int k = ev.key.keysym.sym;
            if (k == SDLK_ESCAPE) {
                quit = 1;
                break;
            }
            if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
                sel ^= 1;
                continue;
            }
            if (k != SDLK_RETURN && k != SDLK_KP_ENTER)
                continue;
            if (step == 0) {
                if (sel == 0) { /* choose the disc */
                    char chosen[LAUNCHER_PATH];
                    if (wiz_pick_disc(chosen, sizeof chosen)) {
                        snprintf(disc, sizeof disc, "%s", chosen);
                        if (!wiz_valid(disc, status, sizeof status)) {
                        } /* status already set */
                        else
                            status[0] = 0;
                    }
                } else {
                    if (wiz_valid(disc, status, sizeof status)) {
                        step = 1;
                        sel = 0;
                    }
                }
            } else {
                if (sel == 1) {
                    saved = 1;
                    quit = 1;
                } /* Continue; the OpenGL entry is a no-op */
            }
        }
        if (quit)
            break;
        int fw = 0, fh = 0;
        SDL_GL_GetDrawableSize(plat_sdl_window(), &fw, &fh);
        if (fw < 1)
            fw = 640;
        if (fh < 1)
            fh = 448;
        plat_video_set_view((float)fw, (float)fh);
        plat_video_begin_frame();
        static const uint8_t white[4] = {255, 255, 255, 255}, ink[4] = {22, 22, 26, 255},
                             dim[4] = {150, 150, 156, 255}, warn[4] = {176, 60, 60, 255};
        plat_draw_quad(NULL, 0, 0, (float)fw, (float)fh, 0, 0, 1, 1, white);
        float cx = fw * .5f;
        wiz_text_center("MOON ENGINE", cx, fh * .20f, 1.7f, ink);
        if (step == 0) {
            wiz_text_center("Find your Gran Turismo 4 disc.", cx, fh * .33f, 1.0f, ink);
            const char *base = disc, *a = strrchr(disc, '/'), *b = strrchr(disc, '\\');
            if (b && (!a || b > a))
                a = b;
            if (a)
                base = a + 1;
            wiz_text_center(disc[0] ? base : "No disc selected.", cx, fh * .43f, 0.95f, dim);
            if (status[0])
                wiz_text_center(status, cx, fh * .51f, 0.85f, warn);
            wiz_text_center(sel == 0 ? "> Choose disc..." : "Choose disc...", cx, fh * .63f, 1.05f,
                            sel == 0 ? ink : dim);
            wiz_text_center(sel == 1 ? "> Continue" : "Continue", cx, fh * .72f, 1.05f,
                            sel == 1 ? ink : dim);
        } else {
            wiz_text_center("Graphics API", cx, fh * .33f, 1.0f, ink);
            wiz_text_center(sel == 0 ? "> OpenGL" : "OpenGL", cx, fh * .51f, 1.2f,
                            sel == 0 ? ink : dim);
            wiz_text_center(sel == 1 ? "> Continue" : "Continue", cx, fh * .65f, 1.05f,
                            sel == 1 ? ink : dim);
        }
        wiz_text_center("arrows move    enter confirm    escape cancel", cx, fh * .88f, 0.7f, dim);
        /* dev (--setup-shot): script the wizard and capture each step, no keyboard needed */
        if (wiz_shot_prefix) {
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
                if (wiz_valid(disc, status, sizeof status)) {
                    step = 1;
                    sel = 0;
                    frame = 0;
                    status[0] = 0;
                } else
                    frame = 0;
            }
            if (step == 1 && frame == 4) {
                snprintf(path, sizeof path, "%s-2.png", wiz_shot_prefix);
                screenshot(path);
                printf("wizard: api step -> %s\n", path);
            }
            if (step == 1 && frame >= 8) {
                saved = 1;
                break;
            }
        }
        plat_video_end_frame();
        plat_sleep_ms(16);
    }
    if (!saved)
        return 0;
    snprintf(prefs->disc, sizeof prefs->disc, "%s", disc);
    snprintf(prefs->api, sizeof prefs->api, "opengl");
    launcher_preferences_save(prefs);
    return 1;
}

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

/* --studio <car>: the white studio room on its own - no menu, no game, no disc
   navigation. The captain's front door lands here once the ISO and the graphics
   backend are chosen; --shot FILE turns it into a picture so it can be judged.

   Interactive, it is his rig: the mouse orbits the camera, the wheel dollies, WASD pans
   the aim point, Q/E lift it, the arrows move the car on the floor and , . turn it. Every
   change is written to the rig file, so the position he finds is the position next time. */
static int studio_run(showroom *cars, gt4_vol *vol, const char *code, const char *shot, int color,
                      int fade_in)
{
    float eye[3], target[3], car[3], angle;
    int locked =
        1; /* the room opens with the camera held: he frames it once, then works on the rest */
    studio_dock dock = {0};
    int dock_sel = 0, dock_dirty = 1;
    int start_fade = 0, start_launch = 0, fade_prints = 0,
        start_shot_mid = 0; /* START: fade the whole launcher to black, then the cinematic */
    /* The studio fills the window: no 4:3 bars, and a resize changes the resolution
       (the framebuffer already tracks the drawable; this drops the letterbox). */
    video_set_fill_window(1);
    for (int i = 0; i < DOCK_COUNT; i++) {
        dock_icons[i].tex = dock_icon_load(vol, dock_icons[i].pkg, dock_icons[i].asset,
                                           &dock_icons[i].w, &dock_icons[i].h);
        printf("studio: dock %s icon %s%s\n", dock_names[i],
               dock_icons[i].tex ? "loaded from the disc" : "MISSING",
               dock_icons[i].tex ? "" : " - check the asset name");
    }
    dock_arrow.tex =
        dock_icon_load(vol, dock_arrow.pkg, dock_arrow.asset, &dock_arrow.w, &dock_arrow.h);
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
        char *pref = SDL_GetPrefPath("MoonEngine", "MoonEngine");
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
    showroom_set_rig(cars, eye, target, car, angle);

    uint64_t start = plat_time_ns();
    while (showroom_prepare(cars, chosen) == 0) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT)
                return 0;
        if (plat_time_ns() - start > UINT64_C(60000000000)) {
            fprintf(stderr, "studio: assets did not arrive for %s\n", chosen);
            return 1;
        }
        plat_video_begin_frame();
        plat_video_end_frame();
        plat_sleep_ms(10);
    }
    /* The paint: --color INDEX picks one of the car's own colours; the name is printed so
       the right one can be found by name rather than guessed at. */
    if (color >= 0)
        showroom_set_color(cars, color);
    if (!shot)
        printf("studio: the rig is yours. drag = orbit, wheel = dolly, WASD = pan,\n"
               "        Q/E = crane the camera up/down, R/F = swing around the car,\n"
               "        arrows = move the car, ',' '.' = turn it.\n"
               "        The camera is %s - press L to %s it. Every change is saved to %s\n",
               locked ? "LOCKED" : "free", locked ? "unlock" : "lock",
               rig_path[0] ? rig_path : "(no user folder)");
    const int frames = shot ? 120 : 0; /* the entrance and the light timeline settle first */
    int dragging = 0, dirty = 0;
    uint64_t saved_at = 0;
    float lx = 0, ly = 0;
    int cur_rw = 0, pend_rw = -1;
    uint64_t pend_at = 0; /* raster width, rebuilt on a debounced resize */
    if (start_shot_prefix || start_launch_env)
        dock_sel = DOCK_START;
    for (int i = 0; frames == 0 || i < frames; i++) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                goto done;
            if (start_fade > 0)
                continue; /* the dock is inert while the launcher fades out */
            else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                if (ev.key.keysym.sym == SDLK_l) {
                    locked = !locked;
                    dirty = 1;
                    printf("studio: camera %s\n", locked ? "locked" : "free");
                } else if (locked && ev.key.keysym.sym == SDLK_LEFT) {
                    dock_sel = (dock_sel + DOCK_COUNT - 1) % DOCK_COUNT;
                    dock_dirty = 1;
                    dock_cursor_target(&dock, dock_sel);
                    printf("studio: dock %s\n", dock_names[dock_sel]);
                } else if (locked && ev.key.keysym.sym == SDLK_RIGHT) {
                    dock_sel = (dock_sel + 1) % DOCK_COUNT;
                    dock_dirty = 1;
                    dock_cursor_target(&dock, dock_sel);
                    printf("studio: dock %s\n", dock_names[dock_sel]);
                } else if (locked && (ev.key.keysym.sym == SDLK_RETURN ||
                                      ev.key.keysym.sym == SDLK_KP_ENTER)) {
                    if (dock_sel == DOCK_START)
                        start_fade = 1; /* START: fade the launcher to black, then the cinematic */
                    else
                        printf("studio: %s pressed (nothing behind it yet)\n",
                               dock_names[dock_sel]);
                }
            } else if (ev.type == SDL_MOUSEMOTION &&
                       locked) /* hover moves the pointer like the arrow keys */
                dock_hover(&dock, &dock_sel, ev.motion.x, ev.motion.y);
            else if (locked)
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
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        float dt = 1.f / 60.f, step = 2.2f * dt;
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
        if (start_fade > 0) {
            start_fade++;
            if (start_fade > FADE_FRAMES)
                start_fade = FADE_FRAMES;
            if (fade_prints < 15) {
                fade_prints++;
                printf("studio: start fade %.3f\n", fade_curve((float)start_fade / FADE_FRAMES));
            }
        }
        plat_video_begin_frame();
        video_begin_scene();
        /* the scene raster follows the window aspect (W = 448*window); the dock then draws in
           framebuffer pixels. Both map to the window at one uniform scale - nothing stretches. */
        plat_video_set_view((float)cur_rw, 448.f);
        showroom_draw(cars, chosen, 1.f / 60.f);
        video_end_scene();
        if (dock_dirty) {
            dock_font_load(&dock, vol);
            dock_dirty = 0;
        }
        dock_cursor_step(&dock, dt);
        plat_video_set_view((float)fw, (float)fh);
        dock_draw(&dock, dock_sel);
        /* one direction, eased: the wizard's white screen rises off the room ... */
        if (fade_in && i < FADE_FRAMES) {
            uint8_t veil[4] = {255, 255, 255,
                               (uint8_t)(255 * (1.f - fade_curve((float)i / FADE_FRAMES)))};
            plat_draw_quad(NULL, 0, 0, (float)fw, (float)fh, 0, 0, 1, 1, veil);
        }
        /* ... and START falls the whole launcher to black, dock and cursor included */
        if (start_fade > 0) {
            uint8_t blk[4] = {0, 0, 0,
                              (uint8_t)(255 * fade_curve((float)start_fade / FADE_FRAMES))};
            plat_draw_quad(NULL, 0, 0, (float)fw, (float)fh, 0, 0, 1, 1, blk);
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
        if (shot && i == frames - 1) {
            screenshot(shot);
            printf("studio: %s painted '%s'\n", chosen, showroom_caption(cars));
        }
        plat_video_end_frame();
        if (start_fade >= FADE_FRAMES) {
            if (!start_shot_prefix)
                start_launch = 1;
            goto done;
        } /* fully black: the cinematic starts */
        plat_sleep_ms(4);
    }
    goto done;
done:
    if (rig_path[0])
        rig_save(rig_path, eye, target, car, angle, chosen, locked);
    printf("studio: white room, %s\n", chosen);
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
        char *pref=SDL_GetPrefPath("MoonEngine","MoonEngine");
        if(pref){snprintf(rig_path,sizeof rig_path,"%sstudio-rig.txt",pref);SDL_free(pref);}
    }
    rig_defaults(eye,target,car,&angle);
    if(rig_path[0])rig_load(rig_path,eye,target,car,&angle,rig_car,sizeof rig_car,&locked);
    showroom_set_rig(cars,eye,target,car,angle);
    uint64_t start=plat_time_ns();
    while(showroom_prepare(cars,chosen)==0){
        SDL_Event ev;while(SDL_PollEvent(&ev))if(ev.type==SDL_QUIT)return 0;
        if(plat_time_ns()-start>UINT64_C(60000000000)){fprintf(stderr,"preview: assets did not arrive for %s\n",chosen);return 1;}
        plat_video_begin_frame();plat_video_end_frame();plat_sleep_ms(10);
    }
    if(color>=0)showroom_set_color(cars,color);
    int dragging=0,panning=0,dirty=0,ready=0;float lx=0,ly=0;
    int cur_rw=0,pend_rw=-1;uint64_t pend_at=0;
    const int frames=shot?60:0;   /* the entrance and the light timeline settle first */
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
        plat_video_begin_frame();
        video_begin_scene();
        plat_video_set_view((float)cur_rw,448.f);
        showroom_draw(cars,chosen,1.f/60.f);
        video_end_scene();
        plat_video_end_frame();
        if(!ready){ready=1;printf("preview: ready car=%s window=%dx%d\n",chosen,fw,fh);fflush(stdout);}
        if(shot && i==frames-1){screenshot(shot);printf("preview: %s painted '%s'\n",chosen,showroom_caption(cars));}
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

int main(int argc, char **argv)
{
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
    gt4_save_init(save_directory[0] ? save_directory : NULL);
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
    int window_w = settings.current.video.width, window_h = settings.current.video.height;
    int window_full = settings.current.video.fullscreen, upscale = settings.current.video.upscale,
        video_vsync = settings.current.video.vsync;
    int race_preview = 0, race_active = 0, race_loading = 0, race_drive = 0, loading_preview = 0,
        race_return = 0;
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
    if (startup_intro)
        proj = show_opening ? 2 : 0;
    int first_run = first_run_override || (startup_intro && !gt4_save_active());
    /* The front-end's START plays the game's own opening first; the profile/import screen is
       reached from the game's menu (GT Mode / New Career -> the "save" project). */
    if (first_run && !front_end) {
        proj = 7;
        startup_intro = 0;
    }
    if (settings_preview) {
        proj = 3;
        startup_intro = 0;
        first_run = 0;
    }

    plat_video_desc desc = {window_w, window_h, window_full, video_vsync, upscale};
#ifndef __SWITCH__
    desc.backend = settings.current.renderer;
#endif
    settings.current.video = desc;
    if (plat_video_init(&desc) != 0) {
        fprintf(stderr, "cannot open a window: %s\n", SDL_GetError());
        return 1;
    }
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
    if (!video_set_upscale(upscale)) {
        settings.current.video.upscale = 0;
        fprintf(stderr, "FSR unavailable; using native resolution.\n");
    }
    if (!shot) {
        plat_audio_desc audio = {48000, 2};
        plat_audio_init(&audio, NULL, NULL);
    }

    /* The no-argument front-end: the disc and the graphics API come from launcher.ini, and on
       the very first run from the wizard (video is up, the volume is not open yet). */
    if (front_end) {
        char msg[256];
        int configured = front_disc[0] && startup.preferences.api[0] &&
                         launcher_validate_disc(front_disc, msg, sizeof msg);
        if (wiz_shot_prefix || !configured) {
            if (!first_run_wizard(&startup.preferences))
                return 0;
            front_disc = startup.preferences.disc;
        } else {
            printf("setup loaded from launcher.ini\n");
        }
        argv[1] = (char *)front_disc;
    }
    /* Disc-dependent tables and the volume open now that the disc is known. */
    app_car_ratings = gt4_car_ratings_open(argv[1]);
    app_car_factory = gt4_career_vehicle_open(argv[1]);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, app_car_factory);
    gt4_game_set_auto_gears(gt4_career_vehicle_auto_gears, app_car_factory);
    gt4_game_set_stats_reader(gt4_career_vehicle_stats, app_car_factory);
    gt4_game_set_service_reader(gt4_career_vehicle_service_geometry, app_car_factory);
    gt4_game_set_setting_reader(gt4_career_vehicle_setting, app_car_factory);
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    if (vol && !gt4_game_load_disc(argv[1]))
        fprintf(stderr,
                "career: original calendar, price or oil constants unavailable for this disc\n");
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
        plat_video_begin_frame();
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
    race_view *race = race_view_open(argv[1]);
    presentation_photo = photo_travel_open(argv[1]);
    if (preview_car)
        return preview_run(cars, preview_car, shot, color_preview);
    if (studio_car)
        return studio_run(cars, vol, studio_car, shot, color_preview, 0);
    if (front_end) {
        const char *fcar = startup.preferences.car[0] ? startup.preferences.car : FRONT_CAR;
        int fcol = startup.preferences.color;
        int fr = studio_run(cars, vol, fcar, front_shot, fcol, 1);
        if (fr != 2)
            return fr; /* 2 = START: the launcher is black, run the game (its opening rises from black) */
        showroom_clear_rig(cars);
        showroom_use_disc_room(cars);
        showroom_set_width(cars, 640); /* hand the showroom back to the game */
        /* The studio drew in its own fill view; put the game's own authentic present back
           (the resolution and ratio from the video options), so nothing is zoomed or cropped. */
        video_set_fill_window(0);
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
    /* the race itself (the physics with the player's controls): driving 1
       while its models load, 2 racing */
    race_game *game = race_game_open(argv[1]);
    int driving = 0;
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
                                                                   : 5,
                                                 msg, sizeof msg)) {
                fprintf(stderr, "race: %s\n", msg);
                exit_code = 1;
                goto done;
            }
            if (split_preview)
                pre_race_preview = 0;
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
    while (!plat_should_quit()) {
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
            menu_set_pad_held(mn, plat_sdl_menu_buttons() | (k[SDL_SCANCODE_Q] ? MENU_PAD_L1 : 0) |
                                      (k[SDL_SCANCODE_E] ? MENU_PAD_R1 : 0));
        }
#ifndef __SWITCH__
        ui_main_menu_layout(menu_ui(mn),settings.current.main_menu);
        ui_pointer_smoothing(menu_ui(mn),settings.current.smooth_cursor);
        ui_pointer_feedback(menu_ui(mn),settings.current.hover_feedback);
        ui_menu_readability(menu_ui(mn),settings.current.compact_cars);
        SDL_ShowCursor(settings.current.desktop ? SDL_DISABLE : SDL_ENABLE);
        if(!settings.current.desktop && mn) ui_pointer(menu_ui(mn),0,0,0);
#endif
        while (plat_sdl_poll(&ev)) {
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
                if (!ev.pad && ev.key == SDLK_F9) {
                    race_game_cinematic_toggle(game);
                    continue;
                }
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
                if (ev.key == SDLK_F5 && race_game_photo_view(game)) {
                    if (race_game_cinematic(game))
                        race_game_cinematic_toggle(game);
                    if (!cache[14])
                        cache[14] = load(vol, 14);
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
            if (driving == 5 && ev.key == SDLK_ESCAPE &&
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
                if (ev.key == SDLK_ESCAPE)
                    menu_send_input(mn, ev.pad ? MENU_CIRCLE : MENU_BACK);
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
                            memcpy(q.total_points, career_race.points, sizeof career_race.points);
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
                int nothers = menu_race_entries(mn, others, other_colors, 5);
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
                    /* GranTurismo4Option: unit_velocity KMPH (0) or MPH (1), 0x005A6978 */
                    race_game_set_unit(
                        game, strcmp(menu_option_text(mn, "unit_velocity", "MPH"), "KMPH") ? 1 : 0);
                    race_game_set_power_unit(game, menu_option_text(mn, "unit_power", "HP"));
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
                    } else
                        started =
                            race_game_start(game, race_course, race_car, race_color, race_other_ptr,
                                            race_other_colors, nothers, msg, sizeof msg);
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
                                       "Nao foi possivel montar esta corrida. Veja o log.", 0, NULL,
                                       NULL);
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
                            for (unsigned i = 0; i < 6; i++)
                                q.total_points[i] = career_race.points[i];
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
                        snprintf(msg, sizeof msg, "The replay could not be saved.");
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
        if (notice < GT4_NOTICE_COUNT) {
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
                        strcmp(page, "MovieRoot") && !(music_faded && plat_sdl_music_fading());
        int prepare_pcm = (!shot || music_with_shot) && !notice_active && !race_active &&
                          driving != 2 && pcm_group && *pcm_group && strcmp(page, "MovieRoot");
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
                                   !race_quick_project(proj);
        update_present_midi(mn);
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
        /* ResultLicense/ResultArcade advance per VBlank (003DF978 +B0),
           including failure colour decay. The mission shares this clock. */
        video_set_menu_pacing(
            (!race_active && driving != 2) ||
            (race_active && (race_game_license_active(game) || race_game_mission_active(game))));
        video_set_menu_projection(!race_active && driving != 2);
        presentation_pregrid = (driving == 3 || driving == 4) && race_quick_project(proj);
        if (!presentation_pregrid)
            pregrid_clock = 0;
        plat_video_begin_frame();
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
            video_end_scene();
        if (driving == 2 && race_game_wants_thumbnail(
                                game)) { /* the race's picture for a saved replay, before its HUD */
            int tw, th;
            unsigned char *px = plat_sdl_read_view(&tw, &th);
            if (px) {
                race_game_set_thumbnail(game, px, tw, th);
                free(px);
            }
        }
        if (driving == 2)
            race_game_draw_hud(game, elapsed);
        int drawn = 0, skipped = 0;
        movie.seen = movie.drawn = 0;
        if (p && !race_active && driving != 2 && !notice_active) {
            ui_set_movie_renderer(p, draw_movie, &movie);
            if (driving == 5 && proj == 14)
                photo_travel_editor_draw(presentation_photo);
            else
                draw_pages(mn, &drawn, &skipped);
        }
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
                plat_video_end_frame();
                SDL_Delay(10);
                continue;
            }
            fprintf(stderr, "presentation: timed out waiting for requested asset\n");
            exit_code = 1;
        } else if (shot)
            shot_wait = 0; /* each movie transition gets a fresh loading deadline */
        int shot_due = shot != NULL;
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
        plat_video_end_frame();
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
            last_root = root;
            last_proj = proj;
        }
    }
done:
    photo_travel_close(presentation_photo);
    presentation_photo = NULL;
    race_game_close(game);
    game = NULL; /* audio writes its jukebox back before the final profile save */
    if (mn) {
        menu_save_capture(mn);
        if (gt4_save_dirty() && gt4_save_auto_enabled() && gt4_save_active() && !gt4_save_store())
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
    pregrid_audio_close(pregrid_music);
    plat_audio_shutdown();
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
