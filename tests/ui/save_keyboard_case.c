/* P: the profile name keyboard (boot.mproject EntryRoot Keyboard, driven by save_ui.c) shows
   the capitals the next key types: the first letter, then every letter while caps is on. */
#define main purchase_fixture_main
#include "../gtmode/purchase_menu.c"
#undef main
#include "../../src/app/save_ui.c"
#undef main /* SDL.h renames it */
SDL_Window *plat_sdl_window(void) { return NULL; }
static save_ui screen_ui;
static int press(menu *m, const char *name)
{
    ui_widget *lower = ui_widget_child(screen_ui.keyboard, "LowerCase"),
              *upper = ui_widget_child(screen_ui.keyboard, "UpperCase");
    ui_widget *key = ui_widget_child(upper->visible ? upper : lower, name);
    if (!key && !strstr(name, "caps")) /* caps: save_ui's toggle; the boot keyboard has no key */
        return 0;
    key_action(NULL, NULL, (void *)name, 0, NULL, &(ah_val){0});
    (void)m;
    return 1;
}
static int capitals(void)
{
    return ui_widget_child(screen_ui.keyboard, "UpperCase")->visible &&
           !ui_widget_child(screen_ui.keyboard, "LowerCase")->visible;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    char directory[1024];
    snprintf(directory, sizeof directory, "%s/save-keyboard-%lu", getenv("TEMP") ? getenv("TEMP") : ".",
             (unsigned long)GetCurrentProcessId());
    gt4_save_init(directory);
    CHECK(gt4_game_load_disc(argv[1]));
    const char *why;
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol);
    menu *m = menu_open(vol, "boot", error, sizeof error);
    CHECK(m);
    save_ui_attach(&screen_ui, m);
    ah_val result;
    CHECK(enter_name(NULL, NULL, &screen_ui, 0, NULL, &result) == 0);
    if (argc > 2) /* negative: the first letter shown in lower case */
        CHECK(!capitals());
    CHECK(capitals());
    CHECK(press(m, "key_a") && !strcmp(screen_ui.name, "A") && !capitals());
    CHECK(press(m, "key_b") && !strcmp(screen_ui.name, "Ab") && !capitals());
    CHECK(press(m, "key_capslock") && capitals());
    CHECK(press(m, "key_c") && !strcmp(screen_ui.name, "AbC") && capitals());
    CHECK(press(m, "key_capslock") && !capitals());
    backspace(&screen_ui);
    backspace(&screen_ui);
    CHECK(!strcmp(screen_ui.name, "A") && !capitals());
    backspace(&screen_ui);
    CHECK(!screen_ui.name[0] && capitals());
    return 0;
}
