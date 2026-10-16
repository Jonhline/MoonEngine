#ifndef OPENGT4_IPIC_H
#define OPENGT4_IPIC_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    const uint8_t *data;
    size_t size;
    unsigned count;
} gt4_ipic;
/* Borrows the archive. Invalid offsets, names and picture headers are rejected. */
int gt4_ipic_open(gt4_ipic *archive, const uint8_t *data, size_t size);
const char *gt4_ipic_name(const gt4_ipic *archive, unsigned index);
unsigned gt4_ipic_height(const gt4_ipic *archive, unsigned index);
int gt4_ipic_find(const gt4_ipic *archive, const char *name);
/* Packs the single pictures into one IPU stream, preserving archive order.
   The caller owns the returned buffer. */
uint8_t *gt4_ipic_stream(const gt4_ipic *archive, unsigned height, size_t *size);
#endif
