/*
 * The mod source: a folder shadows the disc (docs/vulcan4/MODS-DESIGN.md, W278).
 *
 * Every byte this test reads is authored here -- no game data, ever (LAW 1).
 * No disc is needed: the lookup runs before the volume is touched, which is
 * itself one of the things being checked (gt4_vol_read(NULL, ...) must still
 * answer from the mod source).
 *
 * The contract under test:
 *   - MOON_OVERLAY unset, empty, or with no such file -> NULL, disc answers;
 *   - a file in a mod folder is served byte-for-byte, with an exact *size;
 *   - the FIRST folder listed wins, a missing folder is skipped;
 *   - "..", a bare "/" and a path that only walks up are refused;
 *   - a zero-byte file is not a shadow (the disc's copy stands).
 */
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

static int fails;
static int checks;

#define EXPECT(cond, what) do {                                              \
        checks++;                                                            \
        if (cond) printf("ok   %s\n", what);                                 \
        else { printf("FAIL %s\n", what); fails++; }                         \
    } while (0)

static int write_file(const char *path, const void *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t wrote = fwrite(data, 1, n, f);
    fclose(f);
    return wrote == n;
}

/* mkdir -p for the one level of nesting this test uses ("car/menu"). */
static void make_dir(const char *path, const char *leaf)
{
    char full[1200];
    snprintf(full, sizeof full, "%s/%s", path, leaf);
    MKDIR(full);
}

/* "car/menu" under a mod folder: two mkdirs, because mkdir is one level deep. */
static void make_menu_dir(const char *mod)
{
    make_dir(mod, "car");
    make_dir(mod, "car/menu");
}

/* Serves `path` from the mod source and compares the bytes with what we wrote. */
static int served(const char *path, const char *expect, size_t expect_len, size_t *got_len)
{
    size_t n = 0;
    uint8_t *b = gt4_overlay_read(path, &n);
    if (!b) { if (got_len) *got_len = 0; return 0; }
    int same = n == expect_len && !memcmp(b, expect, n);
    if (got_len) *got_len = n;
    free(b);
    return same;
}

int main(int argc, char **argv)
{
    const char *base = argc > 1 ? argv[1] : (getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
    char a[1200], b[1200], env[2600];
    static const char alpha[] = "AAAA-mod-alpha";        /* ours, not the disc's */
    static const char beta[]  = "BBBB-mod-beta";

    snprintf(a, sizeof a, "%s/mod_a", base);
    snprintf(b, sizeof b, "%s/mod_b", base);
    MKDIR(base); MKDIR(a); MKDIR(b);
    make_menu_dir(a); make_menu_dir(b);
    char af[1200], bf[1200], empty[1200];
    snprintf(af, sizeof af, "%s/car/menu/zzz_postcar.mdl", a);
    snprintf(bf, sizeof bf, "%s/car/menu/zzz_postcar.mdl", b);
    snprintf(empty, sizeof empty, "%s/car/menu/zzz_empty.mdl", a);
    if (!write_file(af, alpha, sizeof alpha - 1) || !write_file(bf, beta, sizeof beta - 1) ||
        !write_file(empty, "", 0)) {
        printf("FAIL cannot author the overlay assets under %s\n", base);
        return 1;
    }

    /* off by default: the disc is the only source (LAW 12). */
#ifdef _WIN32
    _putenv_s("MOON_OVERLAY", "");
#else
    unsetenv("MOON_OVERLAY");
#endif
    EXPECT(gt4_overlay_read("/car/menu/zzz_postcar.mdl", NULL) == NULL,
           "no overlay set -> the mod source answers nothing");
    EXPECT(gt4_vol_read(NULL, "/car/menu/zzz_postcar.mdl", NULL) == NULL,
           "no overlay set -> a mod-only lookup answers nothing");

    /* the first folder listed wins, so mod order is a list order. */
    snprintf(env, sizeof env, "%s;%s", b, a);
#ifdef _WIN32
    _putenv_s("MOON_OVERLAY", env);
#else
    setenv("MOON_OVERLAY", env, 1);
#endif
    size_t n = 0;
    EXPECT(served("/car/menu/zzz_postcar.mdl", beta, sizeof beta - 1, &n),
           "the first folder listed is the one served");
    EXPECT(n == sizeof beta - 1, "the served size is exact");

    /* a folder that is not there is skipped, not fatal. */
    snprintf(env, sizeof env, "%s/mod_missing;%s", base, a);
#ifdef _WIN32
    _putenv_s("MOON_OVERLAY", env);
#else
    setenv("MOON_OVERLAY", env, 1);
#endif
    EXPECT(served("/car/menu/zzz_postcar.mdl", alpha, sizeof alpha - 1, NULL),
           "a missing folder is skipped and the next one answers");

    /* the mod source is independent of the volume. */
    {
        size_t got = 0;
        uint8_t *bytes = gt4_vol_read(NULL, "/car/menu/zzz_postcar.mdl", &got);
        EXPECT(bytes && got == sizeof alpha - 1 && !memcmp(bytes, alpha, got),
               "gt4_vol_read with no volume still serves the mod's file");
        free(bytes);
    }

    /* an empty shadow is not a shadow: the disc's copy must stand. */
    EXPECT(gt4_overlay_read("/car/menu/zzz_empty.mdl", NULL) == NULL,
           "a zero-byte file does not shadow the disc");

    /* hostile paths must fall through to the disc without reading the disk. */
    EXPECT(gt4_overlay_read("/../etc/passwd", NULL) == NULL, "\"..\" is refused");
    EXPECT(gt4_overlay_read("/car/../../etc/passwd", NULL) == NULL, "a traversal in the middle is refused");
    EXPECT(gt4_overlay_read("..", NULL) == NULL, "a bare \"..\" is refused");
    EXPECT(gt4_overlay_read("/", NULL) == NULL, "the root alone is not a file");
    EXPECT(gt4_overlay_read("", NULL) == NULL, "an empty path is refused");

    /* an empty variable and an unset one behave the same. */
#ifdef _WIN32
    _putenv_s("MOON_OVERLAY", "");
#else
    setenv("MOON_OVERLAY", "", 1);
#endif
    EXPECT(gt4_overlay_read("/car/menu/zzz_postcar.mdl", NULL) == NULL,
           "an empty variable behaves like no variable");

    remove(af); remove(bf); remove(empty);
    printf("%s: %d checks, %d failure(s)\n", fails ? "MOD SOURCE FAILED" : "mod source", checks, fails);
    return fails ? 1 : 0;
}
