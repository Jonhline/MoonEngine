/* Actual authored MusicRoot scroll followed by controller input. */
#define plat_draw_rotated fixture_draw_rotated
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
#undef plat_draw_rotated
static float tip_y;
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
    (void)a;
    (void)c;
    tip_y = cy;
    cursor_draws++;
}
static void paint(menu *m)
{
    for (int i = 0; i < 60; i++) {
        menu_tick(m);
        menu_animate(m, 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
    }
}
int main(int argc, char **argv)
{
    CHECK(argc>=2 && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char error[256];
    menu *m = menu_open(vol, "slide", error, sizeof error);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    int root = -1;
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "MusicRoot"))
            root = i;
    CHECK(root>=0);
    menu_enter(m, root);
    paint(m);
    ui_widget *page = ui_root_widget(m->ui, root),
              *list = ui_widget_child(ui_widget_child(page, "List"), "list_box");
    CHECK(list);
    fprintf(stderr, "music rows %d focus %s index %d errors %u\n", list->nkids,
            m->focus[root] ? m->focus[root]->name : "nil", list->focus_index, errors);
    CHECK(list->nkids>15);
    if (argc > 2)
        list->list_drag_index = 0;
    ah_val drag;
    CHECK(ah_get_member(m->vm,widget_module(m,list),"drag_index",&drag) && drag.t==AV_INT && drag.u.i==-1);
    ah_set_member(m->vm, widget_module(m, list), "drag_index", ah_int(3));
    CHECK(ah_get_member(m->vm,widget_module(m,list),"drag_index",&drag) && drag.u.i==3);
    ah_set_member(m->vm, widget_module(m, list), "drag_index", ah_int(-1));
    set_focus(m, list);
    list->focus_index = 0;
    paint(m);
    float *r = list->pointer_rect;
    CHECK(menu_pointer(m,1,r[0]+r[2]/2,r[1]+r[3]/2,0,-25));
    paint(m);
    fprintf(stderr, "mouse offset %f selected %d\n", list->desktop_offset, list->focus_index);
    CHECK(list->desktop_scrolled && list->focus_index==0);
    menu_send_input(m, MENU_DOWN);
    paint(m);
    fprintf(stderr, "pad index %d focus %s row y %f h%f\n", list->focus_index,
            m->focus[root] ? m->focus[root]->name : "nil", list->kids[1]->pointer_rect[1],
            list->kids[1]->pointer_rect[3]);
    CHECK(!list->desktop_scrolled && list->focus_index==1);
    CHECK(list->kids[1]->pointer_rect[3]>0);
    for (int i = 0; i < 4; i++)
        menu_send_input(m, MENU_DOWN);
    paint(m);
    CHECK(list->focus_index==5);
    fprintf(stderr, "cursor y %f draws%d\n", tip_y, cursor_draws);
    CHECK(tip_y>58 && tip_y<418);
    /* A later selection, mouse movement without clicking, and bottom/top scroll. */
    for (int pass = 0; pass < 3; pass++) {
        list->focus_index = pass ? 35 : 64;
        paint(m);
        menu_pointer(m, 1, 300, 200, 0, pass == 1 ? 25 : -25);
        paint(m);
        int old = list->focus_index;
        menu_send_input(m, MENU_UP);
        paint(m);
        CHECK(list->focus_index==old-1 && !list->desktop_scrolled);
        CHECK(list->kids[old-1]->pointer_rect[3]>0 && tip_y>58 && tip_y<418);
    }
    CHECK(!errors);
    menu_close(m);
    gt4_vol_close(vol);
    return 0;
}
