#ifndef MOON_SDL_MOVIE_H
#define MOON_SDL_MOVIE_H
#include "ipic.h"
#include <stddef.h>
#include <stdint.h>
typedef struct sdl_movie sdl_movie;
/* Silent menu MPEG-2 video. Decoding runs off the render/audio threads. */
sdl_movie *sdl_movie_open(const uint8_t *data, size_t size, int interlace, int repeat,
                          int audio_track);
#ifdef MOON_SWITCH_MOVIES
/* Transfers the complete disc buffer to the native decoder, including failure. */
sdl_movie *sdl_movie_open_owned(uint8_t *data, size_t size, int interlace, int repeat,
                                int audio_track);
#endif
/* Worker-only: decode count IPU pictures into caller-owned 640x448 RGBA frames.
   No renderer calls or playback clock; NULL unless all pictures decode. */
uint8_t *sdl_movie_ipic_decode(const uint8_t *stream, size_t size, unsigned count);
uint8_t *sdl_movie_ipic_archive_decode(const gt4_ipic *archive);
/* Called only by the rendering thread. Disabling closes the device before
   the decoder can be handed to the disposal worker. */
void sdl_movie_audio(sdl_movie *m, int enabled);
int sdl_movie_draw(sdl_movie *m, float x, float y, float w, float h, float opacity);
uint64_t sdl_movie_frame_count(const sdl_movie *m);
int sdl_movie_finished(sdl_movie *m);
void sdl_movie_close(sdl_movie *m);

/* Release renderer-owned resources on the main thread before worker disposal. */
void sdl_movie_release_texture(sdl_movie *m);
int sdl_movie_ready(sdl_movie *m);

#endif
