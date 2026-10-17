/* Original: func_0035F808 +0x19C..0x1B8. Check both historical host
   arithmetic against its captures and EE arithmetic against original code.
   The PCSX2 snapshots retain the emulator's historical host multiplier. */
#include "car.h"
#include "fields_conductor.h"
#include "ee.h"

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
ee_fn recomp_lookup(uint32_t);
void legacy_gt4_surface_bumps(uint32_t);

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
    legacy_gt4_surface_bumps(conductor);
    if (memcmp(expected, ram + SURFACE_WAVES, sizeof expected)) {
        fprintf(stderr, "%s: historical capture/legacy arithmetic mismatch\n", path);
        free(ram);
        fesetenv(&environment);
        return 1;
    }
    ee_ctx original;
    if (ee_init(&original)) {
        free(ram);
        fesetenv(&environment);
        return 1;
    }
    memcpy(original.ram, ram, RAM_SIZE);
    original.lookup = recomp_lookup;
    original.r[4].uw[0] = conductor;
    original.r[29].uw[0] = 0x01ef0000;
    recomp_lookup(0x0035f808)(&original);
    if (original.end_reason != EE_END_NONE) {
        fprintf(stderr, "%s: original surface initializer stopped: %s\n", path,
                original.end_detail);
        ee_free(&original);
        free(ram);
        fesetenv(&environment);
        return 1;
    }
    memcpy(expected, original.ram + SURFACE_WAVES, sizeof expected);
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
    ee_free(&original);
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
    printf("PASS %d captures: legacy words retained, %d EE surface words match original each\n",
           argc - first, SURFACE_COUNT);
    return 0;
}
