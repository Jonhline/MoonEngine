/*
 * moonmod_catalog: prints the disc's own nation -> maker catalogue, so the SDK
 * can offer the same brands the game's menus do, without shipping a brand list
 * of its own.
 *
 *   moonmod_catalog <disc.iso|GT4.VOL> [nation]
 *
 * The new-car dealers and the "browse by maker" strips are built from the
 * authored MakerData in the disc's /projects/arcade/arcade.adc: one
 * `<nation>_makers` array per nation, ordered by the region's own
 * `<region>_nation_list` (us_nation_list on the US disc). This tool reads
 * exactly those arrays and prints them:

 *   nation <key>
 *     maker <name>
 *
 * which is what the engine's menus read at run time. A second argument limits
 * the listing to one nation. Nothing here is authored in the tool; the names
 * come from the user's own disc.
 *
 * Note: the `/projects/gtmode/US_carlist.adc` next to it holds the per-maker
 * CAR arrays (`<maker>_carlist`), not the nation -> maker mapping; the mapping
 * lives in the arcade project's MakerData, which is what this tool reads.
 */
#include "adhoc/adhoc_code.h"
#include "vol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIONS_MAX 32
#define MAKERS_MAX 64
#define NAME_MAX 64

struct nation {
    char key[NAME_MAX];
    char makers[MAKERS_MAX][NAME_MAX];
    int nmakers;
};

static int ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && !strcmp(s + n - m, suffix);
}

/* The string elements that follow a STATIC_DEFINE in this frame: skip the
   VARIABLE_PUSH that names the slot, take every STRING_CONST, stop at the
   ARRAY_CONST_OLD (or the next definition) that closes the array. */
static int take_strings(const ah_code *f, int from, char out[][NAME_MAX], int cap)
{
    int count = 0;
    for (int k = from; k < f->count; k++) {
        const ah_ins *z = &f->ins[k];
        if (z->op == AH_STRING_CONST && z->s) {
            if (count < cap) snprintf(out[count], NAME_MAX, "%s", z->s);
            count++;
            continue;
        }
        if (count)
            break;
        if (z->op == AH_STATIC_DEFINE || z->op == AH_MODULE_DEFINE)
            break;
    }
    return count > cap ? cap : count;
}

/* The `<nation>_makers` arrays live in the MakerData module, which (like every
   module here) expands linearly into the enclosing frame, so walk the frame and
   switch on each MODULE_DEFINE the way the VM does. */
static void collect_makers(const ah_code *c, struct nation *out, int *n)
{
    if (!c) return;
    int in_maker = 0;
    for (int i = 0; i < c->count; i++) {
        const ah_ins *x = &c->ins[i];
        if (x->op == AH_MODULE_DEFINE) {
            in_maker = x->npath > 0 && x->path && !strcmp(x->path[0], "MakerData");
            continue;
        }
        if (!in_maker || x->op != AH_STATIC_DEFINE || !x->s || !ends_with(x->s, "_makers"))
            continue;
        if (*n >= NATIONS_MAX)
            return;
        struct nation *cur = &out[(*n)++];
        size_t klen = strlen(x->s) - strlen("_makers");
        if (klen >= NAME_MAX) klen = NAME_MAX - 1;
        memcpy(cur->key, x->s, klen);
        cur->key[klen] = 0;
        cur->nmakers = take_strings(c, i + 1, cur->makers, MAKERS_MAX);
    }
    for (int i = 0; i < c->count; i++)
        collect_makers(c->ins[i].frame, out, n);
}

/* The region's own nation ordering. The US disc is what the SDK ships against;
   its build activates us_nation_list, so prefer that, then any other region's. */
static int collect_named_list(const ah_code *c, const char *name, char out[][NAME_MAX], int *n)
{
    if (!c) return 0;
    for (int i = 0; i < c->count; i++) {
        const ah_ins *x = &c->ins[i];
        if (x->op == AH_STATIC_DEFINE && x->s && !strcmp(x->s, name)) {
            *n = take_strings(c, i + 1, out, NATIONS_MAX);
            return 1;
        }
    }
    for (int i = 0; i < c->count; i++)
        if (collect_named_list(c->ins[i].frame, name, out, n))
            return 1;
    return 0;
}

static const struct nation *find_nation(struct nation *ns, int n, const char *key)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(ns[i].key, key))
            return &ns[i];
    return NULL;
}

static void print_nation(const struct nation *nat)
{
    printf("nation %s\n", nat->key);
    for (int i = 0; i < nat->nmakers; i++)
        printf("  maker %s\n", nat->makers[i]);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <disc.iso|GT4.VOL> [nation]\n", argv[0]);
        return 2;
    }
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(argv[1], &err);
    if (!v) { fprintf(stderr, "cannot open volume: %s\n", err); return 1; }

    size_t size = 0;
    uint8_t *data = gt4_vol_read(v, "/projects/arcade/arcade.adc", &size);
    if (!data) {
        fprintf(stderr, "no /projects/arcade/arcade.adc in this volume\n");
        gt4_vol_close(v);
        return 1;
    }
    char msg[256] = "";
    ah_code *c = ah_load(data, size, msg, sizeof msg);
    free(data);
    if (!c) { fprintf(stderr, "cannot load arcade.adc: %s\n", msg); gt4_vol_close(v); return 1; }

    static struct nation ns[NATIONS_MAX];
    int n = 0;
    collect_makers(c, ns, &n);

    static char order[NATIONS_MAX][NAME_MAX];
    int nord = 0;
    for (const char *const *r = (const char *const[]){ "us_nation_list", "jp_nation_list",
                                                        "uk_nation_list", "kr_nation_list", NULL };
         r && *r && !nord; r++)
        collect_named_list(c, *r, order, &nord);
    ah_free(c);
    gt4_vol_close(v);

    if (!n) { fprintf(stderr, "the disc declares no nation maker lists\n"); return 1; }

    const char *only = argc > 2 ? argv[2] : NULL;

    /* The disc's own order first; any nation the region list does not name is
       appended so nothing is silently dropped. */
    for (int i = 0; i < nord; i++) {
        const struct nation *nat = find_nation(ns, n, order[i]);
        if (!nat) continue;
        if (only && strcmp(only, nat->key)) continue;
        print_nation(nat);
    }
    for (int i = 0; i < n; i++) {
        int listed = 0;
        for (int j = 0; j < nord; j++)
            if (!strcmp(order[j], ns[i].key)) { listed = 1; break; }
        if (listed) continue;
        if (only && strcmp(only, ns[i].key)) continue;
        print_nation(&ns[i]);
    }

    if (only && !find_nation(ns, n, only)) {
        fprintf(stderr, "no nation '%s' in the disc's catalogue\n", only);
        return 1;
    }
    return 0;
}
