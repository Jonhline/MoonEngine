/* Launcher-only quick audio controls; all coordinates use the dock's scale. */
static void studio_mixer_draw(const studio_dock *d, const launcher_mixer *m, float alpha)
{
    float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    if (alpha <= 0)
        return;
#define MA(a_) (uint8_t)((a_) * alpha)
    const uint8_t panel[4] = {12, 13, 16, MA(225)};
    const uint8_t head[4] = {235, 237, 241, MA(255)};
    const uint8_t ink_light[4] = {38, 40, 45, MA(235)}, ink_dark[4] = {228, 230, 235, MA(240)};
    const uint8_t dim_light[4] = {96, 99, 106, MA(230)}, dim_dark[4] = {150, 153, 160, MA(230)};
    const uint8_t soft[4] = {120, 124, 131, MA(210)};
    const uint8_t edge[4] = {255, 255, 255, MA(90)};
#undef MA
    const uint8_t *ink = studio_dark ? ink_dark : ink_light;
    const uint8_t *dim = studio_dark ? dim_dark : dim_light;
    /* Match the room header: quiet typography, secondary shortcuts, no flat tiles.
       The original quick-control hit rectangles remain unchanged. */
    dock_text(d, "Audio", 22 * S, 79 * S, .7f, 0, ink);
    dock_text(d, "F8", 74 * S, 79 * S, .55f, 2, dim);
    dock_text(d, m->muted ? "Music: Muted" : "Music: On", 92 * S, 79 * S, .7f, 0,
              m->muted ? dim : ink);
    dock_text(d, "M", 192 * S, 79 * S, .55f, 2, dim);
    if (m->open)
        plat_draw_quad(NULL, 22 * S, 84 * S, 52 * S, S, 0, 0, 1, 1, ink);
    if (!m->muted)
        plat_draw_quad(NULL, 92 * S, 84 * S, 100 * S, S, 0, 0, 1, 1, dim);
    if (!m->open)
        return;
    /* Same charcoal surface and fine top rule as the OPTIONS panel. */
    plat_draw_quad(NULL, 16 * S, 90 * S, 228 * S, 94 * S, 0, 0, 1, 1, panel);
    plat_draw_quad(NULL, 16 * S, 90 * S, 228 * S, S, 0, 0, 1, 1, edge);
    ink = head;
    const char *labels[] = {"Music", "Effects"};
    int values[] = {m->music, m->effects};
    for (int i = 0; i < 2; i++) {
        float y = (108 + i * 28) * S;
        dock_text(d, labels[i], 26 * S, y + 4 * S, .7f, 0, ink);
        plat_draw_quad(NULL, 86 * S, y, 112 * S, 3 * S, 0, 0, 1, 1, soft);
        float x = (86 + 112 * values[i] / 128.f) * S;
        plat_draw_quad(NULL, 86 * S, y, (x - 86 * S), 3 * S, 0, 0, 1, 1, ink);
        plat_draw_quad(NULL, x - 3 * S, y - 5 * S, 6 * S, 13 * S, 0, 0, 1, 1, ink);
        char percent[16];
        snprintf(percent, sizeof percent, "%d%%", (values[i] * 100 + 64) / 128);
        dock_text(d, percent, 234 * S, y + 4 * S, .65f, 2, ink);
        if (m->row == i)
            plat_draw_quad(NULL, 26 * S, y + 8 * S, 38 * S, S, 0, 0, 1, 1, ink);
    }
    dock_text(d, "Drag sliders or use arrow keys", 26 * S, 172 * S, .55f, 0, ink);
}
