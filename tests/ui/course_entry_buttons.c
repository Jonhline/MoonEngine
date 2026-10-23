/* Real GT Mode course selection through the authored race-mode buttons. */
#define main purchase_fixture_main
#include "../gtmode/purchase_menu.c"
#undef main
static ui_widget *course_find(ui_widget *w, const char *name)
{
    if (w->name && !strcmp(w->name, name))
        return w;
    for (int i = 0; i < w->nkids; i++) {
        ui_widget *found = course_find(w->kids[i], name);
        if (found)
            return found;
    }
    return NULL;
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    const char *error = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &error);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    const char *categories[] = {"RealCircuit", "RoadTrack",   "CityCourse",
                                "DirtAndSnow", "DrivingPark", "PowerAndSpeed"};
    unsigned checked = 0;
    for (unsigned category = 0; category < 6; category++) {
        char msg[256];
        menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
        CHECK(m);
        ah_set_log(m->vm, log_script, NULL);
        menu_enter(m, 0);
        ticks(m);
        ui_widget *item =
            course_find(ui_root_widget(m->ui, menu_active_root(m)), categories[category]);
        CHECK(item);
        set_focus(m, item);
        menu_send_input(m, MENU_ACCEPT);
        ticks(m);
        CHECK(!strcmp(ui_root_name(m->ui,menu_active_root(m)),"CourseEntryRoot"));
        ui_widget *root = ui_root_widget(m->ui, menu_active_root(m));
        ui_widget *buttons = kid(root, "Buttons");
        CHECK(buttons);
        ui_widget *list = kid(kid(root, "CourseList"), "SelectBox");
        CHECK(list && list->nkids);
        ui_widget *free_button = kid(kid(buttons, "Unit"), "FreeRun");
        CHECK(free_button);
        if (argc > 2 && !strcmp(argv[2], "--negative"))
            free_button->has_focus_override = 0;
        CHECK(!navigation_can_focus(free_button));
        CHECK(!ah_truthy(get(m->vm,widget_module(m,free_button),"can_focus")));
        unsigned prior_errors = script_errors;
        for (int i = 0; i < list->nkids; i++) {
            set_focus(m, list->kids[i]);
            ticks(m);
            fprintf(stderr, "%s course=%d/%d before=%s\n", categories[category], i + 1, list->nkids,
                    menu_focus_name(m));
            fflush(stderr);
            if (argc > 2) {
                ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
                float *r = list->kids[i]->pointer_rect;
                float x = r[0] + r[2] * .5f, y = r[1] + r[3] * .5f;
                CHECK(menu_pointer(m,1,x,y,0,0));
                CHECK(menu_pointer(m,1,x,y,1,0));
                for (int frame = 0; frame < 60; frame++) {
                    menu_tick(m);
                    menu_animate(m, 1.f / 60);
                    ui_draw_root(m->ui, menu_active_root(m), NULL, NULL);
                    menu_pointer(m, 1, x, y, 0, 0);
                }
            } else {
                menu_send_input(m, MENU_ACCEPT);
                ticks(m);
            }
            fprintf(stderr, "after=%s buttons=%d\n", menu_focus_name(m), buttons->visible);
            fflush(stderr);
            CHECK(buttons->visible);
            CHECK(!navigation_can_focus(list->kids[i]));
            CHECK(!ah_truthy(get(m->vm,widget_module(m,list->kids[i]),"can_focus")));
            ui_widget *enabled[512];
            int enabled_count = 0;
            candidates(root, enabled, &enabled_count);
            for (int j = 0; j < enabled_count; j++)
                CHECK(enabled[j]->parent!=list);
            CHECK(strcmp(menu_focus_name(m),"CourseItem"));
            menu_send_input(m, MENU_BACK);
            ticks(m);
            CHECK(navigation_can_focus(list->kids[i]));
            CHECK(script_errors==prior_errors);
            checked++;
        }
        menu_close(m);
    }
    printf(
        "PASS %u available courses across six categories: authored focus gating, buttons, return and no additional VM errors\n",
        checked);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    return 0;
}
