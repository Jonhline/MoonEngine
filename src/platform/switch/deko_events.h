/* SDL pumps Switch events/audio/controllers without owning an EGL window. */
#ifndef OPENGT4_DEKO_EVENTS_H
#define OPENGT4_DEKO_EVENTS_H
#include <SDL.h>

static inline int nd_events_init(void)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER);
}
#endif
