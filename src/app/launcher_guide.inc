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
    {"Lighting", GO(modern_lighting),
     "Modern swaps the PS2 lighting model for physically based highlights, a soft sky fill on "
     "the shaded side and filmic compression of the brightest parts.",
     "The paint's highlights and the side of the car away from the sun.",
     "Light on the graphics card.", {.05f, .29f, .65f, .67f}},
    {"Material Shading", GO(material_shading),
     "Works out each material's lighting for every pixel instead of at the corners of each "
     "polygon, so curved panels shade smoothly.",
     "The bonnet and the doors: reflections run across them without steps.",
     "Light.", {.08f, .31f, .62f, .63f}},
    {"Reflection Quality", GO(reflection_quality),
     "The resolution of the surroundings reflected in the paint and the glass. Original keeps "
     "the PS2's 128-pixel reflection.",
     "The sky and the course mirrored on the bodywork and the windscreen.",
     "Each step up costs more graphics time.", {.10f, .33f, .58f, .59f}},
    {"Car Shadows", GO(car_shadows),
     "Projected lays the car's real outline on the ground under it. Original keeps the game's "
     "own soft shadow shape.",
     "The shadow's shape around the wheels and the nose.",
     "Light.", {.04f, .35f, .66f, .71f}},
    {"Shadow Maps", GO(shadow_maps),
     "Depth shadows from the sun for the car on the course and on itself, at the size chosen.",
     "Shadow edges, and the car shading its own sills and mirrors.",
     "Higher sizes cost more graphics time.", {.04f, .31f, .66f, .71f}},
    {"Daylight", GO(daylight),
     "Sunlight and daytime atmosphere that follow the hour set in Options.",
     "The sky, the haze on the hills and where the light comes from.",
     "Light.", {.04f, .10f, .96f, .64f}},
    {"HDR", GO(hdr),
     "Renders the scene with a high range of light, then compresses it like a camera, with a "
     "restrained bloom.",
     "Bright sky and highlights keep their detail instead of turning flat white.",
     "Moderate.", {.04f, .10f, .96f, .64f}},
    {"Sun Rays", GO(sun_rays),
     "Light scattered through the air around the sun. Needs HDR and Daylight; Volumetric "
     "turns them on.",
     "The air towards the sun and around the hills.",
     "Volumetric is the most expensive option here.", {.04f, .10f, .96f, .64f}},
    {"Ambient Occlusion", GO(ambient_occlusion),
     "Darkens corners and contact points that light hardly reaches. Needs HDR and Daylight; "
     "turning it on turns them on.",
     "Under the car, the wheel arches and the gaps between panels.",
     "Moderate.", {.06f, .37f, .62f, .65f}},
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
    if (!d->font_ok || !text) return;
    static const uint8_t s1[4] = {6, 7, 9, 120}, s2[4] = {6, 7, 9, 70};
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float w = dock_text_width(&d->font, text) * DOCK_NAME_SCALE * size * S;
    float cx = align == 1 ? x : align == 2 ? x - w * .5f : x + w * .5f;
    dock_name_pass(d, text, cx, base, S, S, s1, size);
    dock_name_pass(d, text, cx, base, 2.f * S, 2.f * S, s2, size);
    dock_name_pass(d, text, cx, base, 0.f, 0.f, col, size);
}
/* a paragraph broken at spaces to a width; returns the baseline after its last line */
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
        dock_text(d, line, x, y, size, 0, col);
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
                         const pc_video_options *look, race_view *track, pc_video_options *drawn)
{
    int settling = guide_rebuilds(look, drawn);
    video_settings_preview_options(look);
    plat_video_begin_frame();
    video_begin_scene();
    guide_scene(cars, code, raster, track, 0); /* identical pose; no car time advances */
    video_end_scene();
    *drawn = *look;
    if (settling)
        return 0;
    *into = video_grab(*into, 0, 0, track ? 640.f : (float)raster, 448.f);
    return 1;
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
    int settle=0, pending=0, shown=0;
    pc_video_options drawn_look=start;
    uint64_t frame_clock=0; double worst_ms=0; /* the test reports the longest frame */
    float split=.5f, pic[4]={0,0,0,0}, hover[3]={0,0,1};
    plat_texture *images[3]={NULL,NULL,NULL}; /* original, chosen, backdrop */
    pc_video_options backdrop_look=start; int backdrop_ok=0, backdrop_w=0, backdrop_h=0;
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
            settle=test?1:8; pending=0; shown=0; pw=w; ph=h; capture=0;
        }
        if(!crop || !scene) {capture=0;settle=pending=0;}
        if(settle && !--settle) pending=1;
        /* The scene is frozen, so it is drawn only into captures, one render per frame at most:
           the two sides once the selection rests, the backdrop when its look changes. */
        const pc_video_options *live=page==GUIDE_WELCOME?&start:&draft;
        if(backdrop_w!=w || backdrop_h!=h) {backdrop_ok=0;backdrop_w=w;backdrop_h=h;}
        if(scene) {
            if(pending) {
                if(guide_capture(&images[pending-1],cars,code,raster,pending==2?&look_b:&look_a,track,&drawn_look)
                   && ++pending>2) {pending=0;shown=1;}
            } else if(!backdrop_ok || memcmp(&backdrop_look,live,sizeof *live)) {
                if(guide_capture(&images[2],cars,code,raster,live,track,&drawn_look)) {
                    backdrop_ok=1;backdrop_look=*live;
                }
            }
        }
        int branch_backdrop=0,drew_race=0,drew_showroom=0;
        plat_video_begin_frame();
        if(scene) {
            plat_texture *back=full && shown?images[flip?1:0]:backdrop_ok?images[2]:NULL;
            const uint8_t white[4]={255,255,255,255};
            if(back) {
                branch_backdrop=1;
                plat_video_set_view(640.f,448.f);
                plat_draw_quad(back,0,0,640.f,448.f,0,1,1,0,white);
            } else { /* the capture is being rebuilt: show the live scene, never black */
                video_settings_preview_options(live);
                drawn_look=*live;
                if(track) drew_race=1; else drew_showroom=1;
                video_begin_scene();
                guide_scene(cars,code,raster,track,1.f/60.f);
                video_end_scene();
            }
        } else { /* the course is loading (or missing): the launcher's room, live */
            video_settings_preview_options(live);
            drawn_look=*live;
            drew_showroom=1;
            video_begin_scene();
            guide_scene(cars,code,raster,NULL,1.f/60.f);
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
        if(test && (track_ready || track_failed) && frame>=8 && !capture && !settle && !pending &&
           !(scene && (!backdrop_ok || memcmp(&backdrop_look,live,sizeof *live)))) {
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
                   branch_backdrop?"backdrop-image":"live-3d",drew_race,drew_showroom);
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
typedef struct {
    race_view *track;
    int ready, failed, want, settle, pending, shown, id, dragging, w, h;
    plat_texture *images[2];
    pc_video_options a, b, drawn;
    float split, pic[4];
} guide_options_state;
static guide_options_state guide_options = {.split = .5f, .id = -1};
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
    for (int j = 0; j < 2; j++) plat_texture_destroy(g->images[j]);
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
    if (!g->want || g->failed) return;
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
    pc_video_options a, b;
    guide_options_looks(o, &a, &b);
    if (id != g->id || w != g->w || h != g->h || memcmp(&a, &g->a, sizeof a) ||
        memcmp(&b, &g->b, sizeof b)) {
        g->id = id, g->w = w, g->h = h, g->a = a, g->b = b;
        g->settle = 8, g->pending = g->shown = 0;
    }
    if (g->settle && !--g->settle) g->pending = 1;
    if (!g->pending) return;
    const pc_video_options *look = g->pending == 2 ? &g->b : &g->a;
    if (!guide_capture(&g->images[g->pending - 1], cars, code, raster, look, g->track, &g->drawn))
        guide_capture(&g->images[g->pending - 1], cars, code, raster, look, g->track, &g->drawn);
    if (++g->pending > 2) {
        g->pending = 0;
        g->shown = 1;
        printf("studio options: comparison %s\n", o->label);
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
static void guide_options_draw(const studio_dock *d, int tab, int id, float alpha)
{
    guide_options_state *g = &guide_options;
    g->pic[0] = g->pic[1] = g->pic[2] = g->pic[3] = 0;
    if (alpha <= 0 || id < 0 || id >= video_setting_count() || !app_video_settings) return;
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    studio_panel p = studio_options_panel(d, tab);
    const uint8_t a8 = (uint8_t)(255 * alpha);
    const uint8_t head[4] = {255, 255, 255, (uint8_t)(240 * alpha)},
                  dim[4] = {160, 163, 170, (uint8_t)(220 * alpha)},
                  soft[4] = {214, 216, 220, (uint8_t)(235 * alpha)},
                  ink[4] = {245, 246, 248, a8}, gold[4] = {255, 204, 64, a8},
                  well[4] = {0, 0, 0, (uint8_t)(200 * alpha)};
    const float rx0 = p.right + 18.f * S, rw = p.x + p.w - 14.f * S - rx0;
    float y = p.row0 - 12.f * S;
    int graphics = 0;
    const guide_option *o = guide_option_for(id, &graphics);
    if (o && graphics) {
        float ih_box = fminf(rw * .52f, p.y + p.h - y - 118.f * S);
        guide_rect(rx0 - 2.f * S, y - 2.f * S, rw + 4.f * S, ih_box + 4.f * S, well);
        if (g->shown && g->id == id && g->images[0] && g->images[1] && alpha > .99f) {
            const pc_video_options *now = &app_video_settings->current;
            char named[96];
            snprintf(named, sizeof named, "%s",
                     guide_value_name(o->label, guide_get(&g->b, o->field)));
            for (char *c = named; *c; c++)
                if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
            guide_compare(g->images[0], g->images[1], 640.f, 448.f, o->crop, rx0, y, rw, ih_box,
                          g->split, g->pic);
            guide_text(d, "ORIGINAL", g->pic[0] + 6.f * S, g->pic[1] + 14.f * S, .7f, 0, ink);
            guide_text(d, named, g->pic[0] + g->pic[2] - 6.f * S, g->pic[1] + 14.f * S, .7f, 2,
                       ink);
            if ((o->field == offsetof(pc_video_options, sun_rays) ||
                 o->field == offsetof(pc_video_options, ambient_occlusion)) &&
                !(now->hdr && now->daylight))
                guide_text(d, "Both sides with HDR and Daylight on: it needs them; choosing it "
                              "turns them on.",
                           g->pic[0] + g->pic[2] * .5f, g->pic[1] + g->pic[3] - 22.f * S, .65f, 1,
                           gold);
            if (!guide_get(now, o->field))
                guide_text(d, "Right: Enhanced's value, not chosen. Right / Left to choose it.",
                           g->pic[0] + g->pic[2] * .5f, g->pic[1] + g->pic[3] - 8.f * S, .65f, 1,
                           gold);
        } else if (g->failed)
            dock_text(d, "Comparisons need the course from the disc.", rx0 + 8.f * S,
                      y + 14.f * S, .75f, 0, dim);
        else if (!g->ready)
            dock_text(d, "Loading " GUIDE_COURSE_NAME " from your disc...", rx0 + 8.f * S,
                      y + 14.f * S, .75f, 0, dim);
        else
            dock_text(d, "...", rx0 + rw * .5f, y + ih_box * .5f, 1.f, 1, dim);
        y += ih_box + 18.f * S;
    }
    dock_text(d, o ? o->label : video_setting(id)->label, rx0, y, .95f, 0, head);
    y = guide_wrap(d, o ? o->what : video_setting(id)->help, rx0, y + 17.f * S, rw, .75f, dim,
                   13.f * S);
    if (o && !graphics)
        y = guide_wrap(d, "It shows in the game's own menus once you press Start. "
                          "On: as Enhanced. Off: as the PS2.",
                       rx0, y + 3.f * S, rw, .75f, soft, 13.f * S);
    if (o && o->look[0]) {
        char line[220];
        snprintf(line, sizeof line, "Look at: %s", o->look);
        y = guide_wrap(d, line, rx0, y + 3.f * S, rw, .75f, soft, 13.f * S);
    }
    if (o && o->cost[0]) {
        char line[160];
        snprintf(line, sizeof line, "Cost: %s", o->cost);
        guide_wrap(d, line, rx0, y + 3.f * S, rw, .75f, soft, 13.f * S);
    }
}
