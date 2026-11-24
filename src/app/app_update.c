/* P: the GitHub release check of stable builds (see app_update.h). PC only;
   no game simulation, disc data or save is involved. */
#include "app_update.h"
#include "launcher_update.h" /* MOON_VERSION */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UPDATE_REPO "Jonhline/MoonEngine"
#ifdef _WIN32
#define UPDATE_ASSET_SUFFIX "-win64.zip"
#else
#define UPDATE_ASSET_SUFFIX "-linux-x86_64.tar.gz"
#endif

/* Which feed the studio reads. Stable = GitHub's "latest" release, which
   GitHub never answers with a pre-release; nightly = the newest release of any
   kind, so a pre-release is the nightly and stable users never see it. */
static int update_nightly;
void app_update_set_channel(int nightly) { update_nightly = nightly ? 1 : 0; }
const char *app_update_feed(void)
{
#if MOON_DEV_BUILD
    const char *env = getenv("MOON_UPDATE_FEED"); /* tests: point at a local feed */
    if (env && *env)
        return env;
#endif
    return update_nightly ? "https://api.github.com/repos/" UPDATE_REPO "/releases?per_page=1"
                          : "https://api.github.com/repos/" UPDATE_REPO "/releases/latest";
}
/* a list answer ("[{...}]") starts at its first object; a single release is
   already an object, so both start at the first '{' */
static const char *release_object(const char *json)
{
    return json ? strchr(json, '{') : NULL;
}

/* X.Y.Z, no leading zeros; 1 when remote is newer than local */
static int version_parts(const char *s, unsigned v[3])
{
    for (int k = 0; k < 3; k++) {
        if (!isdigit((unsigned char)*s) || (*s == '0' && isdigit((unsigned char)s[1])))
            return 0;
        unsigned n = 0;
        while (isdigit((unsigned char)*s)) {
            n = n * 10 + (unsigned)(*s++ - '0');
            if (n > 1000000)
                return 0;
        }
        v[k] = n;
        if (k < 2 && *s++ != '.')
            return 0;
    }
    return !*s;
}
static int newer(const char *remote, const char *local)
{
    unsigned a[3], b[3];
    if (!version_parts(remote, a) || !version_parts(local, b))
        return 0;
    for (int k = 0; k < 3; k++)
        if (a[k] != b[k])
            return a[k] > b[k];
    return 0;
}

/* ---- the release JSON: a small walker, only what the update needs ---- */
static void ws(const char **s)
{
    while (isspace((unsigned char)**s))
        ++*s;
}
static int jstr(const char **s, char *out, size_t cap)
{
    if (**s != '"')
        return 0;
    ++*s;
    size_t n = 0;
    while (**s && **s != '"') {
        char c = *(*s)++;
        if (c == '\\') {
            c = *(*s)++;
            if (!c)
                return 0;
            if (c == 'u') { /* names/urls/versions are ASCII; anything else is replaced */
                for (int i = 0; i < 4; i++)
                    if (!isxdigit((unsigned char)*(*s)++))
                        return 0;
                c = '?';
            } else if (c == 'n' || c == 't' || c == 'r' || c == 'b' || c == 'f')
                c = ' ';
        }
        if (out && n + 1 < cap)
            out[n++] = c;
        else if (out)
            return 0;
    }
    if (**s != '"')
        return 0;
    ++*s;
    if (out)
        out[n] = 0;
    return 1;
}
static int jskip(const char **s, int depth)
{
    ws(s);
    if (depth > 32)
        return 0;
    if (**s == '"')
        return jstr(s, NULL, 0);
    if (**s == '{' || **s == '[') {
        char close = **s == '{' ? '}' : ']';
        ++*s;
        ws(s);
        if (**s == close) {
            ++*s;
            return 1;
        }
        for (;;) {
            if (close == '}') {
                ws(s);
                if (!jstr(s, NULL, 0))
                    return 0;
                ws(s);
                if (*(*s)++ != ':')
                    return 0;
            }
            if (!jskip(s, depth + 1))
                return 0;
            ws(s);
            if (**s == close) {
                ++*s;
                return 1;
            }
            if (*(*s)++ != ',')
                return 0;
        }
    }
    const char *start = *s;
    while (**s && (isalnum((unsigned char)**s) || strchr("+-.", **s)))
        ++*s;
    return *s > start;
}
static int jnum(const char **s, unsigned long long *out)
{
    ws(s);
    if (!isdigit((unsigned char)**s))
        return 0;
    unsigned long long n = 0;
    while (isdigit((unsigned char)**s)) {
        n = n * 10 + (unsigned)(*(*s)++ - '0');
        if (n > (1ull << 31))
            return 0;
    }
    *out = n;
    return 1;
}
/* one object's members: fn(key, cursor at the value) consumes the value or returns 0 to skip */
typedef int (*member_fn)(void *ctx, const char *key, const char **s);
static int jobject(const char **s, member_fn fn, void *ctx)
{
    ws(s);
    if (*(*s)++ != '{')
        return 0;
    ws(s);
    if (**s == '}') {
        ++*s;
        return 1;
    }
    for (;;) {
        char key[64];
        ws(s);
        if (!jstr(s, key, sizeof key))
            return 0;
        ws(s);
        if (*(*s)++ != ':')
            return 0;
        ws(s);
        const char *before = *s;
        int took = fn(ctx, key, s);
        if (took < 0)
            return 0;
        if (!took) {
            *s = before;
            if (!jskip(s, 1))
                return 0;
        }
        ws(s);
        if (**s == '}') {
            ++*s;
            return 1;
        }
        if (*(*s)++ != ',')
            return 0;
    }
}
typedef struct {
    char name[160], url[1024], digest[80];
    unsigned long long size;
} asset_fields;
static int asset_member(void *ctx, const char *key, const char **s)
{
    asset_fields *a = ctx;
    if (!strcmp(key, "name"))
        return jstr(s, a->name, sizeof a->name) ? 1 : -1;
    if (!strcmp(key, "browser_download_url"))
        return jstr(s, a->url, sizeof a->url) ? 1 : -1;
    if (!strcmp(key, "digest"))
        return **s == '"' ? (jstr(s, a->digest, sizeof a->digest) ? 1 : -1) : 0;
    if (!strcmp(key, "size"))
        return jnum(s, &a->size) ? 1 : -1;
    return 0;
}
typedef struct {
    char tag[64], want[200];
    int draft, prerelease, found;
    asset_fields asset;
} release_fields;
static int release_member(void *ctx, const char *key, const char **s)
{
    release_fields *r = ctx;
    if (!strcmp(key, "tag_name"))
        return jstr(s, r->tag, sizeof r->tag) ? 1 : -1;
    if (!strcmp(key, "draft") || !strcmp(key, "prerelease")) {
        int v = !strncmp(*s, "true", 4);
        if (!strncmp(*s, "true", 4) || !strncmp(*s, "false", 5)) {
            *s += v ? 4 : 5;
            *(!strcmp(key, "draft") ? &r->draft : &r->prerelease) = v;
            return 1;
        }
        return -1;
    }
    if (!strcmp(key, "assets")) {
        if (*(*s)++ != '[')
            return -1;
        ws(s);
        if (**s == ']') {
            ++*s;
            return 1;
        }
        for (;;) {
            asset_fields a;
            memset(&a, 0, sizeof a);
            if (!jobject(s, asset_member, &a))
                return -1;
            size_t n = strlen(a.name), m = strlen(r->want);
            if (!r->found && n >= m && !strcmp(a.name + n - m, r->want)) {
                r->asset = a;
                r->found = 1;
            }
            ws(s);
            if (**s == ']') {
                ++*s;
                return 1;
            }
            if (*(*s)++ != ',')
                return -1;
        }
    }
    return 0;
}
int app_update_parse_release(const char *json, const char *asset_suffix, app_update_release *out)
{ return app_update_parse_release_ex(json, asset_suffix, 0, out); }
int app_update_parse_release_ex(const char *json, const char *asset_suffix, int allow_prerelease,
                                app_update_release *out)
{
    release_fields r;
    memset(&r, 0, sizeof r);
    memset(out, 0, sizeof *out);
    json = release_object(json);
    const char *s = json, *tag;
    if (!json || !asset_suffix)
        return 0;
    if (!jobject(&s, release_member, &r) || r.draft || (r.prerelease && !allow_prerelease))
        return 0;
    tag = r.tag[0] == 'v' || r.tag[0] == 'V' ? r.tag + 1 : r.tag;
    if (strlen(tag) >= sizeof out->version || !newer(tag, "0.0.0"))
        return 0;
    /* the asset this platform needs: MoonEngine-<version><suffix> */
    char want[200];
    snprintf(want, sizeof want, "MoonEngine-%s%s", tag, asset_suffix);
    snprintf(r.want, sizeof r.want, "%s", want);
    s = json;
    memset(&r.asset, 0, sizeof r.asset);
    r.found = 0;
    if (!jobject(&s, release_member, &r) || !r.found || strcmp(r.asset.name, want))
        return 0;
    /* only GitHub's own HTTPS download, a known size and its SHA-256 */
#if MOON_DEV_BUILD
    int dev_fake = getenv("MOON_UPDATE_FAKE") && !strncmp(r.asset.url, "http://127.0.0.1:", 17);
#else
    int dev_fake = 0;
#endif
    if (!dev_fake &&
        (strncmp(r.asset.url, "https://github.com/" UPDATE_REPO "/releases/download/",
                 strlen("https://github.com/" UPDATE_REPO "/releases/download/")) ||
         strstr(r.asset.url, "..") || !r.asset.size || strncmp(r.asset.digest, "sha256:", 7) ||
         strlen(r.asset.digest + 7) != 64))
        return 0;
    for (int i = 0; i < 64; i++)
        if (!isxdigit((unsigned char)r.asset.digest[7 + i]))
            return 0;
    snprintf(out->version, sizeof out->version, "%s", tag);
    snprintf(out->url, sizeof out->url, "%s", r.asset.url);
    for (int i = 0; i < 64; i++)
        out->sha256[i] = (char)tolower((unsigned char)r.asset.digest[7 + i]);
    out->sha256[64] = 0;
    out->size = r.asset.size;
    return 1;
}

/* MOON_UPDATE_FORCE: the tests build the Windows path in dev builds too */
#if defined(_WIN32) && (!MOON_DEV_BUILD || defined(MOON_UPDATE_FORCE))
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include "miniz.h"

enum { UP_IDLE, UP_CHECKING, UP_DOWNLOADING, UP_READY, UP_AVAILABLE, UP_NONE };
static volatile LONG state = UP_IDLE;
static app_update_release release;
static wchar_t folder[MAX_PATH]; /* the program's own folder */
static char status_line[160];

/* GET over HTTPS; redirects followed (release downloads go to GitHub's storage) */
static unsigned char *fetch(const char *url, size_t limit, size_t *got)
{
    wchar_t wurl[1100], host[256], path[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1100))
        return NULL;
    URL_COMPONENTS u = {0};
    u.dwStructSize = sizeof u;
    u.lpszHostName = host;
    u.dwHostNameLength = 256;
    u.lpszUrlPath = path;
    u.dwUrlPathLength = 1024;
    if (!WinHttpCrackUrl(wurl, 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS)
        return NULL;
    HINTERNET session = WinHttpOpen(L"MoonEngine-updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
              connection = NULL, r = NULL;
    unsigned char *data = NULL;
    size_t n = 0;
    int ok = 0;
    if (!session)
        return NULL;
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
    protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
    WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
    WinHttpSetTimeouts(session, 5000, 5000, 15000, 30000);
    connection = WinHttpConnect(session, host, u.nPort, 0);
    if (connection)
        r = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (r) {
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
        WinHttpSetOption(r, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);
        DWORD status = 0, sn = sizeof status;
        if (WinHttpSendRequest(r, L"Accept: application/vnd.github+json, application/octet-stream\r\n",
                               (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(r, NULL) &&
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                                &status, &sn, NULL) &&
            status == 200 && (data = malloc(limit + 1))) {
            ok = 1;
            for (;;) {
                DWORD k = 0;
                if (n >= limit) { /* more than allowed */
                    unsigned char probe;
                    if (WinHttpReadData(r, &probe, 1, &k) && k)
                        ok = 0;
                    break;
                }
                if (!WinHttpReadData(r, data + n, (DWORD)(limit - n < 65536 ? limit - n : 65536),
                                     &k)) {
                    ok = 0;
                    break;
                }
                if (!k)
                    break;
                n += k;
            }
        }
    }
    if (r)
        WinHttpCloseHandle(r);
    if (connection)
        WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!ok) {
        free(data);
        return NULL;
    }
    data[n] = 0;
    *got = n;
    return data;
}
static int sha256_matches(const unsigned char *data, size_t n, const char *hex)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    unsigned char digest[32];
    int ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0 &&
             BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0) == 0 &&
             BCryptHashData(hash, (PUCHAR)data, (ULONG)n, 0) == 0 &&
             BCryptFinishHash(hash, digest, sizeof digest, 0) == 0;
    if (hash)
        BCryptDestroyHash(hash);
    if (alg)
        BCryptCloseAlgorithmProvider(alg, 0);
    for (int i = 0; ok && i < 32; i++) {
        char pair[3] = {hex[i * 2], hex[i * 2 + 1], 0};
        ok = digest[i] == (unsigned char)strtoul(pair, NULL, 16);
    }
    return ok;
}
/* a relative path the update may write: plain, inside the folder, never the list */
static int member_path_ok(const char *p)
{
    if (!*p || strlen(p) > 400 || strstr(p, "..") || strchr(p, ':') || strchr(p, '\\') ||
        p[0] == '/' || p[strlen(p) - 1] == '/' || !_stricmp(p, "moon-update.list"))
        return 0;
    for (const char *c = p; *c; c++)
        if ((unsigned char)*c < 32)
            return 0;
    return 1;
}
/* a zip member's path inside the release folder, or NULL when it is not a plain file there */
static const char *member_path(const char *name)
{
    const char *p = strchr(name, '/');
    return p && member_path_ok(p + 1) ? p + 1 : NULL;
}
static void list_path(wchar_t *out)
{
    swprintf(out, MAX_PATH * 2, L"%s\\moon-update.list", folder);
}
static int target_path(const char *rel, const wchar_t *suffix, wchar_t *out, size_t cap)
{
    wchar_t wrel[512];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, rel, -1, wrel, 512))
        return 0;
    for (wchar_t *c = wrel; *c; c++)
        if (*c == L'/')
            *c = L'\\';
    return swprintf(out, cap, L"%s\\%s%s", folder, wrel, suffix) > 0;
}
static void make_parents(wchar_t *path)
{
    for (wchar_t *c = path + wcslen(folder) + 1; *c; c++)
        if (*c == L'\\') {
            *c = 0;
            CreateDirectoryW(path, NULL);
            *c = L'\\';
        }
}
/* every file of the release beside the program as *.moon-new; all or nothing */
static int unpack(const unsigned char *zip, size_t n)
{
    mz_zip_archive z;
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_mem(&z, zip, n, 0))
        return 0;
    int ok = 1, files = 0, has_exe = 0;
    mz_uint count = mz_zip_reader_get_num_files(&z);
    /* the list, before any file: a failed unpack is then removed by it */
    static char list[65536];
    size_t used = 0;
    for (mz_uint i = 0; ok && i < count; i++) {
        mz_zip_archive_file_stat st;
        const char *rel;
        if (!mz_zip_reader_file_stat(&z, i, &st))
            ok = 0;
        else if (!st.m_is_directory) {
            size_t k = (rel = member_path(st.m_filename)) ? strlen(rel) : 0;
            if (!rel || used + k + 2 > sizeof list)
                ok = 0;
            else {
                memcpy(list + used, rel, k);
                used += k;
                list[used++] = '\n';
            }
        }
    }
    wchar_t lp[MAX_PATH * 2];
    list_path(lp);
    HANDLE lf = ok ? CreateFileW(lp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                 NULL)
                   : INVALID_HANDLE_VALUE;
    DWORD lw = 0;
    ok = ok && lf != INVALID_HANDLE_VALUE && WriteFile(lf, list, (DWORD)used, &lw, NULL) &&
         lw == used;
    if (lf != INVALID_HANDLE_VALUE)
        CloseHandle(lf);
    for (mz_uint i = 0; ok && i < count; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) {
            ok = 0;
            break;
        }
        if (st.m_is_directory)
            continue;
        const char *rel = member_path(st.m_filename);
        wchar_t out[MAX_PATH * 2];
        if (!rel || !target_path(rel, L".moon-new", out, MAX_PATH * 2)) {
            ok = 0;
            break;
        }
        size_t size = 0;
        void *bytes = mz_zip_reader_extract_to_heap(&z, i, &size, 0);
        if (!bytes) {
            ok = 0;
            break;
        }
        make_parents(out);
        HANDLE f = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                               NULL);
        DWORD written = 0;
        ok = f != INVALID_HANDLE_VALUE && WriteFile(f, bytes, (DWORD)size, &written, NULL) &&
             written == size;
        if (f != INVALID_HANDLE_VALUE)
            CloseHandle(f);
        mz_free(bytes);
        files++;
        if (!_stricmp(rel, "MoonEngine.exe"))
            has_exe = 1;
    }
    mz_zip_reader_end(&z);
    return ok && files && has_exe;
}
/* The files an update wrote are listed in moon-update.list beside the program
   (one relative path per line, UTF-8), so nothing else in the folder is touched.
   apply 1: each *.moon-new replaces its file, the old one kept as *.moon-old;
   apply 0: the *.moon-new are discarded; apply -1: the *.moon-old are removed
   and the *.moon-new discarded, then the list goes. */
static void sweep(int apply)
{
    wchar_t lp[MAX_PATH * 2];
    list_path(lp);
    HANDLE f = CreateFileW(lp, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return;
    static char text[65536];
    DWORD n = 0;
    int ok = ReadFile(f, text, sizeof text - 1, &n, NULL);
    CloseHandle(f);
    text[ok ? n : 0] = 0;
    for (char *line = text, *next; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next)
            *next++ = 0;
        wchar_t target[MAX_PATH * 2], fresh[MAX_PATH * 2], old[MAX_PATH * 2];
        if (!member_path_ok(line) || !target_path(line, L"", target, MAX_PATH * 2) ||
            !target_path(line, L".moon-new", fresh, MAX_PATH * 2) ||
            !target_path(line, L".moon-old", old, MAX_PATH * 2))
            continue;
        if (apply > 0) {
            if (GetFileAttributesW(fresh) == INVALID_FILE_ATTRIBUTES)
                continue;
            /* a running program can be renamed, not overwritten */
            if (GetFileAttributesW(target) != INVALID_FILE_ATTRIBUTES)
                MoveFileExW(target, old, MOVEFILE_REPLACE_EXISTING);
            if (!MoveFileExW(fresh, target, MOVEFILE_REPLACE_EXISTING))
                MoveFileExW(old, target, MOVEFILE_REPLACE_EXISTING); /* put it back */
        } else {
            DeleteFileW(fresh);
            if (apply < 0)
                DeleteFileW(old);
        }
    }
    if (apply < 0 || apply == 0)
        DeleteFileW(lp);
}
static DWORD WINAPI worker(void *unused)
{
    (void)unused;
    size_t n = 0;
    unsigned char *json = fetch(app_update_feed(), 1 << 20, &n);
    app_update_release r;
    if (!json || memchr(json, 0, n) ||
        !app_update_parse_release_ex((char *)json, UPDATE_ASSET_SUFFIX, update_nightly, &r) ||
        !newer(r.version, MOON_VERSION)) {
        free(json);
        InterlockedExchange(&state, UP_NONE);
        return 0;
    }
    free(json);
    release = r;
    InterlockedExchange(&state, UP_DOWNLOADING);
    unsigned char *zip = r.size <= (512u << 20) ? fetch(r.url, (size_t)r.size, &n) : NULL;
    int ok = zip && n == r.size && sha256_matches(zip, n, r.sha256) && unpack(zip, n);
    free(zip);
    if (!ok)
        sweep(0); /* nothing half-unpacked stays */
    InterlockedExchange(&state, ok ? UP_READY : UP_AVAILABLE);
    return 0;
}
void app_update_start(int enabled)
{
    if (folder[0])
        return;
    DWORD k = GetModuleFileNameW(NULL, folder, MAX_PATH);
    wchar_t *end = k && k < MAX_PATH ? wcsrchr(folder, L'\\') : NULL;
    if (!end) {
        folder[0] = 0;
        return;
    }
    *end = 0;
    sweep(-1); /* the last update's leftovers (or one never applied) */
    if (!enabled)
        return;
    InterlockedExchange(&state, UP_CHECKING);
    HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (t)
        CloseHandle(t);
    else
        InterlockedExchange(&state, UP_NONE);
}
const char *app_update_status(void)
{
    switch (InterlockedCompareExchange(&state, 0, 0)) {
    case UP_DOWNLOADING:
        snprintf(status_line, sizeof status_line, "Downloading update %s", release.version);
        return status_line;
    case UP_READY:
        snprintf(status_line, sizeof status_line, "Update %s installs when you quit",
                 release.version);
        return status_line;
    case UP_AVAILABLE:
        snprintf(status_line, sizeof status_line, "Update %s available on GitHub",
                 release.version);
        return status_line;
    default:
        return NULL;
    }
}
void app_update_finish(void)
{
    if (InterlockedCompareExchange(&state, 0, 0) == UP_READY)
        sweep(1);
}
#ifdef MOON_UPDATE_FORCE
/* tests: unpack a release zip into dir, then sweep(step) for each step until 2 */
int app_update_test_install(const wchar_t *dir, const unsigned char *zip, size_t n,
                            const int *steps, int count)
{
    wcsncpy(folder, dir, MAX_PATH - 1);
    int ok = unpack(zip, n);
    if (!ok)
        sweep(0);
    for (int i = 0; ok && i < count; i++)
        sweep(steps[i]);
    return ok;
}
int app_update_test_sweep(const wchar_t *dir, int step)
{
    wcsncpy(folder, dir, MAX_PATH - 1);
    sweep(step);
    return 1;
}
#endif
#elif !defined(_WIN32)
#include "app_update_linux.inc"
void app_update_start(int enabled)
{
    const char *tf = getenv("MOON_UPDATE_TEST_TAR");
    if (tf && *tf) {
        snprintf(lup_dir, sizeof lup_dir, "/tmp/moon-update-test");
        snprintf(lup_exe, sizeof lup_exe, "MoonEngine");
        mkdir(lup_dir, 0755);
        FILE *f = fopen(tf, "rb");
        if (!f) return;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char *p = n > 0 ? malloc((size_t)n) : NULL;
        int ok = p && fread(p, 1, (size_t)n, f) == (size_t)n;
        fclose(f);
        if (ok) {
            fprintf(stderr, "tar test: %s %ld bytes\n", tf, n);
            tar_unpack(p, (size_t)n);
        }
        free(p);
        return;
    }
    if (!enabled) return;
    char exe[1024];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n <= 0) return;
    exe[n] = 0;
    snprintf(lup_dir, sizeof lup_dir, "%s", exe);
    char *slash = strrchr(lup_dir, '/');
    if (slash) *slash = 0;
    snprintf(lup_exe, sizeof lup_exe, "%s", slash ? slash + 1 : "MoonEngine");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    pthread_t th;
    if (pthread_create(&th, NULL, update_thread, NULL) == 0)
        pthread_detach(th);
}
const char *app_update_status(void)
{
    return lup_state == LUP_IDLE ? NULL : lup_status;
}
void app_update_finish(void)
{
    if (lup_state == LUP_READY) sweep(1);
}
#else
/* Windows developer build: the updater is compiled only with MOON_UPDATE_FORCE. */
void app_update_start(int enabled) { (void)enabled; }
const char *app_update_status(void) { return NULL; }
void app_update_finish(void) {}
#endif
