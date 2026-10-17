/* Native, bounded writer for the CAR4/MDLS/Tex1 formats the decoder reads
   (model.c, tex1.c). The mirror of the reader: a decoded gt4_model goes back
   out as bytes the same readers accept, so a mod can bring a mesh of its own.
   Format layout cross-checked with Nenkai's MIT PDTools; see docs/MODELS.md. */
#ifndef OPENGT4_MODEL_WRITE_H
#define OPENGT4_MODEL_WRITE_H
#include "model.h"
#include <stddef.h>
#include <stdint.h>

/* A car's race cameras: the CAR4 header table at +0x80 (count) / +0x84 (offset),
   32 bytes (8 floats) per entry, up to 32. [0..2] the point's position on the
   car, [3..5] its turns in degrees, [6] the lens, [7] spare (docs/vulcan4/
   research/wheels-and-cameras.md). */
#define GT4_CAR_CAMERA_MAX 32
typedef struct {
    int count;
    float point[GT4_CAR_CAMERA_MAX][8];
} gt4_car_cameras;

/* Encode a decoded model set as one CAR4 file (see gt4_model_encode_car4),
   writing the given camera table at +0x80/+0x84. Pass the donor car's rows
   verbatim (read with gt4_model_read_cameras) or authored ones. A NULL pointer
   (or count 0) writes no table and leaves the output byte-for-byte as before. */
uint8_t *gt4_model_encode_car4_cameras(const gt4_model *m, const gt4_car_cameras *cameras, size_t *size);

/* Read a CAR4's camera table into *out (bounds-checked, <= GT4_CAR_CAMERA_MAX).
   Returns the count (0 when the file carries no table, e.g. a file written
   without cameras). The same header layout the reader uses (car_model.c:412). */
int gt4_model_read_cameras(const uint8_t *bytes, size_t size, gt4_car_cameras *out);

/* Encode a decoded model set as one CAR4 file (a single MDLS at +0x18, no
   Pat0, no wheels/skins/morphs). Positions, normals and UVs are written as
   floats in the model's own units (metres for cars), which the reader returns
   unchanged. No camera table (gt4_model_encode_car4_cameras with NULL).
   Returns a malloc'd buffer and sets *size, or NULL when the model
   cannot be represented (a named refusal, never a guess). Free with free(). */
uint8_t *gt4_model_encode_car4(const gt4_model *m, size_t *size);

/* Encode RGBA images as one "Tex1" file (PSMCT32, optional authored mips).
   Returns a malloc'd buffer and sets *size, or NULL on refusal. */
uint8_t *gt4_tex1_encode(const tex1_image *images, int count, size_t *size);

#endif /* OPENGT4_MODEL_WRITE_H */
