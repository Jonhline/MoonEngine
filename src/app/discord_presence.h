#ifndef MOON_DISCORD_PRESENCE_H
#define MOON_DISCORD_PRESENCE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *course;
    const char *mode;
    const char *car;
    int cars;
    int lap, laps;
    int speed_kmh;
    int paused;
} gen_presence;

void discord_presence_configure(int enabled, const char *app_id);
void discord_presence_tick(const gen_presence *p);
void discord_presence_shutdown(void);

int discord_presence_encode_frame(uint32_t opcode, const char *json, size_t json_len,
                                  unsigned char *out, size_t cap, size_t *out_len);

#ifdef MOON_DISCORD_PRESENCE_TEST
void discord_presence_test_reset(void);
void discord_presence_test_set_fd(int fd, int ready);
void discord_presence_test_set_search_dir(const char *dir);
int discord_presence_test_connected(void);
#endif

#endif
