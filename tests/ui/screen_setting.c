#include "../../src/app/race_game.c"
#include "../../src/core/race_pause.c"
#include "screen_setting.h"
#include "video_gl.h"
#include "license_image.h"
#include <time.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "screen setting line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static int check_image(race_game *game, const int options[4], int negative)
{
    const uint8_t input[] = {32, 96, 192, 255};
    plat_texture *texture = plat_texture_create(1, 1, input);
    CHECK(texture);
    plat_video_begin_frame();
    const uint8_t white[4] = {255, 255, 255, 255};
    plat_draw_quad(texture, 0, 0, 640, 448, 0, 0, 1, 1, white);
    video_flush2d();
    int sent[4];
    memcpy(sent, options, sizeof sent);
    if (negative)
        sent[0] += 4;
    CHECK(gt4_save_option_array_set(display_option(game->replaying), sent, 4));
    race_game_draw_hud(game, 0);
    float parameters[6];
    uint8_t lut[256][3], mix[3][3], expected[3];
    gt4_license_image_options(options, parameters);
    gt4_license_image_lut(parameters, lut);
    gt4_license_image_mix(parameters[5], mix);
    gt4_license_image_pixel(lut, mix, input, expected);
    int x, y, w, h;
    video_viewport(0, 0, 640, 448, &x, &y, &w, &h);
    for (int i = 0; i < 5; i++) {
        uint8_t pixel[4];
        glReadPixels(x + 1 + (w - 3) * i / 4, y + h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        for (int c = 0; c < 3; c++) {
            if (abs((int)pixel[c]-expected[c])>1) fprintf(stderr,"pixel %d channel %d got %d expected %d viewport %d %d %d %d\n",i,c,pixel[c],expected[c],x,y,w,h);
            CHECK(abs((int)pixel[c] - expected[c]) <= 1);
        }
    }
    /* Display overlays follow correction and retain their authored colors. */
    plat_draw_quad(NULL, 300, 200, 40, 40, 0, 0, 1, 1, white);
    video_flush2d();
    uint8_t hud[4];
    glReadPixels(x + w / 2, y + h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, hud);
    CHECK(hud[0] == 255 && hud[1] == 255 && hud[2] == 255);
    plat_texture_destroy(texture);
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    char directory[1024];
    snprintf(directory, sizeof directory, "%s-%llu", argv[1], (unsigned long long)time(NULL));
    gt4_save_init(directory);
    CHECK(gt4_save_create(1, "Screen settings"));
    int race[] = {3, 2, 1, -1}, replay[] = {-2, -1, -2, 1};
    CHECK(gt4_save_option_array_set("race_color_correction", race, 4));
    CHECK(gt4_save_option_array_set("replay_color_correction", replay, 4));
    race_pause p = {0};
    strb_value args[] = {{0}, {0}}, result;
    args[0].t = STRB_STR;
    strcpy(args[0].s, "BRIGHTNESS");
    args[1].t = STRB_NUM;
    args[1].n = 5;
    CHECK(call(&p, "SETVAL", args, 2, &result));
    race[0] = 5;
    int actual[4];
    race_pause_display_options(actual);
    CHECK(!memcmp(actual, race, sizeof race));
    p.replay = 1;
    strcpy(args[0].s, "CONTRAST");
    args[1].n = 4;
    CHECK(call(&p, "SETVAL", args, 2, &result));
    replay[1] = 4;
    CHECK(call(&p, "DEFAULTVAL", args, 1, &result));
    CHECK(result.n == 4);
    race_pause_display_options_for(1, actual);
    CHECK(!memcmp(actual, replay, sizeof replay));
    CHECK(gt4_save_store());
    CHECK(gt4_save_load_slot(1));
    race_pause_display_options_for(0, actual);
    CHECK(!memcmp(actual, race, sizeof race));
    race_pause_display_options_for(1, actual);
    CHECK(!memcmp(actual, replay, sizeof replay));
    plat_video_desc desc = {896, 448, 0, 0, 0};
    CHECK(!plat_video_init(&desc));
    SDL_HideWindow(SDL_GL_GetCurrentWindow());
    plat_video_set_view(640, 448);
    video_set_menu_projection(0);
    /* Only the presentation boundary is exercised: clean frames return
       before accessing the non-NULL race/disc sentinels. */
    race_game game = {0};
    game.ready = 1;
    game.run = (gt4_race_run *)&game;
    game.vol = (gt4_vol *)&game;
    game.finished = -1;
    game.cinema.on = game.cinema.clean = 1;
    for (int wide = 0; wide < 2; wide++) {
        video_set_aspect(wide);
        game.replaying = 0;
        CHECK(!check_image(&game, race, argc > 2));
        game.replaying = 1;
        CHECK(!check_image(&game, replay, 0));
        const int identity[4] = {0};
        CHECK(!check_image(&game, identity, 0));
    }
    screen_setting_close(&game.screen_setting);
    CHECK(gt4_save_delete_slot(1));
    gt4_save_shutdown();
    plat_video_shutdown();
    puts("PASS Screen Setting: separate race/replay settings, save, callbacks, GPU pixels, wide capture and HUD");
    return 0;
}
