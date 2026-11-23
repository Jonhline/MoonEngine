/* The GitHub release reader of the stable-build updater (src/app/app_update.c).
   --negative: a wrong expectation, so the harness proves this test can fail. */
#include "app_update.h"
#include <stdio.h>
#include <string.h>

#define DIGEST "0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789abcdef"
static const char *good =
    "{\"url\":\"x\",\"tag_name\":\"v0.3.0\",\"draft\":false,\"prerelease\":false,"
    "\"author\":{\"login\":\"a\",\"name\":\"MoonEngine-0.9.9-win64.zip\"},"
    "\"assets\":[{\"name\":\"MoonEngine-0.3.0-source.zip\",\"size\":5,\"digest\":null,"
    "\"browser_download_url\":\"https://github.com/Jonhline/MoonEngine/releases/download/v0.3.0/s.zip\"},"
    "{\"url\":\"u\",\"id\":7,\"name\":\"MoonEngine-0.3.0-win64.zip\",\"label\":null,"
    "\"uploader\":{\"login\":\"b\",\"size\":1},\"content_type\":\"application/zip\","
    "\"size\":123456,\"digest\":\"sha256:" DIGEST "\",\"download_count\":0,"
    "\"browser_download_url\":\"https://github.com/Jonhline/MoonEngine/releases/download/v0.3.0/MoonEngine-0.3.0-win64.zip\"}],"
    "\"body\":\"notes \\\"quoted\\\" \\u00e9\"}";

static int expect(const char *what, int got, int want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %d want %d\n", what, got, want);
        return 1;
    }
    return 0;
}
static int bad(const char *what, const char *from, const char *to)
{
    char json[4096];
    const char *at = strstr(good, from);
    if (!at) {
        fprintf(stderr, "FAIL %s: pattern missing\n", what);
        return 1;
    }
    snprintf(json, sizeof json, "%.*s%s%s", (int)(at - good), good, to, at + strlen(from));
    app_update_release r;
    return expect(what, app_update_parse_release(json, "-win64.zip", &r), 0);
}

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#include "miniz.h"
int app_update_test_install(const wchar_t *dir, const unsigned char *zip, size_t n,
                            const int *steps, int count);
static void put(const wchar_t *dir, const wchar_t *name, const char *text)
{
    wchar_t p[MAX_PATH];
    swprintf(p, MAX_PATH, L"%s\\%s", dir, name);
    FILE *f = _wfopen(p, L"wb");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}
static int has(const wchar_t *dir, const wchar_t *name, const char *text)
{
    wchar_t p[MAX_PATH];
    char buf[64] = {0};
    swprintf(p, MAX_PATH, L"%s\\%s", dir, name);
    FILE *f = _wfopen(p, L"rb");
    if (!f)
        return text == NULL;
    fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    return text && !strcmp(buf, text);
}
static int make_zip(const char *const *names, const char *const *bodies, int count, void **out,
                    size_t *n)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_writer_init_heap(&z, 0, 0))
        return 0;
    for (int i = 0; i < count; i++)
        if (!mz_zip_writer_add_mem(&z, names[i], bodies[i], strlen(bodies[i]), MZ_DEFAULT_COMPRESSION))
            return 0;
    return mz_zip_writer_finalize_heap_archive(&z, out, n) && mz_zip_writer_end(&z);
}
/* unpack + swap in a scratch folder: listed files replaced, nothing else touched */
static int install_test(void)
{
    int f = 0;
    wchar_t tmp[MAX_PATH], dir[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    swprintf(dir, MAX_PATH, L"%smoon_update_test_%lu", tmp, GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    put(dir, L"MoonEngine.exe", "old exe");
    put(dir, L"README.txt", "old readme");
    put(dir, L"keep.txt.moon-old", "not ours");
    put(dir, L"keep.txt.moon-new", "not ours either");
    const char *names[] = {"MoonEngine-0.3.0-win64/MoonEngine.exe",
                           "MoonEngine-0.3.0-win64/README.txt",
                           "MoonEngine-0.3.0-win64/launcher_icons/a.png"};
    const char *bodies[] = {"new exe", "new readme", "icon"};
    void *zip = NULL;
    size_t n = 0;
    f += expect("zip", make_zip(names, bodies, 3, &zip, &n), 1);
    int unpack_only[] = {2};
    f += expect("unpack", app_update_test_install(dir, zip, n, unpack_only, 0), 1);
    f += expect("staged", has(dir, L"MoonEngine.exe.moon-new", "new exe"), 1);
    f += expect("not yet", has(dir, L"MoonEngine.exe", "old exe"), 1);
    int apply[] = {1};
    f += expect("apply", app_update_test_install(dir, zip, n, apply, 1), 1);
    f += expect("exe swapped", has(dir, L"MoonEngine.exe", "new exe"), 1);
    f += expect("old kept", has(dir, L"MoonEngine.exe.moon-old", "old exe"), 1);
    f += expect("readme", has(dir, L"README.txt", "new readme"), 1);
    f += expect("subfolder", has(dir, L"launcher_icons\\a.png", "icon"), 1);
    int next_start[] = {-1};
    mz_free(zip);
    /* the next start: leftovers of the update go, nothing else */
    const char *none[] = {"MoonEngine-0.3.0-win64/MoonEngine.exe"};
    const char *none_body[] = {"x"};
    f += expect("zip2", make_zip(none, none_body, 1, &zip, &n), 1);
    put(dir, L"moon-update.list", "MoonEngine.exe\nREADME.txt\nlauncher_icons/a.png\n");
    {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"%s\\MoonEngine.exe.moon-new", dir);
        DeleteFileW(p);
    }
    extern int app_update_test_sweep(const wchar_t *dir, int step);
    f += expect("sweep", app_update_test_sweep(dir, next_start[0]), 1);
    f += expect("old gone", has(dir, L"MoonEngine.exe.moon-old", NULL), 1);
    f += expect("list gone", has(dir, L"moon-update.list", NULL), 1);
    f += expect("exe stays", has(dir, L"MoonEngine.exe", "new exe"), 1);
    f += expect("foreign old untouched", has(dir, L"keep.txt.moon-old", "not ours"), 1);
    f += expect("foreign new untouched", has(dir, L"keep.txt.moon-new", "not ours either"), 1);
    mz_free(zip);
    /* a member escaping the folder: refused, nothing staged */
    const char *evil[] = {"MoonEngine-0.3.0-win64/MoonEngine.exe", "MoonEngine-0.3.0-win64/../evil.txt"};
    const char *evil_body[] = {"bad exe", "bad"};
    f += expect("zip3", make_zip(evil, evil_body, 2, &zip, &n), 1);
    f += expect("evil refused", app_update_test_install(dir, zip, n, unpack_only, 0), 0);
    f += expect("evil not staged", has(dir, L"MoonEngine.exe.moon-new", NULL), 1);
    f += expect("exe intact", has(dir, L"MoonEngine.exe", "new exe"), 1);
    mz_free(zip);
    /* no program in the release: refused */
    const char *noexe[] = {"MoonEngine-0.3.0-win64/README.txt"};
    f += expect("zip4", make_zip(noexe, bodies + 1, 1, &zip, &n), 1);
    f += expect("no exe refused", app_update_test_install(dir, zip, n, unpack_only, 0), 0);
    f += expect("readme not staged", has(dir, L"README.txt.moon-new", NULL), 1);
    mz_free(zip);
    const wchar_t *clean[] = {L"MoonEngine.exe", L"README.txt", L"keep.txt.moon-old",
                              L"keep.txt.moon-new", L"launcher_icons\\a.png"};
    for (int i = 0; i < 5; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"%s\\%s", dir, clean[i]);
        DeleteFileW(p);
    }
    wchar_t sub[MAX_PATH];
    swprintf(sub, MAX_PATH, L"%s\\launcher_icons", dir);
    RemoveDirectoryW(sub);
    RemoveDirectoryW(dir);
    return f;
}
#endif

int main(int argc, char **argv)
{
    int negative = argc > 1 && !strcmp(argv[1], "--negative"), f = 0;
    app_update_release r;
#ifdef _WIN32
    f += install_test();
#endif
    f += expect("good", app_update_parse_release(good, "-win64.zip", &r), 1);
    f += expect("version", strcmp(r.version, "0.3.0"), 0);
    f += expect("size", r.size == 123456, 1);
    f += expect("sha lower", strcmp(r.sha256, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"), 0);
    f += expect("url", !!strstr(r.url, "/v0.3.0/MoonEngine-0.3.0-win64.zip"), 1);
    f += bad("draft", "\"draft\":false", "\"draft\":true");
    f += bad("prerelease", "\"prerelease\":false", "\"prerelease\":true");
    f += bad("no digest", "\"digest\":\"sha256:", "\"digest\":\"md5:");
    f += bad("short digest", DIGEST "\"", "0123\"");
    f += bad("other host", "https://github.com/Jonhline/MoonEngine/releases/download/v0.3.0/MoonEngine",
             "https://example.com/Jonhline/MoonEngine/releases/download/v0.3.0/MoonEngine");
    f += bad("other repo", "github.com/Jonhline/MoonEngine/releases/download/v0.3.0/MoonEngine-0.3.0",
             "github.com/someone/MoonEngine/releases/download/v0.3.0/MoonEngine-0.3.0");
    f += bad("bad tag", "\"tag_name\":\"v0.3.0\"", "\"tag_name\":\"latest\"");
    f += bad("asset of another version", "\"name\":\"MoonEngine-0.3.0-win64.zip\"",
             "\"name\":\"MoonEngine-0.2.9-win64.zip\"");
    f += bad("zero size", "\"size\":123456", "\"size\":0");
    f += bad("truncated", "}],", "}");
    f += expect("no assets", app_update_parse_release("{\"tag_name\":\"v1.0.0\",\"assets\":[]}",
                                                      "-win64.zip", &r), 0);
    if (negative)
        f += expect("negative", app_update_parse_release(good, "-win64.zip", &r), 0);
    if (!f)
        printf("app_update_parse: ok\n");
    return f != 0;
}
