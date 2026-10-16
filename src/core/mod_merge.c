#include "mod_merge.h"

#include "specdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_TABLE 16u

static uint32_t le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static int magic_is(const uint8_t *b, size_t n, const char *m) { return b && n >= MIN_TABLE && !memcmp(b, m, 4); }

int moon_mod_is_table(const uint8_t *bytes, size_t size)
{
    return magic_is(bytes, size, "GTDB") || magic_is(bytes, size, "GTID") || magic_is(bytes, size, "GTST");
}

/* ---- a table of rows: GTDB -------------------------------------------------
   Verified by decoding every row through the engine's own reader -- the same
   function the game uses -- so whatever the disc's encoding (raw, template,
   patched template) the merge agrees with the game about what the row IS. */
typedef struct { uint32_t id; const uint8_t *row; int from_mod; } merged_row;

static int by_id(const void *a, const void *b)
{
    const merged_row *x = a, *y = b;
    int32_t xi, yi;
    /* The readers' own order: original GTDB entries compare as SIGNED ids
       (specdb.c:45 sign-extends each identifier before the comparison). */
    memcpy(&xi, &x->id, 4);
    memcpy(&yi, &y->id, 4);
    if (xi != yi) return xi < yi ? -1 : 1;
    return x->from_mod - y->from_mod;          /* the disc's row wins a clash */
}

static uint8_t *merge_gtdb(const uint8_t *d, size_t dn, const uint8_t *m, size_t mn, size_t *out_size, const char **why)
{
    uint32_t dc = le32(d + 8), dw = le32(d + 12);
    uint32_t mc = le32(m + 8), mw = le32(m + 12);
    if (!dw || !dc || dc > (dn - MIN_TABLE) / 8) { *why = "malformed GTDB (disc)"; return NULL; }
    if (!mc || mc > (mn - MIN_TABLE) / 8) { *why = "malformed GTDB (mod)"; return NULL; }
    if (dw != mw) { *why = "row width differs from the disc's table"; return NULL; }
    if (dw > 4096) { *why = "implausible row width"; return NULL; }

    uint32_t total = dc + mc;
    merged_row *rows = malloc((size_t)total * sizeof *rows);
    uint8_t *rows_data = malloc((size_t)total * (size_t)dw);
    uint8_t *out = NULL;
    if (rows && rows_data) {
        int ok = 1;
        for (uint32_t i = 0; i < dc && ok; i++) {
            uint8_t *dst = rows_data + (size_t)i * dw;
            if (gt4_specdb_row_at(d, dn, i, dst, dw) != (int)dw) ok = 0;
            else { rows[i].id = le32(d + 16 + (size_t)i * 8); rows[i].row = dst; rows[i].from_mod = 0; }
        }
        for (uint32_t i = 0; i < mc && ok; i++) {
            uint8_t *dst = rows_data + (size_t)(dc + i) * dw;
            if (gt4_specdb_row_at(m, mn, i, dst, dw) != (int)dw) ok = 0;
            else { rows[dc + i].id = le32(m + 16 + (size_t)i * 8); rows[dc + i].row = dst; rows[dc + i].from_mod = 1; }
        }
        if (!ok) *why = "a row would not decode with the engine's own reader";
        else {
            size_t base = 16 + (size_t)total * 8;
            out = malloc(base + (size_t)total * dw);
            if (!out) *why = "out of memory";
            else {
                qsort(rows, total, sizeof *rows, by_id);
                memset(out, 0, base);
                memcpy(out, "GTDB", 4);
                put32(out + 8, total);
                put32(out + 12, dw);
                for (uint32_t i = 0; i < total; i++) {
                    put32(out + 16 + (size_t)i * 8, rows[i].id);
                    put32(out + 20 + (size_t)i * 8, i * dw);
                    memcpy(out + base + (size_t)i * dw, rows[i].row, dw);
                }
                if (out_size) *out_size = base + (size_t)total * dw;
                for (uint32_t i = 1; i < total; i++)
                    if (rows[i].id == rows[i - 1].id)
                        fprintf(stderr, "mod: id %u exists in the disc's table and in a mod; the disc's row is kept\n",
                                (unsigned)rows[i].id);
            }
        }
    } else *why = "out of memory";
    free(rows);
    free(rows_data);
    return out;
}

/* ---- a table of offsets: GTID (label -> id) and GTST (a string pool) -------
   count at +4; `entry_size` bytes per entry at +16 (offset first, then the id
   for GTID); the pool itself follows, addressing back into it. Appending means
   one more entry and the mod's own pool glued on the end -- the disc's strings
   are copied whole and never re-encoded, so a label the disc already has still
   answers byte-for-byte. */
static uint8_t *merge_offset_table(const uint8_t *d, size_t dn, const uint8_t *m, size_t mn,
                                   uint32_t entry_size, size_t *out_size, const char **why)
{
    uint32_t dc = le32(d + 4), mc = le32(m + 4);
    size_t base_d = 16 + (size_t)dc * entry_size, base_m = 16 + (size_t)mc * entry_size;
    if (!dc || dc > (dn - MIN_TABLE) / entry_size || 16 + (size_t)dc * entry_size > dn) { *why = "malformed table (disc)"; return NULL; }
    if (!mc || mc > (mn - MIN_TABLE) / entry_size || 16 + (size_t)mc * entry_size > mn) { *why = "malformed table (mod)"; return NULL; }
    if (dc + mc > 0xffffffu) { *why = "the merged list is implausibly long"; return NULL; }

    size_t pool_d = dn - base_d, pool_m = mn - base_m, total = dc + mc;
    size_t base = 16 + total * entry_size, size = base + pool_d + pool_m;
    uint8_t *out = malloc(size);
    if (!out) { *why = "out of memory"; return NULL; }
    memset(out, 0, base);
    memcpy(out, d, 4);
    put32(out + 4, (uint32_t)total);
    memcpy(out + base, d + base_d, pool_d);
    memcpy(out + base + pool_d, m + base_m, pool_m);
    for (uint32_t i = 0; i < dc; i++) {
        /* The disc's own pool is copied verbatim at the head of the new one, so
           an entry's offset -- always relative to the pool start -- is unchanged. */
        uint32_t off = le32(d + 16 + (size_t)i * entry_size);
        put32(out + 16 + (size_t)i * entry_size, off);
        if (entry_size == 8) put32(out + 20 + (size_t)i * 8, le32(d + 20 + (size_t)i * 8));
    }
    for (uint32_t i = 0; i < mc; i++) {
        uint32_t off = le32(m + 16 + (size_t)i * entry_size);
        size_t at = 16 + (dc + (size_t)i) * entry_size;
        /* A mod entry's offset is pool-relative (0 = the pool's first string),
           but tolerate an absolute offset too: an offset at or past the mod's
           own entry table is measured from the file's start. Both map into the
           merged pool, which holds the disc's pool first. */
        uint32_t rel = off < base_m ? off : (uint32_t)(off - base_m);
        put32(out + at, (uint32_t)(pool_d + rel));
        if (entry_size == 8) put32(out + at + 4, le32(m + 20 + (size_t)i * 8));
    }
    if (entry_size == 8)                     /* label -> id: an id may not be reused (docs/NEW-CARS.md §2) */
        for (uint32_t i = 0; i < mc; i++)
            for (uint32_t j = 0; j < dc; j++)
                if (le32(m + 20 + (size_t)i * 8) == le32(d + 20 + (size_t)j * 8))
                    fprintf(stderr, "mod: id %u is already in the disc's list and a mod adds it again; "
                                    "the disc's entry answers first\n", (unsigned)le32(m + 20 + (size_t)i * 8));
    if (out_size) *out_size = size;
    return out;
}

uint8_t *moon_mod_merge_table(const uint8_t *disc, size_t disc_size, const uint8_t *mod, size_t mod_size,
                              size_t *out_size, const char **why)
{
    const char *dummy = NULL;
    if (!why) why = &dummy;
    *why = NULL;
    if (!disc || !mod || !disc_size || !mod_size) { *why = "empty input"; return NULL; }
    int gtdb = magic_is(disc, disc_size, "GTDB") && magic_is(mod, mod_size, "GTDB");
    int gtid = magic_is(disc, disc_size, "GTID") && magic_is(mod, mod_size, "GTID");
    int gtst = magic_is(disc, disc_size, "GTST") && magic_is(mod, mod_size, "GTST");
    if (gtdb) return merge_gtdb(disc, disc_size, mod, mod_size, out_size, why);
    if (gtid) return merge_offset_table(disc, disc_size, mod, mod_size, 8, out_size, why);
    if (gtst) return merge_offset_table(disc, disc_size, mod, mod_size, 4, out_size, why);
    *why = "the disc's file and the mod's are not the same kind of table";
    return NULL;
}
