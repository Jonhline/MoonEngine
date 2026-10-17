#ifndef MOON_OPTIONS_STORE_H
#define MOON_OPTIONS_STORE_H
/* P: shared PC preferences. No simulation code reads this store. */
typedef struct { char key[64], value[256]; } moon_option_entry;
typedef struct {
    moon_option_entry entries[128];
    int count, modified;
    char path[1024], preset[64];
} moon_options;
const char *moon_options_config_path(const char *config, const char *launcher_owned);
int moon_options_load(moon_options *s, const char *path, const char *legacy);
const char *moon_options_get(const moon_options *s, const char *key, const char *fallback);
int moon_options_int(const moon_options *s, const char *key, int fallback, int low, int high);
int moon_options_set(moon_options *s, const char *key, const char *value);
int moon_options_save(const moon_options *s);
void moon_options_preset(moon_options *s, int enhanced);
#endif
