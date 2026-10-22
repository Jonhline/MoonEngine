#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "discord_presence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DP_PAYLOAD_MAX 2048
#define DP_FRAME_MAX (DP_PAYLOAD_MAX + 8)
#define DP_BACKOFF_MS 60000ull
#define DP_FLOOR_MS 15000ull
#define DP_KEEPALIVE_MS 60000ull

int discord_presence_encode_frame(uint32_t opcode, const char *json, size_t json_len,
                                  unsigned char *out, size_t cap, size_t *out_len)
{
    if (!out || !out_len || (!json && json_len) || json_len > DP_PAYLOAD_MAX ||
        cap < json_len + 8)
        return 0;
    out[0] = (unsigned char)(opcode & 0xff);
    out[1] = (unsigned char)((opcode >> 8) & 0xff);
    out[2] = (unsigned char)((opcode >> 16) & 0xff);
    out[3] = (unsigned char)((opcode >> 24) & 0xff);
    out[4] = (unsigned char)(json_len & 0xff);
    out[5] = (unsigned char)((json_len >> 8) & 0xff);
    out[6] = (unsigned char)((json_len >> 16) & 0xff);
    out[7] = (unsigned char)((json_len >> 24) & 0xff);
    if (json_len)
        memcpy(out + 8, json, json_len);
    *out_len = json_len + 8;
    return 1;
}

#if !defined(__SWITCH__) && !defined(__ANDROID__)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define DP_INVALID_FD INVALID_HANDLE_VALUE
typedef HANDLE dp_fd_t;
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#define DP_INVALID_FD (-1)
typedef int dp_fd_t;
#endif

typedef struct {
    int enabled;
    int connected;
    int handshaked;
    int ready;
    int has_last;
    int reported;
    dp_fd_t fd;
    char app_id[128];
    char last_activity[DP_PAYLOAD_MAX];
    unsigned char rx[8192];
    size_t rx_len;
    uint64_t disabled_until_ms;
    uint64_t last_send_ms;
    uint64_t session_start;
} dp_state;

static dp_state dp = {.fd = DP_INVALID_FD};

#ifdef MOON_DISCORD_PRESENCE_TEST
static char test_search_dir[512];
static int test_search_dir_set;
#endif

static uint64_t dp_now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)(ts.tv_nsec / 1000000);
#endif
}

static unsigned long dp_pid(void)
{
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static uint64_t dp_rng = 0x9e3779b97f4a7c15ull;

static void dp_seed(void)
{
    if (!dp_rng)
        dp_rng = 0x9e3779b97f4a7c15ull;
    dp_rng ^= (uint64_t)time(NULL) ^ ((uint64_t)dp_pid() << 32) ^ dp_now_ms();
    if (!dp_rng)
        dp_rng = 1;
}

static uint64_t dp_random(void)
{
    uint64_t x = dp_rng;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    dp_rng = x;
    return x;
}

static void dp_uuid(char out[37])
{
    unsigned char b[16];
    for (int i = 0; i < 16; i++)
        b[i] = (unsigned char)(dp_random() >> 24);
    b[6] = (unsigned char)((b[6] & 0x0f) | 0x40);
    b[8] = (unsigned char)((b[8] & 0x3f) | 0x80);
    snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12],
             b[13], b[14], b[15]);
}

static int dp_escape(char *out, size_t cap, const char *in)
{
    size_t n = 0;
    if (!in)
        in = "";
    while (*in) {
        unsigned char c = (unsigned char)*in++;
        char tmp[8], *rep = tmp;
        if (c == '"')
            rep = "\\\"";
        else if (c == '\\')
            rep = "\\\\";
        else if (c == '\b')
            rep = "\\b";
        else if (c == '\f')
            rep = "\\f";
        else if (c == '\n')
            rep = "\\n";
        else if (c == '\r')
            rep = "\\r";
        else if (c == '\t')
            rep = "\\t";
        else if (c < 0x20) {
            snprintf(tmp, sizeof tmp, "\\u%04x", c);
        } else {
            tmp[0] = (char)c;
            tmp[1] = 0;
        }
        size_t l = strlen(rep);
        if (l >= cap - n)
            return 0;
        memcpy(out + n, rep, l);
        n += l;
    }
    out[n] = 0;
    return 1;
}

static int dp_puts(char *out, size_t cap, size_t *n, const char *s)
{
    size_t l = strlen(s);
    if (l >= cap - *n)
        return 0;
    memcpy(out + *n, s, l);
    *n += l;
    out[*n] = 0;
    return 1;
}

static int dp_slug(char *out, size_t cap, const char *in)
{
    size_t n = 0;
    int dash = 0;
    if (!in)
        in = "";
    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        int alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z');
        if (alnum) {
            if (n + 1 >= cap)
                return 0;
            out[n++] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
            dash = 0;
        } else if (n && !dash) {
            if (n + 1 >= cap)
                return 0;
            out[n++] = '-';
            dash = 1;
        }
    }
    while (n && out[n - 1] == '-')
        n--;
    out[n] = 0;
    return 1;
}

static int dp_build_activity(const gen_presence *p, char *out, size_t cap, uint64_t start)
{
    char course[768], mode[192], car[512], slug[192], details[1100], state[512];
    size_t dl = 0;
    int has_course = p && p->course && *p->course;
    if (!dp_escape(course, sizeof course, has_course ? p->course : "") ||
        !dp_escape(mode, sizeof mode, p && p->mode ? p->mode : "") ||
        !dp_escape(car, sizeof car, p && p->car ? p->car : ""))
        return 0;
    details[0] = state[0] = 0;
    if (mode[0] && !dp_puts(details, sizeof details, &dl, mode))
        return 0;
    if (has_course) {
        if (dl && !dp_puts(details, sizeof details, &dl, " \xc2\xb7 "))
            return 0;
        if (!dp_puts(details, sizeof details, &dl, course))
            return 0;
    }
    snprintf(state, sizeof state, "%s", car);
    int w;
    if (has_course) {
        if (!dp_slug(slug, sizeof slug, p->course))
            return 0;
        w = snprintf(
            out, cap,
            "{\"details\":\"%s\",\"state\":\"%s\",\"timestamps\":{\"start\":%llu},"
            "\"assets\":{\"large_image\":\"track-%s\",\"large_text\":\"%s\","
            "\"small_image\":\"moonengine\",\"small_text\":\"Moon Engine\"},"
            "\"instance\":false}",
            details, state, (unsigned long long)start, slug, course);
    } else {
        w = snprintf(
            out, cap,
            "{\"details\":\"%s\",\"state\":\"%s\",\"timestamps\":{\"start\":%llu},"
            "\"assets\":{\"large_image\":\"moonengine\",\"large_text\":\"Moon Engine\"},"
            "\"instance\":false}",
            details, state, (unsigned long long)start);
    }
    return w > 0 && (size_t)w < cap;
}

static int dp_build_payload(const gen_presence *p, char *out, size_t cap)
{
    char uuid[37];
    dp_uuid(uuid);
    if (!p) {
        int w = snprintf(out, cap,
                         "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%s\",\"args\":{\"pid\":%lu,"
                         "\"activity\":null}}",
                         uuid, dp_pid());
        return w > 0 && (size_t)w < cap;
    }
    char activity[1600];
    if (!dp_build_activity(p, activity, sizeof activity, dp.session_start))
        return 0;
    int w = snprintf(out, cap,
                     "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%s\",\"args\":{\"pid\":%lu,"
                     "\"activity\":%s}}",
                     uuid, dp_pid(), activity);
    return w > 0 && (size_t)w < cap;
}

static void dp_close(void)
{
#ifdef _WIN32
    if (dp.fd != DP_INVALID_FD) {
        CloseHandle(dp.fd);
        dp.fd = DP_INVALID_FD;
    }
#else
    if (dp.fd >= 0) {
        close(dp.fd);
        dp.fd = -1;
    }
#endif
    dp.connected = 0;
    dp.handshaked = 0;
    dp.ready = 0;
    dp.rx_len = 0;
}

static void dp_fail(void)
{
    dp_close();
    dp.disabled_until_ms = dp_now_ms() + DP_BACKOFF_MS;
    if (!dp.reported) {
        dp.reported = 1;
        fprintf(stderr, "discord presence: unavailable, retrying in 60 s\n");
    }
}

static int dp_write_frame(uint32_t opcode, const char *json, size_t len)
{
    unsigned char frame[DP_FRAME_MAX];
    size_t frame_len = 0;
    if (!dp.connected || !discord_presence_encode_frame(opcode, json, len, frame, sizeof frame,
                                                        &frame_len))
        return 0;
#ifdef _WIN32
    OVERLAPPED ov;
    memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent)
        return 0;
    DWORD written = 0;
    BOOL ok = WriteFile(dp.fd, frame, (DWORD)frame_len, &written, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD state = WaitForSingleObject(ov.hEvent, 1);
        if (state == WAIT_OBJECT_0)
            ok = GetOverlappedResult(dp.fd, &ov, &written, FALSE);
        else {
            CancelIo(dp.fd);
            ok = FALSE;
        }
    }
    CloseHandle(ov.hEvent);
    return ok && written == (DWORD)frame_len;
#else
    ssize_t n = send(dp.fd, frame, frame_len, MSG_NOSIGNAL);
    return n == (ssize_t)frame_len;
#endif
}

static int dp_send_handshake(void)
{
    char id[256], json[384];
    if (!dp_escape(id, sizeof id, dp.app_id))
        return 0;
    int w = snprintf(json, sizeof json, "{\"v\":1,\"client_id\":\"%s\"}", id);
    if (w <= 0 || (size_t)w >= sizeof json)
        return 0;
    if (!dp_write_frame(0, json, (size_t)w))
        return 0;
    dp.handshaked = 1;
    return 1;
}

static int dp_mem_has(const unsigned char *data, size_t len, const char *needle)
{
    size_t n = strlen(needle);
    if (!n || len < n)
        return 0;
    for (size_t i = 0; i + n <= len; i++)
        if (!memcmp(data + i, needle, n))
            return 1;
    return 0;
}

static void dp_rx_append(const unsigned char *data, size_t len)
{
    if (len > sizeof dp.rx - dp.rx_len)
        len = sizeof dp.rx - dp.rx_len;
    if (!len)
        return;
    memcpy(dp.rx + dp.rx_len, data, len);
    dp.rx_len += len;
}

static int dp_process_rx(void)
{
    size_t off = 0;
    while (dp.rx_len - off >= 8) {
        uint32_t opcode = (uint32_t)dp.rx[off] | ((uint32_t)dp.rx[off + 1] << 8) |
                          ((uint32_t)dp.rx[off + 2] << 16) | ((uint32_t)dp.rx[off + 3] << 24);
        uint32_t len = (uint32_t)dp.rx[off + 4] | ((uint32_t)dp.rx[off + 5] << 8) |
                       ((uint32_t)dp.rx[off + 6] << 16) | ((uint32_t)dp.rx[off + 7] << 24);
        if (len > DP_PAYLOAD_MAX + 4096) {
            dp_fail();
            return 0;
        }
        if (dp.rx_len - off - 8 < len)
            break;
        const unsigned char *json = dp.rx + off + 8;
        if (opcode == 1 && dp_mem_has(json, len, "READY"))
            dp.ready = 1;
        else if (opcode == 3) {
            char pong[DP_PAYLOAD_MAX + 1];
            size_t n = len < sizeof pong - 1 ? len : sizeof pong - 1;
            memcpy(pong, json, n);
            pong[n] = 0;
            if (!dp_write_frame(4, pong, n)) {
                dp_fail();
                return 0;
            }
        } else if (opcode == 2) {
            dp_fail();
            return 0;
        }
        off += 8 + len;
    }
    if (off) {
        memmove(dp.rx, dp.rx + off, dp.rx_len - off);
        dp.rx_len -= off;
    }
    return 1;
}

static int dp_read_some(void)
{
    unsigned char buf[1024];
#ifdef _WIN32
    OVERLAPPED ov;
    memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!ov.hEvent)
        return 0;
    DWORD got = 0;
    BOOL ok = ReadFile(dp.fd, buf, (DWORD)sizeof buf, &got, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD state = WaitForSingleObject(ov.hEvent, 0);
        if (state == WAIT_OBJECT_0)
            ok = GetOverlappedResult(dp.fd, &ov, &got, FALSE);
        else {
            CancelIo(dp.fd);
            CloseHandle(ov.hEvent);
            return 1;
        }
    }
    CloseHandle(ov.hEvent);
    if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
            dp_fail();
            return 0;
        }
        return 1;
    }
    if (!got) {
        dp_fail();
        return 0;
    }
    dp_rx_append(buf, got);
#else
    ssize_t got = recv(dp.fd, buf, sizeof buf, MSG_DONTWAIT);
    if (got == 0) {
        dp_fail();
        return 0;
    }
    if (got < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return 1;
        dp_fail();
        return 0;
    }
    dp_rx_append(buf, (size_t)got);
#endif
    return dp_process_rx();
}

#ifndef _WIN32
static int dp_connect_unix(void)
{
#ifdef MOON_DISCORD_PRESENCE_TEST
    const char *dirs[1];
    int ndirs = 0;
    if (test_search_dir_set)
        dirs[ndirs++] = test_search_dir;
#else
    const char *dirs[5];
    int ndirs = 0;
#endif
#ifndef MOON_DISCORD_PRESENCE_TEST
    const char *candidates[] = {getenv("XDG_RUNTIME_DIR"), getenv("TMPDIR"), getenv("TMP"),
                                getenv("TEMP"), "/tmp"};
    const char *dir = NULL;
    for (unsigned i = 0; i < sizeof candidates / sizeof *candidates; i++) {
        struct stat st;
        if (candidates[i] && candidates[i][0] && !stat(candidates[i], &st) && S_ISDIR(st.st_mode)) {
            dir = candidates[i];
            break;
        }
    }
    if (dir)
        dirs[ndirs++] = dir;
#endif
    for (int d = 0; d < ndirs; d++) {
        for (int n = 0; n < 10; n++) {
            char path[600];
            int w = snprintf(path, sizeof path, "%s/discord-ipc-%d", dirs[d], n);
            if (w <= 0 || (size_t)w >= sizeof path)
                continue;
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0)
                continue;
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0)
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            fcntl(fd, F_SETFD, FD_CLOEXEC);
            struct sockaddr_un addr;
            size_t plen = strlen(path);
            if (plen >= sizeof addr.sun_path) {
                close(fd);
                continue;
            }
            memset(&addr, 0, sizeof addr);
            addr.sun_family = AF_UNIX;
            memcpy(addr.sun_path, path, plen + 1);
            int r = connect(fd, (struct sockaddr *)&addr, sizeof addr);
            if (r < 0 && errno == EINPROGRESS) {
                struct pollfd pfd = {.fd = fd, .events = POLLOUT};
                if (poll(&pfd, 1, 1) == 1) {
                    int err = 0;
                    socklen_t elen = sizeof err;
                    if (!getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen) && !err)
                        r = 0;
                }
            }
            if (r == 0) {
                dp.fd = fd;
                dp.connected = 1;
                dp.rx_len = 0;
                return 1;
            }
            close(fd);
        }
    }
    return 0;
}
#else
static int dp_connect_windows(void)
{
    for (int n = 0; n < 10; n++) {
        char path[128];
        snprintf(path, sizeof path, "\\\\?\\pipe\\discord-ipc-%d", n);
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_PIPE_BUSY)
                continue;
            continue;
        }
        dp.fd = h;
        dp.connected = 1;
        dp.rx_len = 0;
        return 1;
    }
    return 0;
}
#endif

static int dp_connect(void)
{
#ifdef _WIN32
    return dp_connect_windows();
#else
    return dp_connect_unix();
#endif
}

void discord_presence_configure(int enabled, const char *app_id)
{
    dp_close();
    memset(&dp, 0, sizeof dp);
    dp.fd = DP_INVALID_FD;
    dp.session_start = (uint64_t)time(NULL);
    dp_rng = 0x9e3779b97f4a7c15ull;
    dp_seed();
    if (app_id) {
        size_t n = strlen(app_id);
        if (n >= sizeof dp.app_id)
            n = sizeof dp.app_id - 1;
        memcpy(dp.app_id, app_id, n);
        dp.app_id[n] = 0;
    }
    dp.enabled = enabled && dp.app_id[0] != 0;
    if (enabled && !dp.enabled && !dp.reported) {
        dp.reported = 1;
        fprintf(stderr, "discord presence: integrations.discord_app_id is empty\n");
    }
}

void discord_presence_tick(const gen_presence *p)
{
    if (!dp.enabled)
        return;
    if (!p && !dp.connected)
        return;
    uint64_t now = dp_now_ms();
    if (now < dp.disabled_until_ms)
        return;

    if (!dp.connected) {
        if (!dp_connect() || !dp_send_handshake()) {
            dp_fail();
            return;
        }
    }
    if (!dp.ready) {
        if (!dp_read_some())
            return;
        if (!dp.ready)
            return;
    }

    char activity[DP_PAYLOAD_MAX];
    int live = p != NULL;
    if (live) {
        if (!dp_build_activity(p, activity, sizeof activity, dp.session_start))
            return;
    } else {
        snprintf(activity, sizeof activity, "null");
    }

    uint64_t since = dp.last_send_ms ? now - dp.last_send_ms : 0;
    int changed = !dp.has_last || strcmp(dp.last_activity, activity);
    if (dp.has_last) {
        if (changed && since < DP_FLOOR_MS)
            return;
        if (!changed && (!live || since < DP_KEEPALIVE_MS))
            return;
    }

    char payload[DP_PAYLOAD_MAX];
    if (!dp_build_payload(p, payload, sizeof payload))
        return;
    size_t len = strlen(payload);
    if (len > DP_PAYLOAD_MAX)
        return;
    if (!dp_write_frame(1, payload, len)) {
        dp_fail();
        return;
    }
    snprintf(dp.last_activity, sizeof dp.last_activity, "%s", activity);
    dp.has_last = 1;
    dp.last_send_ms = now;
}

void discord_presence_shutdown(void)
{
    if (dp.enabled && dp.connected && dp.ready) {
        char payload[DP_PAYLOAD_MAX];
        if (dp_build_payload(NULL, payload, sizeof payload)) {
            size_t len = strlen(payload);
            if (len <= DP_PAYLOAD_MAX)
                dp_write_frame(1, payload, len);
        }
    }
    dp_close();
    dp.enabled = 0;
}

#ifdef MOON_DISCORD_PRESENCE_TEST
void discord_presence_test_reset(void)
{
    dp_close();
    memset(&dp, 0, sizeof dp);
    dp.fd = DP_INVALID_FD;
    test_search_dir_set = 0;
    test_search_dir[0] = 0;
    dp_rng = 0x9e3779b97f4a7c15ull;
    dp_seed();
}

void discord_presence_test_set_fd(int fd, int ready)
{
    dp_close();
    dp.fd = fd;
    dp.connected = 1;
    dp.handshaked = 1;
    dp.ready = ready;
    dp.disabled_until_ms = 0;
}

void discord_presence_test_set_search_dir(const char *dir)
{
    test_search_dir_set = 1;
    snprintf(test_search_dir, sizeof test_search_dir, "%s", dir ? dir : "");
}

int discord_presence_test_connected(void)
{
    return dp.connected;
}
#endif

#else

void discord_presence_configure(int enabled, const char *app_id)
{
    (void)enabled;
    (void)app_id;
}

void discord_presence_tick(const gen_presence *p)
{
    (void)p;
}

void discord_presence_shutdown(void)
{
}

#endif
