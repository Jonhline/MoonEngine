static void dock_window_to_pixels(int x, int y, float *px, float *py);
/* Launcher-only binding list: pointer and pad share one pending choice. */
static void studio_binding_accept(void)
{
    int port = studio_binding_picker.port;
    studio_binding_confirm(&studio_binding_picker, &studio_game_options.pad[port]);
    studio_game_options_dirty |= 4 << port;
}
static int studio_binding_hit(const studio_dock *d, float x, float y)
{
    studio_panel p = studio_panel_at(d, 12);
    float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    float left = p.x + 14 * S, width = (p.w - 28 * S) / 2;
    int col = (int)floorf((x - left) / width);
    int row = (int)floorf((y - (p.row0 - p.pitch * .72f)) / p.pitch);
    int value = col * 11 + row, n = studio_binding_picker.action ? 21 : 4;
    return col >= 0 && col < 2 && row >= 0 && row < 11 && value < n ? value : -1;
}
static int studio_binding_event(const studio_dock *d, const SDL_Event *ev)
{
    if (ev->type == SDL_KEYDOWN) {
        int k = ev->key.keysym.sym;
        if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE)
            studio_binding_picker.open = 0;
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
            studio_binding_accept();
        else if (k == SDLK_UP)
            studio_binding_move(&studio_binding_picker, -1);
        else if (k == SDLK_DOWN)
            studio_binding_move(&studio_binding_picker, 1);
        else if (k == SDLK_LEFT && studio_binding_picker.action)
            studio_binding_move(&studio_binding_picker, -11);
        else if (k == SDLK_RIGHT && studio_binding_picker.action)
            studio_binding_move(&studio_binding_picker, 11);
        return 1;
    }
    if (ev->type == SDL_MOUSEWHEEL) {
        if (ev->wheel.y)
            studio_binding_move(&studio_binding_picker, ev->wheel.y > 0 ? -1 : 1);
        return 1;
    }
    if (ev->type == SDL_MOUSEMOTION || ev->type == SDL_MOUSEBUTTONDOWN ||
        ev->type == SDL_MOUSEBUTTONUP) {
        float x, y;
        dock_window_to_pixels(ev->type == SDL_MOUSEMOTION ? ev->motion.x : ev->button.x,
                              ev->type == SDL_MOUSEMOTION ? ev->motion.y : ev->button.y, &x, &y);
        int value = studio_binding_hit(d, x, y);
        if (value >= 0)
            studio_binding_picker.value = value;
        if (ev->type == SDL_MOUSEBUTTONDOWN) {
            if (ev->button.button == SDL_BUTTON_RIGHT || value < 0)
                studio_binding_picker.open = 0;
            else if (ev->button.button == SDL_BUTTON_LEFT)
                studio_binding_accept();
        }
        return 1;
    }
    return 0;
}
static void studio_binding_draw(const studio_dock *d)
{
    studio_panel p = studio_panel_at(d, 12);
    float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    const uint8_t bg[4] = {20, 22, 26, 255}, white[4] = {245, 245, 245, 255},
                  gold[4] = {255, 204, 64, 255}, selected[4] = {100, 90, 65, 255};
    plat_draw_quad(NULL, p.x, p.y, p.w, p.h, 0, 0, 1, 1, bg);
    char title[100];
    snprintf(title, sizeof title, "%s: choose a button",
             studio_binding_names[studio_binding_picker.action]);
    dock_text(d, title, p.x + 14 * S, p.tab_y, .85f, 0, white);
    int n = studio_binding_picker.action ? 21 : 4;
    float width = (p.w - 28 * S) / 2;
    for (int i = 0; i < n; i++) {
        float x = p.x + 14 * S + (i / 11) * width, y = p.row0 + (i % 11) * p.pitch;
        if (i == studio_binding_picker.value)
            plat_draw_quad(NULL, x - 3 * S, y - p.pitch * .72f, width - 5 * S, p.pitch * .95f, 0, 0,
                           1, 1, selected);
        const char *name =
            studio_binding_picker.action ? studio_key_names[i] : studio_steer_names[i];
        dock_text(d, name, x, y, studio_text_fit(d, name, .76f, width - 10 * S), 0,
                  i == studio_binding_picker.value ? gold : white);
    }
    const char *help = "Enter / A: confirm    Esc / B: cancel";
    char conflict[130];
    const gt4_pad_config *c = &studio_game_options.pad[studio_binding_picker.port];
    int conflict_action =
        studio_binding_conflict(c, studio_binding_picker.action, studio_binding_picker.value);
    if (conflict_action >= 0) {
        snprintf(conflict, sizeof conflict, "Used by %s. Reassigned only when confirmed.",
                 studio_binding_names[conflict_action]);
        help = conflict;
    }
    dock_text(d, help, p.x + 14 * S, p.y + p.h - 10 * S,
              studio_text_fit(d, help, .66f, p.w - 28 * S), 0, white);
}

/* Explicit local regression fixture: the ordinary event handlers, isolated prefs. */
static int studio_binding_input_check(const studio_dock *d, int negative)
{
    gt4_pad_config before[2];
    memcpy(before, studio_game_options.pad, sizeof before);
    int dirty = studio_game_options_dirty, action = studio_binding_picker.action,
        port = studio_binding_picker.port;
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN;
    for (int i = 0; i < 42; i++) {
        studio_binding_event(d, &e);
        if (negative)
            studio_binding_set(&studio_game_options.pad[port], action, studio_binding_picker.value);
        if (memcmp(before, studio_game_options.pad, sizeof before) ||
            dirty != studio_game_options_dirty)
            return 0;
    }
    e.key.keysym.sym = SDLK_ESCAPE;
    studio_binding_event(d, &e);
    if (studio_binding_picker.open || memcmp(before, studio_game_options.pad, sizeof before))
        return 0;
    studio_binding_begin(&studio_binding_picker, &studio_game_options.pad[port], action, port);
    studio_panel p = studio_panel_at(d, 12);
    float S = d->scale > 0 ? d->scale : DOCK_SCALE;
    int ww, wh, fw, fh;
    SDL_Window *window = plat_sdl_window();
    SDL_GetWindowSize(window, &ww, &wh);
    SDL_GL_GetDrawableSize(window, &fw, &fh);
    if (!fw || !fh)
        return 0;
    float left = p.x + 14 * S, width = (p.w - 28 * S) / 2;
    e.type = SDL_MOUSEMOTION;
    e.motion.x = (int)((left + width + 10 * S) * ww / fw);
    e.motion.y = (int)((p.row0 + 9 * p.pitch) * wh / fh);
    studio_binding_event(d, &e);
    if (studio_binding_picker.value != 20 || memcmp(before, studio_game_options.pad, sizeof before))
        return 0;
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (int)((left + width + 10 * S) * ww / fw);
    e.button.y = (int)((p.row0 + 6 * p.pitch) * wh / fh);
    studio_binding_event(d, &e);
    return !studio_binding_picker.open &&
           studio_binding_get(&studio_game_options.pad[port], action) == 17 &&
           !memcmp(&before[1 - port], &studio_game_options.pad[1 - port], sizeof before[0]) &&
           (studio_game_options_dirty & (4 << port));
}
