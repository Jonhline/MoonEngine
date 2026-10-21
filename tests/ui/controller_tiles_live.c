/* P: both values of interface.controller_tiles on the same cached page.
   The disc builds device tiles in option.ad L228..310; do not reload the VM. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
static ui_widget *tile(ui_widget *space, const char *image)
{
    for (int i = 0; i < space->nkids; i++) {
        ui_widget *bg = ui_widget_child(space->kids[i], "bg");
        if (bg && bg->image_path && strstr(bg->image_path, image))
            return space->kids[i];
    }
    return NULL;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_save_init("");
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    char msg[256];
    menu_set_controller_tiles(0);
    menu *m = menu_open(vol, "option", msg, sizeof msg);
    CHECK(m);
    ah_set_log(m->vm, log_script, NULL);
    int root = -1;
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), "OptionRoot")) root = i;
    CHECK(root >= 0);
    menu_enter(m, root);
    menu_tick(m);
    ui_widget *page = ui_root_widget(m->ui, root);
    ui_widget *space = ui_widget_child(ui_widget_child(ui_widget_child(page,"ScrollWindow"),"clip"),"space");
    CHECK(space);
    ui_widget *wheel = tile(space,"CTR_port0"), *pad = tile(space,"DS2_port0");
    CHECK(wheel && pad && wheel->visible);
    float height = space->h, y = wheel->y;
    char label[256];
    snprintf(label, sizeof label, "%s", ui_widget_child(pad,"label")->text);
    int count = space->nkids;
    for (int pass = 0; pass < 6; pass++) {
        set_focus(m, wheel);
        menu_set_controller_tiles(1);
        if (argc <= 2 || pass) menu_tick(m); /* negative retains the stale page */
        CHECK(!wheel->visible && m->focus[root] == pad && space->h < height);
        CHECK(strstr(ui_widget_child(pad,"label")->text, "Controller"));
        menu_set_controller_tiles(0);
        menu_tick(m);
        CHECK(wheel->visible && wheel->y == y && space->h == height);
        CHECK(!strcmp(ui_widget_child(pad,"label")->text,label));
        CHECK(space->nkids == count && menu_active_root(m) == root);
    }
    CHECK(!errors);
    menu_close(m);
    gt4_vol_close(vol);
    puts("PASS live reversible controller tiles, focus, labels and no cumulative layout shift");
    return 0;
}
