/* Transfer controls stay outside the onboarding modal, for both option values. */
#define main fixture_main
#include "pregrid_menu.c"
#undef main
#include "../../src/app/save_ui.c"
#undef main
SDL_Window *plat_sdl_window(void)
{
    return NULL;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    CHECK(gt4_game_load_disc(argv[1]));
    const char *why;
    char error[256];
    gt4_vol *vol = gt4_vol_open(argv[1], &why);
    CHECK(vol);
    menu *m = menu_open(vol, "boot", error, sizeof error);
    CHECK(m);
    save_ui state;
    save_ui_attach(&state, m);
    save_ui_first_run(&state);
    ui_widget *button = ui_widget_child(state.page, "Export");
    CHECK(button);
    for (int enabled = 0; enabled < 2; enabled++) {
        for (int step = 0; step <= 10; step++) {
            state.welcome_step = step;
            welcome_question(&state);
            save_ui_transfer_tools(&state, enabled);
            save_ui_transfer_tools(&state, enabled);
            if (argc > 2 && enabled)
                button->visible = 1; /* Negative: the option exposes a modal footer. */
            CHECK(!button->visible);
        }
    }
    state.first_run = state.onboarding = 0;
    ah_val result;
    enter_cards(NULL, NULL, &state, 0, NULL, &result);
    CHECK(button->visible);
    save_ui_transfer_tools(&state, 0);
    CHECK(!button->visible);
    enter_cards(NULL, NULL, &state, 0, NULL, &result);
    CHECK(!button->visible);
    menu_close(m);
    gt4_vol_close(vol);
    puts("PASS export visibility across all onboarding steps and normal save screen");
    return 0;
}
