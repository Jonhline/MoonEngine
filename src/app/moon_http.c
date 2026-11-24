#include "moon_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
/* The Windows updater keeps its WinHTTP path; the launcher check is Linux-first. */
unsigned char *moon_http_get(const char *url, size_t limit, size_t *got)
{
    (void)url; (void)limit; (void)got;
    return NULL;
}
#else
#include <curl/curl.h>
typedef struct { unsigned char *p; size_t n, cap; } buf;
static size_t wr(void *data, size_t sz, size_t nm, void *user)
{
    buf *b = user; size_t n = sz * nm;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 65536;
        while (cap < b->n + n + 1) cap *= 2;
        unsigned char *p = realloc(b->p, cap);
        if (!p) return 0;
        b->p = p; b->cap = cap;
    }
    memcpy(b->p + b->n, data, n); b->n += n; b->p[b->n] = 0;
    return n;
}
static int host_ok(const char *u)
{
    return !strncmp(u, "https://github.com/", 19) ||
           !strncmp(u, "https://api.github.com/", 23) ||
           !strncmp(u, "https://objects.githubusercontent.com/", 39);
}
unsigned char *moon_http_get(const char *url, size_t limit, size_t *got)
{
    if (!url || !host_ok(url)) return NULL;
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    buf b = {0};
    struct curl_slist *h = curl_slist_append(NULL, "Accept: application/vnd.github+json");
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 15L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "MoonEngine-updater");
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, h);
    curl_easy_setopt(c, CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)limit);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, wr);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
    CURLcode r = curl_easy_perform(c);
    long code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(h); curl_easy_cleanup(c);
    if (r != CURLE_OK || code != 200 || b.n > limit) { free(b.p); return NULL; }
    *got = b.n;
    return b.p;
}
#endif
