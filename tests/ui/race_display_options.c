/* Original option.ad RaceRoot.apply, persistent flags and actual HUD draws.
   Platform stubs keep this test windowless and silent. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
void plat_video_set_view(float w, float h)
{
    (void)w;
    (void)h;
}
static const plat_texture *seen[4096];
static int seen_count;
static void display_quad(const plat_texture *t, float x, float y, float w, float h, float u0,
                         float v0, float u1, float v1, const uint8_t c[4])
{
    if (seen_count < 4096)
        seen[seen_count++] = t;
    plat_draw_quad(t, x, y, w, h, u0, v0, u1, v1, c);
}
#define advance hud_advance
#define plat_draw_quad display_quad
#include "../../src/core/race_hud.c"
#undef plat_draw_quad
#undef advance

static int image_count(const plat_texture *texture)
{
    int count = 0;
    for (int i = 0; i < seen_count; i++)
        count += seen[i] == texture;
    return count;
}
static void choose_index(menu *m, ui_widget *root, const char *row, int value)
{
    ui_widget *pane = ui_widget_child(root, "Pane");
    ui_widget *selector = ui_widget_child(ui_widget_child(pane, row), "OptionMenu");
    CHECK(selector);
    ah_val result, args[] = {ah_objv(m->context), ah_int(value)};
    CHECK(ah_call_member(m->vm, widget_module(m, selector), "setIndex", 2, args, &result) >= 0);
}
static void check_panel(race_hud *h, uint32_t hidden, int panel)
{
    race_hud_state state = {0};
    state.display_hidden = hidden;
    state.panel = panel;
    state.rank = 1;
    state.lap = 1;
    state.laps = 3;
    state.speed_scale = 160;
    state.tacho_scale = 8000;
    state.rev_limit = 6500;
    state.suggested_gear = 3;
    seen_count = 0;
    race_hud_draw(h, &state, 1.f / 60);
    const struct {
        int image;
        uint32_t bit;
    } checks[] = {{IMG_POSITION, RACE_DISPLAY_POSITION},
                  {IMG_LAP, RACE_DISPLAY_LAP},
                  {IMG_BEST, RACE_DISPLAY_TIME},
                  {IMG_LAST, RACE_DISPLAY_TIME},
                  {panel ? IMG_PANEL_BASE : IMG_REMOTE_KMPH, RACE_DISPLAY_METER},
                  {IMG_G_BASE, RACE_DISPLAY_METER | RACE_DISPLAY_G_METER},
                  {IMG_G_POINT, RACE_DISPLAY_METER | RACE_DISPLAY_G_METER}};
    CHECK((image_count(h->map) > 0) == !(hidden & RACE_DISPLAY_MAP));
    for (unsigned i = 0; i < sizeof checks / sizeof *checks; i++) {
        CHECK(h->img[checks[i].image].tex);
        CHECK((image_count(h->img[checks[i].image].tex) > 0) == !(hidden & checks[i].bit));
    }
}
static void check_draws(race_hud *h, uint32_t hidden)
{
    check_panel(h, hidden, 0);
    check_panel(h, hidden, 1);
}

int main(int argc, char **argv)
{
    CHECK(argc >= 3);
    const char *why;
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    remove(argv[2]);
    menu_options_storage(argv[2]);
    char error[256];
    menu *m = menu_open(vol, "option", error, sizeof error);
    CHECK(m && menu_race_display_flags(m) == RACE_DISPLAY_DEFAULT);
    int index = -1;
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "RaceRoot"))
            index = i;
    CHECK(index >= 0);
    menu_enter(m, index);
    menu_tick(m);
    ui_widget *root = ui_root_widget(m->ui, index);
    race_hud *h = race_hud_open(vol);
    CHECK(h);
    h->map = plat_texture_create(1, 1, NULL);
    h->map_w = h->map_h = h->map_tw = h->map_th = 1;
    CHECK(h->map);
    for (int simple = 0; simple < 2; simple++)
        for (int map = 0; map < 2; map++)
            for (int steering = 0; steering < 2; steering++) {
                choose_index(m, root, "DisplayType", simple);
                choose_index(m, root, "Map", 1 - map);
                choose_index(m, root, "Steering", 1 - steering);
                ah_val result;
                CHECK(ah_call_member(m->vm, widget_module(m, root), "apply", 0, NULL, &result) >= 0);
                uint32_t flags = RACE_DISPLAY_DEFAULT & ~RACE_DISPLAY_MAP;
                if (simple) {
                    flags &= ~(RACE_DISPLAY_LAP | RACE_DISPLAY_CAR_NAME |
                               RACE_DISPLAY_SUGGESTED_GEAR | RACE_DISPLAY_G_METER);
                    flags |= RACE_DISPLAY_CONFIG;
                }
                if (map)
                    flags |= RACE_DISPLAY_MAP;
                if (steering)
                    flags |= RACE_DISPLAY_STEERING;
                CHECK(menu_race_display_flags(m) == flags);
                options_capture(m);
                menu_options_storage(argv[2]);
                options_restore(m);
                if (argc > 3)
                    flags ^= RACE_DISPLAY_LAP; /* Mutation: stale Full/Simple derived bit. */
                CHECK(menu_race_display_flags(m) == flags);
                CHECK(menu_race_display_flags(NULL) == flags);
                check_draws(h, ~flags);
            }
    /* Independent flags, not just the two scripted presets. */
    for (uint32_t bit = RACE_DISPLAY_MAP; bit <= RACE_DISPLAY_G_METER; bit <<= 1)
        check_draws(h, bit);
    const char *fields[] = {"race_display_meter",    "race_display_time",
                            "race_display_lap",      "race_display_position",
                            "race_display_car_name", "race_display_suggested_gear",
                            "race_display_g_meter"};
    ah_val option_value;
    CHECK(ah_get_member(m->vm, game_object(m), "option", &option_value));
    ah_obj *option = ah_as_obj(option_value, AO_MODULE);
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; i++) {
        for (int value = 0; value < 2; value++) {
            ah_set_member(m->vm, option, fields[i], ah_int(value));
            options_capture(m);
            ah_set_member(m->vm, option, fields[i], ah_int(1 - value));
            menu_options_storage(argv[2]);
            options_restore(m);
            CHECK(menu_option_number(m, fields[i], -1) == value);
            CHECK(menu_option_number(NULL, fields[i], -1) == value);
        }
    }
    FILE *legacy = fopen(argv[2], "w");
    CHECK(legacy);
    fputs("race_display_config 0 1\n", legacy);
    CHECK(fclose(legacy) == 0);
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; i++)
        ah_set_member(m->vm, option, fields[i], ah_int(1));
    menu_options_storage(argv[2]);
    options_restore(m);
    CHECK(!(menu_race_display_flags(m) & (RACE_DISPLAY_LAP | RACE_DISPLAY_CAR_NAME |
                                         RACE_DISPLAY_SUGGESTED_GEAR | RACE_DISPLAY_G_METER)));
    race_hud_close(h);
    menu_close(m);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    remove(argv[2]);
    puts("PASS original Full/Simple, Map, Steering, persistence and per-element HUD gating");
    return 0;
}
