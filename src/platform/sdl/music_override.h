#ifndef MOON_MUSIC_OVERRIDE_H
#define MOON_MUSIC_OVERRIDE_H
#include <stdint.h>
/* Optional app-owned PCM source. Contexts: menu/race/replay/slideshow/launcher.
   Unregistered by default; the original callbacks and volume remain authoritative. */
typedef int (*moon_music_override)(void *, int, int16_t *, int, int, int);
typedef int (*moon_music_title)(void *, int, char *, int);
void moon_music_override_titles(moon_music_title callback);
int moon_music_override_title(int context, char *out, int capacity);
void moon_music_override_clear(void *user);
void moon_music_override_set(moon_music_override callback, void *user);
int moon_music_override_render(int context, int16_t *out, int frames, int gain, int paused);
#endif
