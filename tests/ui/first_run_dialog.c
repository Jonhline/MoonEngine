/* Original dialogue resources, visible icon alignment and onboarding routes. */
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
    ah_val result;
    welcome_answer(NULL, NULL, NULL, 0, NULL, &result);
    CHECK(state.welcome_step == 1);
    ui_widget *message = ui_widget_child(
        ui_widget_child(ui_widget_child(state.welcome, "Dialog"), "Message"), "message");
    CHECK(strstr(message->text, "import a GT4 savegame"));
    state.welcome_step = 5;
    welcome_question(&state);
    ui_widget *indicator = ui_widget_child(state.page, "Memorycard");
    CHECK(indicator && state.welcome_arrow && state.welcome_arrow->visible);
    ui_widget *arrow_image = ui_widget_child(state.welcome_arrow, "bg");
    CHECK(arrow_image);
    float arrow_center = state.welcome_arrow->x + arrow_image->x + arrow_image->w * .5f;
    float disk_center = indicator->x + (4.5f + 20.5f) * .5f;
    if (argc > 2) /* Negative: old enclosing-rectangle alignment. */
        arrow_center = indicator->x + 15 + arrow_image->x;
    CHECK(fabsf(arrow_center - disk_center) < .001f);
    welcome_answer(NULL, NULL, (void *)1, 0, NULL, &result);
    CHECK(state.welcome_step == 6);
    welcome_answer(NULL, NULL, (void *)1, 0, NULL, &result);
    CHECK(state.welcome_step == 7 && !state.welcome_arrow->visible);
    ui_widget *buttons = ui_widget_child(ui_widget_child(state.welcome, "Dialog"), "Buttons");
    CHECK(ui_widget_child(buttons, "YesNo")->visible && !ui_widget_child(buttons, "OK")->visible);
    for (int step = 8; step <= 10; step++) {
        welcome_answer(NULL, NULL, (void *)1, 0, NULL, &result);
        CHECK(state.welcome_step == step && ui_widget_child(buttons, "OK")->visible);
    }
    CHECK(strstr(message->text, "Status") && strstr(message->text, "START"));
    welcome_answer(NULL, NULL, (void *)1, 0, NULL, &result);
    CHECK(!state.onboarding && !state.first_run && state.autosave_explained);
    state.onboarding = state.first_run = 1;
    state.welcome_step = 7;
    welcome_question(&state);
    welcome_answer(NULL, NULL, NULL, 0, NULL, &result);
    CHECK(!state.onboarding && !state.first_run);
    menu_close(m);
    gt4_vol_close(vol);
    puts("PASS GT4 import question, aligned HD arrow, optional legacy transfer instructions");
    return 0;
}
