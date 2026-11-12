#ifndef MOON_LAUNCHER_MIXER_H
#define MOON_LAUNCHER_MIXER_H
#include "options_store.h"
#include <SDL.h>
typedef struct {
    int music, effects, muted, open, row, dragging, dirty;
} launcher_mixer;
void launcher_mixer_load(launcher_mixer *m, const moon_options *store);
int launcher_mixer_save(launcher_mixer *m, moon_options *store);
int launcher_mixer_event(launcher_mixer *m, const SDL_Event *event, float x, float y);
int launcher_mixer_music(const launcher_mixer *m);
#endif
