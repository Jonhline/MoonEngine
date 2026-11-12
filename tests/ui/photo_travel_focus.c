/* PhotoTravelRoot.init_menu (gtmode.ad L13605..13630) sets the focus on the first Ticket inside
   onInitialize, which then hides Slide (L13518). The original setFocus only stores the widget
   (00233268 -> 00235C80); the manager's update (0022FE6C) sends FocusEnter afterwards, so
   Ticket.onFocusEnter (L13703..13712) loads the first course's preview and shows Slide. */
#define main purchase_fixture_main
#include "../gtmode/purchase_menu.c"
#undef main

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    const char *err = NULL;
    gt4_vol *vol = gt4_vol_open(argv[1], &err);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_career_vehicle *factory = gt4_career_vehicle_open(argv[1]);
    CHECK(factory);
    gt4_game_set_car_factory(gt4_career_vehicle_create_seeded, factory);
    gt4_save_init("");
    char msg[256];
    menu *m = menu_open(vol, "gtmode", msg, sizeof msg);
    CHECK(m);
    menu_enter(m, 0);
    ticks(m);
    CHECK(enter_named(m, "PhotoTravelRoot"));
    int root = menu_active_root(m);
    CHECK(!strcmp(ui_root_name(m->ui, root), "PhotoTravelRoot"));
    ui_widget *focus = m->focus[root];
    CHECK(focus && focus->name && !strcmp(focus->name, "Ticket"));
    ui_widget *slide = kid(ui_root_widget(m->ui, root), "Slide");
    ui_widget *flash = kid(slide, "flash");
    CHECK(flash);
    fprintf(stderr, "photo travel: Slide visible %d, flash %s\n", slide->visible,
            flash->flash_path ? flash->flash_path : "(none)");
    if (argc > 2) /* negative: the preview of the course the init focused stays hidden */
        CHECK(!slide->visible);
    CHECK(slide->visible);
    CHECK(flash->flash_path && strstr(flash->flash_path, "image/gtmode/photo/") &&
          strstr(flash->flash_path, ".swf"));
    return 0;
}
