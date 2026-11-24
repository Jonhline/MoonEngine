/* Linux/macOS updater: libcurl transport + miniz inflate + tar walk. */
#include <curl/curl.h>
#include <miniz.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

enum { LUP_IDLE, LUP_CHECKING, LUP_AVAILABLE, LUP_DOWNLOADING, LUP_READY, LUP_NONE, LUP_ERROR };
static volatile int lup_state = LUP_IDLE;
static app_update_release lup_release;
static char lup_status[160], lup_dir[1024], lup_exe[256];

#include "moon_http.h"
static unsigned char *fetch(const char *url, size_t limit, size_t *got)
{
    return moon_http_get(url, limit, got);
}
static int sha256_file(const char *path, const char *want)
{
    char cmd[1200], out[256] = "";
    snprintf(cmd, sizeof cmd, "sha256sum '%s' 2>/dev/null", path);
    FILE *p = popen(cmd, "r");
    if (!p) return 0;
    if (!fgets(out, sizeof out, p)) out[0] = 0;
    pclose(p);
    return strlen(out) >= 64 && !strncasecmp(out, want, 64);
}
static unsigned long long octal(const unsigned char *p, int n)
{
    unsigned long long v = 0;
    while (n-- && *p >= '0' && *p <= '7') v = v * 8 + (unsigned)(*p++ - '0');
    return v;
}
static int tar_header_ok(const unsigned char *h)
{
    unsigned sum = 0;
    for (int i = 0; i < 512; i++)
        sum += i >= 148 && i < 156 ? ' ' : h[i];
    return sum == octal(h + 148, 8);
}
static int mkdirs(const char *path)
{
    char tmp[1400];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    return 0;
}
static void map_name(const char *in, char *out, size_t size)
{
    const char *p = in;
    while (p[0] == '.' && p[1] == '/') p += 2;
    if (!strncmp(p, "MoonEngine-", 11)) {
        const char *slash = strchr(p, '/');
        if (slash) p = slash + 1;
    }
    snprintf(out, size, "%s", p);
}
static int tar_unpack(const unsigned char *tgz, size_t n)
{
    size_t hdr = 10;
    if (n < 18 || tgz[0] != 0x1f || tgz[1] != 0x8b) return 0;
    int flags = tgz[3];
    if (flags & 4) hdr += 2 + (tgz[hdr] | (tgz[hdr + 1] << 8));
    if (flags & 8) while (hdr < n && tgz[hdr++]) {}
    if (flags & 16) while (hdr < n && tgz[hdr++]) {}
    if (flags & 2) hdr += 2;
    size_t raw_n = 0;
    void *raw = tinfl_decompress_mem_to_heap(tgz + hdr, n - hdr - 8, &raw_n, 0);
    if (!raw || raw_n < 1024) { free(raw); return 0; }
    const unsigned char *t = raw;
    static char names[512][512];
    static unsigned long long sizes[512];
    static const unsigned char *datas[512];
    int count = 0;
    char longname[512] = "";
    for (size_t at = 0; at + 512 <= raw_n;) {
        const unsigned char *h = t + at;
        if (!h[0]) break;
        if (!tar_header_ok(h)) { free(raw); return 0; }
        char name[512];
        snprintf(name, sizeof name, "%s", longname[0] ? longname : (const char *)h);
        unsigned long long sz = octal(h + 124, 12);
        char type = (char)h[156];
        const unsigned char *data = h + 512;
        if (type == 'L') {
            size_t k = sz < sizeof longname - 1 ? (size_t)sz : sizeof longname - 1;
            memcpy(longname, data, k); longname[k] = 0;
        } else if (type == 'x' || type == 'g' || type == 'K') {
            /* pax/link metadata: skip */
        } else if (type == '0' || type == 0) {
            char rel[512];
            map_name(name, rel, sizeof rel);
            if (!*rel || rel[0] == '/' || strstr(rel, "..")) { free(raw); return 0; }
            if (count < 512) {
                snprintf(names[count], sizeof names[count], "%s", rel);
                sizes[count] = sz;
                datas[count] = data;
                count++;
            }
        }
        longname[0] = 0;
        at += 512 + ((sz + 511) & ~511ULL);
    }
    if (!count) { free(raw); return 0; }
    char list[1400];
    snprintf(list, sizeof list, "%s/moon-update.list", lup_dir);
    FILE *lf = fopen(list, "wb");
    if (!lf) { free(raw); return 0; }
    for (int i = 0; i < count; i++) fprintf(lf, "%s\n", names[i]);
    fclose(lf);
    int has = 0;
    for (int i = 0; i < count; i++) {
        char path[1600], out[1700];
        snprintf(path, sizeof path, "%s/%s", lup_dir, names[i]);
        if (!strcmp(names[i], "MoonEngine") && lup_exe[0]) {
            snprintf(path, sizeof path, "%s/%s", lup_dir, lup_exe);
            snprintf(names[i], sizeof names[i], "%s", lup_exe);
        }
        snprintf(out, sizeof out, "%s.moon-new", path);
        if (getenv("MOON_UPDATE_TAR_TRACE"))
            fprintf(stderr, "tar member: %s %llu -> %s\n", names[i], sizes[i], out);
        mkdirs(out);
        FILE *f = fopen(out, "wb");
        if (!f) { free(raw); return 0; }
        size_t w = fwrite(datas[i], 1, (size_t)sizes[i], f);
        fclose(f);
        if (w != sizes[i]) { free(raw); return 0; }
        if (!strcmp(names[i], lup_exe)) has = 1;
    }
    free(raw);
    return has;
}
static void sweep(int apply)
{
    char list[1400];
    snprintf(list, sizeof list, "%s/moon-update.list", lup_dir);
    FILE *f = fopen(list, "rb");
    if (!f) return;
    char line[1200];
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!n) continue;
        char old[1600], newp[1700], bak[1700];
        snprintf(old, sizeof old, "%s/%s", lup_dir, line);
        snprintf(newp, sizeof newp, "%s.moon-new", old);
        snprintf(bak, sizeof bak, "%s.moon-old", old);
        if (apply == 1) {
            unlink(bak);
            if (access(newp, F_OK) == 0) {
                rename(old, bak);
                rename(newp, old);
            }
        } else if (apply == 0) unlink(newp);
        else if (apply == -1) { unlink(bak); unlink(newp); }
    }
    fclose(f);
    unlink(list);
}
static void *update_thread(void *unused);
static int begin_started;
void app_update_begin(int auto_check)
{
    (void)auto_check;
    if (begin_started) return;
    begin_started = 1;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    pthread_t th;
    if (pthread_create(&th, NULL, update_thread, NULL) == 0) pthread_detach(th);
}
int app_update_state(void)
{
    switch (lup_state) {
    case LUP_CHECKING: return 1;
    case LUP_AVAILABLE: return 6;
    case LUP_DOWNLOADING: return 2;
    case LUP_READY: return 3;
    case LUP_NONE: return 4;
    case LUP_ERROR: return 5;
    default: return 0;
    }
}
int app_update_progress(void) { return lup_state == LUP_DOWNLOADING ? moon_http_progress() : (lup_state == LUP_READY ? 100 : 0); }
const char *app_update_message(void) { return lup_status; }
int app_update_available(char *ver, size_t n)
{
    if (!ver || !n || (lup_state != LUP_AVAILABLE && lup_state != LUP_DOWNLOADING &&
                       lup_state != LUP_READY)) return 0;
    snprintf(ver, n, "%s", lup_release.version);
    return 1;
}
static void *download_thread(void *unused);
int app_update_download(void)
{
    if (lup_state != LUP_AVAILABLE) return 0;
    pthread_t th;
    if (pthread_create(&th, NULL, download_thread, NULL) != 0) return 0;
    pthread_detach(th);
    return 1;
}
static void *update_thread(void *unused)
{
    (void)unused;
    size_t n = 0;
    const char *feed = getenv("MOON_UPDATE_FEED");
    const char *jsonurl = feed && *feed ? feed
        : "https://api.github.com/repos/Jonhline/MoonEngine/releases/latest";
    unsigned char *json = fetch(jsonurl, 1 << 20, &n);
    if (!json) { snprintf(lup_status, sizeof lup_status, "Update check failed."); lup_state = LUP_ERROR; return NULL; }
    fprintf(stderr, "update: state checking\n"); fflush(stderr);
    if (!app_update_parse_release((char *)json, UPDATE_ASSET_SUFFIX, &lup_release)) {
        free(json); snprintf(lup_status, sizeof lup_status, "No update for this platform."); lup_state = LUP_NONE; return NULL;
    }
    free(json);
    if (!newer(lup_release.version, MOON_VERSION)) {
        snprintf(lup_status, sizeof lup_status, "Moon Engine is up to date."); lup_state = LUP_NONE; return NULL;
    }
    fprintf(stderr, "update: state available %s\n", lup_release.version); fflush(stderr);
    snprintf(lup_status, sizeof lup_status, "Update %s is available.", lup_release.version);
    lup_state = LUP_AVAILABLE;
    return NULL;
}
static void *download_thread(void *unused)
{
    (void)unused;
    size_t n = 0;
    lup_state = LUP_DOWNLOADING;
    fprintf(stderr, "update: state downloading 0%%\n"); fflush(stderr);
    unsigned char *tar = fetch(lup_release.url, (size_t)lup_release.size, &n);
    if (!tar || n != lup_release.size) { free(tar); snprintf(lup_status, sizeof lup_status, "Update download failed."); lup_state = LUP_ERROR; return NULL; }
    char tmp[] = "/tmp/moon-update-XXXXXX";
    int fd = mkstemp(tmp);
    int ok = 0;
    if (fd >= 0) {
        size_t w = write(fd, tar, n);
        close(fd);
        ok = w == n && sha256_file(tmp, lup_release.sha256);
        unlink(tmp);
    }
    if (!ok) { free(tar); snprintf(lup_status, sizeof lup_status, "Update digest mismatch."); lup_state = LUP_ERROR; return NULL; }
    fprintf(stderr, "update: state downloading 100%%\n"); fflush(stderr);
    if (!tar_unpack(tar, n)) { free(tar); snprintf(lup_status, sizeof lup_status, "Update unpack failed."); lup_state = LUP_ERROR; return NULL; }
    free(tar);
    snprintf(lup_status, sizeof lup_status, "Update %s ready; it applies on exit.", lup_release.version);
    lup_state = LUP_READY;
    fprintf(stderr, "update: state ready %s\n", lup_release.version); fflush(stderr);
    return NULL;
}
