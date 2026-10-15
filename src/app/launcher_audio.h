#ifndef OPENGT4_LAUNCHER_AUDIO_H
#define OPENGT4_LAUNCHER_AUDIO_H
#include <stdint.h>
typedef struct launcher_audio launcher_audio;
launcher_audio *launcher_audio_open(const char *disc, int device);
void launcher_audio_close(launcher_audio *audio);
void launcher_audio_volume(launcher_audio *audio, int music, int effects);
void launcher_audio_play(launcher_audio *audio, const char *name);
int launcher_audio_ready(launcher_audio *audio);
void launcher_audio_render(launcher_audio *audio, int16_t *out, int frames);
#endif
