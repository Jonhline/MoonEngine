#include "ee.h"
#include "license_image.h"
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t pc);
static void power(ee_ctx *context)
{
    context->fpr[0].f = powf(context->fpr[12].f, context->fpr[13].f);
}
static ee_fn boundary(uint32_t pc)
{
    return pc == 0x57d6c0 ? power : NULL;
}
int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    ee_ctx context;
    if (ee_init(&context))
        return 2;
    context.lookup = recomp_lookup;
    context.hook = boundary;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    size_t size = fread(context.ram + 0x100000, 1, EE_RAM_SIZE - 0x100000, file);
    fclose(file);
    if (size < 0x522f50)
        return 2;
    const uint32_t game = 0x1a00000, output = 0x1800000;
    memcpy(context.ram + 0x622f4c, &game, 4);
    int options[2][4] = {{3, 2, 1, -1}, {-2, -1, -2, 1}};
    for (int mode = 0; mode < 2; mode++)
        memcpy(context.ram + game + (mode ? 0x39e44 : 0x39e34), options[mode], 16);
    fesetround(FE_TOWARDZERO);
    for (int mode = 0; mode < 2; mode++) {
        memset(context.r, 0, sizeof context.r);
        context.r[4].uw[0] = output;
        context.r[29].uw[0] = 0x1ef0000;
        jmp_buf jump;
        context.exit_jmp = &jump;
        if (setjmp(jump)) {
            fprintf(stderr, "display original stopped: %s\n", context.end_detail);
            return 1;
        }
        context.lookup(mode ? 0x3efee8 : 0x3efea8)(&context);
        float actual[6];
        gt4_license_image_options(options[mode], actual);
        if (argc > 2)
            actual[4] = 1;
        if (memcmp(actual, context.ram + output, sizeof actual)) {
            fprintf(stderr, "race/replay display parameters differ from original\n");
            return 1;
        }
    }
    ee_free(&context);
    puts("PASS original race/replay display option selection and six parameters");
    return 0;
}
