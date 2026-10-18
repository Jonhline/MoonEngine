/* Championship score identity must survive the pre-race grid permutation. */
#include <SDL.h>
#undef main
#define main points_fixture_main
#include "fitted_parts.c"
#undef main

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    gt4_race_desc desc = {0};
    desc.cars = 6;
    snprintf(desc.course, sizeof desc.course, "tsukuba");
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    size_t size;
    uint8_t *labels = gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &size);
    CHECK(labels && gt4_specdb_label(labels, size, "ka_01", &desc.car[0]));
    for (int i = 1; i < 6; i++)
        desc.car[i] = desc.car[0];
    free(labels);
    gt4_vol_close(vol);
    char error[256];
    race_game game = {0};
    game.run = gt4_race_run_open(argv[1], &desc, error, sizeof error);
    CHECK(game.run);
    menu_quick_race grid;
    CHECK(race_game_quick_grid(&game, &grid));
    CHECK(grid.player == 5);
    for (int round = 0; round < 3; round++) {
        int32_t points[6] = {10 + round * 10, 6 + round, 4 + round, 3, 2, 1};
        CHECK(race_game_quick_points(&game, &grid, points));
        if (argc > 2)
            memcpy(grid.total_points, points, sizeof points);
        CHECK(grid.total_points[grid.player] == points[0]);
        for (int i = 0; i < 6; i++) {
            gt4_race_hud hud;
            CHECK(gt4_race_run_hud(game.run, i, &hud));
            CHECK(grid.total_points[hud.grid_slot] == points[i]);
        }
    }
    gt4_race_run_close(game.run);
    puts("Championship points follow driver identity into all six pre-race grid rows");
    return 0;
}
