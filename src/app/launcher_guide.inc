/* P: the first drive - a guide that explains what Moon Engine changes and lets the player
   choose, told in the launcher's own language: the wordmark and the status in the top corners,
   the OPTIONS glass panel with its tabs and "<  value  >" rows, the dealers' metal band with
   the game's pointer under the chosen action, and the help strip under it.
   Every choice is a draft: the scene previews it, DONE (or OPTIONS) writes it through the same
   video_setting_set the Options use, SKIP and Escape leave the preferences untouched.
   Each graphics option is shown on its own: the same frozen instant rendered with the option
   at its original value and at the chosen one, split by a divider the mouse can drag, framed
   where the effect shows. Disc-backed car, course, icons and face. */
#include <stdarg.h>
static void gl_log(const char *fmt, ...)
{
    char pref[1200], path[1300], line[512];
    if (!plat_pref_dir(pref, sizeof pref))
        return;
    snprintf(path, sizeof path, "%sguide.log", pref);
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
#define GUIDE_PAGES 5
#define GUIDE_WELCOME 0
#define GUIDE_PRESET 1
#define GUIDE_GRAPHICS 2
#define GUIDE_MENUS 3
#define GUIDE_READY 4
static const char *const guide_tabs[GUIDE_PAGES] = {"WELCOME", "PRESET", "GRAPHICS", "MENUS",
                                                    "READY"};
static const char *const guide_captions[GUIDE_PAGES] = {
    "Welcome: What Moon Engine is, and what this guide sets up.",
    "Preset: The starting point. Every option can still be changed one by one.",
    "Graphics: Each improvement on its own, against the original picture.",
    "Menus: Mouse, keyboard and the PC additions to the game's menus.",
    "Ready: What will change. Done saves it; Skip leaves everything as it was."};
/* One option the guide explains. label is the Options' own (video_setting), so the names,
   the choices and the write all come from there. what/look/cost are the guide's words; crop is
   the part of the frame where the difference shows (u0, v0, u1, v1). */
typedef struct {
    const char *label;
    size_t field;
    const char *what, *look, *cost;
    float crop[4];
} guide_option;
#define GO(field) offsetof(pc_video_options, field)
static const guide_option guide_graphics[] = {
    {"Realistic Lighting", GO(modern_lighting),
     "Direct sunlight, shaded surfaces and reflections that respond to the material. "
     "Off keeps the original lighting.",
     "The paint's highlights and the side of the car away from the sun.",
     "Uses more graphics time.", {.05f, .29f, .65f, .67f}},
    {"Reflection Quality", GO(reflection_quality),
     "The resolution of the surroundings reflected in the paint and the glass. Original keeps "
     "the PS2's 128-pixel reflection.",
     "The sky and the course mirrored on the bodywork and the windscreen.",
     "Each step up costs more graphics time.", {.10f, .33f, .58f, .59f}},
    {"Realistic Skybox", GO(daylight),
     "Clouds, atmosphere, sun, moon and stars following the course and replay clock.",
     "The sky, the haze on the hills and where the light comes from.",
     "Uses more graphics time.", {.04f, .10f, .96f, .64f}},
    {"Texture Filtering", GO(texture_filtering),
     "Keeps surfaces sharp as they recede. Original keeps the textures exactly as the disc "
     "samples them.",
     "The tarmac and the lines in the distance, and the kerbs at an angle.",
     "Light.", {.40f, .48f, 1, .90f}},
    {"Anti-Aliasing", GO(antialiasing),
     "Smooths the stair steps on edges. Edge AA filters the finished picture; SSAA renders "
     "more pixels and shrinks them.",
     "The car's outline against the track, the fence and thin poles.",
     "Edge AA is light; SSAA 200% is heavy.", {.10f, .33f, .42f, .55f}},
};
static const guide_option guide_menus[] = {
    {"Desktop Input", GO(desktop),
     "Mouse and keyboard in the game's menus: point, click, scroll. Off keeps the menus to "
     "the pad, as on the PS2.", "", "", {0}},
    {"Input Hints", GO(input_hints),
     "The on-screen buttons follow the device in use: keyboard keys, Xbox or PlayStation.",
     "", "", {0}},
    {"Smooth Cursor", GO(smooth_cursor),
     "The game's menu pointer glides to the mouse instead of jumping.", "", "", {0}},
    {"Hover Feedback", GO(hover_feedback),
     "Menu items light up under the mouse before a click.", "", "", {0}},
    {"Main Menu", GO(main_menu),
     "An overview of the PC additions on the main menu.", "", "", {0}},
    {"Map Edge Pan", GO(edge_pan),
     "The GT Mode map scrolls when the mouse reaches the edge of the screen.", "", "", {0}},
    {"Compact Car Strip", GO(compact_cars),
     "A tighter car list in the garage and at the dealers, so more cars fit.", "", "", {0}},
    {"Graphics Swap Key", GO(graphics_swap),
     "F7 in a race or a replay swaps between Classic and your graphics on the spot, like a "
     "remaster's classic mode. Nothing is saved; leaving the race returns to yours.", "", "", {0}},
    {"Aspect Ratio", GO(aspect),
     "The game's picture: the original 4:3 or widescreen 16:9. Applies at the next start.",
     "", "", {0}},
};
#undef GO
#define GUIDE_GRAPHICS_N (int)(sizeof guide_graphics / sizeof *guide_graphics)
#define GUIDE_MENUS_N (int)(sizeof guide_menus / sizeof *guide_menus)
static const float guide_tilt[3] = {22.5f, 0.f, -22.5f};
#define GUIDE_NAME_Y 390.f
#define GUIDE_CURSOR_Y 393.f
static float guide_slot_cx(int i)
{
    return DOCK_PAD + (DOCK_WIDTH - DOCK_PAD * 2.f) * (i + .5f) / 3.f;
}
static void guide_rect(float x, float y, float w, float h, const uint8_t c[4])
{
    plat_draw_quad(NULL, x, y, w, h, 0, 0, 1, 1, c);
}
static int guide_get(const pc_video_options *v, size_t field)
{
    return *(const int *)((const char *)v + field);
}
/* a change in the draft, with the dependencies video_setting_set applies */
static void guide_put(pc_video_options *v, size_t field, int value)
{
    *(int *)((char *)v + field) = value;
    if(field==offsetof(pc_video_options,daylight))video_options_realistic_skybox(v,value);
    if(field==offsetof(pc_video_options,modern_lighting))video_options_realistic_lighting(v,value);
    if (field == offsetof(pc_video_options, sun_rays) && value == 2)
        v->hdr = v->daylight = 1;
    if (field == offsetof(pc_video_options, ambient_occlusion) && value)
        v->hdr = v->daylight = 1;
    if (field == offsetof(pc_video_options, antialiasing))
        v->render_scale = 2, v->video.upscale = 0;
}
static int guide_setting(const char *label)
{
    for (int i = 0; i < video_setting_count(); i++)
        if (!strcmp(video_setting(i)->label, label))
            return i;
    return -1;
}
static const char *guide_value_name(const char *label, int value)
{
    const video_setting_info *in = video_setting(guide_setting(label));
    return in && value >= 0 && value < in->count ? in->names[value] : "-";
}
static int guide_value_count(const char *label)
{
    const video_setting_info *in = video_setting(guide_setting(label));
    return in ? in->count : 1;
}
/* the dock's names over a scene: the disc face on a soft dark shadow, at any size and side */
static void guide_text(const studio_dock *d, const char *text, float x, float base, float size,
                       int align, const uint8_t col[4])
{
    if (!d->font_ok || !text || !col[3]) return;
    const uint8_t s1[4] = {6, 7, 9, (uint8_t)(120 * col[3] / 255)},
                  s2[4] = {6, 7, 9, (uint8_t)(70 * col[3] / 255)};
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float w = dock_text_width(&d->font, text) * DOCK_NAME_SCALE * size * S;
    float cx = align == 1 ? x : align == 2 ? x - w * .5f : x + w * .5f;
    dock_name_pass(d, text, cx, base, S, S, s1, size);
    dock_name_pass(d, text, cx, base, 2.f * S, 2.f * S, s2, size);
    dock_name_pass(d, text, cx, base, 0.f, 0.f, col, size);
}
/* a paragraph broken at spaces to a width; returns the baseline after its last line. A NULL
   colour only measures. */
static float guide_wrap(const studio_dock *d, const char *text, float x, float y, float maxw,
                        float size, const uint8_t col[4], float leading)
{
    const float k = DOCK_NAME_SCALE * size * d->scale;
    char line[256];
    const char *p = text;
    while (p && *p) {
        size_t best = 0;
        for (const char *e = p;; e++) {
            if (*e && *e != ' ') continue;
            size_t len = (size_t)(e - p);
            if (len >= sizeof line) break;
            memcpy(line, p, len);
            line[len] = 0;
            if (best && dock_text_width(&d->font, line) * k > maxw) break;
            best = len;
            if (!*e) break;
        }
        if (!best) break;
        memcpy(line, p, best);
        line[best] = 0;
        if (col)
            dock_text(d, line, x, y, studio_text_fit(d, line, size, maxw), 0, col);
        y += leading;
        p += best;
        while (*p == ' ') p++;
    }
    return y;
}
/* the OPTIONS panel's place: short on the welcome, down to the band elsewhere */
static studio_panel guide_panel(const studio_dock *d, int page)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    studio_panel p = {14.f * S, 80.f * S, (d->fw > 0 ? d->fw : 640.f * S) - 28.f * S, 0, 0,
                      18.f * S, 0};
    p.tab_y = p.y + 24.f * S;
    p.row0 = p.y + 54.f * S;
    p.h = page == GUIDE_WELCOME ? 166.f * S : d->oy + (363.f - 12.f) * S - p.y;
    int rows = page == GUIDE_GRAPHICS ? GUIDE_GRAPHICS_N : page == GUIDE_MENUS ? GUIDE_MENUS_N : 1;
    if (p.row0 + p.pitch * rows > p.y + p.h - 6.f * S)
        p.pitch = (p.y + p.h - 6.f * S - p.row0) / rows;
    return p;
}
static float guide_tab_w(const studio_dock *d, int t)
{
    return dock_text_width(&d->font, guide_tabs[t]) * DOCK_NAME_SCALE * .9f * d->scale;
}
static float guide_tab_x(const studio_dock *d, const studio_panel *p, int t)
{
    float x = p->x + 12.f * d->scale;
    for (int k = 0; k < t; k++)
        x += guide_tab_w(d, k) + 22.f * d->scale;
    return x;
}
/* The demonstration: an R34 Skyline GT-R at Tsukuba, the course every PC renderer addition
   was validated on (docs/RENDERER.md), so each comparison shows a proven path. */
#define GUIDE_COURSE "tsukuba"
#define GUIDE_COURSE_NAME "Tsukuba Circuit"
#define GUIDE_CAR "r34_gtr_v_gtcolor_01"
/* the car's name as the game's own spec database writes it */
static void guide_car_title(gt4_vol *vol, char *out, size_t size)
{
    snprintf(out, size, "%s", GUIDE_CAR);
    size_t ln = 0, nn = 0, sn = 0;
    uint8_t *labels = gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &ln),
            *names = gt4_vol_read(vol, "/specdb/GT4_US2560/CAR_NAME_american.dbt", &nn),
            *strs = gt4_vol_read(vol, "/specdb/GT4_US2560/american_StrDB.sdb", &sn);
    uint32_t id;
    uint8_t row[256];
    if (labels && names && strs && gt4_specdb_label(labels, ln, GUIDE_CAR, &id) &&
        gt4_specdb_row(names, nn, id, row, sizeof row) == 16) {
        const char *title = gt4_specdb_string(strs, sn, (uint32_t)row[0] | (uint32_t)row[1] << 8 |
                                                          (uint32_t)row[2] << 16 |
                                                          (uint32_t)row[3] << 24);
        if (title) snprintf(out, size, "%s", title);
    }
    free(labels);
    free(names);
    free(strs);
}
/* The course and the car through the race's own renderer (race_view_draw, as in a race
   and the track viewer), not the Photo Travel film path. */
static void guide_scene(showroom *cars,const char *code,int raster,race_view *track,float dt)
{
    if(track) {
        plat_video_set_view(640.f,448.f);
        race_view_draw(track);
    } else {
        plat_video_set_view((float)raster,448.f);
        showroom_draw(cars,code,dt);
    }
}
/* Options whose change rebuilds a render target (HDR's float scene, the sampling, the shadow and
   reflection sizes): after one of them the first frame is not representative. */
static int guide_rebuilds(const pc_video_options *a, const pc_video_options *b)
{
    return a->hdr != b->hdr || a->antialiasing != b->antialiasing ||
           a->render_scale != b->render_scale || a->video.upscale != b->video.upscale ||
           a->shadow_maps != b->shadow_maps || a->reflection_quality != b->reflection_quality ||
           a->daylight != b->daylight;
}
/* One capture of the frozen scene drawn with `look`, copied on the GPU (video_grab, as the
   race's cinema and replay captures) into *into, which is reused: no read-back. One render per
   call; when the look rebuilds a render target that frame only settles it and 0 is returned,
   so the caller draws again on the next frame. Only the final composed frame is presented. */
static int guide_capture(plat_texture **into, showroom *cars, const char *code, int raster,
                         const pc_video_options *look, race_view *track, pc_video_options *drawn,
                         int drawable_w, int drawable_h)
{
    int live_w = 0, live_h = 0;
    SDL_GL_GetDrawableSize(plat_sdl_window(), &live_w, &live_h);
    if (live_w > 0 && live_h > 0 && (live_w != drawable_w || live_h != drawable_h))
        return -1; /* the resize landed after this frame's size read: never read it */
    static int capture_w = -1, capture_h = -1;
    if (drawable_w != capture_w || drawable_h != capture_h) {
        capture_w = drawable_w;
        capture_h = drawable_h;
        if (*into) {
            plat_texture_destroy(*into);
            *into = NULL;
        }
        return -1;
    }
    int settling = guide_rebuilds(look, drawn);
    /* Captures use the game's fitted viewport, not the launcher's width-filled
       layout: a wide/maximized window can otherwise request rows outside the
       framebuffer (e.g. 1920x1344 from a 1920x1080 image). */
    int fill = video_fill_window();
    video_set_fill_window(0);
    video_settings_preview_options(look);
    plat_video_begin_frame();
    video_begin_scene();
    guide_scene(cars, code, raster, track, 0);
    video_end_scene();
    *drawn = *look;
    if (settling) {
        video_set_fill_window(fill);
        return 0;
    }
    int cw = track ? 640 : raster, ch = 448, pw = 0, ph = 0;
    unsigned char *px = plat_sdl_read_pixels(&pw, &ph);
    long sum = 0;
    unsigned max_luma = 0;
    if (px && pw > 0 && ph > 0) {
        const unsigned char *q = px;
        for (long i = 0; i < (long)pw * ph; i++, q += 4) {
            unsigned luma = (q[0] * 77u + q[1] * 150u + q[2] * 29u) >> 8;
            sum += luma;
            if (luma > max_luma)
                max_luma = luma;
        }
    }
    int mean = px && pw > 0 && ph > 0 ? (int)(sum / ((long)pw * ph)) : 0;
    int blank = !px || max_luma <= 4;
    gl_log("guide: drawable %dx%d capture=%dx%d mean=%d accepted=%d", drawable_w, drawable_h, cw,
           ch, mean, blank ? 0 : 1);
    free(px);
    if (!blank)
        *into = video_grab(*into, 0, 0, (float)cw, (float)ch);
    video_set_fill_window(fill);
    return blank ? -1 : 1;
}
/* Two captures of one instant through one window: a's left of the divider, b's right. The
   crop (top-down) is fitted into the box; grabbed rows run bottom up, so v is flipped. out
   receives the picture's rectangle for the mouse. */
static void guide_compare(plat_texture *a, plat_texture *b, float iw, float ih, const float crop[4],
                          float x, float y, float w, float h, float split, float out[4])
{
    if (!a || !b || iw < 1 || ih < 1) return;
    float cw = (crop[2] - crop[0]) * iw, ch = (crop[3] - crop[1]) * ih;
    float k = fminf(w / cw, h / ch), dw = cw * k, dh = ch * k;
    float dx = x + (w - dw) * .5f, dy = y + (h - dh) * .5f, um = crop[0] + (crop[2] - crop[0]) * split;
    float vt = 1 - crop[1], vb = 1 - crop[3];
    const uint8_t white[4] = {255, 255, 255, 255}, line[4] = {255, 255, 255, 200};
    plat_draw_quad(a, dx, dy, dw * split, dh, crop[0], vt, um, vb, white);
    plat_draw_quad(b, dx + dw * split, dy, dw * (1 - split), dh, um, vt, crop[2], vb, white);
    guide_rect(dx + dw * split - 1.f, dy, 2.f, dh, line);
    out[0] = dx, out[1] = dy, out[2] = dw, out[3] = dh;
}
/* What DONE writes: the preset first (as the Options apply it), then every option the guide
   shows whose value differs, one by one, so the preset reads Modified exactly as in Options. */
static int guide_commit(int preset, const pc_video_options *d)
{
    video_settings *s = app_video_settings;
    int changes = 0;
    if (preset < 2 && preset != video_setting_get(s, guide_setting("PC Preset"))) {
        video_setting_set(s, guide_setting("PC Preset"), preset);
        changes++;
    }
    for (int g = 0; g < 2; g++) {
        const guide_option *list = g ? guide_menus : guide_graphics;
        int n = g ? GUIDE_MENUS_N : GUIDE_GRAPHICS_N;
        for (int i = 0; i < n; i++) {
            int id = guide_setting(list[i].label), want = guide_get(d, list[i].field);
            if (id >= 0 && video_setting_get(s, id) != want) {
                video_setting_set(s, id, want);
                changes++;
            }
        }
    }
    video_settings_apply_current(s);
    return changes;
}
/* the test's input: tokens run one every few frames once the scene is ready - key names
   (RETURN ESCAPE TAB SPACE UP DOWN LEFT RIGHT) or @name for a screenshot */
static int guide_script_step(const char **script, int page)
{
    const char *p = *script;
    while (*p == ' ') p++;
    if (!*p) return 0;
    char token[64];
    size_t n = strcspn(p, " ");
    snprintf(token, sizeof token, "%.*s", (int)(n < 63 ? n : 63), p);
    *script = p + n;
    if (token[0] == '@') {
        char path[1400];
        snprintf(path, sizeof path, "%s-%s.png", getenv("MOONSTUDIO_GUIDE_SHOT"), token + 1);
        screenshot(path);
        printf("studio guide: shot %s on page %d\n", token + 1, page);
        return 1;
    }
    int width, height;
    if (sscanf(token, "RESIZE:%dx%d", &width, &height) == 2 && width > 0 && height > 0 && width <= 4096 && height <= 2160) {
        SDL_SetWindowSize(plat_sdl_window(), width, height);
        printf("studio guide: resize %d %d\n", width, height);
        return 1;
    }
    static const struct { const char *name; int key; } keys[] = {
        {"RETURN", SDLK_RETURN}, {"ESCAPE", SDLK_ESCAPE}, {"TAB", SDLK_TAB}, {"SPACE", SDLK_SPACE},
        {"UP", SDLK_UP}, {"DOWN", SDLK_DOWN}, {"LEFT", SDLK_LEFT}, {"RIGHT", SDLK_RIGHT}};
    for (unsigned k = 0; k < sizeof keys / sizeof *keys; k++)
        if (!strcmp(token, keys[k].name)) {
            SDL_Event key = {0};
            key.type = SDL_KEYDOWN;
            key.key.keysym.sym = keys[k].key;
            SDL_PushEvent(&key);
        }
    return 1;
}
/* Returns 0 only for window-close, 2 to open OPTIONS. Escape and SKIP leave every preference
   as it was. F7 opens it from the studio straight on the graphics. Focus: the page's rows,
   then the band (rows). Space shows the chosen option full screen; Tab flips it there. */
static int studio_guide(showroom *cars,gt4_vol *vol,const char *code,int raster,studio_dock *dock,int intro)
{
    if (!app_video_settings) return 1;
    const char *icon_names[GUIDE_PAGES]={"image/option/GT4logo.png","image/option/Misc.png","image/option/Screen.png","image/option/Misc.png","image/option/Sound.png"};
    dock_icon stage_icons[GUIDE_PAGES]={0};
    for(int n=0;n<GUIDE_PAGES;n++) {
        float crop[4]={0,0,1,1};
        stage_icons[n].tex=dock_icon_load_crop(vol,"/projects/option/US/OptionRoot.gpb",icon_names[n],&stage_icons[n].w,&stage_icons[n].h,crop);
        stage_icons[n].u0=crop[0];stage_icons[n].v0=crop[1];
        stage_icons[n].u1=crop[2];stage_icons[n].v1=crop[3];
        printf("studio guide: disc icon %s %s\n",icon_names[n],stage_icons[n].tex?"loaded":"missing");
    }
    video_settings *vs=app_video_settings;
    const pc_video_options start=vs->current;
    pc_video_options draft=start, classic_look=start, enhanced_look=start;
    video_settings_preset_values(&classic_look,0);
    video_settings_preset_values(&enhanced_look,1);
    const int was=video_setting_get(vs,guide_setting("PC Preset")); /* 0, 1, 2 modified */
    int preset=was;   /* 0 Classic, 1 Enhanced, 2 keep the player's own */
    int page=intro?GUIDE_WELCOME:GUIDE_GRAPHICS, row[GUIDE_PAGES]={0}, focus_band=1, sel=2;
    int full=0, flip=1, capture=1, pw=0, ph=0, dragging=0;
    /* a comparison is drawn once the selection has rested (settle frames), one side per
       frame (pending 1: original, 2: chosen), so scrolling the list never stalls */
    int settle=0, pending=0, shown=0, retry_hold=0;
    pc_video_options drawn_look=start;
    uint64_t frame_clock=0; double worst_ms=0; /* the test reports the longest frame */
    float split=.5f, pic[4]={0,0,0,0}, hover[3]={0,0,1};
    plat_texture *images[3]={NULL,NULL,NULL}; /* original, chosen, (unused) */
    int running=1, keep_running=1, frame=0, commit=0;
    studio_dock before_dock=*dock;
    moon_options before_store=vs->store;
    race_view *track=NULL;
    int track_ready=0, track_on=1, track_failed=0;
    track=race_view_open(studio_disc);
    if(track) race_view_set_parts(track,"","");
    if(!track || !race_view_request(track,GUIDE_COURSE,GUIDE_CAR,0)) {
        track_failed=1;track_on=0;
    }
    char car_title[160];
    guide_car_title(vol,car_title,sizeof car_title);
    const int test=getenv("MOONSTUDIO_GUIDE_SHOT") && (getenv("MOON_WINDOW_HIDDEN") || getenv("MOONSTUDIO_GUIDE_SCRIPT"));
    const char *script=getenv("MOONSTUDIO_GUIDE_SCRIPT");
    if(!script) script="";
    dock->cx=dock->ctx=guide_slot_cx(sel); dock->cy=dock->cty=GUIDE_CURSOR_Y;
    dock->ca=dock->cta=guide_tilt[sel]; dock->ticks=0;
    int last_w=-1,last_h=-1;
    while(running) {
        int w=0,h=0; SDL_GL_GetDrawableSize(plat_sdl_window(),&w,&h);
        if(w<1||h<1) { plat_sleep_ms(16); continue; }
        dock_metrics(dock,w,h);
        const float S=dock->scale, ox=dock->ox, oy=dock->oy;
        studio_panel p=guide_panel(dock,page);
        const guide_option *list=page==GUIDE_GRAPHICS?guide_graphics:page==GUIDE_MENUS?guide_menus:NULL;
        int rows=list?(page==GUIDE_GRAPHICS?GUIDE_GRAPHICS_N:GUIDE_MENUS_N):page==GUIDE_PRESET?1:0;
        const float col_w=270.f*S, right=p.x+col_w;
        float px=-1,py=-1;
        {
            int mx=0,my=0; SDL_GetMouseState(&mx,&my);
            dock_window_to_pixels(mx,my,&px,&py);
        }
        /* what the mouse is on: a band slot (0-2), a row (10+r), a tab (40+t), the picture (99) */
        int hot=-1, hot_side=0;
        float band_top=oy+363.f*S, strip_top=oy+418.f*S;
        if(py>=band_top && py<strip_top)
            for(int n=0;n<3;n++) {
                float cx=ox+guide_slot_cx(n)*S, half=(DOCK_WIDTH-DOCK_PAD*2.f)/6.f*S;
                if(px>=cx-half && px<cx+half) hot=n;
            }
        if(!full && px>=p.x && px<=p.x+p.w && py>=p.y && py<=p.y+p.h) {
            if(py<p.tab_y+10.f*S) {
                for(int t=0;t<GUIDE_PAGES;t++) {
                    float tx=guide_tab_x(dock,&p,t);
                    if(px>=tx-6.f*S && px<tx+guide_tab_w(dock,t)+6.f*S) hot=40+t;
                }
            } else if(px<right+6.f*S)
                for(int r=0;r<rows;r++) {
                    float ry=p.row0+p.pitch*r;
                    if(py>=ry-p.pitch*.72f && py<ry+p.pitch*.28f) {
                        hot=10+r;
                        hot_side=px>=right-110.f*S?(px<right-40.f*S?-1:1):0;
                    }
                }
            else if(pic[2]>0 && px>=pic[0] && px<pic[0]+pic[2] && py>=pic[1] && py<pic[1]+pic[3]) hot=99;
        }
        if(hot==0 && page==GUIDE_WELCOME) hot=-1; /* no page before the first */
        int action=0, delta=0, page_to=-1;
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            if(ev.type==SDL_MOUSEMOTION) {
                dock->mouse=1;
                dock_window_to_pixels(ev.motion.x,ev.motion.y,&dock->mx,&dock->my);
                if(hot>=0 && hot<3) {sel=hot;focus_band=1;}
                if(hot>=10 && hot<40) {row[page]=hot-10;focus_band=0;}
                if(dragging && pic[2]>0) split=fminf(.98f,fmaxf(.02f,(px-pic[0])/pic[2]));
            }
            if(ev.type==SDL_MOUSEBUTTONUP) dragging=0;
            if(ev.type==SDL_KEYDOWN || ev.type==SDL_CONTROLLERBUTTONDOWN) dock->mouse=0;
            if(ev.type==SDL_QUIT) {keep_running=0;running=0;}
            int key=0; /* 1 up, 2 down, 3 left, 4 right, 5 activate */
            if(ev.type==SDL_KEYDOWN) {
                int k=ev.key.keysym.sym;
                if(k==SDLK_UP) key=1;
                else if(k==SDLK_DOWN) key=2;
                else if(k==SDLK_LEFT) key=3;
                else if(k==SDLK_RIGHT) key=4;
                else if(!ev.key.repeat) {
                    if(k==SDLK_ESCAPE || k==SDLK_F7) action=full?6:3;
                    else if(k==SDLK_RETURN || k==SDLK_KP_ENTER) key=5;
                    else if(k==SDLK_TAB) action=4;
                    else if(k==SDLK_SPACE) action=6;
                    else if(k==SDLK_o) action=7;
                    else if(k==SDLK_BACKSPACE) action=2;
                }
            }
            if(ev.type==SDL_CONTROLLERBUTTONDOWN) {
                int b=ev.cbutton.button;
                if(b==SDL_CONTROLLER_BUTTON_DPAD_UP) key=1;
                if(b==SDL_CONTROLLER_BUTTON_DPAD_DOWN) key=2;
                if(b==SDL_CONTROLLER_BUTTON_DPAD_LEFT) key=3;
                if(b==SDL_CONTROLLER_BUTTON_DPAD_RIGHT) key=4;
                if(b==SDL_CONTROLLER_BUTTON_A) key=5;
                if(b==SDL_CONTROLLER_BUTTON_B) action=full?6:3;
                if(b==SDL_CONTROLLER_BUTTON_X) action=4;
                if(b==SDL_CONTROLLER_BUTTON_Y) action=6;
                if(b==SDL_CONTROLLER_BUTTON_LEFTSHOULDER) action=2;
                if(b==SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) action=1;
                if(b==SDL_CONTROLLER_BUTTON_START) action=7;
            }
            if(ev.type==SDL_MOUSEBUTTONDOWN && ev.button.button==SDL_BUTTON_LEFT) {
                if(hot>=0 && hot<3) {sel=hot;focus_band=1;key=5;}
                if(hot>=10 && hot<40) {row[page]=hot-10;focus_band=0;delta=hot_side?hot_side:1;}
                if(hot>=40) page_to=hot-40;
                if(hot==99) {dragging=1;split=fminf(.98f,fmaxf(.02f,(px-pic[0])/pic[2]));}
                if(full && hot<0) action=4;
            }
            if(ev.type==SDL_MOUSEWHEEL && rows && !full) {
                row[page]=(row[page]-ev.wheel.y+rows*8)%rows;focus_band=0;
            }
            if(key==1) {
                if(focus_band && rows) {focus_band=0;row[page]=rows-1;}
                else if(!focus_band && row[page]>0) row[page]--;
            }
            if(key==2) {
                if(!focus_band && row[page]<rows-1) row[page]++;
                else focus_band=1;
            }
            if((key==3 || key==4) && !focus_band && rows) delta=key==3?-1:1;
            else if(key==3 || key==4) {
                int next=sel+(key==3?-1:1);
                if(next>=0 && next<3 && !(next==0 && page==GUIDE_WELCOME)) sel=next;
            }
            if(key==5) {
                if(!focus_band && rows) delta=1;
                else action=sel==0?(page==GUIDE_READY?7:2):sel==1?3:1;
            }
        }
        if(rows && !focus_band && row[page]>=rows) row[page]=rows-1;
        if(delta && page==GUIDE_PRESET) {
            int n=was==2?3:2;
            preset=(preset+delta+n)%n;
            draft=start;
            if(preset<2) video_settings_preset_values(&draft,preset);
            capture=1;
        } else if(delta && list) {
            const guide_option *o=&list[row[page]];
            int n=guide_value_count(o->label);
            guide_put(&draft,o->field,(guide_get(&draft,o->field)+delta+n)%n);
            capture=1;
        }
        if(page_to>=0 && page_to!=page) {page=page_to;full=0;capture=1;focus_band=1;}
        if(action==3) running=0;
        if(action==7) {commit=1;keep_running=2;running=0;}
        if(action==2 && page>0) {page--;full=0;capture=1;}
        if(action==1) {if(page<GUIDE_READY) {page++;full=0;capture=1;} else {commit=1;running=0;}}
        if(action==4) flip^=1;
        if(action==6 && (page==GUIDE_PRESET || page==GUIDE_GRAPHICS)) {full^=1;flip=1;}
        if(page==GUIDE_WELCOME && sel==0) sel=2;
        dock->ctx=guide_slot_cx(sel); dock->cty=GUIDE_CURSOR_Y; dock->cta=guide_tilt[sel];
        dock_cursor_step(dock,1.f/60.f);
        for(int n=0;n<3;n++) hover[n]+=((n==sel && focus_band?1.f:0.f)-hover[n])*(1-expf(-12.f/60.f));
        if(track && !track_ready && !track_failed) {
            int ready=race_view_poll(track,0);
            if(ready==1) {
                track_ready=1;capture=1;
                race_view_set_pose(track,NULL,NULL);
                race_view_set_camera(track,NULL,NULL,0,0,.2f);
                race_view_camera(track,2.4f,2.f);
                race_view_daylight_time(track,0);
            }
            else if(ready<0) {track_failed=1;track_on=0;}
        }
        if(!running) break;
        p=guide_panel(dock,page);
        list=page==GUIDE_GRAPHICS?guide_graphics:page==GUIDE_MENUS?guide_menus:NULL;
        rows=list?(page==GUIDE_GRAPHICS?GUIDE_GRAPHICS_N:GUIDE_MENUS_N):page==GUIDE_PRESET?1:0;
        /* the two looks being compared: Classic and Enhanced on the preset page; on the
           graphics page the draft with the focused option at its original value, and the draft */
        pc_video_options look_a=draft, look_b=draft;
        int needs=0;
        const float *crop=NULL;
        static const float whole[4]={0,.16f,1,.80f};
        if(page==GUIDE_PRESET) {look_a=classic_look;look_b=enhanced_look;crop=whole;}
        /* one sampling for both sides (and the live scene) unless sampling is what is
           compared: a different AA or scale reallocates the scene targets */
        if(!(page==GUIDE_GRAPHICS && guide_graphics[row[page]].field==offsetof(pc_video_options,antialiasing))) {
            look_a.antialiasing=look_b.antialiasing=draft.antialiasing;
            look_a.render_scale=look_b.render_scale=draft.render_scale;
            look_a.video.upscale=look_b.video.upscale=draft.video.upscale;
        }
        if(page==GUIDE_GRAPHICS) {
            const guide_option *o=&guide_graphics[row[page]];
            guide_put(&look_a,o->field,0);
            /* still at the original: show what Enhanced would put there instead */
            if(!guide_get(&draft,o->field)) guide_put(&look_b,o->field,guide_get(&enhanced_look,o->field));
            /* Sun Rays and Ambient Occlusion live in the HDR daylight pass: compare them with it */
            if(o->field==offsetof(pc_video_options,sun_rays) || o->field==offsetof(pc_video_options,ambient_occlusion)) {
                needs=!(draft.hdr && draft.daylight);
                look_a.hdr=look_a.daylight=look_b.hdr=look_b.daylight=1;
            }
            crop=o->crop;
        }
        static int last_row=-1,last_page=-1;
        if(page!=last_page || row[page]!=last_row) {capture=1;last_page=page;last_row=row[page];}
        const int scene=track_on && track_ready;
        if(crop && (capture || pw!=w || ph!=h) && (track_ready || track_failed)) {
            settle=test?1:8; pending=0; shown=0; retry_hold=0; pw=w; ph=h; capture=0;
        }
        if(last_w>=0 && (w!=last_w || h!=last_h)) {
            for(int j=0;j<3;j++) { plat_texture_destroy(images[j]); images[j]=NULL; }
            settle=0; pending=0; shown=0; retry_hold=0; pw=0; ph=0; capture=1;
        }
        if(!crop || !scene) {capture=0;settle=pending=0;}
        if(settle && !--settle) pending=1;
        /* Only the A/B comparison is frozen. The background itself is the live scene, as in
           the studio, so a capture texture is never the whole screen. */
        const pc_video_options *live=page==GUIDE_WELCOME?&start:&draft;
        if(scene && settle<=0) {
            if(retry_hold>0) retry_hold--;
            else if(pending) {
                int got=guide_capture(&images[pending-1],cars,code,raster,
                                      pending==2?&look_b:&look_a,track,&drawn_look,w,h);
                if(got==1) { if(++pending>2) {pending=0;shown=1;} }
                else if(got<0) retry_hold=4;
            }
        }
        int branch_capture=0,drew_race=0,drew_showroom=0;
        plat_video_begin_frame();
        if(full && shown && images[flip?1:0]) { /* frozen full-screen comparison */
            const uint8_t white[4]={255,255,255,255};
            branch_capture=1;
            plat_video_set_view(640.f,448.f);
            plat_draw_quad(images[flip?1:0],0,0,640.f,448.f,0,1,1,0,white);
        } else { /* live scene every frame, exactly as the studio */
            video_settings_preview_options(live);
            drawn_look=*live;
            if(scene) drew_race=1; else drew_showroom=1;
            video_begin_scene();
            guide_scene(cars,code,raster,scene?track:NULL,1.f/60.f);
            video_end_scene();
        }
        plat_video_set_view((float)w,(float)h);
        const uint8_t head[4]={255,255,255,240}, dim[4]={160,163,170,220}, ink[4]={245,246,248,255},
                      soft[4]={214,216,220,235}, panel[4]={12,13,16,175}, deep[4]={12,13,16,205}, edge[4]={255,255,255,90},
                      hl[4]={255,255,255,30}, gold[4]={255,204,64,255}, well[4]={0,0,0,200};
        /* the corners: a soft dusk over the sky so the wordmark and the status read */
        {
            const uint8_t dusk[4][4]={{0,0,0,150},{0,0,0,150},{0,0,0,0},{0,0,0,0}};
            plat_draw_quad4(NULL,0,0,(float)w,p.y+20.f*S,0,0,1,1,dusk);
        }
        const float pad=16.f*S;
        guide_text(dock,"MOON ENGINE",pad,pad+22.f*S,1.9f,0,ink);
        guide_text(dock,"First drive",pad+1.f*S,pad+36.f*S,.7f,0,soft);
        {
            float y=pad+12.f*S, rx=(float)w-pad;
            const char *state=track_failed?GUIDE_COURSE_NAME " unavailable on this disc"
                             :track_ready?GUIDE_COURSE_NAME:"Loading " GUIDE_COURSE_NAME " from your disc...";
            const uint8_t lamp[4]={track_failed?205:track_ready?40:230,track_failed?50:170,
                                   track_failed?45:track_ready?80:40,255};
            float tw=dock_text_width(&dock->font,state)*DOCK_NAME_SCALE*.78f*S;
            guide_rect(rx-tw-12.f*S,y-7.f*S,7.f*S,7.f*S,lamp);
            guide_text(dock,state,rx,y,.78f,2,ink);
            guide_text(dock,car_title,rx,y+15.f*S,.7f,2,soft);
            guide_text(dock,"Previews only until Done",rx,y+30.f*S,.7f,2,soft);
        }
        pic[0]=pic[1]=pic[2]=pic[3]=0;
        if(full) {
            /* the chosen look on the whole screen; Tab flips to the other */
            const char *name=page==GUIDE_PRESET?(flip?"ENHANCED":"CLASSIC")
                            :flip?guide_value_name(guide_graphics[row[page]].label,guide_get(&look_b,guide_graphics[row[page]].field))
                            :guide_value_name(guide_graphics[row[page]].label,0);
            char line[160];
            snprintf(line,sizeof line,"%s%s%s",page==GUIDE_GRAPHICS?guide_graphics[row[page]].label:"Preset",
                     page==GUIDE_GRAPHICS?": ":": ",name);
            guide_text(dock,line,pad,p.y+18.f*S,1.2f,0,flip?ink:gold);
            guide_text(dock,!flip?"Original":page==GUIDE_GRAPHICS && !guide_get(&draft,guide_graphics[row[page]].field)
                      ?"Enhanced's value, not chosen yet":"Your choice",pad,p.y+36.f*S,.8f,0,soft);
        } else {
        /* the OPTIONS glass: tabs for the pages, the open one lit and underlined */
        guide_rect(p.x,p.y,p.w,p.h,page==GUIDE_WELCOME?panel:deep);
        guide_rect(p.x,p.y,p.w,fmaxf(1.f,.7f*S),edge);
        for(int t=0;t<GUIDE_PAGES;t++) {
            float tx=guide_tab_x(dock,&p,t), tw=guide_tab_w(dock,t);
            int lit=hot==40+t && t!=page;
            if(lit) {
                const uint8_t box[4]={255,255,255,30}, line[4]={255,255,255,110};
                guide_rect(tx-6.f*S,p.tab_y-15.f*S,tw+12.f*S,24.f*S,box);
                guide_rect(tx,p.tab_y+5.f*S,tw,fmaxf(1.f,S),line);
            }
            dock_text(dock,guide_tabs[t],tx,p.tab_y,.9f,0,t==page || lit?head:dim);
            if(t==page) guide_rect(tx,p.tab_y+5.f*S,tw,fmaxf(1.f,S),head);
        }
        {
            /* the page's own icon from the game's Options, at the tab row's end */
            dock_icon *ic=&stage_icons[page];
            if(ic->tex) {
                float ciw=ic->w*(ic->u1-ic->u0), cih=ic->h*(ic->v1-ic->v0);
                float k=fminf(52.f*S/fmaxf(1.f,ciw),24.f*S/fmaxf(1.f,cih));
                plat_draw_quad(ic->tex,p.x+p.w-14.f*S-ciw*k,p.tab_y-4.f*S-cih*k*.5f,ciw*k,cih*k,
                               ic->u0,ic->v0,ic->u1,ic->v1,head);
            }
        }
        guide_rect(p.x+8.f*S,p.tab_y+12.f*S,p.w-16.f*S,fmaxf(1.f,.5f*S),hl);
        const float tx=p.x+14.f*S, wide=p.w-28.f*S, rx0=right+18.f*S, rw=p.x+p.w-14.f*S-rx0;
        const float lead=14.f*S;
        if(page==GUIDE_WELCOME) {
            dock_text(dock,"Welcome to Moon Engine.",tx,p.row0,1.f,0,head);
            float y=guide_wrap(dock,"Gran Turismo 4 running natively on your PC, read from your own disc. "
                               "The game itself is unchanged: the same physics, cars and saves.",
                               tx,p.row0+22.f*S,wide,.8f,dim,lead);
            y=guide_wrap(dock,"What Moon Engine adds - sharper graphics, mouse and keyboard, widescreen - "
                         "is a set of options, each of which can be turned back to the PS2 original. "
                         "This guide shows every one against the original and lets you choose. "
                         "Nothing is saved until Done; F7 brings the guide back later.",
                         tx,y+6.f*S,wide,.8f,dim,lead);
        } else if(page==GUIDE_READY) {
            const char *preset_name=preset==0?"Classic":preset==1?"Enhanced":"Your own settings";
            int changed=0;
            pc_video_options base=start;
            if(preset<2) video_settings_preset_values(&base,preset);
            char line[200];
            dock_text(dock,"Ready for the first drive.",tx,p.row0,1.f,0,head);
            snprintf(line,sizeof line,"Starting point:  %s",preset_name);
            dock_text(dock,line,tx,p.row0+24.f*S,.8f,0,head);
            float y=p.row0+42.f*S;
            for(int g=0;g<2;g++) {
                const guide_option *l=g?guide_menus:guide_graphics;
                int n=g?GUIDE_MENUS_N:GUIDE_GRAPHICS_N;
                for(int i=0;i<n;i++) {
                    int b=guide_get(&base,l[i].field), v=guide_get(&draft,l[i].field);
                    if(b==v) continue;
                    if(changed<8) {
                        snprintf(line,sizeof line,"%s:  %s  >  %s",l[i].label,guide_value_name(l[i].label,b),guide_value_name(l[i].label,v));
                        dock_text(dock,line,tx+12.f*S,y,.75f,0,soft);
                        y+=lead;
                    }
                    changed++;
                }
            }
            if(!changed) {dock_text(dock,"No option changed from the starting point.",tx+12.f*S,y,.75f,0,dim);y+=lead;}
            else if(changed>8) {snprintf(line,sizeof line,"... and %d more.",changed-8);dock_text(dock,line,tx+12.f*S,y,.75f,0,dim);y+=lead;}
            if(changed && preset<2) {
                snprintf(line,sizeof line,"Options will show the preset as %s (modified).",preset_name);
                dock_text(dock,line,tx+12.f*S,y,.75f,0,dim);y+=lead;
            }
            y+=8.f*S;
            guide_rect(tx,y-12.f*S,wide,fmaxf(1.f,.5f*S),hl);
            dock_text(dock,"Sound",tx,y+2.f*S,.85f,0,head);
            y=guide_wrap(dock,"F8 opens the launcher's mixer and M turns its music on or off. "
                         "The game's own music and effects volumes are under Options, Sound.",
                         tx,y+18.f*S,wide,.8f,dim,lead);
            dock_text(dock,"Later",tx,y+8.f*S,.85f,0,head);
            guide_wrap(dock,"Every choice here is in Options, one by one, with the Classic preset to go back "
                       "to the PS2 game. F7 on the launcher opens this guide again.",
                       tx,y+24.f*S,wide,.8f,dim,lead);
        } else {
            /* the rows, OPTIONS style, in the left column */
            guide_rect(right+8.f*S,p.tab_y+18.f*S,fmaxf(1.f,.5f*S),p.y+p.h-p.tab_y-26.f*S,hl);
            for(int r=0;r<rows;r++) {
                float ry=p.row0+p.pitch*r;
                int on=!focus_band && row[page]==r;
                const char *label=page==GUIDE_PRESET?"Start From":list[r].label;
                char v[80];
                if(page==GUIDE_PRESET) snprintf(v,sizeof v,"<  %s  >",preset==0?"Classic":preset==1?"Enhanced":"Your Settings");
                else snprintf(v,sizeof v,"<  %s  >",guide_value_name(list[r].label,guide_get(&draft,list[r].field)));
                if(on) guide_rect(p.x+4.f*S,ry-p.pitch*.72f,right-p.x,p.pitch*.95f,hl);
                int changed=page!=GUIDE_PRESET && guide_get(&draft,list[r].field)!=guide_get(&start,list[r].field);
                dock_text(dock,label,tx,ry,.8f,0,on?head:dim);
                if(changed) guide_rect(tx-8.f*S,ry-7.f*S,3.f*S,7.f*S,gold);
                int arrow=on && dock->mouse && hot==10+r ? hot_side : 0;
                float vw=dock_text_width(&dock->font,v)*DOCK_NAME_SCALE*.8f*S,
                      aw=dock_text_width(&dock->font,"<")*DOCK_NAME_SCALE*.8f*S;
                if(arrow) {
                    const uint8_t box[4]={255,255,255,46};
                    guide_rect((arrow<0?right-vw:right-aw)-5.f*S,ry-p.pitch*.68f,aw+10.f*S,p.pitch*.86f,box);
                }
                dock_text(dock,v,right,ry,.8f,2,on?head:dim);
                if(arrow<0) dock_text(dock,"<",right-vw,ry,.8f,0,gold);
                if(arrow>0) dock_text(dock,">",right,ry,.8f,2,gold);
            }
            /* the right column: the comparison and what to look at */
            float y=p.row0-12.f*S;
            const char *title, *what, *look="", *cost="";
            char named[2][96];
            if(page==GUIDE_PRESET) {
                title=preset==0?"Classic: the PS2 game":preset==1?"Enhanced: the recommended improvements":"Your settings, as they are now";
                what=preset==0?"Every Moon Engine option at its original value: the PS2 picture, the pad-only menus "
                               "and 4:3. The reference the port is measured against."
                     :preset==1?"Every improvement on: modern lighting, sharper reflections and shadows, daylight, HDR, "
                                "anti-aliasing, mouse and keyboard. The physics are the same in both."
                     :"You have already changed options; they stay as they are. Pick Classic or Enhanced to start over from one.";
                look="Left of the line Classic, right Enhanced. Drag the line, or Space for full screen.";
                snprintf(named[0],sizeof named[0],"CLASSIC");snprintf(named[1],sizeof named[1],"ENHANCED");
            } else {
                const guide_option *o=&list[row[page]];
                title=o->label;what=o->what;look=o->look;cost=o->cost;
                snprintf(named[0],sizeof named[0],"ORIGINAL");
                snprintf(named[1],sizeof named[1],"%s",guide_value_name(o->label,guide_get(&look_b,o->field)));
                for(char *c=named[1];*c;c++) if(*c>='a' && *c<='z') *c=(char)(*c-'a'+'A');
            }
            if(crop && shown && images[0] && images[1]) {
                float ih_box=fminf(rw*.52f,p.y+p.h-y-118.f*S);
                guide_rect(rx0-2.f*S,y-2.f*S,rw+4.f*S,ih_box+4.f*S,well);
                guide_compare(images[0],images[1],track_on && track_ready?640.f:(float)raster,448.f,crop,rx0,y,rw,ih_box,split,pic);
                if(pic[2]>0) {
                    guide_text(dock,named[0],pic[0]+6.f*S,pic[1]+14.f*S,.7f,0,ink);
                    guide_text(dock,named[1],pic[0]+pic[2]-6.f*S,pic[1]+14.f*S,.7f,2,ink);
                    if(needs)
                        guide_text(dock,"Both sides with HDR and Daylight on: it needs them; choosing it turns them on.",
                                   pic[0]+pic[2]*.5f,pic[1]+pic[3]-22.f*S,.65f,1,gold);
                    if(page==GUIDE_GRAPHICS && guide_get(&draft,list[row[page]].field)==0)
                        guide_text(dock,"Right: Enhanced's value, not chosen. Right / Left to choose it.",
                                   pic[0]+pic[2]*.5f,pic[1]+pic[3]-8.f*S,.65f,1,gold);
                }
                y+=ih_box+18.f*S;
            } else if(crop) { /* the same box, waiting for the selection to rest */
                if(!scene && track_failed) dock_text(dock,"Comparisons need the course from the disc.",rx0,y+14.f*S,.75f,0,dim);
                float ih_box=fminf(rw*.52f,p.y+p.h-y-118.f*S);
                guide_rect(rx0-2.f*S,y-2.f*S,rw+4.f*S,ih_box+4.f*S,well);
                dock_text(dock,"...",rx0+rw*.5f,y+ih_box*.5f,1.f,1,dim);
                y+=ih_box+18.f*S;
            }
            dock_text(dock,title,rx0,y,.95f,0,head);
            y=guide_wrap(dock,what,rx0,y+17.f*S,rw,.75f,dim,13.f*S);
            if(page==GUIDE_MENUS)
                y=guide_wrap(dock,"It shows in the game's own menus once you press Start, not in this scene. "
                             "On: as Enhanced. Off: as the PS2.",rx0,y+3.f*S,rw,.75f,soft,13.f*S);
            if(look[0]) {
                char line[220];
                snprintf(line,sizeof line,"%s%s",page==GUIDE_PRESET?"":"Look at: ",look);
                y=guide_wrap(dock,line,rx0,y+3.f*S,rw,.75f,soft,13.f*S);
            }
            if(cost[0]) {
                char line[160];
                snprintf(line,sizeof line,"Cost: %s",cost);
                guide_wrap(dock,line,rx0,y+3.f*S,rw,.75f,soft,13.f*S);
            }
        }
        }
        /* the dealers' metal band with three actions, the game's black strip under it */
        {
            const uint8_t bedge[4]={255,255,255,110}, dark[4]={0,0,0,240}, seam[4]={70,72,76,255},
                          metal[4]={255,255,255,240};
            if(dock_band.tex) plat_draw_quad(dock_band.tex,0,band_top,(float)w,strip_top-band_top,0,.30f,1,.62f,metal);
            else { const uint8_t band[4]={38,40,44,230}; guide_rect(0,band_top,(float)w,strip_top-band_top,band); }
            guide_rect(0,band_top,(float)w,fmaxf(1.f,S*.7f),bedge);
            guide_rect(0,strip_top,(float)w,(float)h-strip_top,dark);
            guide_rect(0,strip_top,(float)w,fmaxf(1.f,S*.5f),seam);
        }
        const char *names[3]={page==GUIDE_READY?"OPTIONS":"BACK","SKIP",page==GUIDE_READY?"DONE":"NEXT"};
        for(int n=0;n<3;n++) {
            int off=n==0 && page==GUIDE_WELCOME;
            uint8_t col[4]={255,255,255,(uint8_t)(off?60:150+105*hover[n])};
            float size=1.f+.12f*hover[n];
            guide_text(dock,names[n],ox+guide_slot_cx(n)*S,oy+(GUIDE_NAME_Y-2.f*hover[n])*S,size,1,col);
        }
        const char *band_help[3]={page==GUIDE_READY?"Options: Save these choices and open Options.":"Back: The page before.",
                                  "Skip: Back to the launcher. Nothing is saved.",
                                  page==GUIDE_READY?"Done: Save these choices and go back to the launcher.":"Next: The next page."};
        char row_help[200]="";
        if(!focus_band && list) {
            snprintf(row_help,sizeof row_help,"%s: Left / Right to change.%s",list[row[page]].label,
                     page==GUIDE_GRAPHICS?" Space: full screen; drag the line to compare.":"");
        }
        const char *caption=track_failed && page!=GUIDE_MENUS && page!=GUIDE_READY?"The course could not be read; the comparisons use the launcher's car."
                           :!track_ready && !track_failed?"Loading " GUIDE_COURSE_NAME " and the Skyline from your disc..."
                           :full?"Tab / X: original or your choice     Space / Escape: back to the guide"
                           :dock->mouse && hot>=0 && hot<3?band_help[hot]
                           :row_help[0]?row_help
                           :page==GUIDE_PRESET && !focus_band?"Start From: Left / Right to choose. Space: full screen."
                           :focus_band && sel!=2?band_help[sel]:guide_captions[page];
        dock_caption(dock,caption,w*.5f,oy+436.f*S,soft);
        if(dock_arrow.tex) {
            const uint8_t opaque[4]={255,255,255,255};
            if(dock->mouse && (px>0 || py>0))
                plat_draw_rotated(dock_arrow.tex,px-16.f*S,py,32.f*S,32.f*S,px,py,0,opaque);
            else if(focus_band)
                plat_draw_rotated(dock_arrow.tex,ox+dock->cx*S-16.f*S,oy+dock->cy*S,32.f*S,32.f*S,
                                  ox+dock->cx*S,oy+dock->cy*S,dock->ca,opaque);
        }
        if(test && (track_ready || track_failed) && frame>=8 && !capture && !settle && !pending) {
            if(!guide_script_step(&script,page)) { /* the script ran out: leave unchanged */
                SDL_Event key={0};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_ESCAPE;SDL_PushEvent(&key);
            }
            frame=0;
        }
        plat_video_end_frame();
        if(last_w>=0 && (w!=last_w || h!=last_h)) {
            int sw=0,sh=0;
            video_scene_target_size(&sw,&sh);
            gl_log("guide: drawable %dx%d -> %dx%d view=%dx%d raster=%d scene=%dx%d branch=%s "
                   "race_view_draw=%d showroom_draw=%d",
                   last_w,last_h,w,h,w,h,raster,sw,sh,
                   branch_capture?"backdrop-image":"live-3d",drew_race,drew_showroom);
        }
        last_w=w;last_h=h;
        {
            uint64_t now=plat_time_ns();
            if(frame_clock && track_ready && test) {
                double ms=(now-frame_clock)/1e6;
                worst_ms=fmax(worst_ms,ms);
                if(ms>40) printf("studio guide: slow frame %.1f ms page %d row %d pending %d full %d\n",ms,page,row[page],pending,full);
            }
            frame_clock=now;
        }
        if(!test || track_ready || track_failed) frame++;
        plat_sleep_ms(16);
    }
    if(test) printf("studio guide: worst frame %.1f ms\n",worst_ms);
    for(int n=0;n<GUIDE_PAGES;n++) plat_texture_destroy(stage_icons[n].tex);
    race_view_close(track);
    for(int j=0;j<3;j++) plat_texture_destroy(images[j]);
    *dock=before_dock;
    if(commit && keep_running) {
        int changes=guide_commit(preset,&draft);
        printf("studio guide: done, %d change(s) saved; preset %s%s\n",changes,vs->store.preset,
               vs->store.modified?" (modified)":"");
        return keep_running;
    }
    video_settings_preview_graphics(vs,0);
    /* No preference/store writes occurred; callers resume the same studio. */
    if(memcmp(&start,&vs->current,sizeof start) ||
       memcmp(&before_store,&vs->store,sizeof before_store)) {
        fprintf(stderr,"studio guide: unexpected preference mutation\n");
        return 0;
    }
    printf("studio guide: dismissed, graphics restored; preferences unchanged\n");
    return keep_running;
}
/* P: the guide in OPTIONS. INTERFACE and QUALITY show the focused option in the guide's right
   column: a graphics option as on the GRAPHICS page - the same frozen instant of the course at
   the original value and at the current one, split by a divider the mouse can drag - and every
   covered option with the guide's own words. Options apply at once, so the right side is the
   player's current value (Enhanced's when it is still at the original). The course is read
   when the column first needs it and closed with the panel. The room keeps the player's own
   look: each capture is restored at once, and only the launcher's frame is presented. */
/* The comparison is a short film, not one instant: the camera swings around the car while
   both sides are recorded, then the two loops play together under the divider (as settings
   screens preview an option with a clip). Recorded once per look, one frame per launcher
   frame; nothing is drawn while it plays. */
#define GUIDE_CLIP 16
#define GUIDE_CLIP_FPS 10.f
/* the swing around the car from the still's angle, in radians (race_view_camera adds) */
static float guide_clip_orbit(int f)
{
    return -.3f + .6f * (float)f / (GUIDE_CLIP - 1);
}
/* Each film is kept like the Options' prints: next to launcher.ini (launcher-comparisons/), one
   file an option with its look's key inside. A film whose key still matches is read back on a
   thread instead of filmed again; a new one is written on a thread once filmed. */
#include <ctype.h>
#define GUIDE_CLIP_MAGIC "MOONCLP1"
typedef struct {
    SDL_Thread *thread;
    SDL_atomic_t done;
    char path[LAUNCHER_PATH + 64];
    uint64_t key;
    int w, h, screen, ok;
    uint8_t *px[2][GUIDE_CLIP];
} guide_clip_job;
static SDL_atomic_t guide_clip_writing; /* films still being written; closing waits */
static void guide_clip_free(guide_clip_job *j)
{
    if (!j) return;
    for (int s = 0; s < 2; s++)
        for (int f = 0; f < GUIDE_CLIP; f++) free(j->px[s][f]);
    free(j);
}
static int guide_clip_write(void *data)
{
    guide_clip_job *j = data;
    char dir[LAUNCHER_PATH + 64], tmp[LAUNCHER_PATH + 72];
    snprintf(dir, sizeof dir, "%s", j->path);
    char *cut = strrchr(dir, '/');
    if (cut) *cut = 0, gt4_save_mkdir(dir);
    snprintf(tmp, sizeof tmp, "%s.tmp", j->path);
    FILE *f = fopen(tmp, "wb");
    int head[4] = {j->w, j->h, j->screen, GUIDE_CLIP};
    int ok = f && fwrite(GUIDE_CLIP_MAGIC, 8, 1, f) == 1 && fwrite(&j->key, 8, 1, f) == 1 &&
             fwrite(head, sizeof head, 1, f) == 1;
    mz_ulong raw = (mz_ulong)j->w * j->h * 4, bound = mz_compressBound(raw);
    uint8_t *z = ok ? malloc(bound) : NULL;
    ok = ok && z;
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < GUIDE_CLIP && ok; i++) {
            mz_ulong packed = bound;
            uint32_t len;
            ok = mz_compress2(z, &packed, j->px[s][i], raw, 1) == MZ_OK;
            len = (uint32_t)packed;
            ok = ok && fwrite(&len, 4, 1, f) == 1 && fwrite(z, packed, 1, f) == 1;
        }
    free(z);
    if (f && fclose(f)) ok = 0;
    remove(j->path);
    if (!ok || rename(tmp, j->path)) remove(tmp);
    guide_clip_free(j);
    SDL_AtomicAdd(&guide_clip_writing, -1);
    return 0;
}
static int guide_clip_read(void *data)
{
    guide_clip_job *j = data;
    FILE *f = fopen(j->path, "rb");
    char magic[8];
    uint64_t key = 0;
    int head[4] = {0};
    int ok = f && fread(magic, 8, 1, f) == 1 && !memcmp(magic, GUIDE_CLIP_MAGIC, 8) &&
             fread(&key, 8, 1, f) == 1 && key == j->key && fread(head, sizeof head, 1, f) == 1 &&
             head[0] > 0 && head[0] <= 8192 && head[1] > 0 && head[1] <= 8192 &&
             head[3] == GUIDE_CLIP;
    mz_ulong raw = ok ? (mz_ulong)head[0] * head[1] * 4 : 0, bound = mz_compressBound(raw);
    uint8_t *z = ok ? malloc(bound) : NULL;
    ok = ok && z;
    j->w = head[0], j->h = head[1], j->screen = head[2];
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < GUIDE_CLIP && ok; i++) {
            uint32_t len = 0;
            mz_ulong out = raw;
            ok = fread(&len, 4, 1, f) == 1 && len > 0 && len <= bound && fread(z, len, 1, f) == 1 &&
                 (j->px[s][i] = malloc(raw)) != NULL &&
                 mz_uncompress(j->px[s][i], &out, z, len) == MZ_OK && out == raw;
        }
    free(z);
    if (f) fclose(f);
    j->ok = ok;
    SDL_AtomicSet(&j->done, 1);
    return 0;
}
/* the option's file, from launcher.ini's folder; 0 without one */
static int guide_clip_path(char *out, size_t size, const char *label)
{
    if (!studio_prefs || !studio_prefs->file[0]) return 0;
    char dir[LAUNCHER_PATH], name[64];
    snprintf(dir, sizeof dir, "%s", studio_prefs->file);
    char *cut = strrchr(dir, '/'), *back = strrchr(dir, '\\');
    if (back && (!cut || back > cut)) cut = back;
    if (cut) *cut = 0;
    else snprintf(dir, sizeof dir, ".");
    int n = 0;
    for (const char *c = label; *c && n < (int)sizeof name - 1; c++)
        if (isalnum((unsigned char)*c)) name[n++] = (char)tolower((unsigned char)*c);
        else if (n && name[n - 1] != '-') name[n++] = '-';
    name[n] = 0;
    snprintf(out, size, "%s/launcher-comparisons/%s.clip", dir, name);
    return 1;
}
static uint64_t guide_clip_key(const char *label, const pc_video_options *a,
                               const pc_video_options *b, int w, int h)
{
    uint64_t k = 1469598103934665603ull;
    const int size[3] = {w, h, GUIDE_CLIP};
    const void *part[4] = {label, a, b, size};
    const size_t len[4] = {strlen(label), sizeof *a, sizeof *b, sizeof size};
    for (int p = 0; p < 4; p++)
        for (size_t i = 0; i < len[p]; i++) k = (k ^ ((const uint8_t *)part[p])[i]) * 1099511628211ull;
    return k;
}
typedef struct {
    race_view *track;
    int ready, failed, want, settle, pending, frame, shown, retry_hold, id, dragging, w, h;
    float orbit; /* the swing applied to the course's camera now */
    plat_texture *images[2][GUIDE_CLIP];
    pc_video_options a, b, drawn;
    float split, pic[4];
    /* the kept film: its key and file, a read on its way, the frames filmed for the file */
    uint64_t key;
    int seek, pw, ph, pscreen;
    char path[LAUNCHER_PATH + 64];
    guide_clip_job *load;
    uint8_t *px[2][GUIDE_CLIP];
} guide_options_state;
static guide_options_state guide_options = {.split = .5f, .id = -1};
static void guide_options_drop_frames(guide_options_state *g)
{
    for (int s = 0; s < 2; s++)
        for (int f = 0; f < GUIDE_CLIP; f++) {
            free(g->px[s][f]);
            g->px[s][f] = NULL;
        }
}
/* the guide's entry for an Options row: graphics (1), menus (0), or none */
static const guide_option *guide_option_for(int id, int *graphics)
{
    if (id < 0 || id >= video_setting_count()) return NULL;
    const char *label = video_setting(id)->label;
    for (int i = 0; i < GUIDE_GRAPHICS_N; i++)
        if (!strcmp(guide_graphics[i].label, label)) {
            *graphics = 1;
            return &guide_graphics[i];
        }
    for (int i = 0; i < GUIDE_MENUS_N; i++)
        if (!strcmp(guide_menus[i].label, label)) {
            *graphics = 0;
            return &guide_menus[i];
        }
    return NULL;
}
static void guide_options_close(void)
{
    guide_options_state *g = &guide_options;
    race_view_close(g->track);
    if (g->load) {
        SDL_WaitThread(g->load->thread, NULL);
        guide_clip_free(g->load);
    }
    guide_options_drop_frames(g);
    for (int waited = 0; SDL_AtomicGet(&guide_clip_writing) && waited < 20000; waited += 10)
        SDL_Delay(10);
    for (int j = 0; j < 2; j++)
        for (int f = 0; f < GUIDE_CLIP; f++) plat_texture_destroy(g->images[j][f]);
    float split = g->split;
    memset(g, 0, sizeof *g);
    g->split = split;
    g->id = -1;
}
/* the two looks, as the GRAPHICS page builds them from its draft */
static void guide_options_looks(const guide_option *o, pc_video_options *a, pc_video_options *b)
{
    const pc_video_options *now = &app_video_settings->current;
    pc_video_options enhanced = *now;
    video_settings_preset_values(&enhanced, 1);
    *a = *b = *now;
    guide_put(a, o->field, 0);
    if (!guide_get(now, o->field)) guide_put(b, o->field, guide_get(&enhanced, o->field));
    if (o->field != offsetof(pc_video_options, antialiasing)) {
        a->antialiasing = b->antialiasing = now->antialiasing;
        a->render_scale = b->render_scale = now->render_scale;
        a->video.upscale = b->video.upscale = now->video.upscale;
    }
    if (o->field == offsetof(pc_video_options, sun_rays) ||
        o->field == offsetof(pc_video_options, ambient_occlusion))
        a->hdr = a->daylight = b->hdr = b->daylight = 1;
}
/* Before the launcher's frame: reads the course, then, once the row has rested, one side per
   frame. A side whose look rebuilds a render target is drawn twice (the first settles it). */
static void guide_options_step(showroom *cars, const char *code, int raster, int id, int w, int h)
{
    guide_options_state *g = &guide_options;
    int graphics = 0;
    const guide_option *o = app_video_settings ? guide_option_for(id, &graphics) : NULL;
    g->want = o && graphics;
    if (!g->want) return;
    pc_video_options a, b;
    guide_options_looks(o, &a, &b);
    if (id != g->id || w != g->w || h != g->h || memcmp(&a, &g->a, sizeof a) ||
        memcmp(&b, &g->b, sizeof b)) {
        /* A capture made against the old drawable is stale: its texture reads back empty once
           the window changed size, so it is thrown away rather than shown. */
        if (w != g->w || h != g->h)
            for (int s = 0; s < 2; s++)
                for (int f = 0; f < GUIDE_CLIP; f++) {
                    plat_texture_destroy(g->images[s][f]);
                    g->images[s][f] = NULL;
                }
        g->id = id, g->w = w, g->h = h, g->a = a, g->b = b;
        g->settle = 8, g->pending = g->shown = g->frame = 0, g->retry_hold = 0;
        guide_options_drop_frames(g);
        g->key = guide_clip_key(o->label, &a, &b, w, h);
        g->seek = guide_clip_path(g->path, sizeof g->path, o->label);
        if (!g->seek) g->path[0] = 0;
    }
    /* the kept film first: read on a thread, its frames made textures again here */
    if (g->load) {
        if (!SDL_AtomicGet(&g->load->done)) return;
        guide_clip_job *j = g->load;
        SDL_WaitThread(j->thread, NULL);
        g->load = NULL;
        if (j->ok && j->key == g->key) {
            for (int s = 0; s < 2; s++)
                for (int f = 0; f < GUIDE_CLIP; f++) {
                    plat_texture_destroy(g->images[s][f]);
                    g->images[s][f] = video_grab_restore(j->w, j->h, j->px[s][f], j->screen);
                }
            g->shown = 1, g->seek = g->settle = g->pending = 0;
            printf("studio options: comparison %s from the cache\n", o->label);
        }
        guide_clip_free(j);
        if (g->shown) return;
    }
    if (g->seek) {
        g->seek = 0;
        FILE *f = fopen(g->path, "rb");
        guide_clip_job *j = f ? calloc(1, sizeof *j) : NULL;
        if (f) fclose(f);
        if (j) {
            snprintf(j->path, sizeof j->path, "%s", g->path);
            j->key = g->key;
            j->thread = SDL_CreateThread(guide_clip_read, "launcher comparison", j);
            if (j->thread) {
                g->load = j;
                return;
            }
            free(j);
        }
    }
    /* filmed: the course opens only now, so a kept film needs no disc read */
    if (g->shown || g->failed) return;
    if (!g->track) {
        g->track = race_view_open(studio_disc);
        if (g->track) race_view_set_parts(g->track, "", "");
        if (!g->track || !race_view_request(g->track, GUIDE_COURSE, GUIDE_CAR, 0)) {
            g->failed = 1;
            printf("studio options: the comparison course is unavailable\n");
            return;
        }
    }
    if (!g->ready) {
        int ready = race_view_poll(g->track, 0);
        if (ready < 0) g->failed = 1;
        if (ready != 1) return;
        g->ready = 1;
        race_view_set_pose(g->track, NULL, NULL);
        race_view_set_camera(g->track, NULL, NULL, 0, 0, .2f);
        race_view_camera(g->track, 2.4f, 2.f);
        race_view_daylight_time(g->track, 0);
        g->drawn = app_video_settings->current;
    }
    if (g->settle && !--g->settle) g->pending = 1;
    if (!g->pending) return;
    if (g->retry_hold > 0) { g->retry_hold--; return; }
    const pc_video_options *look = g->pending == 2 ? &g->b : &g->a;
    plat_texture **into = &g->images[g->pending - 1][g->frame];
    race_view_camera(g->track, guide_clip_orbit(g->frame) - g->orbit, 0);
    g->orbit = guide_clip_orbit(g->frame);
    int got = guide_capture(into, cars, code, raster, look, g->track, &g->drawn, g->w, g->h);
    if (got == 0)
        got = guide_capture(into, cars, code, raster, look, g->track, &g->drawn, g->w, g->h);
    if (got < 0) { g->retry_hold = 4; return; }
    if (got == 0) return;
    if (g->path[0]) {
        int pw = 0, ph = 0, screen = 0;
        uint8_t **px = &g->px[g->pending - 1][g->frame];
        free(*px);
        *px = video_grab_pixels(*into, &pw, &ph, &screen);
        if (g->pending == 1 && !g->frame) g->pw = pw, g->ph = ph, g->pscreen = screen;
        else if (pw != g->pw || ph != g->ph) free(*px), *px = NULL;
    }
    if (++g->frame == GUIDE_CLIP) {
        g->frame = 0;
        if (++g->pending > 2) {
            g->pending = 0;
            g->shown = 1;
            printf("studio options: comparison %s (%d frames)\n", o->label, GUIDE_CLIP);
            /* kept for the next time: every frame read back, written on a thread */
            int all = g->path[0] != 0;
            for (int s = 0; s < 2; s++)
                for (int f = 0; f < GUIDE_CLIP; f++) all = all && g->px[s][f];
            guide_clip_job *j = all ? calloc(1, sizeof *j) : NULL;
            if (j) {
                snprintf(j->path, sizeof j->path, "%s", g->path);
                j->key = g->key, j->w = g->pw, j->h = g->ph, j->screen = g->pscreen;
                memcpy(j->px, g->px, sizeof j->px);
                memset(g->px, 0, sizeof g->px);
                SDL_AtomicAdd(&guide_clip_writing, 1);
                SDL_Thread *t = SDL_CreateThread(guide_clip_write, "launcher comparison", j);
                if (t) SDL_DetachThread(t);
                else guide_clip_free(j), SDL_AtomicAdd(&guide_clip_writing, -1);
            }
            guide_options_drop_frames(g);
        }
    }
    video_settings_preview_graphics(app_video_settings, 0);
    g->drawn = app_video_settings->current;
}
/* a shot waits while the focused comparison is still coming */
static int guide_options_busy(void)
{
    return guide_options.want && !guide_options.failed && !guide_options.shown;
}
/* the divider: a press on the picture takes it, the motion moves it */
static int guide_options_mouse(const SDL_Event *ev, float px, float py)
{
    guide_options_state *g = &guide_options;
    const float *r = g->pic;
    if (ev->type == SDL_MOUSEBUTTONUP) {
        int was = g->dragging;
        g->dragging = 0;
        return was;
    }
    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT && r[2] > 0 &&
        px >= r[0] && px < r[0] + r[2] && py >= r[1] && py < r[1] + r[3])
        g->dragging = 1;
    if (!g->dragging || r[2] <= 0) return 0;
    g->split = fminf(.98f, fmaxf(.02f, (px - r[0]) / r[2]));
    return 1;
}
/* the help strip: the guide's instruction for a compared option, else the Options' line */
static const char *guide_options_help(int id)
{
    static char line[200];
    int graphics = 0;
    const guide_option *o = guide_option_for(id, &graphics);
    if (!o || !graphics) return studio_row_help(id);
    snprintf(line, sizeof line, "%s: Left / Right to change. Drag the line to compare.", o->label);
    return line;
}
#include "launcher_renders.inc"
#include "launcher_prints.inc"
#include "launcher_visuals.inc"
/* Keep the description title below the tab divider; trace actual font ink for captures. */
static void guide_title(const studio_dock *d, const char *title, float x, float y, float w,
                        const uint8_t col[4])
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float size = studio_text_fit(d, title, .95f, w);
    dock_text(d, title, x, y, size, 0, col);
    if (getenv("MOONSTUDIO_LAYOUT_TRACE")) {
        float top = y;
        for (const char *c = title; *c;) {
            const gt4_glyph *glyph = gt4_font_glyph(&d->font, gt4_text_next(&c));
            if (glyph && glyph->w && glyph->h)
                top = fminf(top, y + glyph->y * DOCK_NAME_SCALE * size * S);
        }
        studio_panel panel = studio_panel_at(d, 0);
        fprintf(stderr, "STUDIO_GUIDE_TOP %.3f %.3f\n", top, panel.tab_y + 12.f * S);
    }
}
/* the column's words under the picture; NULL colours measure. Returns their height. */
/* compact: a short column (a small window) keeps what and when, without Look at and Cost;
   note: the comparison's own remark, in gold under the picture's words */
static float guide_options_words(const studio_dock *d, const guide_option *o, int graphics, int id,
                                 float x, float y, float w, const uint8_t *const cols[3],
                                 int compact, const char *note)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE, y0 = y;
    /* a launcher row's help line is "Name: what it does"; the column shows the name above */
    const char *help = studio_row_help(id), *colon = strstr(help, ": ");
    if (id >= 0 && id < video_setting_count()) help = video_setting(id)->help;
    else if (colon) help = colon + 2;
    char first[512];
    snprintf(first, sizeof first, "%s", help); /* after the colon it starts in lower case */
    if (first[0] >= 'a' && first[0] <= 'z') first[0] = (char)(first[0] - 'a' + 'A');
    help = first;
    if (cols)
        guide_title(d, o ? o->label : studio_row_label(id), x, y, w, cols[0]);
    y = guide_wrap(d, o ? o->what : help, x, y + 17.f * S, w, .75f,
                   cols ? cols[1] : NULL, 13.f * S);
    if (o && !graphics)
        y = guide_wrap(d, "It shows in the game's own menus once you press Start. "
                          "On: as Enhanced. Off: as the PS2.",
                       x, y + 3.f * S, w, .75f, cols ? cols[2] : NULL, 13.f * S);
    if (o && o->look[0] && !compact) {
        char line[220];
        snprintf(line, sizeof line, "Look at: %s", o->look);
        y = guide_wrap(d, line, x, y + 3.f * S, w, .75f, cols ? cols[2] : NULL, 13.f * S);
    }
    if (o && o->cost[0] && !compact) {
        char line[160];
        snprintf(line, sizeof line, "Cost: %s", o->cost);
        y = guide_wrap(d, line, x, y + 3.f * S, w, .75f, cols ? cols[2] : NULL, 13.f * S);
    }
    if (note && note[0]) {
        const uint8_t gold[4] = {255, 204, 64, 235};
        y = guide_wrap(d, note, x, y + 4.f * S, w, .68f, cols ? gold : NULL, 12.f * S);
    }
    float facts = vis_facts(d, id, x, y + 4.f * S, w, 1.f, cols != NULL, o && !graphics);
    if (facts > 0)
        y += 4.f * S + facts;
    return y - y0;
}
static void guide_options_draw(const studio_dock *d, int tab, int id, int theme, int mood,
                               float alpha)
{
    guide_options_state *g = &guide_options;
    g->pic[0] = g->pic[1] = g->pic[2] = g->pic[3] = 0;
    if (alpha <= 0 || id < -3 || id >= video_setting_count() && id < 1000 || !app_video_settings)
        return;
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    studio_panel p = studio_options_panel(d, tab);
    const uint8_t a8 = (uint8_t)(255 * alpha);
    const uint8_t head[4] = {255, 255, 255, (uint8_t)(240 * alpha)},
                  dim[4] = {160, 163, 170, (uint8_t)(220 * alpha)},
                  soft[4] = {214, 216, 220, (uint8_t)(235 * alpha)},
                  ink[4] = {245, 246, 248, a8}, gold[4] = {255, 204, 64, a8},
                  well[4] = {0, 0, 0, (uint8_t)(200 * alpha)};
    const float rx0 = p.right + 18.f * S, rw = p.x + p.w - 14.f * S - rx0;
    float y = p.row0; /* align the description with the first option, below the tabs */
    int graphics = 0;
    const guide_option *o = guide_option_for(id, &graphics);
    /* the column's own dark ground: the room's car stands behind the panel there, and the
       words must not run over it */
    const uint8_t ground[4] = {16, 17, 20, (uint8_t)(150 * alpha)};
    guide_rect(rx0 - 8.f * S, p.row0 - 22.f * S, rw + 14.f * S, p.y + p.h - p.row0 + 16.f * S,
               ground);
    const float bottom = p.y + p.h - 8.f * S;
    /* the comparison's remarks, under the picture, not on it: a small picture has no room */
    char note[200] = "";
    if (o && graphics) {
        const pc_video_options *now = &app_video_settings->current;
        if ((o->field == offsetof(pc_video_options, sun_rays) ||
             o->field == offsetof(pc_video_options, ambient_occlusion)) &&
            !(now->hdr && now->daylight))
            snprintf(note, sizeof note, "Both sides with HDR and Daylight on: it needs them; "
                                        "choosing it turns them on. ");
        if (!guide_get(now, o->field))
            snprintf(note + strlen(note), sizeof note - strlen(note),
                     "Right side: Enhanced's value, not chosen yet.");
    }
    /* the picture keeps at least a third of the column: else the words drop Look at and Cost */
    int compact = 0;
    float words = guide_options_words(d, o, graphics, id, rx0, 0, rw, NULL, 0, note);
    if (bottom - y - 18.f * S - words < rw * .34f) {
        compact = 1;
        words = guide_options_words(d, o, graphics, id, rx0, 0, rw, NULL, 1, note);
    }
    if (o && graphics) {
        /* the picture takes what the words leave, so they never run past the panel */
        float ih_box = fminf(rw * .52f, bottom - y - 18.f * S - words);
        guide_rect(rx0 - 2.f * S, y - 2.f * S, rw + 4.f * S, ih_box + 4.f * S, well);
        /* the clip back and forth, both sides on the same frame */
        int f = (int)(SDL_GetTicks() * GUIDE_CLIP_FPS / 1000.f) % (2 * GUIDE_CLIP - 2);
        if (f >= GUIDE_CLIP) f = 2 * GUIDE_CLIP - 2 - f;
        if (getenv("MOONSTUDIO_CLIP_FRAME")) /* dev: one frame of the clip, for shots */
            f = abs(atoi(getenv("MOONSTUDIO_CLIP_FRAME"))) % GUIDE_CLIP;
        if (g->shown && g->id == id && g->images[0][f] && g->images[1][f] && alpha > .99f) {
            const pc_video_options *now = &app_video_settings->current;
            char named[96];
            snprintf(named, sizeof named, "%s",
                     guide_value_name(o->label, guide_get(&g->b, o->field)));
            for (char *c = named; *c; c++)
                if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
            guide_compare(g->images[0][f], g->images[1][f], 640.f, 448.f, o->crop, rx0, y, rw, ih_box,
                          g->split, g->pic);
            /* the two names on a dark tag each, made smaller to fit their half of the picture */
            const char *names[2] = {"ORIGINAL", named};
            for (int k = 0; k < 2; k++) {
                float size = .7f, half = g->pic[2] * .5f - 10.f * S,
                      tw = dock_text_width(&d->font, names[k]) * DOCK_NAME_SCALE * size * S;
                if (tw > half && tw > 0)
                    size *= half / tw, tw = half;
                float tx = k ? g->pic[0] + g->pic[2] - 4.f * S - tw - 4.f * S : g->pic[0] + 4.f * S;
                const uint8_t tag[4] = {0, 0, 0, (uint8_t)(150 * alpha)};
                guide_rect(tx, g->pic[1] + 4.f * S, tw + 8.f * S, 14.f * S, tag);
                guide_text(d, names[k], tx + 4.f * S, g->pic[1] + 15.f * S, size, 0, ink);
            }
            (void)now;
        } else if (g->failed)
            dock_text(d, "Comparisons need the course from the disc.", rx0 + 8.f * S,
                      y + 14.f * S, .75f, 0, dim);
        else if (g->load)
            dock_text(d, "Loading the comparison...", rx0 + rw * .5f, y + ih_box * .5f, .8f, 1, dim);
        else if (!g->ready)
            dock_text(d, "Loading " GUIDE_COURSE_NAME " from your disc...", rx0 + 8.f * S,
                      y + 14.f * S, .75f, 0, dim);
        else {
            char line[64];
            int done = g->pending ? (g->pending - 1) * GUIDE_CLIP + g->frame : 0;
            snprintf(line, sizeof line, "Filming the comparison... %d%%", done * 100 / (2 * GUIDE_CLIP));
            dock_text(d, line, rx0 + rw * .5f, y + ih_box * .5f, .8f, 1, dim);
        }
        y += ih_box + 18.f * S;
    }
    else {
        /* every other row: its own small demonstration above the words */
        float box = fminf(rw * .5f, bottom - y - 18.f * S - words);
        if (box > 60.f * S && vis_draw(d, id, theme, mood, rx0, y, rw, box, alpha))
            y += box + 18.f * S;
    }
    const uint8_t *cols[3] = {head, dim, soft};
    guide_options_words(d, o, graphics, id, rx0, y, rw, cols, compact, note);
}
