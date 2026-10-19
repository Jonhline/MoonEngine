/* Authored wrapped quick menu and non-wrapped Options root. */
#define main pregrid_fixture_main
#include "pregrid_menu.c"
#undef main
static ui_widget *named_root(menu *m, const char *name)
{
    for (int i = 0; i < ui_root_count(m->ui); i++)
        if (!strcmp(ui_root_name(m->ui, i), name))
            return ui_root_widget(m->ui, i);
    return NULL;
}
static void check_edges(menu *m, ui_widget *root, int wrap, int negative)
{
    CHECK(root && (int)mp_number(root->src,"navigate_wraparound",1)==wrap);
    ui_widget *items[512];
    int count = 0;
    candidates(root, items, &count);
    CHECK(count>1);
    unsigned edge = 0, move = 0;
    for (int i = 0; i < count; i++) {
        for (int dir = MENU_UP; dir <= MENU_RIGHT; dir++) {
            menu_nav_rect r = navigation_rect(items[i], "navigate_source", dir);
            int forward = 0;
            for (int j = 0; j < count; j++) {
                menu_nav_rect t = navigation_rect(items[j], "navigate_source", dir);
                forward |= (r.l + r.r) * .5f < (t.l + t.r) * .5f && r.t == t.t && r.b == t.b;
            }
            int next = navigation_pick(root, items, count, i, dir);
            CHECK(next>=0 && next<count);
            if (!forward && next != i)
                edge++;
            if (next != i)
                move++;
        }
    }
    fprintf(stderr, "%s count%d moves%u edge%u wrap%d\n", root->name, count, move, edge, wrap);
    CHECK(move);
    if (wrap)
        CHECK(edge && !negative);
    else {
        /* A line fixture under the actual authored root isolates its flag
           from unrelated focusable items elsewhere on the screen. */
        ui_widget line[3] = {{0}}, fixture = *root;
        ui_widget *list[3] = {line, line + 1, line + 2};
        for (int i = 0; i < 3; i++) {
            line[i].type = "Button";
            line[i].visible = 1;
            line[i].parent = &fixture;
            line[i].x = 50 + i * 80;
            line[i].w = 50;
            line[i].h = 30;
        }
        fixture.kids = list;
        fixture.nkids = 3;
        fixture.parent = NULL;
        CHECK(navigation_pick(&fixture,list,3,2,MENU_RIGHT)==2);
        CHECK(navigation_pick(&fixture,list,3,0,MENU_LEFT)==0);
    }
    (void)m;
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    const char *why;
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    gt4_save_init("");
    menu *quick = menu_open(vol, "quick-arcade", error, sizeof error);
    CHECK(quick);
    check_edges(quick, named_root(quick, "ArcadeRoot"), 1, argc > 2);
    menu *option = menu_open(vol, "option", error, sizeof error);
    CHECK(option);
    check_edges(option, named_root(option, "OptionRoot"), 0, 0);
    menu_close(option);
    menu_close(quick);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    puts("PASS disc quick-menu wrap and authored Options no-wrap");
    return 0;
}
