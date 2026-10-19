/* Pointer scrolling must not strand the controller focus outside its clip. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
static void paint(menu *m)
{
    menu_animate(m, 1.f);
    ui_draw_root(menu_ui(m), menu_active_root(m), NULL, NULL);
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char error[256];
    menu *m = menu_open(vol, "option", error, sizeof error);
    CHECK(m);
    int root = menu_active_root(m);
    if (root < 0) {
        menu_enter(m, 0);
        root = menu_active_root(m);
    }
    ui_widget *page = ui_root_widget(m->ui, root);
    for (int i = 0; i < page->nkids; i++)
        page->kids[i]->visible = 0;
    ui_widget *scroll = ui_widget_from_text(
        m->ui, root,
        "Composite {name string {\"Scroll\"} geometry rectangle {80 40 300 80} children [1] {"
        "Composite {name string {\"clip\"} clip digit {1} geometry rectangle {0 0 300 80} children [1] {"
        "Composite {name string {\"space\"} geometry rectangle {0 0 300 320} children [4] {"
        "Button {name string {\"A\"} geometry rectangle {10 0 100 20}} "
        "Button {name string {\"B\"} geometry rectangle {10 40 100 20}} "
        "Button {name string {\"C\"} geometry rectangle {10 80 100 20}} "
        "Button {name string {\"D\"} geometry rectangle {10 120 100 20}} }}}}}}");
    CHECK(scroll);
    ui_widget_append(page, scroll);
    ui_widget *clip = ui_widget_child(scroll, "clip"), *space = ui_widget_child(clip, "space");
    ui_widget *a = ui_widget_child(space, "A"), *b = ui_widget_child(space, "B");
    menu_native_focus(m, a);
    paint(m);
    CHECK(menu_pointer(m, 1, 100, 60, 0, -25));
    paint(m);
    CHECK(space->y < -100 && m->focus[root] == a);
    menu_send_input(m, MENU_DOWN);
    paint(m);
    if (argc > 2) {
        space->y = space->scroll_ty = -240;
        paint(m);
    }
    CHECK(m->focus[root] == b);
    CHECK(b->pointer_rect[3] > 0);
    CHECK(b->pointer_rect[1] >= clip->pointer_rect[1]);
    menu_send_input(m, MENU_DOWN);
    paint(m);
    CHECK(m->focus[root] != b);
    menu_close(m);
    gt4_vol_close(vol);
    return 0;
}
