#include "../../src/app/race_game.c"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "pause buttons line %d: %s\n", __LINE__, #x);                          \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    CHECK(!SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS));
    SDL_setenv("OPENGT4_WINDOW_HIDDEN", "1", 1);
    plat_video_desc video = {320, 240, 0, 0, 0};
    CHECK(!plat_video_init(&video));
    SDL_Joystick *joy[2];
    for (int port = 0; port < 2; port++) {
        int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6,
                                              SDL_CONTROLLER_BUTTON_MAX, 0);
        CHECK(index >= 0);
        joy[port] = SDL_JoystickOpen(index);
        CHECK(joy[port]);
    }
    gt4_vol *vol = gt4_vol_open(argv[1], NULL);
    CHECK(vol);
    for (int port = 0; port < 2; port++) {
        for (int circle = 0; circle < 2; circle++) {
            race_game game = {0};
            game.vol = vol;
            game.split = port;
            game.finished = -1;
            game.pause = race_pause_open(vol, 0, 1, 0, NULL, NULL);
            CHECK(game.pause);
            for (int tick = 0; tick < 120; tick++)
                race_pause_update(game.pause, 1.0 / 60);
            int button = circle ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A;
            if (argc > 2 && circle)
                button = SDL_CONTROLLER_BUTTON_Y;
            CHECK(!SDL_JoystickSetVirtualButton(joy[port], button, 1));
            SDL_GameControllerUpdate();
            pause_input(&game);
            for (int tick = 0; tick < 120; tick++)
                race_pause_update(game.pause, 1.0 / 60);
            fprintf(stderr, "port=%d circle=%d pad=%p button=%d state=%d\n", port, circle,
                    (void *)game.pad, SDL_GameControllerGetButton(plat_sdl_pad(port), button),
                    race_pause_state(game.pause));
            CHECK(race_pause_state(game.pause) == RACE_PAUSE_CONTINUE);
            CHECK(!SDL_JoystickSetVirtualButton(joy[port], button, 0));
            SDL_GameControllerUpdate();
            race_pause_close(game.pause);
        }
    }
    gt4_vol_close(vol);
    plat_video_shutdown();
    puts("Cross and Circle confirm Continue for both players through the actual pause poller");
    SDL_Quit();
    return 0;
}
