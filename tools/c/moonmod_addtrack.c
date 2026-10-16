/*
 * The SDK's second function: give a course a row of its own.
 *
 *   moonmod_addtrack <disc.iso> <source course label> <new label> <out dir>
 *
 * A course is two files plus two table rows (docs/NEW-MAPS.md §0): the label is
 * the key into /crs/<label> and /crs/<label>.ad, and the SpecDB pair
 * COURSE.idi (label -> id) and COURSE.dbt (the row, width 22) names the id an
 * event can point at.
 *
 * So an added course is: one label -> id, one row, and the two files under the
 * new label. This tool copies the source course's own files -- the user's own
 * data, read from their disc and written back under a new name, nothing shipped
 * by us -- which proves the DATA path before any track is authored by hand.
 *
 * The id is NEW and one past the highest, never a hole and never a reuse: a
 * reused id silently re-points every save that already names it.
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

static void mkdir_p(const char *path)
{
    char buf[1200];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++)
        if (*p == '/' || *p == '\\') { *p = 0; MKDIR(buf); *p = '/'; }
    MKDIR(buf);
}

static size_t gtid_with(const char *label, uint32_t id, uint8_t *out, size_t cap)
{
    size_t len = strlen(label) + 1, total = 16 + 8 + 2 + len;
    if (total > cap || len > 0xffff) return 0;
    memset(out, 0, total);
    memcpy(out, "GTID", 4);
    put32(out + 4, 1);
    put32(out + 16, 0);
    put32(out + 20, id);
    put16(out + 24, (unsigned)len);
    memcpy(out + 26, label, len);
    return total;
}

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
        fprintf(stderr, "usage: %s <disc.iso> <source course label> <new label> <out dir>\n", argv[0]);
        return 2;
    }
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "volume: %s\n", err ? err : "failed"); return 1; }

    size_t idi_size = 0, db_size = 0;
    uint8_t *idi = read_table(v, "COURSE.idi", &idi_size);
    uint8_t *db = read_table(v, "COURSE.dbt", &db_size);
    if (!idi || !db) { fprintf(stderr, "the disc's course tables are unreadable\n"); return 1; }

    uint32_t source_id = 0;
    if (!gt4_specdb_label(idi, idi_size, argv[2], &source_id)) {
        fprintf(stderr, "no course labelled \"%s\" on this disc\n", argv[2]);
        return 1;
    }
    uint32_t count = le32(idi + 4), highest = 0;
    if (count > (idi_size - 16) / 8) { fprintf(stderr, "malformed COURSE.idi\n"); return 1; }
    for (uint32_t i = 0; i < count; i++)
        if (le32(idi + 20 + (size_t)i * 8) > highest) highest = le32(idi + 20 + (size_t)i * 8);
    uint32_t new_id = highest + 1;
    if (new_id > 0xffff) { fprintf(stderr, "course ids are named by a u16 in a RACE row; %u is past it\n", new_id); return 1; }

    uint8_t row[256];
    if (gt4_specdb_row(db, db_size, source_id, row, sizeof row) != 22) {
        fprintf(stderr, "the source course's own row does not read back\n");
        return 1;
    }

    /* The course's two files, under the new label. */
    char path[192], sub[1200], file[1200];
    size_t course_size = 0, ad_size = 0;
    snprintf(path, sizeof path, "/crs/%s", argv[2]);
    uint8_t *course = gt4_vol_read(v, path, &course_size);
    snprintf(path, sizeof path, "/crs/%s.ad", argv[2]);
    uint8_t *ad = gt4_vol_read(v, path, &ad_size);
    if (!course) { fprintf(stderr, "the source course's own file is unreadable\n"); return 1; }

    snprintf(sub, sizeof sub, "%s/specdb/GT4_US2560", argv[4]);
    mkdir_p(sub);
    mkdir_p(argv[4]);
    snprintf(file, sizeof file, "%s/crs", argv[4]);
    mkdir_p(file);

    uint8_t buf[4096];
    size_t n;
    int ok = 1;
    snprintf(file, sizeof file, "%s/COURSE.idi", sub);
    ok = ok && (n = gtid_with(argv[3], new_id, buf, sizeof buf)) && write_file(file, buf, n);
    printf("%s  label \"%s\" -> id %u\n", file, argv[3], new_id);
    snprintf(file, sizeof file, "%s/COURSE.dbt", sub);
    ok = ok && (n = gtdb_raw(row, 22, new_id, buf, sizeof buf)) && write_file(file, buf, n);
    printf("%s  id %u -> the source course's own row\n", file, new_id);
    snprintf(file, sizeof file, "%s/crs/%s", argv[4], argv[3]);
    ok = ok && write_file(file, course, course_size);
    printf("%s  %zu bytes (the disc's /crs/%s)\n", file, course_size, argv[2]);
    if (ad) {
        snprintf(file, sizeof file, "%s/crs/%s.ad", argv[4], argv[3]);
        ok = ok && write_file(file, ad, ad_size);
        printf("%s  %zu bytes (the disc's autodrive line)\n", file, ad_size);
    } else fprintf(stderr, "note: the source course has no .ad companion\n");

    printf("\nsource course: \"%s\" (id %u) stays on the disc, untouched.\n", argv[2], source_id);
    printf("new course:    \"%s\" (id %u), same course and line as the source.\n", argv[3], new_id);

    snprintf(file, sizeof file, "%s/mod.txt", argv[4]);
    FILE *manifest = fopen(file, "wb");
    if (manifest) {
        fprintf(manifest, "# MOON ENGINE mod manifest -- read by the engine, written by the SDK.\n");
        fprintf(manifest, "name %s\n", argv[3]);
        fprintf(manifest, "source %s\n", argv[2]);
        fprintf(manifest, "course %s\n", argv[3]);
        fclose(manifest);
        printf("%s  the manifest\n", file);
    }
    free(idi); free(db); free(course); free(ad);
    gt4_vol_close(v);
    return ok ? 0 : 1;
}
