#include "mod_manifest.h"

#include "platform/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MANIFEST     "mod.txt"
#define MANIFEST_MAX (64u << 10)      /* a manifest is a page of text, not data */
#define LABEL_MAX    96

typedef struct {
    const char **labels;
    const char **makers;
    size_t cap, count;
    const char *kind;                 /* "car" or "course" */
} sink;

/* The next whitespace-delimited word of `p`, terminated in place. No strtok:
   the line is scanned inside a loop that is already scanning lines. */
static char *word(char **p)
{
    char *s = *p;
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    if (!*s) { *p = s; return NULL; }
    char *start = s;
    while (*s && *s != ' ' && *s != '\t' && *s != '\r') s++;
    if (*s) { *s = 0; s++; }
    *p = s;
    return start;
}

/* "car <label> [maker=<name>] ..." -- the first word is the kind, the second the
   label, the rest key=value. An unknown kind is ignored, not an error: a
   manifest is written by our own tool and read by a user who may add notes. */
static void parse_line(char *line, sink *s)
{
    char *hash = strchr(line, '#');
    if (hash) *hash = 0;
    char *p = line;
    char *kind = word(&p), *label = word(&p);
    if (!kind || !label || strcmp(kind, s->kind) || s->count >= s->cap) return;
    if (!*label || strlen(label) >= LABEL_MAX) return;

    char *copy = strdup(label);
    if (!copy) return;
    s->labels[s->count] = copy;
    if (s->makers) s->makers[s->count] = "";
    for (char *kv = word(&p); kv; kv = word(&p)) {
        if (s->makers && !strncmp(kv, "maker=", 6) && kv[6] && strlen(kv + 6) < LABEL_MAX) {
            char *value = strdup(kv + 6);
            if (value) s->makers[s->count] = value;
        }
    }
    s->count++;
}

static void read_manifest(const char *file, sink *s)
{
    plat_file *f = plat_file_open(file);
    if (!f) return;
    uint64_t size = plat_file_size(f);
    char *text = size && size <= MANIFEST_MAX ? malloc((size_t)size + 1) : NULL;
    if (text && plat_file_read_at(f, 0, text, (size_t)size) == (size_t)size) {
        text[size] = 0;
        for (char *line = text; line && *line;) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = 0;
            if (s->count < s->cap) parse_line(line, s);
            line = nl ? nl + 1 : NULL;
        }
    }
    free(text);
    plat_file_close(f);
}

static size_t collect(sink *s)
{
    const char *roots = moon_mod_roots();
    if (!*roots) return 0;
    for (const char *r = roots; *r;) {
        const char *end = strchr(r, ';');
        size_t len = end ? (size_t)(end - r) : strlen(r);
        char file[1400];
        if (len && len + 1 + sizeof MANIFEST <= sizeof file) {
            memcpy(file, r, len);
            file[len] = '/';
            memcpy(file + len + 1, MANIFEST, sizeof MANIFEST);
            read_manifest(file, s);
        }
        r = end ? end + 1 : r + len;
    }
    for (size_t i = 0; i < s->count; i++)                 /* one label, once */
        for (size_t j = i + 1; j < s->count; j++)
            if (!strcmp(s->labels[j], s->labels[i])) {
                for (size_t k = j + 1; k < s->count; k++) {
                    s->labels[k - 1] = s->labels[k];
                    if (s->makers) s->makers[k - 1] = s->makers[k];
                }
                s->count--;
                j--;
            }
    return s->count;
}

size_t moon_mod_added_cars(const char **labels, const char **makers, size_t cap)
{
    sink s = { labels, makers, cap, 0, "car" };
    if (!labels || !cap) return 0;
    return collect(&s);
}

size_t moon_mod_added_courses(const char **labels, size_t cap)
{
    sink s = { labels, NULL, cap, 0, "course" };
    if (!labels || !cap) return 0;
    return collect(&s);
}

/* ---- one value out of a manifest, for the manager's card ------------------ */
int moon_manifest_field(const char *mod_dir, const char *key, char *out, size_t cap)
{
    if (!mod_dir || !key || !out || !cap) return 0;
    out[0] = 0;
    char file[1500];
    snprintf(file, sizeof file, "%s/mod.txt", mod_dir);
    plat_file *f = plat_file_open(file);
    if (!f) return 0;
    uint64_t size = plat_file_size(f);
    char *text = size && size <= MANIFEST_MAX ? malloc((size_t)size + 1) : NULL;
    int found = 0;
    if (text && plat_file_read_at(f, 0, text, (size_t)size) == (size_t)size) {
        text[size] = 0;
        size_t key_len = strlen(key);
        for (char *line = text; line && *line && !found;) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = 0;
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (!strncmp(p, key, key_len) && (p[key_len] == ' ' || p[key_len] == '\t')) {
                p += key_len;
                while (*p == ' ' || *p == '\t') p++;
                size_t n = strlen(p);
                while (n && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r')) p[--n] = 0;
                if (p[0] != '#') { snprintf(out, cap, "%s", p); found = 1; }
            }
            line = nl ? nl + 1 : NULL;
        }
    }
    free(text);
    plat_file_close(f);
    return found;
}

/* ---- which mod folders are in force ---------------------------------------
   One place, so the byte lookups (vol.c) and the lists (here, and menu.c) can
   never disagree about what is loaded. */
static char mod_roots[4096];
static int  mod_roots_ready;

int moon_mods_dir(char *out, size_t cap)
{
    char pref[1200];
    if (!out || !cap || !plat_pref_dir(pref, sizeof pref)) return 0;
    snprintf(out, cap, "%smods", pref);          /* plat_pref_dir ends with a separator */
    return 1;
}

const char *moon_mod_roots(void)
{
    if (mod_roots_ready) return mod_roots;
    mod_roots_ready = 1;
    mod_roots[0] = 0;

    const char *env = getenv("MOON_OVERLAY");   /* what the launcher hands over */
    if (env && *env) {
        snprintf(mod_roots, sizeof mod_roots, "%s", env);
        return mod_roots;
    }

    /* No hand-over: the manager's tick list decides. A name here is a folder
       name only -- a path with a separator or a ".." is not a mod. */
    char file[1400], base[1400], line[512];
    if (!moon_mods_dir(base, sizeof base)) return mod_roots;
    snprintf(file, sizeof file, "%s.ini", base);   /* <prefs>/mods.ini */
    plat_file *f = plat_file_open(file);
    if (!f) return mod_roots;
    uint64_t size = plat_file_size(f);
    char *text = size && size <= (64u << 10) ? malloc((size_t)size + 1) : NULL;
    if (text && plat_file_read_at(f, 0, text, (size_t)size) == (size_t)size) {
        text[size] = 0;
        for (char *p = text; p && *p;) {
            char *nl = strchr(p, '\n');
            if (nl) *nl = 0;
            snprintf(line, sizeof line, "%s", p);
            char *hash = strchr(line, '#');
            if (hash) *hash = 0;
            size_t len = strlen(line);                      /* trim the newline and blanks */
            while (len && (line[len - 1] == ' ' || line[len - 1] == '\t' || line[len - 1] == '\r')) line[--len] = 0;
            char *name = line;
            while (*name == ' ' || *name == '\t') name++;
            if (*name && !strchr(name, '/') && !strchr(name, '\\') && !strstr(name, "..")) {
                if (mod_roots[0]) strncat(mod_roots, ";", sizeof mod_roots - strlen(mod_roots) - 1);
                strncat(mod_roots, base, sizeof mod_roots - strlen(mod_roots) - 1);
                strncat(mod_roots, "/", sizeof mod_roots - strlen(mod_roots) - 1);
                strncat(mod_roots, name, sizeof mod_roots - strlen(mod_roots) - 1);
            }
            p = nl ? nl + 1 : NULL;
        }
    }
    free(text);
    plat_file_close(f);
    if (mod_roots[0] && getenv("MOON_OVERLAY_TRACE"))
        fprintf(stderr, "mods: %s\n", mod_roots);
    return mod_roots;
}
