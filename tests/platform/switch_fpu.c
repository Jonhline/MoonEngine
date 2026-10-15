#include "switch_fpu.h"
#include "phys.h"
#include <fenv.h>
#include <switch.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    consoleInit(NULL);
    gt4_phys_fpu_enter();
    if (argc > 1 && !strcmp(argv[1], "--negative"))
        fesetround(FE_TONEAREST);
    int ok = opengt4_switch_fpu_check();
    printf("OpenGT4 ARM FPU: %s\nPress + to exit.\n", ok ? "PASS" : "FAIL");
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return ok ? 0 : 1;
}
