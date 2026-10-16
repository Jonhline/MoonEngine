/* Native, bounded writer for the CAR4/MDLS/Tex1 formats the decoder reads
   (model.c, tex1.c). The mirror of the reader: a decoded gt4_model goes back
   out as bytes the same readers accept, so a mod can bring a mesh of its own.
   Format layout cross-checked with Nenkai's MIT PDTools; see docs/MODELS.md. */
#ifndef OPENGT4_MODEL_WRITE_H
#define OPENGT4_MODEL_WRITE_H
#include "model.h"
#include <stddef.h>
#include <stdint.h>

/* Encode a decoded model set as one CAR4 file (a single MDLS at +0x18, no
   Pat0, no wheels/skins/morphs). Positions, normals and UVs are written as
   floats in the model's own units (metres for cars), which the reader returns
   unchanged. Returns a malloc'd buffer and sets *size, or NULL when the model
   cannot be represented (a named refusal, never a guess). Free with free(). */
uint8_t *gt4_model_encode_car4(const gt4_model *m, size_t *size);

/* Encode RGBA images as one "Tex1" file (PSMCT32, optional authored mips).
   Returns a malloc'd buffer and sets *size, or NULL on refusal. */
uint8_t *gt4_tex1_encode(const tex1_image *images, int count, size_t *size);

#endif /* OPENGT4_MODEL_WRITE_H */
