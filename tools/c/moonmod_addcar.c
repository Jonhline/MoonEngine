/*
 * The SDK's first function: give a car a row of its own.
 *
 *   moonmod_addcar <disc.iso> <source car label> <new label> <out dir>
 *
 * It reads the user's own disc (never ships anything) and writes a MOD FOLDER
 * holding the three tables a new car needs to exist in the catalog:
 *
 *   GENERIC_CAR.idi               one more label -> id   (the catalog's list)
 *   CAR_VARIATION_american.dbt    the new id -> an existing variation
 *   GENERIC_CAR.dbt               the new id -> the source car's own row
 *
 * The model itself is the source car's, on purpose: this proves the DATA path
 * (a car that is not on the disc appears in the game's list) before any mesh is
 * authored -- docs/NEW-CARS.md §5, step C2. New ids only: the source car stays
 * exactly where it was, and no id is ever reused (a reuse would re-point a car
 * somebody already owns in their save).
 *
 * Tables are written RAW: flags bit 0 clear, rows uncompressed. The engine reads
 * a raw table with the same reader as a compressed one, and the mod-source merge
 * produces raw tables too, so a mod file and a merged table look alike.
 */
#include "specdb.h"
#include "vol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(p) mkdir((p), 0777)
#endif

static uint32_t le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static int write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 0; }
    int ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok;
}

/* mkdir -p: every component, so the caller can name a folder that does not
   exist yet ("/mnt/ssd/mods/my-car/specdb/GT4_US2560"). */
static void mkdir_p(const char *path)
{
    char buf[1200];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++)
        if (*p == '/' || *p == '\\') { *p = 0; MKDIR(buf); *p = '/'; }
    MKDIR(buf);
}

/* GTID: "GTID", count, then {string offset, id} per entry, then the pool of
   u16-length-prefixed, NUL-terminated labels. Offsets are pool-relative. */
static size_t gtid_with(const char *label, uint32_t id, uint8_t *out, size_t cap)
{
    size_t len = strlen(label) + 1;                 /* the stored length counts the NUL */
    size_t total = 16 + 8 + 2 + len;
    if (total > cap || len > 0xffff) return 0;
    memset(out, 0, total);
    memcpy(out, "GTID", 4);
    put32(out + 4, 1);
    put32(out + 16, 0);                             /* the only string sits at the pool's start */
    put32(out + 20, id);
    put16(out + 24, (unsigned)len);
    memcpy(out + 26, label, len);
    return total;
}

/* GTDB: "GTDB", flags(0 = raw), count, width, then {id, row offset} per entry,
   then the rows back to back. */
static size_t gtdb_raw(const uint8_t *row, uint32_t width, uint32_t id, uint8_t *out, size_t cap)
{
    size_t total = 16 + 8 + width;
    if (total > cap) return 0;
    memset(out, 0, total);
    memcpy(out, "GTDB", 4);
    put32(out + 8, 1);
    put32(out + 12, width);
    put32(out + 16, id);
    put32(out + 20, 0);
    memcpy(out + 24, row, width);
    return total;
}

static uint8_t *read_table(gt4_vol *v, const char *name, size_t *size)
{
    char path[192];
    snprintf(path, sizeof path, "/specdb/GT4_US2560/%s", name);
    return gt4_vol_read(v, path, size);
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s <disc.iso> <source car label> <new label> <out dir> [maker] [price]\n", argv[0]);
        fprintf(stderr, "  e.g. %s disc.iso ford_gt_no_stripe_05 moon_coupe /mnt/ssd/mods/moon-coupe ford 500\n", argv[0]);
        return 2;
    }
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "volume: %s\n", err ? err : "failed"); return 1; }

    size_t idi_size = 0, link_size = 0, gen_size = 0;
    uint8_t *idi = read_table(v, "GENERIC_CAR.idi", &idi_size);
    uint8_t *links = read_table(v, "CAR_VARIATION_american.dbt", &link_size);
    uint8_t *generic = read_table(v, "GENERIC_CAR.dbt", &gen_size);
    if (!idi || !links || !generic) { fprintf(stderr, "the disc's car tables are unreadable\n"); return 1; }

    uint32_t source_id = 0;
    if (!gt4_specdb_label(idi, idi_size, argv[2], &source_id)) {
        fprintf(stderr, "no car labelled \"%s\" on this disc\n", argv[2]);
        return 1;
    }
    /* The next free additive id: one past the highest the catalog already uses,
       never a hole (holes are ids the disc reserved, not ids it freed). */
    uint32_t count = le32(idi + 4), highest = 0;
    if (count > (idi_size - 16) / 8) { fprintf(stderr, "malformed GENERIC_CAR.idi\n"); return 1; }
    for (uint32_t i = 0; i < count; i++)
        if (le32(idi + 20 + (size_t)i * 8) > highest) highest = le32(idi + 20 + (size_t)i * 8);
    uint32_t new_id = highest + 1;

    uint8_t link_row[256], gen_row[256];
    if (gt4_specdb_row(links, link_size, source_id, link_row, sizeof link_row) != 8 ||
        gt4_specdb_row(generic, gen_size, source_id, gen_row, sizeof gen_row) != 24) {
        fprintf(stderr, "the source car's own rows do not read back\n");
        return 1;
    }
    /* The price IS this row: the game reads it as a 32-bit INTEGER at +8
       (menu.c GetPrice: the u32 at row+8, sign-extended), so the SDK writes
       credits, not a float. (A float here is read as its bit pattern -- 50.0f
       showed up as 1,112,014,848 credits.) */
    long price = 0;
    if (argc > 6) {
        char *end = NULL;
        double v = strtod(argv[6], &end);
        if (!*argv[6] || *end || v < 1 || v > 1000000000.0) {
            fprintf(stderr, "price must be a whole number of credits, 1..1000000000\n");
            return 2;
        }
        price = (long)(v + 0.5);
        int32_t p = (int32_t)price; memcpy(gen_row + 8, &p, 4);
    } else memcpy(&price, gen_row + 8, 4);       /* the source car's own price (an int32) */

    char sub[1024];
    snprintf(sub, sizeof sub, "%s/specdb/GT4_US2560", argv[4]);
    mkdir_p(sub);

    uint8_t buf[4096];
    char file[1200];
    size_t n;
    int ok = 1;
    snprintf(file, sizeof file, "%s/GENERIC_CAR.idi", sub);
    ok = ok && (n = gtid_with(argv[3], new_id, buf, sizeof buf)) && write_file(file, buf, n);
    printf("%s  label \"%s\" -> id %u\n", file, argv[3], new_id);
    snprintf(file, sizeof file, "%s/CAR_VARIATION_american.dbt", sub);
    ok = ok && (n = gtdb_raw(link_row, 8, new_id, buf, sizeof buf)) && write_file(file, buf, n);
    printf("%s  id %u -> variation %u\n", file, new_id, le32(link_row));
    snprintf(file, sizeof file, "%s/GENERIC_CAR.dbt", sub);
    ok = ok && (n = gtdb_raw(gen_row, 24, new_id, buf, sizeof buf)) && write_file(file, buf, n);
    printf("%s  id %u -> default parts %u, roof byte 0x%02x, price %ld credits\n",
           file, new_id, le32(gen_row), gen_row[0x14], price);

    printf("\nsource car: \"%s\" (id %u) stays on the disc, untouched.\n", argv[2], source_id);
    printf("new car:    \"%s\" (id %u), model = the source car's own.\n", argv[3], new_id);

    /* The manifest: what this mod ADDS, so the engine can put it in the game's
       own lists (the Arcade list and the maker screens are script data, which a
       table merge cannot reach). */
    snprintf(file, sizeof file, "%s/mod.txt", argv[4]);
    FILE *manifest = fopen(file, "wb");
    if (manifest) {
        fprintf(manifest, "# MOON ENGINE mod manifest -- read by the engine, written by the SDK.\n");
        fprintf(manifest, "# optional keys the manager shows: author, version, description, image\n");
        fprintf(manifest, "name %s\n", argv[3]);
        fprintf(manifest, "version 1.0\n");
        fprintf(manifest, "source %s\n", argv[2]);
        fprintf(manifest, "price %ld\n", price);
        fprintf(manifest, "car %s%s%s\n", argv[3], argc > 5 ? " maker=" : "", argc > 5 ? argv[5] : "");
        fclose(manifest);
        printf("%s  the manifest%s\n", file, argc > 5 ? "" : " (no maker: it lists in Arcade, not under a brand)");
    }
    free(idi); free(links); free(generic);
    gt4_vol_close(v);
    return ok ? 0 : 1;
}
