/* Native, bounded writer for the CAR4/MDLS/Tex1 formats the decoder reads
   (model.c, tex1.c). The mirror of the reader: a decoded gt4_model goes back
   out as bytes the same readers accept, so a mod can bring a mesh of its own.
   Format layout cross-checked with Nenkai's MIT PDTools; see docs/MODELS.md. */
#ifndef MOON_MODEL_WRITE_H
#define MOON_MODEL_WRITE_H
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

/* Give an already-encoded CAR4 a donor's rim/front-tyre/rear-tyre (header
   +0x20/+0x2c/+0x30, car_model.c:155) by copying those three byte ranges out
   of the donor's own CAR4 verbatim and appending them, then pointing this
   car's own header fields at the copies. Quick and always safe (the donor's
   geometry is already proven), but ties the wheel to someone else's car; for
   an authored rim/tyre of the author's own, see gt4_model_car4_attach_wheel_parts.
   A wheel missing on the donor (offset 0, or past its own file) is left
   missing here too, never invented. Returns a malloc'd buffer (free() it) or
   NULL when `car4` is not a CAR4 this function can extend. */
uint8_t *gt4_model_car4_attach_wheels(const uint8_t *car4, size_t car4_size,
                                      const uint8_t *donor, size_t donor_size, size_t *out_size);

/* Encode `m` as a bare MDLS (no CAR4 wrapper) -- the exact format a rim or
   tyre needs, since the reader takes one straight from the CAR4 header's
   +0x20/+0x2c/+0x30 offset with no magic of its own there
   (car_model.c:163-164). Feed the result to gt4_model_car4_attach_wheel_parts,
   or free() it. NULL on refusal, same rules as gt4_model_encode_car4. */
uint8_t *gt4_model_encode_wheel_part(const gt4_model *m, size_t *size);

/* Give an already-encoded CAR4 its OWN rim and/or tyre, authored rather than
   borrowed from a donor. The deformation the engine applies to each is
   measured and documented (docs/vulcan4/research/wheels-and-cameras.md §A2,
   wheel_geometry.h): a rim is scaled UNIFORMLY by the real rim radius, so
   model it at radius 1.0 around the local X axis (the spin axis); a tyre is
   remapped RADIALLY from whatever its own min/max radius happens to be onto
   the car's real rim/tyre radii, so its absolute scale does not matter, only
   its shape (a radial profile around local X, inner ring at the bead, outer
   at the tread). `tyre` is used for both the front and rear offset (the
   engine stores them twice regardless of whether they differ); pass NULL for
   either to leave that slot as the body-only "none" sentinel. Returns a
   malloc'd buffer (free() it) or NULL on refusal. */
uint8_t *gt4_model_car4_attach_wheel_parts(const uint8_t *car4, size_t car4_size,
                                           const gt4_model *rim, const gt4_model *tyre,
                                           size_t *out_size);

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

/* P: a mod car's textures at their authored size, beside its CAR4 (car/hires/<model>).
   The CAR4 keeps the PS2-sized copies (4 MB of GS memory, 1024 a side) as the fallback;
   on PC the engine swaps these in after decoding. "MHT1", u32 count, then per image
   u32 index (into the CAR4's textures), u32 width, u32 height, u32 packed size and
   the zlib-packed RGBA (alpha 0..255, already the material's rule). */
uint8_t *gt4_hires_encode(const tex1_image *images, const int *index, int count, size_t *size);
/* Replaces images[index] with the file's when both are powers of two (the UVs then mean
   the same thing at either size). Returns how many were replaced. */
int gt4_hires_apply(const uint8_t *data, size_t size, tex1_image *images, int count);

#endif /* MOON_MODEL_WRITE_H */
