/* P (mouse): hover is presentation only and replays the authored prelight
   (arcade.ad MakerRoot::hidden::Item L3601 bg.actor.out=1, L3615 =0). A dealer clicked,
   left with the right button and then passed over must not stay lit on return. */
#define main pregrid_regression_main
#include "pregrid_menu.c"
#undef main
static ui_widget *root_named(menu *m, const char *name)
{
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), name))
            return ui_root_widget(m->ui, i);
    return NULL;
}
static void frames(menu *m)
{
    for (int i = 0; i < 60; i++) {
        menu_tick(m);
        menu_animate(m, 1.f / 60);
        ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
    }
}
static int lit(menu *m, ui_widget *item)
{
    ah_val actor, out;
    ah_obj *bg = widget_module(m, ui_widget_child(item, "bg"));
    if (!ah_get_member(m->vm, bg, "actor", &actor) || actor.t == AV_NIL ||
        !ah_get_member(m->vm, actor.u.o, "out", &out))
        return -1;
    return ah_truthy(out);
}
static int point(menu *m, ui_widget *w, int action)
{
    const float *r = w->pointer_rect;
    return menu_pointer(m, 1, r[0] + r[2] * .5f, r[1] + r[3] * .5f, action, 0);
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    const char *error = NULL;
    char msg[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    menu *m = menu_open(vol, "arcade", msg, sizeof msg);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    ah_set_member(m->vm, widget_module(m, root_named(m, "GameRoot")), "current_game",
                  ah_str(m->vm, "SingleRace"));
    ah_set_member(m->vm, widget_module(m, root_named(m, "CourseRoot")), "current_crs",
                  ah_str(m->vm, "tsukuba"));
    ah_set_member(m->vm, widget_module(m, root_named(m, "MethodRoot")), "current_method",
                  ah_str(m->vm, "ByMaker"));
    ah_set_member(m->vm, game_object(m), "car_code", ah_str(m->vm, ""));
    ui_widget *maker = root_named(m, "MakerRoot");
    CHECK(maker);
    menu_enter(m, maker->root);
    frames(m);
    CHECK(menu_active_root(m) == maker->root);
    ui_widget *window = ui_widget_child(maker, "ScrollWindow"), *clip = window ? ui_widget_child(window, "clip") : NULL;
    ui_widget *list = clip ? ui_widget_child(clip, "space") : NULL;
    CHECK(list && list->nkids > 2);
    ui_widget *a = list->kids[0], *b = list->kids[1];
    frames(m);
    CHECK(point(m, a, 0) && m->pointer_hover == a);
    CHECK(point(m, a, 1));
    frames(m);
    CHECK(!strcmp(ui_root_name(m->ui, menu_active_root(m)), "CarRoot"));
    /* P: the hover previews a car (CarRoot CarScroll onValueChanged) and keeps
       focus_index; passing back over the focused car must preview it again. */
    ui_widget *car_root = root_named(m, "CarRoot"), *cars = ui_widget_child(car_root, "CarScroll"),
              *face = ui_widget_child(car_root, "CarFace");
    cars = cars ? ui_widget_child(cars, "SelectBox") : NULL;
    CHECK(cars && face && cars->nkids > 2 && cars->focus_index == 0 && face->car_code);
    char first[64];
    snprintf(first, sizeof first, "%s", face->car_code);
    for (int pass = 0; pass < 2; pass++) {
        CHECK(point(m, cars->kids[pass ? 2 : 1], 0));
        frames(m);
        CHECK(cars->focus_index == 0 && strcmp(face->car_code, first));
        if (argc > 3) /* negative: the old guard skipped the focused row */
            cars->hover_shown = 0;
        CHECK(point(m, cars->kids[0], 0));
        frames(m);
        fprintf(stderr, "car hover: back on the first car shows %s (first %s)\n", face->car_code, first);
        CHECK(cars->focus_index == 0 && !strcmp(face->car_code, first));
    }
    menu_pointer(m, 1, 4, 4, 0, 0);
    menu_pointer(m, 1, 4, 4, 2, 0);
    frames(m);
    CHECK(menu_active_root(m) == maker->root);
    /* MakerRoot rebuilds its list on return: look the two dealers up again */
    const char *clicked = a->name, *other = b->name;
    a = ui_widget_child(list, clicked);
    b = ui_widget_child(list, other);
    CHECK(a && b && m->focus[maker->root] == a);
    CHECK(point(m, b, 0) && m->pointer_hover == b);
    frames(m);
    fprintf(stderr, "dealer hover: clicked %s lit %d, hovered %s lit %d\n", a->name, lit(m, a),
            b->name, lit(m, b));
    if (argc > 2) /* negative: the clicked dealer keeps its prelight */
        CHECK(lit(m, a) == 1);
    CHECK(lit(m, b) == 1);
    CHECK(lit(m, a) == 0);
    return 0;
}
