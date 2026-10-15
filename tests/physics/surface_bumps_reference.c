/* Original: func_0035F808 +0x1B8. Captured surface wave numbers are
   independent of the native/oracle host divider and are compared as bits. */
#include "car.h"
#include "fields_conductor.h"

#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    RAM_SIZE = 32 * 1024 * 1024,
    RACE_POINTER = 0x00621cb4,
    SURFACE_WAVES = 0x00620a44,
    SURFACE_COUNT = 24
};

static int check_capture(const char *path, int negative)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    uint8_t *ram = malloc(RAM_SIZE);
    if (!ram) {
        fclose(file);
        return 1;
    }
    size_t size = fread(ram, 1, RAM_SIZE, file);
    int tail = fgetc(file);
    fclose(file);
    if (size != RAM_SIZE || tail != EOF) {
        free(ram);
        return 1;
    }
    gt4_phys_mem = ram;
    uint32_t race = GT4_U32(RACE_POINTER);
    if (race > RAM_SIZE - 0x100 || !race) {
        free(ram);
        return 1;
    }
    uint32_t world = GT4_U32(race + 0x84);
    if (world > RAM_SIZE - 0x78 || !world) {
        free(ram);
        return 1;
    }
    uint32_t conductor = GT4_U32(world + 0x70);
    if (conductor > RAM_SIZE - 0x10160 || !conductor) {
        free(ram);
        return 1;
    }
    uint32_t expected[SURFACE_COUNT];
    memcpy(expected, ram + SURFACE_WAVES, sizeof expected);
    fenv_t environment;
    fegetenv(&environment);
    gt4_phys_fpu_enter();
    gt4_surface_bumps(conductor);
    fesetenv(&environment);
    if (negative)
        GT4_U32(SURFACE_WAVES) ^= 1;
    int different = 0;
    for (unsigned surface = 0; surface < SURFACE_COUNT; surface++) {
        uint32_t actual = GT4_U32(SURFACE_WAVES + surface * 4);
        if (actual != expected[surface]) {
            fprintf(stderr, "%s surface=%u original=%08x native=%08x\n", path, surface,
                    expected[surface], actual);
            different = 1;
        }
    }
    free(ram);
    gt4_phys_mem = NULL;
    return different;
}

int main(int argc, char **argv)
{
    int first = argc > 1 && !strcmp(argv[1], "--negative") ? 2 : 1;
    if (argc <= first)
        return 1;
    for (int i = first; i < argc; i++)
        if (check_capture(argv[i], first == 2))
            return 1;
    printf("PASS %d captures, %d original surface wave numbers each\n", argc - first,
           SURFACE_COUNT);
    return 0;
}
