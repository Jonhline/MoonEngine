/* Exercise real profile storage with fsdev's non-overwriting rename model. */
#include "platform/switch_file.h"
#include "save.h"
#include "../test_process.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0700)
#endif

static int fail_install, fail_rollback, fail_commit, negative;
uint64_t switch_test_ticks(void)
{
    return 42;
}
int switch_test_commit(const char *path)
{
    (void)path;
    if (fail_commit && --fail_commit == 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}
int switch_test_rename(const char *from, const char *to)
{
    struct stat info;
    if (stat(to, &info) == 0) {
        errno = EEXIST;
        return -1;
    }
    if ((negative && strstr(to, ".replace-old")) || (fail_install && strstr(from, ".tmp")) ||
        (fail_rollback && strstr(from, ".replace-old"))) {
        if (strstr(from, ".tmp"))
            fail_install = 0;
        errno = EIO;
        return -1;
    }
    return rename(from, to);
}
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Switch save line %d: %s (%s)\n", __LINE__, #x, gt4_save_error());     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int put(const char *path, const char *value)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return 0;
    int ok = fwrite(value, 1, strlen(value), file) == strlen(value);
    return fclose(file) == 0 && ok;
}
static int equals(const char *path, const char *value)
{
    char bytes[64] = {0};
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    size_t n = fread(bytes, 1, sizeof bytes, file);
    fclose(file);
    return n == strlen(value) && !memcmp(bytes, value, n);
}

int main(int argc, char **argv)
{
    CHECK(argc==2 || argc==3);
    negative = argc == 3 && !strcmp(argv[2], "--negative");
    CHECK(MKDIR(argv[1])==0 || errno==EEXIST);
    char directory[1200];
    CHECK(snprintf(directory, sizeof directory, "%s/run-%lu", argv[1], test_process_id()) < sizeof directory);
    CHECK(MKDIR(directory)==0 || errno==EEXIST);
    argv[1] = directory;
    char destination[1200], source[1200], old[1250], legacy[1250];
    snprintf(destination, sizeof destination, "%s/replace.ini", argv[1]);
    snprintf(source, sizeof source, "%s/replace.ini.tmp", argv[1]);
    snprintf(old, sizeof old, "%s.replace-old", destination);
    remove(destination);
    remove(source);
    remove(old);
    CHECK(put(destination,"before") && put(source,"after"));
    fail_install = 1;
    CHECK(!plat_switch_replace_file(source,destination));
    CHECK(equals(destination,"before") && equals(source,"after"));
    CHECK(plat_switch_replace_file(source,destination));
    CHECK(equals(destination,"after"));
    CHECK(put(source,"next"));
    fail_commit = 1;
    CHECK(!plat_switch_replace_file(source,destination));
    CHECK(equals(destination,"after") && equals(source,"next"));
    fail_commit = 2;
    CHECK(!plat_switch_replace_file(source,destination));
    CHECK(equals(destination,"after") && equals(source,"next"));
    fail_install = fail_rollback = 1;
    CHECK(!plat_switch_replace_file(source,destination));
    CHECK(equals(old,"after") && equals(source,"next"));
    fail_rollback = 0;
    CHECK(plat_switch_recover_file(destination));
    CHECK(equals(destination,"after"));
    CHECK(plat_switch_replace_file(source,destination));
    CHECK(equals(destination,"next"));
    /* Interrupted promotion: both complete versions exist; a later write is safe. */
    CHECK(put(old,"older") && put(source,"latest"));
    CHECK(plat_switch_replace_file(source,destination));
    CHECK(equals(destination,"latest"));
    char first[1200], second[1200];
    CHECK(plat_switch_temp_path(destination,first,sizeof first));
    CHECK(put(first,"unfinished"));
    CHECK(plat_switch_temp_path(destination,second,sizeof second));
    CHECK(strcmp(first,second) && equals(first,"unfinished"));

    for (int slot = 1; slot <= GT4_SAVE_SLOTS; slot++) {
        char path[1200];
        snprintf(path, sizeof path, "%s/slot%02d.ogt4", argv[1], slot);
        remove(path);
        snprintf(old, sizeof old, "%s.bak", path);
        remove(old);
    }
    snprintf(destination, sizeof destination, "%s/active.ogt4", argv[1]);
    remove(destination);
    snprintf(old, sizeof old, "%s.bak", destination);
    remove(old);
    gt4_save_init(argv[1]);
    CHECK(gt4_save_create(1,"Switch driver"));
    gt4_save_shutdown();
    gt4_save_init(argv[1]);
    CHECK(gt4_save_active()==1 && !strcmp(gt4_save_player(),"Switch driver"));
    snprintf(destination, sizeof destination, "%s/slot01.ogt4", argv[1]);
    snprintf(legacy, sizeof legacy, "%s.tmp.4294967295", destination);
    CHECK(put(legacy,"retained legacy progress"));
    for (int i = 0; i < 3; i++) {
        CHECK(gt4_save_rename(i==2?"Returned":"Updated"));
        gt4_save_shutdown();
        gt4_save_init(argv[1]);
        CHECK(gt4_save_active()==1);
    }
    CHECK(!strcmp(gt4_save_player(),"Returned"));
    CHECK(equals(legacy,"retained legacy progress"));
    fail_install = 1;
    CHECK(!gt4_save_rename("Lost"));
    CHECK(!strcmp(gt4_save_player(),"Returned"));
    gt4_save_shutdown();
    gt4_save_init(argv[1]);
    CHECK(gt4_save_active()==1 && !strcmp(gt4_save_player(),"Returned"));
    gt4_save_shutdown();
    snprintf(old, sizeof old, "%s.replace-old", destination);
    CHECK(switch_test_rename(destination, old)==0);
    gt4_save_init(argv[1]);
    CHECK(gt4_save_active()==1 && !strcmp(gt4_save_player(),"Returned"));
    gt4_save_shutdown();
    return 0;
}
