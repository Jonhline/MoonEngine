/* P: OPTIONS' right column for every row without a course comparison: a real picture when
   one can show the option (the launcher's own room rendered in each look, the game's menus
   printed with each value: launcher_prints.inc), the named choices for a few rows, else none;
   no drawn stand-ins. Under it a line of facts: when it applies and what the Classic and
   Enhanced presets set. Values are read, never written. Included by launcher_guide.inc. */

static float vis_time(void)
{
    if (getenv("MOONSTUDIO_CLIP_FRAME")) /* dev: a fixed instant, for shots */
        return (float)atoi(getenv("MOONSTUDIO_CLIP_FRAME")) * .1f;
    return (float)SDL_GetTicks() / 1000.f;
}
static uint8_t vis_a(float alpha, int a)
{
    return (uint8_t)(a * alpha);
}
static void vis_outline(float x, float y, float w, float h, float t, const uint8_t c[4])
{
    guide_rect(x, y, w, t, c);
    guide_rect(x, y + h - t, w, t, c);
    guide_rect(x, y, t, h, c);
    guide_rect(x + w - t, y, t, h, c);
}
/* the setting's value now (its index), -1 for rows that are not settings */
static int vis_value(int id)
{
    return id >= 0 && id < video_setting_count() && app_video_settings
               ? video_setting_get(app_video_settings, id)
               : -1;
}
static int vis_is(int id, const char *label)
{
    return id >= 0 && id < video_setting_count() && !strcmp(video_setting(id)->label, label);
}
/* what a preset sets for a row: the current options with that preset's values laid over */
static int vis_preset_value(int id, int enhanced)
{
    static video_settings copy;
    copy = *app_video_settings;
    video_settings_preset_values(&copy.current, enhanced);
    return video_setting_get(&copy, id);
}
/* when a row takes effect, only when it is not at once (NULL) */
static const char *vis_applies(int id)
{
    if (id < 0 || id >= video_setting_count()) return NULL;
    const char *g = video_setting(id)->group, *l = video_setting(id)->label;
    if (!strcmp(l, "Graphics API") || !strcmp(l, "Aspect Ratio")) return "Takes effect at the next start.";
    if (!strcmp(g, "Sound")) return "Takes effect the next time the Jukebox opens.";
    return NULL;
}
/* the facts line, only what the words above do not say: a change that waits, and what the
   two presets set when they differ (noted: the guide's Menus note already says it). Returns
   its height, 0 when there is nothing to add. */
static float vis_facts(const studio_dock *d, int id, float x, float y, float w, float alpha,
                       int draw, int noted)
{
    const float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    char line[200];
    const char *when = vis_applies(id);
    int n = snprintf(line, sizeof line, "%s", when ? when : "");
    if (!noted && id >= 0 && id < video_setting_count() && app_video_settings &&
        strcmp(video_setting(id)->label, "PC Preset") && strcmp(video_setting(id)->group, "Sound")) {
        const video_setting_info *in = video_setting(id);
        int c = vis_preset_value(id, 0), e = vis_preset_value(id, 1);
        if (c != e && c >= 0 && c < in->count && e >= 0 && e < in->count)
            snprintf(line + n, sizeof line - n, "%sClassic: %s. Enhanced: %s.", n ? " " : "",
                     in->names[c], in->names[e]);
    }
    if (!line[0])
        return 0;
    const uint8_t gold[4] = {255, 204, 64, vis_a(alpha, 235)};
    return guide_wrap(d, line, x, y, w, .68f, draw ? gold : NULL, 12.f * S) - y;
}

/* ---- the demonstrations: (x, y, w, h) is the box, t seconds, v the value ---- */
typedef struct {
    const studio_dock *d;
    float x, y, w, h, t, S, alpha;
    int v;
} vis_ctx;
/* text made smaller until it fits max (0: any width), so a narrow panel never spills it */
static float vis_fit(const vis_ctx *c, const char *text, float size, float max)
{
    float tw = dock_text_width(&c->d->font, text) * DOCK_NAME_SCALE * size * c->S;
    return max > 0 && tw > max ? size * max / tw : size;
}
static void vis_label_w(const vis_ctx *c, const char *text, float x, float y, int align, int lit,
                        float max)
{
    const uint8_t on[4] = {255, 255, 255, vis_a(c->alpha, 240)},
                  off[4] = {150, 153, 160, vis_a(c->alpha, 210)};
    guide_text(c->d, text, x, y, vis_fit(c, text, .62f, max), align, lit ? on : off);
}
static void vis_label(const vis_ctx *c, const char *text, float x, float y, int align, int lit)
{
    vis_label_w(c, text, x, y, align, lit, 0);
}
static void vis_cards_sub(const vis_ctx *c, const char *const names[], const char *const subs[], int n,
                          int chosen)
{
    float gap = 6.f * c->S, cw = (c->w - gap * (n - 1)) / n, ch = c->h - 18.f * c->S;
    for (int k = 0; k < n; k++) {
        const uint8_t well[4] = {30, 32, 38, vis_a(c->alpha, 230)}, frame[4] = {255, 204, 64, vis_a(c->alpha, 230)};
        float x = c->x + k * (cw + gap), y = c->y + 16.f * c->S;
        guide_rect(x, y, cw, ch, well);
        if (k == chosen) vis_outline(x - 2.f * c->S, y - 2.f * c->S, cw + 4.f * c->S, ch + 4.f * c->S, 2.f * c->S, frame);
        float room = cw - 8.f * c->S;
        vis_label_w(c, names[k], x + cw * .5f, y + ch * .5f + (subs ? -4.f : 4.f) * c->S, 1,
                    k == chosen, room);
        if (subs) {
            const uint8_t dim[4] = {150, 153, 160, vis_a(c->alpha, 200)};
            guide_text(c->d, subs[k], x + cw * .5f, y + ch * .5f + 14.f * c->S,
                       vis_fit(c, subs[k], .55f, room), 1, dim);
        }
    }
}
static void vis_cards(const vis_ctx *c, const char *const names[], int n, int chosen)
{
    vis_cards_sub(c, names, NULL, n, chosen);
}
/* the game's own menu printed with each of two values, the one chosen framed; 0 when the
   prints could not be taken (no picture then) */
static int vis_prints(const vis_ctx *c, int r)
{
    const print_recipe *p = &print_recipes[r];
    int st[2] = {launcher_prints.state[r][0], launcher_prints.state[r][1]};
    if (st[0] < 0 || st[1] < 0)
        return 0;
    if (st[0] != 2 || st[1] != 2) {
        vis_label(c, "PRINTING THE GAME'S MENU...", c->x + c->w * .5f, c->y + c->h * .5f, 1, 0);
        return 1;
    }
    float gap = 8.f * c->S, pw = (c->w - gap) * .5f, ph = c->h - 18.f * c->S;
    const uint8_t frame[4] = {255, 204, 64, vis_a(c->alpha, 230)},
                  white[4] = {255, 255, 255, vis_a(c->alpha, 255)};
    for (int k = 0; k < 2; k++) {
        /* the print whole in its half, centred */
        float iw = (float)launcher_prints.w[r][k], ih = (float)launcher_prints.h[r][k];
        float s = fminf(pw / iw, ph / ih), dw = iw * s, dh = ih * s;
        float px = c->x + k * (pw + gap) + (pw - dw) * .5f, py = c->y + 16.f * c->S + (ph - dh) * .5f;
        plat_draw_quad(launcher_prints.image[r][k], px, py, dw, dh, 0, 0, 1, 1, white);
        int chosen = c->v == p->values[k];
        if (chosen)
            vis_outline(px - 2.f * c->S, py - 2.f * c->S, dw + 4.f * c->S, dh + 4.f * c->S, 2.f * c->S, frame);
        vis_label_w(c, p->names[k], c->x + k * (pw + gap) + pw * .5f, c->y + 10.f * c->S, 1, chosen, pw);
    }
    return 1;
}
/* a grabbed raster x 448 picture (rows bottom up) filling a box, cropped around u_mid */
static void vis_picture(plat_texture *t, float iw, float ih, float u_mid, float x, float y, float w,
                        float h, float alpha)
{
    if (!t || w <= 0 || h <= 0)
        return;
    float fu = fminf(1.f, (w / h) / (iw / ih)), fv = fminf(1.f, (h / w) / (ih / iw));
    float u0 = fminf(1.f - fu, fmaxf(0.f, u_mid - fu * .5f)), v0 = (1.f - fv) * .5f;
    const uint8_t white[4] = {255, 255, 255, vis_a(alpha, 255)};
    plat_draw_quad(t, x, y, w, h, u0, 1.f - v0, u0 + fu, v0, white);
}
/* real renders side by side (looks: theme, colour), the chosen one framed; returns 0 while one
   is still coming */
static int vis_renders(const vis_ctx *c, const int looks[][2], const char *const *names, int n,
                       int chosen)
{
    for (int k = 0; k < n; k++)
        if (!studio_renders_ready(looks[k][0], looks[k][1])) {
            vis_label(c, "RENDERING...", c->x + c->w * .5f, c->y + c->h * .5f, 1, 0);
            return 0;
        }
    float gap = 8.f * c->S, pw = (c->w - gap * (n - 1)) / n, ph = c->h - 18.f * c->S;
    const uint8_t frame[4] = {255, 204, 64, vis_a(c->alpha, 230)};
    for (int k = 0; k < n; k++) {
        float px = c->x + k * (pw + gap), py = c->y + 16.f * c->S;
        vis_picture(studio_renders.image[looks[k][0] & 1][(looks[k][1] + STUDIO_MOODS) % STUDIO_MOODS],
                    (float)studio_renders.raster, 448.f, .5f, px, py, pw, ph, c->alpha);
        if (k == chosen)
            vis_outline(px - 2.f * c->S, py - 2.f * c->S, pw + 4.f * c->S, ph + 4.f * c->S, 2.f * c->S, frame);
        vis_label_w(c, names[k], px + pw * .5f, c->y + 10.f * c->S, 1, k == chosen, pw);
    }
    return 1;
}
/* whether a row has a picture (the box is not drawn for one without) */
static int vis_has(int id)
{
    int r = launcher_prints_recipe(id);
    if (r >= 0)
        return launcher_prints.state[r][0] >= 0 && launcher_prints.state[r][1] >= 0;
    return (id >= -3 && id <= -1) || id >= 2000 || vis_is(id, "Graphics API") ||
           vis_is(id, "PC Preset");
}
/* the picture for a row; 0 when it has none */
static int vis_draw(const studio_dock *d, int id, int theme, int mood, float x, float y, float w,
                    float h, float alpha)
{
    if (!vis_has(id))
        return 0;
    vis_ctx c = {d, x, y, w, h, vis_time(), d->scale > 0 ? d->scale : DOCK_SCALE, alpha, vis_value(id)};
    const uint8_t well[4] = {0, 0, 0, vis_a(alpha, 170)};
    guide_rect(x - 2.f * c.S, y - 2.f * c.S, w + 4.f * c.S, h + 4.f * c.S, well);
    x += 6.f * c.S, y += 4.f * c.S, w -= 12.f * c.S, h -= 8.f * c.S;
    c.x = x, c.y = y, c.w = w, c.h = h;
    if (id == -1) { /* the launcher's own room, rendered light and dark */
        const int looks[2][2] = {{0, mood}, {1, mood}};
        static const char *const names[2] = {"LIGHT ROOM", "DARK ROOM"};
        vis_renders(&c, looks, names, 2, theme);
        return 1;
    }
    if (id == -3) {
        /* the picture itself once taken; the room's sweep for the room */
        const uint8_t white[4] = {255, 255, 255, vis_a(alpha, 255)};
        float ih = fminf(h - 18.f * c.S, w * 448.f / 640.f), iw = ih * 640.f / 448.f, ix = x + (w - iw) * .5f;
        if (launcher_scenes[launcher_scene.index].code) {
            if (launcher_scene.image && launcher_scene.live >= 3)
                plat_draw_quad(launcher_scene.image, ix, y + 16.f * c.S, iw, ih, 0, 1, 1, 0, white);
            else
                vis_label(&c, launcher_scene.failed ? "NOT ON THIS DISC" : "DRAWING THE SCENE...",
                          x + w * .5f, y + 16.f * c.S + ih * .5f, 1, 0);
        } else if (studio_renders_ready(theme, mood)) /* the room as it is, rendered */
            vis_picture(studio_renders.image[theme & 1][mood], (float)studio_renders.raster, 448.f,
                        .5f, ix, y + 16.f * c.S, iw, ih, alpha);
        else
            vis_label(&c, "RENDERING...", x + w * .5f, y + 16.f * c.S + ih * .5f, 1, 0);
        vis_label(&c, studio_scene_name(), x + w * .5f, y + 10.f * c.S, 1, 1);
        return 1;
    }
    if (id == -2) { /* the room in the colour before, the chosen one and the next */
        const int looks[3][2] = {{theme, mood - 1}, {theme, mood}, {theme, mood + 1}};
        const char *names[3] = {studio_moods[(mood + STUDIO_MOODS - 1) % STUDIO_MOODS].name,
                                studio_moods[mood].name, studio_moods[(mood + 1) % STUDIO_MOODS].name};
        vis_renders(&c, looks, names, 3, 1);
        return 1;
    }
    if (id >= 3000) {
        static const char *const names[2] = {"LOAD A .MOONPRESET", "SAVE YOURS TO ONE"},
                                 *const subs[2] = {"Someone's settings, in one file", "To keep or to share"};
        vis_cards_sub(&c, names, subs, 2, id - 3000);
        return 1;
    }
    if (id >= 2000) {
        if (id == 2000) {
            static const char *const names[2] = {"PLAYER 1", "PLAYER 2"};
            vis_cards(&c, names, 2, studio_pad_port);
            return 1;
        }
        const uint8_t cap[4] = {230, 232, 236, vis_a(alpha, 255)}, ink[4] = {20, 20, 24, vis_a(alpha, 255)};
        const char *value = studio_row_value(id, theme, mood);
        float cw = fminf(w * .7f, 200.f * c.S), cx = x + (w - cw) * .5f, cy = y + h * .3f;
        guide_rect(cx, cy, cw, 40.f * c.S, cap);
        guide_text(d, value, cx + cw * .5f, cy + 27.f * c.S, .95f, 1, ink);
        vis_label(&c, studio_binding_names[id - 2001], x + w * .5f, cy + 64.f * c.S, 1, 1);
        return 1;
    }
    if (id >= 1000)
        return 0;
    int print = launcher_prints_recipe(id);
    if (print >= 0)
        return vis_prints(&c, print);
    if (vis_is(id, "Graphics API")) {
        static const char *const names[2] = {"OPENGL", "VULKAN"},
                                 *const subs[2] = {"Runs on most cards", "Newer, faster on some"};
        vis_cards_sub(&c, names, subs, 2, c.v);
        return 1;
    }
    if (vis_is(id, "PC Preset")) {
        static const char *const names[3] = {"CLASSIC", "ENHANCED", "MODIFIED"},
                                 *const subs[3] = {"The PS2 game", "All PC additions", "Your own mix"};
        vis_cards_sub(&c, names, subs, 3, c.v);
        return 1;
    }
    return 0;
}
