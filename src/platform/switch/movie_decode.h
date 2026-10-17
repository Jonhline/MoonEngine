/* In-process MPEG-2/PS reader. Data remains owned by the caller until close. */
#ifndef MOON_MOVIE_DECODE_H
#define MOON_MOVIE_DECODE_H
#include <stddef.h>
#include <stdint.h>
enum {
    GT4_MOVIE_WIDTH = 640,
    GT4_MOVIE_HEIGHT = 448,
    GT4_MOVIE_BYTES = GT4_MOVIE_WIDTH * GT4_MOVIE_HEIGHT * 4
};
typedef struct gt4_movie_decoder gt4_movie_decoder;
gt4_movie_decoder *gt4_movie_decoder_open(const uint8_t *data, size_t size, int interlace,
                                          int repeat);
/* One 640x448 RGBA picture; 1 picture, 0 end, negative decoding error. */
int gt4_movie_decoder_read(gt4_movie_decoder *decoder, uint8_t *rgba);
void gt4_movie_decoder_close(gt4_movie_decoder *decoder);
#endif
