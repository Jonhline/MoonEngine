/* Real SDL/controller events with no SDL window or GL context. */
#include "../../src/platform/switch/deko_events.h"
#include "plat_sdl.h"
#include <stdio.h>
#include <string.h>

#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "deko events line %d: %s (%s)\n", __LINE__, #c, SDL_GetError());       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED, "0");
    CHECK(!nd_events_init());
    CHECK(!SDL_GetWindowFromID(1) && !SDL_GetKeyboardFocus() && !SDL_GL_GetCurrentContext());
    CHECK(!strcmp(SDL_GetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS), "1"));
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6,
                                          SDL_CONTROLLER_BUTTON_MAX, 0);
    CHECK(index >= 0);
    SDL_Joystick *joy = SDL_JoystickOpen(index);
    CHECK(joy && plat_sdl_pad(0));
    plat_key_event event;
    while (plat_sdl_poll(&event)) {
    }
    CHECK(!SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 1));
    SDL_GameControllerUpdate();
    int found = 0;
    while (plat_sdl_poll(&event))
        if (event.pad && event.key)
            found = 1;
    if (argc > 1 && !strcmp(argv[1], "--negative"))
        found = 0; /* Deliberately discard the windowless controller event. */
    CHECK(found);
    CHECK(!SDL_GetWindowFromID(1) && !SDL_GL_GetCurrentContext());
    SDL_JoystickClose(joy);
    CHECK(!SDL_JoystickDetachVirtual(index));
    SDL_Quit();
    puts("deko windowless SDL initialization and controller event: passed");
    return 0;
}
