/* Original MissionRoot focus callbacks, gtmode.ad L9007..9015. */
#define plat_draw_rotated fixture_draw_rotated
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#undef plat_draw_rotated
static float cursor_angle;
static int cursor_draws;
void plat_draw_rotated(const plat_texture *t, float x, float y, float w, float h, float cx,
                       float cy, float a, const uint8_t c[4])
{
    (void)t;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)cx;
    (void)cy;
    (void)c;
    cursor_angle = a;
    cursor_draws++;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    CHECK(gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char msg[256];
    menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    int root = -1;
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "MissionRoot"))
            root = i;
    CHECK(root>=0);
    menu_enter(m, root);
    for (int i = 0; i < 60; i++) {
        menu_tick(m);
        menu_animate(m, 1.f / 60);
        ui_draw_root(m->ui, root, NULL, NULL);
    }
    ui_widget *page = ui_root_widget(m->ui, root), *items[512];
    int n = 0;
    candidates(page, items, &n);
    CHECK(n>=34 && !errors);
    ui_widget *space =
        ui_widget_child(ui_widget_child(ui_widget_child(page, "ScrollWindow"), "clip"), "space");
    CHECK(space && space->w>640 && space->h<=386);
    ui_widget *over = ui_widget_child(items[1], "over");
    CHECK(over);
    float *r = items[1]->pointer_rect;
    CHECK(menu_pointer(m,1,r[0]+r[2]*.5f,r[1]+r[3]*.5f,0,0));
    CHECK(m->pointer_hover==items[1]);
    float previous = over->opacity;
    menu_animate(m, 1.f / 60);
    fprintf(stderr, "mission fade %f -> %f\n", previous, over->opacity);
    CHECK(over->opacity<previous && over->opacity>0);
    for (int i = 0; i < 60; i++)
        menu_animate(m, 1.f / 60);
    CHECK(over->opacity==0);
    menu_pointer(m, 1, 10, 10, 0, 0);
    menu_animate(m, 1.f / 60);
    CHECK(over->opacity>0 && over->opacity<1);
    ui_widget *focus = m->focus[root];
    CHECK(menu_pointer(m,1,320,200,0,-4));
    if (argc > 2)
        space->scroll_tx = 0; /* negative: old vertical-only wheel */
    CHECK(space->scroll_tx<0 && m->focus[root]==focus);
    float target = space->scroll_tx;
    menu_animate(m, 1.f / 60);
    CHECK(space->x<0 && space->x>target);
    for (int i = 0; i < 60; i++) {
        menu_animate(m, 1.f / 60);
        ui_draw_root(m->ui, root, NULL, NULL);
    }
    CHECK(fabsf(space->x-target)<.5f);
    /* Same frame order as main: generic edges, map adapter, animation/draw. */
    ui_pointer_smoothing(m->ui, 1);
    space->x = space->scroll_tx = 0;
    menu_pointer(m, 1, 639, 200, 0, 0);
    ui_pointer_edge_scroll(m->ui, root, .02f);
    CHECK(space->scroll_tx < 0 && m->focus[root] == focus);
    menu_edge_pan(m, 1, .02f);
    if (getenv("GT4_MISSION_EDGE_NEGATIVE"))
        ui_pointer_pan_direction(m->ui, 0, 0);
    for (int i = 0; i < 60; i++)
        menu_animate(m, 1.f / 60);
    ui_draw_root(m->ui, root, NULL, NULL);
    fprintf(stderr, "edge right cursor %f draws%d\n", cursor_angle, cursor_draws);
    CHECK(fabsf(cursor_angle-90)<.1f);
    menu_pointer(m, 1, 1, 200, 0, 0);
    ui_pointer_edge_scroll(m->ui, root, .02f);
    menu_edge_pan(m, 1, .02f);
    if (getenv("GT4_MISSION_EDGE_NEGATIVE"))
        ui_pointer_pan_direction(m->ui, 0, 0);
    for (int i = 0; i < 60; i++)
        menu_animate(m, 1.f / 60);
    ui_draw_root(m->ui, root, NULL, NULL);
    fprintf(stderr,"edge left cursor %f target %f clipx%f clipw%f\n",cursor_angle,space->scroll_tx,space->parent->pointer_rect[0],space->parent->pointer_rect[2]);
    CHECK(fabsf(remainderf(cursor_angle+90,360))<.1f);
    menu_send_input(m, MENU_RIGHT);
    for (int i = 0; i < 60; i++) {
        menu_tick(m);
        menu_animate(m, 1.f / 60);
        ui_draw_root(m->ui, root, NULL, NULL);
    }
    CHECK(m->focus[root] && m->focus[root]!=focus);
    CHECK(m->focus[root]->pointer_rect[2]>0 && m->focus[root]->pointer_rect[3]>0);
    CHECK(!errors);
    menu_close(m);
    gt4_vol_close(vol);
    puts("PASS authored mission hover fade and smooth horizontal mouse wheel, unchanged focus");
    return 0;
}
