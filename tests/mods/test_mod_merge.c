/*
 * Lists merge: the disc's rows stay and a mod's rows are added
 * (docs/vulcan4/MODS-DESIGN.md). No disc, no game data -- every table here is
 * built by this test, so it can assert the merge's contract exactly:
 *
 *   - a GTDB gains rows and keeps every row the disc already had;
 *   - the merged table is readable by the engine's own reader (gt4_specdb_row);
 *   - ids stay in the reader's order (signed, ascending) so bisection works;
 *   - a GTID answer for the disc's labels and for the mod's new ones;
 *   - a GTST keeps its strings and gains the mod's;
 *   - a width mismatch is refused, and the reason is named.
 */
#include "mod_merge.h"
#include "specdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define EXPECT(cond, what) do {                                              \
        checks++;                                                            \
        if (cond) printf("ok   %s\n", what);                                 \
        else { printf("FAIL %s\n", what); fails++; }                         \
    } while (0)

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* A raw GTDB: "GTDB", flags 0, count, width, {id, rowoff}, rows. */
static size_t gtdb(uint8_t *out, const uint32_t *ids, const uint8_t *rows, unsigned count, uint32_t width)
{
    size_t base = 16 + (size_t)count * 8;
    memset(out, 0, base + (size_t)count * width);
    memcpy(out, "GTDB", 4);
    put32(out + 8, count);
    put32(out + 12, width);
    for (unsigned i = 0; i < count; i++) {
        put32(out + 16 + (size_t)i * 8, ids[i]);
        put32(out + 20 + (size_t)i * 8, i * width);
        memcpy(out + base + (size_t)i * width, rows + (size_t)i * width, width);
    }
    return base + (size_t)count * width;
}

/* A GTID: "GTID", count, {string offset, id}, then u16-length-prefixed labels. */
static size_t gtid(uint8_t *out, const char **labels, const uint32_t *ids, unsigned count)
{
    size_t pool = 0;
    for (unsigned i = 0; i < count; i++) pool += 2 + strlen(labels[i]) + 1;
    size_t base = 16 + (size_t)count * 8, at = 0;
    memset(out, 0, base + pool);
    memcpy(out, "GTID", 4);
    put32(out + 4, count);
    for (unsigned i = 0; i < count; i++) {
        put32(out + 16 + (size_t)i * 8, (uint32_t)at);
        put32(out + 20 + (size_t)i * 8, ids[i]);
        size_t len = strlen(labels[i]) + 1;
        put16(out + base + at, (unsigned)len);
        memcpy(out + base + at + 2, labels[i], len);
        at += 2 + len;
    }
    return base + pool;
}

int main(void)
{
    /* ---- GTDB: two disc rows, one mod row, and a row of a different width ---- */
    uint8_t disc_rows[3 * 4] = { 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3 };
    uint8_t mod_rows[1 * 4]  = { 9, 9, 9, 9 };
    uint8_t wide_rows[1 * 8] = { 7, 7, 7, 7, 7, 7, 7, 7 };
    uint32_t disc_ids[3] = { 21, 1200, 4000 }, mod_ids[1] = { 4100 }, wide_ids[1] = { 4100 };
    uint32_t upside_ids[2] = { 5000, 30 };               /* out of order on purpose */
    uint8_t upside_rows[2 * 4] = { 5, 5, 5, 5, 3, 3, 3, 3 };

    uint8_t disc[4096], mod[4096], wide[4096], unordered[4096];
    size_t dn = gtdb(disc, disc_ids, disc_rows, 3, 4);
    size_t mn = gtdb(mod, mod_ids, mod_rows, 1, 4);
    size_t wn = gtdb(wide, wide_ids, wide_rows, 1, 8);
    size_t un = gtdb(unordered, upside_ids, upside_rows, 2, 4);
    EXPECT(moon_mod_is_table(disc, dn) && moon_mod_is_table(mod, mn), "both inputs are recognised as tables");

    size_t merged_size = 0;
    const char *why = NULL;
    uint8_t *merged = moon_mod_merge_table(disc, dn, mod, mn, &merged_size, &why);
    EXPECT(merged != NULL, "a GTDB with the same width merges");
    if (merged) {
        uint8_t row[64];
        EXPECT(merged_size == 16 + 4 * 8 + 4 * 4, "the merged table is the disc's rows plus the mod's");
        EXPECT(gt4_specdb_row(merged, merged_size, 21, row, sizeof row) == 4 && row[0] == 1,
               "the disc's first row survives");
        EXPECT(gt4_specdb_row(merged, merged_size, 1200, row, sizeof row) == 4 && row[0] == 2,
               "the disc's middle row survives");
        EXPECT(gt4_specdb_row(merged, merged_size, 4000, row, sizeof row) == 4 && row[0] == 3,
               "the disc's last row survives");
        EXPECT(gt4_specdb_row(merged, merged_size, 4100, row, sizeof row) == 4 && row[0] == 9,
               "the mod's new row is in the merged table");
        EXPECT(gt4_specdb_row_index(merged, merged_size, 4100) == 3,
               "the new id sits in the reader's order (bisection finds it)");
        EXPECT(gt4_specdb_row(merged, merged_size, 9999, row, sizeof row) == 0,
               "an id nobody added still resolves to nothing");
        EXPECT(!moon_mod_merge_table(disc, dn, wide, wn, &merged_size, &why) && why && *why,
               "a different row width is refused, with a reason");
        free(merged);
    }
    /* A mod's rows arrive in any order; the merge puts them in the reader's. */
    merged = moon_mod_merge_table(disc, dn, unordered, un, &merged_size, &why);
    {   /* disc 21,1200,4000 + mod 5000,30 -> 21, 30, 1200, 4000, 5000 */
        uint8_t row[64];
        EXPECT(merged && gt4_specdb_row_index(merged, merged_size, 30) == 1,
               "a mod row that sorts into the middle lands there");
        EXPECT(merged && gt4_specdb_row_index(merged, merged_size, 5000) == 4,
               "and the reader's bisection finds the mod's highest id");
        EXPECT(merged && gt4_specdb_row(merged, merged_size, 5000, row, sizeof row) == 4 && row[0] == 5,
               "with the mod's own bytes in it");
    }
    free(merged);

    /* ---- GTID: the disc's labels still answer, and the mod's label appears ---- */
    const char *disc_labels[2] = { "disc_one", "disc_two" };
    const char *mod_labels[1] = { "moon_added" };
    uint32_t disc_label_ids[2] = { 100, 200 }, mod_label_ids[1] = { 300 };
    dn = gtid(disc, disc_labels, disc_label_ids, 2);
    mn = gtid(mod, mod_labels, mod_label_ids, 1);
    merged = moon_mod_merge_table(disc, dn, mod, mn, &merged_size, &why);
    EXPECT(merged != NULL, "a GTID merges");
    if (merged) {
        uint32_t id = 0;
        EXPECT(gt4_specdb_label(merged, merged_size, "disc_one", &id) && id == 100, "the disc's first label survives");
        EXPECT(gt4_specdb_label(merged, merged_size, "disc_two", &id) && id == 200, "the disc's second label survives");
        EXPECT(gt4_specdb_label(merged, merged_size, "moon_added", &id) && id == 300, "the mod's label answers");
        EXPECT(gt4_specdb_label(merged, merged_size, "not_here", &id) == 0, "an unknown label still answers nothing");
        const char *back = gt4_specdb_label_for_id(merged, merged_size, 300);
        EXPECT(back && !strcmp(back, "moon_added"), "and the id resolves back to the mod's label");
        free(merged);
    }

    /* ---- GTST: a string pool gains strings and keeps its own ---- */
    const char *pool_labels[2] = { "model_a", "model_b" };
    const char *pool_new[1] = { "moon_model" };
    uint8_t disc_pool[512], mod_pool[512];
    {
        size_t base = 16 + 2u * 4, pool = 0;
        for (int i = 0; i < 2; i++) { size_t len = strlen(pool_labels[i]) + 1; pool += 2 + len; }
        memset(disc_pool, 0, base + pool);
        memcpy(disc_pool, "GTST", 4);
        put32(disc_pool + 4, 2);
        size_t at = 0;
        for (int i = 0; i < 2; i++) {
            put32(disc_pool + 16 + (size_t)i * 4, (uint32_t)at);
            size_t len = strlen(pool_labels[i]) + 1;
            put16(disc_pool + base + at, (unsigned)len);
            memcpy(disc_pool + base + at + 2, pool_labels[i], len);
            at += 2 + len;
        }
        dn = base + pool;
        size_t base_m = 16 + 1u * 4, len = strlen(pool_new[0]) + 1;
        memset(mod_pool, 0, base_m + 2 + len);
        memcpy(mod_pool, "GTST", 4);
        put32(mod_pool + 4, 1);
        put32(mod_pool + 16, 0);
        put16(mod_pool + base_m, (unsigned)len);
        memcpy(mod_pool + base_m + 2, pool_new[0], len);
        mn = base_m + 2 + len;
    }
    merged = moon_mod_merge_table(disc_pool, dn, mod_pool, mn, &merged_size, &why);
    EXPECT(merged != NULL, "a GTST merges");
    if (merged) {
        EXPECT(!strcmp(gt4_specdb_string(merged, merged_size, 0), "model_a"), "the pool's first string survives");
        EXPECT(!strcmp(gt4_specdb_string(merged, merged_size, 1), "model_b"), "the pool's second string survives");
        EXPECT(!strcmp(gt4_specdb_string(merged, merged_size, 2), "moon_model"), "the mod's string is appended");
        EXPECT(gt4_specdb_string(merged, merged_size, 3) == NULL, "past the end there is still nothing");
        free(merged);
    }

    printf("%s: %d checks, %d failure(s)\n", fails ? "MOD MERGE FAILED" : "mod merge", checks, fails);
    return fails ? 1 : 0;
}
