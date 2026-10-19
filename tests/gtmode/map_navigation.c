/* The GT Mode map (gtmode TopRoot) with the controller: every landmark is left by some
   direction. Its authored grid (navigate_source/target) lies apart from the landmarks:
   DirtAndSnow's cell is 520 to its right, outside the clip once the native map scroll
   brings DirtAndSnow into view, and the original navigator (0026F9E0) then found no
   neighbour: a player stuck at the map's bottom-left courses. Optional arguments: a
   sequence of l r u d x o inputs to trace, or "dump" for the map's widgets. */
#define main purchase_fixture_main
#include "purchase_menu.c"
#undef main
static void dump(ui_widget *w, int depth, float ox, float oy)
{
    if (!w || depth > 6 || !w->visible)
        return;
    ox += w->x;
    oy += w->y;
    if (w->name)
        printf("%*s%s [%s] %.0f,%.0f %.0fx%.0f\n", depth * 2, "", w->name, w->type ? w->type : "",
               ox, oy, w->w, w->h);
    for (int i = 0; i < w->nkids; i++)
        dump(w->kids[i], depth + 1, ox, oy);
}
static menu_input input_of(char c)
{
    return c == 'l' ? MENU_LEFT : c == 'r' ? MENU_RIGHT : c == 'u' ? MENU_UP
         : c == 'd' ? MENU_DOWN : c == 'x' ? MENU_ACCEPT : MENU_BACK;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    int negative = argc > 2 && !strcmp(argv[2], "--negative");
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
    int root = menu_active_root(m);
    CHECK(!strcmp(ui_root_name(m->ui, root), "TopRoot"));
    if (argc > 2 && !strcmp(argv[2], "dump")) {
        dump(ui_root_widget(m->ui, root), 0, 0, 0);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[2], "sweep")) { /* each landmark: where each direction goes */
        ui_widget *space = kid(kid(kid(ui_root_widget(m->ui, root), "ScrollWindow"), "clip"), "space");
        CHECK(space);
        for (int i = 0; i < space->nkids; i++) {
            ui_widget *w = space->kids[i];
            ui_widget *model = kid(w, "bg");
            if (!w->visible || !w->name || !model || strcmp(model->type, "ModelFace"))
                continue;
            printf("%-16s", w->name);
            for (int d = 0; d < 4; d++) {
                menu_native_focus(m, w);
                ticks(m);
                menu_send_input(m, input_of("lrud"[d]));
                ticks(m);
                printf(" %c:%-16s", "lrud"[d], strcmp(menu_focus_name(m), w->name) ? menu_focus_name(m) : "-");
            }
            printf("\n");
        }
        return 0;
    }
    if (argc > 2 && !negative) {
        for (const char *p = argv[2]; *p; p++) {
            menu_send_input(m, input_of(*p));
            ticks(m);
            root = menu_active_root(m);
            printf("%c -> %s focus %s\n", *p, ui_root_name(m->ui, root), menu_focus_name(m));
        }
        return 0;
    }
    /* the player's path: down from Home to the bottom-left courses, then right */
    const char *path = "dddr";
    for (const char *p = path; *p; p++) {
        menu_send_input(m, input_of(*p));
        ticks(m);
    }
    printf("map: dddr -> %s\n", menu_focus_name(m));
    if (negative) /* negative: stuck on DirtAndSnow as before */
        CHECK(!strcmp(menu_focus_name(m), "DirtAndSnow"));
    CHECK(!strcmp(menu_focus_name(m), "TunerCar"));
    /* every landmark: reached by the controller's own focus, at least one way out */
    ui_widget *space = kid(kid(kid(ui_root_widget(m->ui, root), "ScrollWindow"), "clip"), "space");
    CHECK(space);
    int landmarks = 0, stuck = 0;
    for (int i = 0; i < space->nkids; i++) {
        ui_widget *w = space->kids[i];
        ui_widget *model = kid(w, "bg");
        if (!w->visible || !w->name || !model || strcmp(model->type, "ModelFace"))
            continue;
        landmarks++;
        int out = 0;
        for (int d = 0; d < 4 && !out; d++) {
            menu_native_focus(m, w);
            ticks(m);
            if (strcmp(menu_focus_name(m), w->name))
                break; /* not focusable now */
            menu_send_input(m, input_of("lrud"[d]));
            ticks(m);
            out = strcmp(menu_focus_name(m), w->name) != 0;
        }
        if (!out && !strcmp(menu_focus_name(m), w->name)) {
            printf("map: stuck on %s\n", w->name);
            stuck++;
        }
    }
    printf("map: %d landmarks, %d stuck\n", landmarks, stuck);
    CHECK(landmarks > 20 && !stuck);
    /* the same direction goes to the same landmark wherever the scroll is (mid-animation) */
    int moved = 0, differ = 0;
    for (int i = 0; i < space->nkids; i++) {
        ui_widget *w = space->kids[i];
        ui_widget *model = kid(w, "bg");
        if (!w->visible || !w->name || !model || strcmp(model->type, "ModelFace"))
            continue;
        for (int d = 0; d < 4; d++) {
            char at[2][64];
            for (int k = 0; k < 2; k++) {
                menu_native_focus(m, w);
                ticks(m);
                space->x += k * 160;
                space->y -= k * 90;
                menu_send_input(m, input_of("lrud"[d]));
                snprintf(at[k], sizeof at[k], "%s", menu_focus_name(m));
                ticks(m);
            }
            moved += strcmp(at[0], w->name) != 0;
            if (strcmp(at[0], at[1])) {
                printf("map: %s %c -> %s, scrolled %s\n", w->name, "lrud"[d], at[0], at[1]);
                differ++;
            }
        }
    }
    printf("map: %d moves, %d depend on the scroll\n", moved, differ);
    CHECK(!differ && moved > 100);
    menu_native_focus(m, kid(space, "Home"));
    ticks(m);
    menu_send_input(m, MENU_LEFT);
    ticks(m);
    CHECK(!strcmp(menu_focus_name(m), "GTAuto"));
    menu_send_input(m, MENU_RIGHT);
    ticks(m);
    CHECK(!strcmp(menu_focus_name(m), "Home"));
    menu_send_input(m, MENU_RIGHT);
    ticks(m);
    CHECK(!strcmp(menu_focus_name(m), "ExtremeHall"));
    return 0;
}
