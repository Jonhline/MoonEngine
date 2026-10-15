/* Native storage adaptation; see libnx fsdev_rename -> fsFsRenameFile.
   Keep a rollback name instead of unlinking the player's destination. This
   is recoverable replacement, not a promise of power-loss atomicity on SD. */
#include "platform/switch_file.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#ifdef OPENGT4_SWITCH_FILE_TEST
extern int switch_test_rename(const char *, const char *);
extern int switch_test_commit(const char *);
extern uint64_t switch_test_ticks(void);
#define MOVE switch_test_rename
#define COMMIT switch_test_commit
#define TICKS switch_test_ticks
#else
#include <switch.h>
#define MOVE rename
#define COMMIT(path) (R_SUCCEEDED(fsdevCommitDevice(path)) ? 0 : -1)
#define TICKS armGetSystemTick
#endif

static int present(const char *path)
{
    struct stat info;
    if (stat(path, &info) == 0)
        return 1;
    return errno == ENOENT ? 0 : -1;
}

int plat_switch_temp_path(const char *destination, char *out, size_t capacity)
{
    /* getpid() is unavailable on libnx (-1). Never truncate a temporary save
       left by the previous process; a failed install may contain newer progress. */
    uint64_t stamp = TICKS();
    for (unsigned i = 0; i < 64; i++) {
        if (snprintf(out, capacity, "%s.tmp.%llu", destination, (unsigned long long)(stamp + i)) >=
            (int)capacity)
            return 0;
        int exists = present(out);
        if (exists < 0)
            return 0;
        if (!exists)
            return 1;
    }
    errno = EEXIST;
    return 0;
}

static int rollback_name(const char *path, char *old, size_t capacity)
{
    if (!path || snprintf(old, capacity, "%s.replace-old", path) >= (int)capacity) {
        errno = ENAMETOOLONG;
        return 0;
    }
    return 1;
}

int plat_switch_recover_file(const char *destination)
{
    char old[1250];
    if (!rollback_name(destination, old, sizeof old))
        return 0;
    int current = present(destination), saved = present(old);
    if (current < 0 || saved < 0)
        return 0;
    if (!current && saved)
        return MOVE(old, destination) == 0 && COMMIT(destination) == 0;
    return 1;
}

int plat_switch_replace_file(const char *source, const char *destination)
{
    char old[1250];
    if (!source || !destination || !strcmp(source, destination) || present(source) != 1 ||
        !rollback_name(destination, old, sizeof old) || !plat_switch_recover_file(destination))
        return 0;
    int current = present(destination), saved = present(old);
    if (current < 0 || saved < 0)
        return 0;
    /* A completed prior installation can leave its rollback name after a
       cleanup failure. The current destination stays intact while clearing it. */
    if (saved && remove(old) != 0)
        return 0;
    if (current && (MOVE(destination, old) != 0 || COMMIT(destination) != 0)) {
        plat_switch_recover_file(destination);
        return 0;
    }
    if (MOVE(source, destination) != 0) {
        int failure = errno;
        if (current)
            plat_switch_recover_file(destination);
        errno = failure;
        return 0;
    }
    if (COMMIT(destination) != 0) {
        /* Retain the new complete bytes at source and restore the old name.
           If either move fails, both versions remain recoverable on SD. */
        if (MOVE(destination, source) == 0 && current)
            plat_switch_recover_file(destination);
        return 0;
    }
    if (current)
        remove(old); /* Cleanup failure does not invalidate the installed file. */
    return 1;
}
