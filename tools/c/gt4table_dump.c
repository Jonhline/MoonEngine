/* A SpecDB table's own header, printed from the user's disc. Read-only, no
   assets extracted: this answers "can a mod APPEND a row to this table", which
   is the whole question behind adding a car (docs/NEW-CARS.md §1-3).
   usage: gt4table_dump disc.iso [/specdb/GT4_US2560/GENERIC_CAR.dbt ...]   */
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }

/* Raw GTDB (flags bit 0 clear): rows live at base = 16 + count*8, each at
   base + rowoff, width bytes. Anything after the last row is trailing data the
   reader never looks at -- but a writer must not walk over it blindly. */
static void dump_gtid(const char *path, const uint8_t *b, size_t n)
{
    unsigned count = u32(b + 4);
    size_t base = 16 + (size_t)count * 8, highest = 0;
    for (unsigned i = 0; i < count && base + 16 + (size_t)i * 8 + 8 <= n; i++)
        if (u32(b + 20 + (size_t)i * 8) > highest) highest = u32(b + 20 + (size_t)i * 8);
    printf("%-58s GTID rows=%u payload=%zu bytes, file=%zu\n", path, count, highest, n);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: gt4table_dump disc.iso TABLE [/path/...]\n"); return 2; }
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "volume: %s\n", err ? err : "failed"); return 1; }
    for (int a = 2; a < argc; a++) {
        char path[256];
        if (argv[a][0] == '/') snprintf(path, sizeof path, "%s", argv[a]);
        else snprintf(path, sizeof path, "/specdb/GT4_US2560/%s", argv[a]);
        size_t n = 0;
        uint8_t *b = gt4_vol_read(v, path, &n);
        if (!b) { printf("%-58s MISSING\n", path); continue; }
        if (n >= 16 && !memcmp(b, "GTID", 4)) { dump_gtid(path, b, n); free(b); continue; }
        if (n < 16 || memcmp(b, "GTDB", 4)) { printf("%-58s not a GTDB/GTID table (%zu bytes)\n", path, n); free(b); continue; }
        unsigned flags = u16(b + 4), count = u32(b + 8), width = u32(b + 12);
        size_t base = 16 + (size_t)count * 8;
        uint32_t highest = 0;
        int ordered = 1, last = -1;
        for (unsigned i = 0; i < count && base + (size_t)i * 8 + 8 <= n; i++) {
            uint32_t off = u32(b + 20 + (size_t)i * 8);
            if ((int)off < last) ordered = 0;
            last = (int)off;
            if (off > highest) highest = off;
        }
        size_t rows_end = base + highest + width;
        printf("%-58s GTDB flags=0x%x count=%u width=%u  base=%zu highest_off=%u rows_end=%zu file=%zu"
               "  %s  trailing=%zu\n", path, flags, count, width, base, highest, rows_end, n,
               (flags & 1) ? "COMPRESSED-rows" : "raw-rows", n > rows_end ? n - rows_end : 0);
        if (!(flags & 1) && ordered && base + 16 <= n) {
            printf("    first row  %s\n", path);
            for (unsigned i = 0; i < width && i < 24; i++) printf("      +%02u %02x\n", i, b[base + i]);
            printf("    last row bytes (the one a mod would copy): ");
            size_t last_off = 0;
            for (unsigned i = 0; i < count; i++) last_off = u32(b + 20 + (size_t)i * 8);
            for (unsigned i = 0; i < width && base + last_off + i < n; i++) printf("%02x", b[base + last_off + i]);
            printf("\n");
        }
        free(b);
    }
    gt4_vol_close(v);
    return 0;
}
