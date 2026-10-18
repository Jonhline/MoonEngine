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
#include "adhoc/adhoc_vm.h"

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

/* GTST: a string pool. count at +4, one u32 offset per string at +16, then the
   pool of u16-length-prefixed NUL-terminated strings. The mod's entries are
   merged AFTER the disc's, so their indices start at the disc's own count. */
static size_t gtst_multi(const char **strs, int count, uint8_t *out, size_t cap)
{
    size_t base = 16 + (size_t)count * 4, pool = 0;
    for (int i = 0; i < count; i++) { size_t l = strlen(strs[i]) + 1; if (l > 0xffff) return 0; pool += 2 + l; }
    if (!count || base + pool > cap) return 0;
    memset(out, 0, base + pool);
    memcpy(out, "GTST", 4);
    put32(out + 4, (uint32_t)count);
    size_t at = 0;
    for (int i = 0; i < count; i++) {
        size_t l = strlen(strs[i]) + 1;
        put32(out + 16 + (size_t)i * 4, (uint32_t)at);
        put16(out + base + at, (unsigned)l);
        memcpy(out + base + at + 2, strs[i], l);
        at += 2 + l;
    }
    return base + pool;
}

/* One past the highest id a GTDB table already uses (never a hole). */
static uint32_t highest_row_id(const uint8_t *b, size_t n)
{
    if (!b || n < 16 || memcmp(b, "GTDB", 4)) return 0;
    uint32_t c = le32(b + 8), high = 0;
    if (c > (n - 16) / 8) return 0;
    for (uint32_t i = 0; i < c; i++) { uint32_t v = le32(b + 16 + (size_t)i * 8); if (v > high) high = v; }
    return high;
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
        fprintf(stderr, "usage: %s <disc.iso> <source car label> <new label> <out dir> [maker] [price] [--model-name NAME]\n", argv[0]);
        fprintf(stderr, "  --model-name NAME gives the car its OWN model name (a new string + VARIATION row),\n");
        fprintf(stderr, "  so the mesh lives at /car/{menu,lod,open}/NAME and no disc car is shadowed.\n");
        fprintf(stderr, "  e.g. %s disc.iso ka_01 moon_banana /mnt/ssd/mods/moon-banana ford 500 --model-name moonbanana\n", argv[0]);
        return 2;
    }
    const char *maker = (argc > 5 && argv[5][0] != '-') ? argv[5] : NULL;
    const char *price_arg = (argc > 6 && argv[6][0] != '-') ? argv[6] : NULL;
    const char *model_name = NULL, *display_name = NULL, *power_arg = NULL;
    for (int i = 5; i < argc; i++) {
        if (!strcmp(argv[i], "--model-name") && i + 1 < argc) model_name = argv[++i];
        else if (!strcmp(argv[i], "--name") && i + 1 < argc) display_name = argv[++i];
        else if (!strcmp(argv[i], "--power") && i + 1 < argc) power_arg = argv[++i];
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
    if (price_arg) {
        char *end = NULL;
        double v = strtod(price_arg, &end);
        if (!*price_arg || *end || v < 1 || v > 1000000000.0) {
            fprintf(stderr, "price must be a whole number of credits, 1..1000000000\n");
            return 2;
        }
        price = (long)(v + 0.5);
        int32_t p = (int32_t)price; memcpy(gen_row + 8, &p, 4);
    } else memcpy(&price, gen_row + 8, 4);       /* the source car's own price (an int32) */

    /* The maker the manifest files it under is also the car's own: GENERIC_CAR +14 the
       country, +15/+16 the maker (Ka/Focus 03 0d 0d, M5/M3 05 06 06 -- the flag and the
       logo the menus draw). Copied from the first car the disc's own dealership lists
       for that maker (/projects/gtmode/US_carlist.adc NewCarData.<maker>_carlist), so a
       Ford built on a BMW donor no longer shows the BMW badge and the German flag. */
    if (maker) {
        size_t cs = 0;
        uint8_t *cat = gt4_vol_read(v, "/projects/gtmode/US_carlist.adc", &cs);
        char e[256];
        ah_code *code = cat ? ah_load(cat, cs, e, sizeof e) : NULL;
        free(cat);
        ah_vm *vm = code ? ah_vm_new() : NULL;
        const char *first = NULL;
        if (vm && ah_run_script(vm, code) >= 0) {
            static const char *const stores[] = {"NewCarData", "CompleteCarData", "CarData"};
            char key[160];
            snprintf(key, sizeof key, "%s_carlist", maker);
            for (size_t k = 0; k < 3 && !first; k++) {
                ah_val store, list;
                if (ah_get_member(vm, ah_root(vm), stores[k], &store) && store.t == AV_OBJ &&
                    ah_get_member(vm, store.u.o, key, &list) && list.t == AV_OBJ &&
                    ah_array_size(list.u.o) > 0)
                    first = ah_cstr(ah_array_get(list.u.o, 0));
            }
        }
        uint32_t ref_id;
        uint8_t ref_row[256];
        if (first && gt4_specdb_label(idi, idi_size, first, &ref_id) &&
            gt4_specdb_row(generic, gen_size, ref_id, ref_row, sizeof ref_row) == 24) {
            memcpy(gen_row + 14, ref_row + 14, 3);
            printf("maker \"%s\": country %u, maker %u (from %s)\n", maker, ref_row[14], ref_row[15], first);
        } else
            fprintf(stderr, "maker \"%s\": not in the disc's dealership lists; the donor's badge stays\n", maker);
        if (vm) ah_vm_free(vm);
    }

    char sub[1024];
    snprintf(sub, sizeof sub, "%s/specdb/GT4_US2560", argv[4]);
    mkdir_p(sub);

    uint8_t buf[4096];
    char file[1200];
    size_t n;
    int ok = 1;

    /* Options that add rows to the mod (all additive; the source car is never
       touched). --model-name gives the car its OWN model name; --name is the
       display name the lists show; --power sets the catalog power. All strings
       this mod adds go into one american_StrDB.sdb pool. */
    const char *mod_strings[8];
    int mod_string_count = 0;
    uint32_t str_base = 0;
    int str_base_ready = 0;

    /* --model-name: a car's model name resolves label -> id -> CAR_VARIATION row
       (variation id) -> VARIATIONamerican row (+0 = string index) ->
       american_StrDB.sdb. One new string, one new VARIATION row, this car's row
       re-pointed. */
    uint32_t new_variation = 0;
    if (model_name) {
        if (!*model_name || strlen(model_name) > 24 || strchr(model_name, '/') || strchr(model_name, '\\')) {
            fprintf(stderr, "model name must be 1..24 characters, without / or \\\n");
            return 2;
        }
        size_t var_size = 0, str_size = 0;
        uint8_t *variants = read_table(v, "VARIATIONamerican.dbt", &var_size);
        uint8_t *strdb = read_table(v, "american_StrDB.sdb", &str_size);
        if (!variants || !strdb) { fprintf(stderr, "the disc's variation/string tables are unreadable\n"); return 1; }
        uint8_t var_row[256];
        uint32_t source_variation = le32(link_row);
        if (gt4_specdb_row(variants, var_size, source_variation, var_row, sizeof var_row) != 52) {
            fprintf(stderr, "the source car's VARIATION row (id %u) does not read back\n", source_variation);
            return 1;
        }
        const char *old_model = gt4_specdb_string(strdb, str_size, le32(var_row));
        str_base = le32(strdb + 4); str_base_ready = 1;
        new_variation = highest_row_id(variants, var_size) + 1;
        uint32_t idx = str_base + (uint32_t)mod_string_count;
        put32(var_row + 0, idx);                           /* the new VARIATION -> our string */
        mod_strings[mod_string_count++] = model_name;
        put32(link_row + 0, new_variation);                /* our car -> the new VARIATION */
        printf("model name \"%s\": string index %u, variation %u (source variation %u -> \"%s\")\n",
               model_name, idx, new_variation, source_variation, old_model ? old_model : "?");
        snprintf(file, sizeof file, "%s/VARIATIONamerican.dbt", sub);
        ok = ok && (n = gtdb_raw(var_row, 52, new_variation, buf, sizeof buf)) && write_file(file, buf, n);
        printf("%s  variation %u -> string %u\n", file, new_variation, le32(var_row));
        free(variants); free(strdb);
    }

    /* --name NAME: the display name the lists show. CAR_NAME_american.dbt (width
       16) is keyed by the car id; +0 = name string, +8 = short-name string,
       +12 = description id (0 = none). */
    if (display_name) {
        if (!str_base_ready) {
            size_t str_size = 0; uint8_t *strdb = read_table(v, "american_StrDB.sdb", &str_size);
            if (!strdb) { fprintf(stderr, "the disc's string table is unreadable\n"); return 1; }
            str_base = le32(strdb + 4); str_base_ready = 1; free(strdb);
        }
        if (!*display_name || strlen(display_name) > 63) { fprintf(stderr, "name must be 1..63 characters\n"); return 2; }
        uint8_t name_row[64]; memset(name_row, 0, sizeof name_row);
        uint32_t idx = str_base + (uint32_t)mod_string_count;
        put32(name_row + 0, idx);                          /* name */
        put32(name_row + 8, idx);                          /* short name */
        put32(name_row + 12, 0);                           /* description id */
        mod_strings[mod_string_count++] = display_name;
        snprintf(file, sizeof file, "%s/CAR_NAME_american.dbt", sub);
        ok = ok && (n = gtdb_raw(name_row, 16, new_id, buf, sizeof buf)) && write_file(file, buf, n);
        printf("%s  id %u -> name string %u = \"%s\"\n", file, new_id, idx, display_name);
    }

    /* The mod's string pool, once, with every string it adds (indices start at
       the disc's own count; the merge appends after the disc's pool). */
    if (mod_string_count) {
        snprintf(file, sizeof file, "%s/american_StrDB.sdb", sub);
        ok = ok && (n = gtst_multi(mod_strings, mod_string_count, buf, sizeof buf)) && write_file(file, buf, n);
        printf("%s  %d string(s), first index %u\n", file, mod_string_count, str_base);
    }

    /* --power PS: the dealer's STOCK power is a stored u16 at ENGINE row +0x1a
       (menu_quick.inc nat_quick_catalog: e[26]|e[27]<<8); the menus' CURRENT power
       is DERIVED from the ENGINE torque curve (career_vehicle.c peak_stats). Both
       are set -- the stored figure and the curve scaled by the same ratio, so the
       stock and tuned numbers agree. A new ENGINE row and a new DEFAULT_PARTS row
       keep the source car untouched. */
    if (power_arg) {
        char *end = NULL; double psd = strtod(power_arg, &end);
        if (!*power_arg || *end || psd < 1 || psd > 20000) { fprintf(stderr, "power must be 1..20000 PS\n"); return 2; }
        unsigned ps = (unsigned)(psd + 0.5);
        size_t dp_size = 0, en_size = 0;
        uint8_t *dparts = read_table(v, "DEFAULT_PARTS.dbt", &dp_size);
        uint8_t *engines = read_table(v, "ENGINE.dbt", &en_size);
        if (!dparts || !engines) { fprintf(stderr, "the disc's engine/parts tables are unreadable\n"); return 1; }
        uint8_t parts_row[256], eng_row[256];
        uint32_t parts_id = le32(gen_row + 0);
        if (gt4_specdb_row(dparts, dp_size, parts_id, parts_row, sizeof parts_row) != 208) {
            fprintf(stderr, "the car's DEFAULT_PARTS row (%u) does not read back\n", parts_id); return 1;
        }
        uint32_t eng_id = le32(parts_row + 0x58);
        if (gt4_specdb_row(engines, en_size, eng_id, eng_row, sizeof eng_row) != 113) {
            fprintf(stderr, "the car's ENGINE row (%u) does not read back\n", eng_id); return 1;
        }
        unsigned old_ps = eng_row[0x1a] | (unsigned)eng_row[0x1b] << 8;
        if (!old_ps) old_ps = 1;
        double k = (double)ps / (double)old_ps;
        uint32_t new_engine = highest_row_id(engines, en_size) + 1;
        uint32_t new_parts = highest_row_id(dparts, dp_size) + 1;
        eng_row[0x1a] = (uint8_t)ps; eng_row[0x1b] = (uint8_t)(ps >> 8);
        unsigned old_tq = eng_row[0x1c] | (unsigned)eng_row[0x1d] << 8;
        unsigned new_tq = (unsigned)(old_tq * k + 0.5); if (new_tq > 65535) new_tq = 65535;
        eng_row[0x1c] = (uint8_t)new_tq; eng_row[0x1d] = (uint8_t)(new_tq >> 8);
        unsigned points = eng_row[0x54]; if (points > 24) points = 24;
        for (unsigned i = 0; i < points; i++) {
            unsigned t = eng_row[0x1e + i * 2] | (unsigned)eng_row[0x1f + i * 2] << 8;
            unsigned v = (unsigned)(t * k + 0.5); if (v > 65535) v = 65535;
            eng_row[0x1e + i * 2] = (uint8_t)v; eng_row[0x1f + i * 2] = (uint8_t)(v >> 8);
        }
        put32(parts_row + 0x58, new_engine);               /* DEFAULT_PARTS+0x58 = {id, ENGINE table} */
        put32(parts_row + 0x5c, 0x0c);
        put32(gen_row + 0, new_parts);                     /* this car -> the new parts row */
        printf("power %u PS: engine %u -> %u (stored +0x1a %u -> %u, curve x %.3f), parts %u -> %u\n",
               ps, eng_id, new_engine, old_ps, ps, k, parts_id, new_parts);
        snprintf(file, sizeof file, "%s/ENGINE.dbt", sub);
        ok = ok && (n = gtdb_raw(eng_row, 113, new_engine, buf, sizeof buf)) && write_file(file, buf, n);
        snprintf(file, sizeof file, "%s/DEFAULT_PARTS.dbt", sub);
        ok = ok && (n = gtdb_raw(parts_row, 208, new_parts, buf, sizeof buf)) && write_file(file, buf, n);
        printf("%s  parts %u -> engine %u\n", file, new_parts, new_engine);
        free(dparts); free(engines);
    }

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
    if (model_name)
        printf("new car:    \"%s\" (id %u), model = \"%s\" (its own; place the mesh at /car/{menu,lod,open}/%s).\n",
               argv[3], new_id, model_name, model_name);
    else
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
        fprintf(manifest, "car %s%s%s\n", argv[3], maker ? " maker=" : "", maker ? maker : "");
        if (model_name) fprintf(manifest, "model %s\n", model_name);
        fclose(manifest);
        printf("%s  the manifest%s\n", file, maker ? "" : " (no maker: it lists in Arcade, not under a brand)");
    }
    free(idi); free(links); free(generic);
    gt4_vol_close(v);
    return ok ? 0 : 1;
}
