#define _GNU_SOURCE
#include "discord_presence.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

static void read_full(int fd, void *buf, size_t len)
{
    unsigned char *p = (unsigned char *)buf;
    while (len) {
        ssize_t n = read(fd, p, len);
        assert(n > 0);
        p += n;
        len -= (size_t)n;
    }
}

static size_t read_frame(int fd, char *json, size_t cap)
{
    unsigned char header[8];
    read_full(fd, header, sizeof header);
    uint32_t opcode = (uint32_t)header[0] | ((uint32_t)header[1] << 8) |
                      ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);
    uint32_t len = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
                   ((uint32_t)header[6] << 16) | ((uint32_t)header[7] << 24);
    assert(opcode == 1);
    assert(len > 0 && len < cap);
    read_full(fd, json, len);
    json[len] = 0;
    return len;
}

static void encoder_test(void)
{
    int sv[2];
    char json[4096];
    size_t len;

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    discord_presence_test_reset();
    discord_presence_configure(1, "123456789012345678");
    discord_presence_test_set_fd(sv[0], 1);

    gen_presence p = {0};
    p.course = "Tsukuba Circuit";
    p.mode = "Arcade";
    p.car = "Honda NSX";
    p.cars = 5;
    p.lap = 3;
    p.laps = 5;
    p.speed_kmh = 312;
    p.paused = 1;
    discord_presence_tick(&p);

    len = read_frame(sv[1], json, sizeof json);
    assert(len == strlen(json));
    assert(strstr(json, "\"cmd\":\"SET_ACTIVITY\""));
    assert(strstr(json, "\"nonce\":\""));
    assert(strstr(json, "\"pid\":"));
    assert(strstr(json, "\"details\":\"Arcade \xc2\xb7 Tsukuba Circuit\""));
    assert(strstr(json, "\"state\":\"Honda NSX\""));
    assert(strstr(json, "\"timestamps\":{\"start\":"));
    assert(strstr(json, "\"large_image\":\"track-tsukuba-circuit\""));
    assert(strstr(json, "\"large_text\":\"Tsukuba Circuit\""));
    assert(strstr(json, "\"small_image\":\"moonengine\""));
    assert(strstr(json, "\"small_text\":\"Moon Engine\""));
    assert(strstr(json, "\"instance\":false"));
    assert(!strstr(json, "Lap"));
    assert(!strstr(json, "km/h"));
    assert(!strstr(json, "cars"));
    assert(!strstr(json, "Paused"));

    discord_presence_test_reset();
    close(sv[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    discord_presence_test_reset();
    discord_presence_configure(1, "123456789012345678");
    discord_presence_test_set_fd(sv[0], 1);
    p.course = "Tsu\"kuba\nline";
    p.mode = "GT\\Mode";
    p.cars = 0;
    p.lap = 0;
    p.laps = 0;
    p.speed_kmh = -1;
    p.paused = 0;
    discord_presence_tick(&p);
    len = read_frame(sv[1], json, sizeof json);
    assert(len == strlen(json));
    assert(strstr(json, "\"details\":\"GT\\\\Mode \xc2\xb7 Tsu\\\"kuba\\nline\""));
    assert(strstr(json, "\"state\":\"Honda NSX\""));
    assert(strstr(json, "\"large_image\":\"track-tsu-kuba-line\""));
    assert(strstr(json, "\"large_text\":\"Tsu\\\"kuba\\nline\""));
    assert(strstr(json, "\"small_image\":\"moonengine\""));
    discord_presence_test_reset();
    close(sv[1]);

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    discord_presence_test_reset();
    discord_presence_configure(1, "123456789012345678");
    discord_presence_test_set_fd(sv[0], 1);
    p.course = NULL;
    p.mode = "GT Mode";
    p.car = "Toyota Corolla";
    discord_presence_tick(&p);
    len = read_frame(sv[1], json, sizeof json);
    assert(len == strlen(json));
    assert(strstr(json, "\"details\":\"GT Mode\""));
    assert(strstr(json, "\"state\":\"Toyota Corolla\""));
    assert(strstr(json, "\"large_image\":\"moonengine\""));
    assert(strstr(json, "\"large_text\":\"Moon Engine\""));
    assert(strstr(json, "\"timestamps\":{\"start\":"));
    assert(!strstr(json, "small_image"));
    assert(!strstr(json, "small_text"));
    assert(!strstr(json, "track-"));
    discord_presence_test_reset();
    close(sv[1]);

    /* Menus before a car is chosen: Discord rejects an empty state field, so
       the field must be absent, not "state":"" . */
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    discord_presence_test_reset();
    discord_presence_configure(1, "123456789012345678");
    discord_presence_test_set_fd(sv[0], 1);
    p.course = NULL;
    p.mode = "Save";
    p.car = NULL;
    discord_presence_tick(&p);
    len = read_frame(sv[1], json, sizeof json);
    assert(len == strlen(json));
    assert(strstr(json, "\"details\":\"Save\""));
    assert(!strstr(json, "\"state\""));
    assert(strstr(json, "\"large_image\":\"moonengine\""));
    discord_presence_test_reset();
    close(sv[1]);
}

static void negative_test(void)
{
    char dir[] = "/tmp/moon-discord-pressence-XXXXXX";
    assert(mkdtemp(dir));
    discord_presence_test_reset();
    discord_presence_configure(1, "123456789012345678");
    discord_presence_test_set_search_dir(dir);

    gen_presence p = {0};
    p.course = "Tsukuba";
    p.mode = "Arcade";
    p.cars = 5;
    p.speed_kmh = 100;

    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    discord_presence_tick(&p);
    clock_gettime(CLOCK_MONOTONIC, &b);
    uint64_t ns = (uint64_t)(b.tv_sec - a.tv_sec) * 1000000000ull +
                  (uint64_t)(b.tv_nsec - a.tv_nsec);
    assert(ns < 1000000ull);
    assert(!discord_presence_test_connected());
    rmdir(dir);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--negative")) {
        negative_test();
        puts("PASS: no Discord socket, tick under 1 ms, nothing sent.");
        return 0;
    }
    encoder_test();
    puts("PASS: SET_ACTIVITY frame opcode, length, JSON fields and escaping.");
    return 0;
}
