#include "moon_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
static int host_ok(const char *u)
{
#if MOON_DEV_BUILD
    if (getenv("MOON_UPDATE_FAKE") && !strncmp(u, "http://127.0.0.1:", 17)) return 1;
#endif
    return !strncmp(u, "https://github.com/", 19) ||
           !strncmp(u, "https://api.github.com/", 23) ||
           !strncmp(u, "https://objects.githubusercontent.com/", 39);
}
unsigned char *moon_http_get(const char *url, size_t limit, size_t *got)
{
    if (!url || !host_ok(url)) return NULL;
    wchar_t wurl[1100], host[256], path[1024];
    if (!MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, 1100)) return NULL;
    URL_COMPONENTS u = {0};
    u.dwStructSize = sizeof u;
    u.lpszHostName = host; u.dwHostNameLength = 256;
    u.lpszUrlPath = path; u.dwUrlPathLength = 1024;
    u.dwUserNameLength = u.dwPasswordLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wurl, 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS) return NULL;
    HINTERNET session = WinHttpOpen(L"MoonEngine-updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0),
              connection = NULL, r = NULL;
    unsigned char *data = NULL;
    size_t n = 0;
    int ok = 0;
    if (!session) return NULL;
    WinHttpSetTimeouts(session, 5000, 5000, 15000, 30000);
    connection = WinHttpConnect(session, host, u.nPort, 0);
    if (connection)
        r = WinHttpOpenRequest(connection, L"GET", path, NULL, WINHTTP_NO_REFERER,
                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (r) {
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
        WinHttpSetOption(r, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);
        DWORD status = 0, sn = sizeof status;
        if (WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(r, NULL) &&
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL,
                                &status, &sn, NULL) && status == 200 && (data = malloc(limit + 1))) {
            ok = 1;
            for (;;) {
                DWORD k = 0;
                if (n >= limit) { unsigned char probe; if (WinHttpReadData(r, &probe, 1, &k) && k) ok = 0; break; }
                if (!WinHttpReadData(r, data + n, (DWORD)(limit - n < 65536 ? limit - n : 65536), &k)) { ok = 0; break; }
                if (!k) break;
                n += k;
            }
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!ok) { free(data); return NULL; }
    data[n] = 0;
    *got = n;
    return data;
}
#else
#include <curl/curl.h>
static volatile int up_progress = -1;
int moon_http_progress(void) { return up_progress; }
void moon_http_reset_progress(void) { up_progress = -1; }
static int xfer(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    (void)p; (void)ultotal; (void)ulnow;
    if (dltotal > 0) {
        int pct = (int)(dlnow * 100 / dltotal), old = up_progress;
        up_progress = pct;
        (void)old;
    }
    else if (dlnow > 0) up_progress = -1;
    return 0;
}
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
#if MOON_DEV_BUILD
    if (getenv("MOON_UPDATE_FAKE") && !strncmp(u, "http://127.0.0.1:", 17)) return 1;
#endif
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
    moon_http_reset_progress();
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xfer);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
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
