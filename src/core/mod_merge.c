#include "mod_merge.h"

#include "specdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_TABLE 16u

static uint32_t le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static int magic_is(const uint8_t *b, size_t n, const char *m) { return b && n >= MIN_TABLE && !memcmp(b, m, 4); }

/* One line per table: the same summary (a table read many times) prints once. */
static void mod_log_once(const char *line)
{
    static char seen[64][128];
    static int n;
    for (int i = 0; i < n; i++) if (!strcmp(seen[i], line)) return;
    if (n < 64) snprintf(seen[n++], sizeof seen[0], "%s", line);
    fputs(line, stderr);
}

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

    /* Decode every row through the engine's own reader, then merge BY ID: a mod
       row whose id the disc already has REPLACES the disc's row when the bytes
       differ, is dropped when they are identical, and is added when the id is
       new. The disc's own rows -- including any id it repeats -- are kept whole.
       (The old code compared adjacent rows after sorting, so a disc table with a
       repeated id printed a per-id "kept" line for its own duplicate.) */
    merged_row *disc = malloc((size_t)dc * sizeof *disc);
    uint8_t *disc_data = malloc((size_t)dc * dw);
    merged_row *mod = malloc((size_t)mc * sizeof *mod);
    uint8_t *mod_data = malloc((size_t)mc * dw);
    uint8_t *keep = calloc(mc, 1);
    uint8_t *out = NULL;
    if (!disc || !disc_data || !mod || !mod_data || !keep) { *why = "out of memory"; goto done; }
    {
        int ok = 1;
        for (uint32_t i = 0; i < dc && ok; i++) {
            if (gt4_specdb_row_at(d, dn, i, disc_data + (size_t)i * dw, dw) != (int)dw) ok = 0;
            else { disc[i].id = le32(d + 16 + (size_t)i * 8); disc[i].row = disc_data + (size_t)i * dw; disc[i].from_mod = 0; }
        }
        for (uint32_t i = 0; i < mc && ok; i++) {
            if (gt4_specdb_row_at(m, mn, i, mod_data + (size_t)i * dw, dw) != (int)dw) ok = 0;
            else { mod[i].id = le32(m + 16 + (size_t)i * 8); mod[i].row = mod_data + (size_t)i * dw; mod[i].from_mod = 1; }
        }
        if (!ok) { *why = "a row would not decode with the engine's own reader"; goto done; }

        uint32_t added = 0, overridden = 0, ignored = 0;
        for (uint32_t i = 0; i < mc; i++) {
            int found = -1;
            for (uint32_t j = 0; j < dc; j++) if (disc[j].id == mod[i].id) { found = (int)j; break; }
            if (found < 0) { keep[i] = 1; added++; }
            else if (!memcmp(disc[found].row, mod[i].row, dw)) ignored++;
            else { memcpy((uint8_t *)disc[found].row, mod[i].row, dw); overridden++; }
        }

        uint32_t total = dc + added;
        size_t base = 16 + (size_t)total * 8;
        merged_row *all = malloc((size_t)total * sizeof *all);
        out = all ? malloc(base + (size_t)total * dw) : NULL;
        if (!all || !out) { free(all); free(out); out = NULL; *why = "out of memory"; goto done; }
        memcpy(all, disc, (size_t)dc * sizeof *all);
        { uint32_t at = dc; for (uint32_t i = 0; i < mc; i++) if (keep[i]) all[at++] = mod[i]; }
        qsort(all, total, sizeof *all, by_id);
        memset(out, 0, base);
        memcpy(out, "GTDB", 4);
        put32(out + 8, total);
        put32(out + 12, dw);
        for (uint32_t i = 0; i < total; i++) {
            put32(out + 16 + (size_t)i * 8, all[i].id);
            put32(out + 20 + (size_t)i * 8, i * dw);
            memcpy(out + base + (size_t)i * dw, all[i].row, dw);
        }
        free(all);
        if (out_size) *out_size = base + (size_t)total * dw;
        { char line[160];
          snprintf(line, sizeof line, "mod: GTDB (row width %u): %u row(s) added, %u overridden, %u identical copies ignored\n",
                   (unsigned)dw, added, overridden, ignored);
          mod_log_once(line); }
    }
done:
    free(disc); free(disc_data); free(mod); free(mod_data); free(keep);
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
    if (entry_size == 8) {                   /* GTID label -> id: an id the disc already lists keeps the disc's entry */
        uint32_t reused = 0;
        for (uint32_t i = 0; i < mc; i++)
            for (uint32_t j = 0; j < dc; j++)
                if (le32(m + 20 + (size_t)i * 8) == le32(d + 20 + (size_t)j * 8)) { reused++; break; }
        { char line[160];
          snprintf(line, sizeof line, "mod: GTID: %u label(s) added, %u id(s) already in the disc's list (the disc's entry answers first)\n",
                   mc - reused, reused);
          mod_log_once(line); }
    } else {                                   /* GTST string pool */
        char line[128];
        snprintf(line, sizeof line, "mod: GTST: %u string(s) added\n", mc);
        mod_log_once(line);
    }
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
