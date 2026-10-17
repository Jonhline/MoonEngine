/* Execute the disc's defaults; generated original code is test-only. */
#include "ee.h"
#include "pad_config.h"
#include "physics/fields_input.h"
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t pc);

static int compare_defaults(ee_ctx *context, int port, int negative)
{
    const uint32_t option_address = 0x1800000;
    const int native_axes[] = {0, 2, 6, 7, 10, 11};
    memset(context->r, 0, sizeof context->r);
    context->r[4].uw[0] = option_address;
    context->r[29].uw[0] = 0x1ef0000;
    jmp_buf jump;
    context->exit_jmp = &jump;
    if (setjmp(jump)) {
        fprintf(stderr, "original controller defaults: %s\n", context->end_detail);
        return 1;
    }
    context->lookup(0x43b808)(context);
    gt4_pad_config config;
    gt4_pad_defaults(&config, port);
    if (negative)
        config.analog[GT4_PAD_RYF] = -1;
    for (int axis = 0; axis < 6; axis++) {
        int native =
            (int8_t)
                context->ram[option_address + INPUT_DUALSHOCK_ANALOG_BINDINGS + native_axes[axis]];
        int expected = native < 0 ? -1 : (native == 0 ? -2 : native + 1);
        if (config.analog[axis] != expected) {
            fprintf(stderr, "port %d axis %d: native %d, host %d\n", port, axis, expected,
                    config.analog[axis]);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    ee_ctx context;
    if (ee_init(&context))
        return 2;
    context.lookup = recomp_lookup;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    size_t size = fread(context.ram + 0x100000, 1, EE_RAM_SIZE - 0x100000, file);
    fclose(file);
    if (size <= 0x33b8d8)
        return 2;
    for (int port = 0; port < 2; port++)
        if (compare_defaults(&context, port, argc > 2))
            return 1;
    ee_free(&context);
    puts("PASS original DualShock analog defaults: both ports, six axes");
    return 0;
}
