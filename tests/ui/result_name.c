/* Results consume the entry's short name, as 003DFC50 copies +0x350C. */
#define main machine_fixture_main
#include "machine_flow.c"
#undef main
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    plat_video_desc video = {640, 480, 0, 0, 0};
    CHECK(!plat_video_init(&video));
    race_game *g = race_game_open(argv[1]);
    CHECK(g);
    char error[256];
    CHECK(race_game_start(g, "tsukuba", "ka_01", 0, NULL, NULL, 0, error, sizeof error));
    int ready = 0;
    for (int i = 0; i < 4000 && !ready; i++) {
        SDL_PumpEvents();
        SDL_Delay(5);
        ready = race_game_poll(g);
    }
    CHECK(ready == 1);
    CHECK(g->hud_names[0][0]);
    char expected[128];
    snprintf(expected, sizeof expected, "%s", g->hud_names[0]);
    snprintf(g->names[0], sizeof g->names[0],
             "Full catalogue label must not replace the original entry short name");
    if (argc > 2)
        snprintf(g->hud_names[0], sizeof g->hud_names[0], "%s", g->names[0]);
    plat_video_begin_frame();
    draw_result(g, 1, STEP);
    plat_video_end_frame();
    CHECK(g->result && !strcmp(g->result->rows[0].name, expected));
    race_game_close(g);
    plat_video_shutdown();
    puts("PASS actual result bridge preserves the entry short name");
    return 0;
}
