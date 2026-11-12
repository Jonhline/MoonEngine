/* Host motor output: playback must never actuate the player's controller. */
#include <SDL.h>
#include "plat_sdl.h"
#include "platform/platform.h"
static unsigned calls;
static Uint16 low_motor, high_motor;
static int last_port;
static float torque;
static SDL_GameController *test_pad(int port)
{
    last_port = port;
    return (SDL_GameController *)(uintptr_t)(port + 1);
}
static int test_rumble(SDL_GameController *pad, Uint16 low, Uint16 high, Uint32 duration)
{
    (void)pad;
    (void)duration;
    calls++;
    low_motor = low;
    high_motor = high;
    return 0;
}
static void test_ffb(int port, float value)
{
    last_port = port;
    torque = value;
}
#define plat_sdl_pad test_pad
#define SDL_GameControllerRumble test_rumble
#define plat_input_set_ffb test_ffb
#undef main
#define main rumble_fixture_main
#include "fitted_parts.c"
#undef main
int main(int argc, char **argv)
{
    (void)argv;
    race_game game = {0};
    playing = &game;
    player = 1;
    player_2p = 2;
    const uint8_t output[] = {1, 127, 64};
    for (int port = 0; port < 2; port++) {
        game.split = port;
        uint32_t car = port ? player_2p : player;
        game.replaying = game.demonstrating = game.pause = game.photo_held = 0;
        CHECK(host_write(NULL, car, output) == 1);
        CHECK(low_motor == 127 * 516 && high_motor == 32768 && torque > 0);
        CHECK(last_port == port);
        for (int mode = 0; mode < 4; mode++) {
            game.replaying = mode == 0;
            game.demonstrating = mode == 1;
            game.pause = mode == 2;
            game.photo_held = mode == 3;
            CHECK(host_write(NULL, car, output) == 1);
            if (argc > 1 && mode == 0)
                low_motor = 1; /* Negative: leaked playback vibration. */
            CHECK(!low_motor && !high_motor && torque == 0);
        }
    }
    unsigned before = calls;
    CHECK(!host_write(NULL, 99, output) && calls == before);
    game.split = 0;
    game.photo_held = 0;
    CHECK(host_write(NULL, player, output) == 1 && low_motor && high_motor);
    race_game_stop(&game);
    CHECK(!low_motor && !high_motor && torque == 0);
    playing = NULL;
    puts("PASS live P1/P2 motors preserved; replay/demo/pause/photo and stop clear motors");
    return 0;
}
