/* UnitRoot.apply must refresh the already-created race on return from Options. */
#include <SDL.h>
#undef main
#define main units_fixture_main
#include "fitted_parts.c"
#undef main

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_save_init("");
    menu_options_storage(NULL);
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol && gt4_game_load_disc(argv[1]));
    char error[256];
    menu *options = menu_open(vol, "option", error, sizeof error);
    CHECK(options);
    int root = -1;
    for (int i = 0; i < ui_root_count(options->ui); i++)
        if (!strcmp(ui_root_name(options->ui, i), "UnitRoot"))
            root = i;
    CHECK(root >= 0);
    race_game race = {0};
    race.unit = race.power_unit = 1;
    const char *power[] = {"PS", "HP", "BHP", "CH", "KW", "CV", "PF"};
    for (int index = 0; index < 7; index++) {
        menu_enter(options, root);
        menu_tick(options);
        ui_widget *page = ui_root_widget(options->ui, root), *pane = ui_widget_child(page, "Pane");
        const char *names[] = {"Velocity", "Power"};
        int values[] = {index % 2, index};
        ah_val result;
        for (int i = 0; i < 2; i++) {
            ui_widget *selector = ui_widget_child(ui_widget_child(pane, names[i]), "OptionMenu");
            CHECK(selector);
            ah_val args[] = {ah_objv(options->context), ah_int(values[i])};
            CHECK(ah_call_member(options->vm, widget_module(options, selector), "setIndex", 2,
                                 args, &result) >= 0);
        }
        CHECK(ah_call_member(options->vm, widget_module(options, page), "apply", 0, NULL,
                             &result) >= 0);
        menu_tick(options);
        CHECK(!strcmp(menu_option_text(options, "unit_power", "?"), power[index]));
        if (argc == 2)
            race_game_apply_units(&race, options);
        CHECK(race.unit == index % 2 && race.power_unit == index);
        /* A cached license/mission menu may still contain older values.
           Race setup must read the published preferences, not that project. */
        CHECK(!strcmp(menu_option_text(NULL, "unit_power", "?"), power[index]));
        race.unit = 1 - index % 2;
        race.power_unit = (index + 1) % 7;
        race_game_apply_units(&race, NULL);
        CHECK(race.unit == index % 2 && race.power_unit == index);
    }
    menu_close(options);
    gt4_vol_close(vol);
    gt4_save_shutdown();
    puts("Applied speed and power units refresh the existing race across repeated Options visits");
    return 0;
}
