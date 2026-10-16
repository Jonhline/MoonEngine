/* Embedded DKSH controls must be writable: dkShaderInitialize patches offsets. */
#ifndef MOON_DEKO_SHADER_BLOB_H
#define MOON_DEKO_SHADER_BLOB_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct nd_shader_blob {
    void *control;
    const unsigned char *code;
    uint32_t code_size;
} nd_shader_blob;

static inline int nd_shader_blob_copy(nd_shader_blob *out, const unsigned char *bytes, size_t size)
{
    uint32_t header[6];
    memset(out, 0, sizeof *out);
    if (!bytes || size < sizeof header || memcmp(bytes, "DKSH", 4))
        return 0;
    memcpy(header, bytes, sizeof header);
    uint32_t control = header[2], code = header[3], programs = header[4];
    if (header[1] < sizeof header || header[1] > control || control > size ||
        code > size - control || !code || (control & 255) || (code & 255) || !header[5] ||
        programs < sizeof header || programs > control || control - programs < 16)
        return 0;
    out->control = malloc(control);
    if (!out->control)
        return 0;
    memcpy(out->control, bytes, control);
    out->code = bytes + control;
    out->code_size = code;
    return 1;
}
#endif
