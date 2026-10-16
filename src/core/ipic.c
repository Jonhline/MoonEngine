#include "ipic.h"
#include <stdlib.h>
#include <string.h>

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void write32(uint8_t *p, uint32_t n)
{
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (8 * i));
}
static size_t picture_end(const gt4_ipic *a, unsigned index)
{
    size_t offset = read32(a->data + 8 + index * 32);
    size_t limit = index + 1 < a->count ? read32(a->data + 8 + (index + 1) * 32) : a->size;
    if (offset > limit || limit > a->size || limit - offset < 20)
        return 0;
    const uint8_t *p = a->data + offset;
    /* /mpeg/main/course.ipic: one 640-wide IPU picture per entry; category and akasaka are 512 high. */
    if (memcmp(p, "ipum", 4) || p[8] != 128 || p[9] != 2 ||
        ((p[10] != 192 || p[11] != 1) && (p[10] != 0 || p[11] != 2)) || read32(p + 12) != 1)
        return 0;
    for (size_t end = offset + 16; end + 4 <= limit; end++)
        if (!memcmp(a->data + end, "\0\0\1\xb0", 4))
            return end + 4;
    return 0;
}
int gt4_ipic_open(gt4_ipic *a, const uint8_t *data, size_t size)
{
    memset(a, 0, sizeof *a);
    if (!data || size < 8 || memcmp(data, "ipua", 4))
        return 0;
    /* Original: func_002771C8 +0x0C..0x20 (count and table).
       Original: func_00277228 +0x38..0x70 (32-byte entries, name+4, offset+0). */
    unsigned count = read32(data + 4);
    if (!count || count > (size - 8) / 32)
        return 0;
    gt4_ipic candidate = {data, size, count};
    for (unsigned i = 0; i < count; i++) {
        const uint8_t *entry = data + 8 + i * 32;
        if (!memchr(entry + 4, 0, 28) || read32(entry) < 8 + count * 32 ||
            !picture_end(&candidate, i))
            return 0;
    }
    *a = candidate;
    return 1;
}
const char *gt4_ipic_name(const gt4_ipic *a, unsigned index)
{
    return index < a->count ? (const char *)a->data + 8 + index * 32 + 4 : NULL;
}
unsigned gt4_ipic_height(const gt4_ipic *a, unsigned index)
{
    if (index >= a->count)
        return 0;
    const uint8_t *p = a->data + read32(a->data + 8 + index * 32);
    return (unsigned)p[10] | (unsigned)p[11] << 8;
}
int gt4_ipic_find(const gt4_ipic *a, const char *name)
{
    for (unsigned i = 0; i < a->count; i++)
        if (!strcmp(gt4_ipic_name(a, i), name))
            return (int)i;
    return -1;
}
uint8_t *gt4_ipic_stream(const gt4_ipic *a, unsigned height, size_t *size)
{
    *size = 0;
    if (!a->count)
        return NULL;
    uint8_t *stream = malloc(a->size);
    if (!stream)
        return NULL;
    unsigned count = 0;
    size_t used = 16;
    for (unsigned i = 0; i < a->count; i++) {
        if (gt4_ipic_height(a, i) != height)
            continue;
        size_t offset = read32(a->data + 8 + i * 32);
        if (!count++)
            memcpy(stream, a->data + offset, 16);
        size_t length = picture_end(a, i) - offset - 16;
        memcpy(stream + used, a->data + offset + 16, length);
        used += length;
    }
    if (!count) {
        free(stream);
        return NULL;
    }
    write32(stream + 4, (uint32_t)(used - 16));
    write32(stream + 12, count);
    *size = used;
    return stream;
}
