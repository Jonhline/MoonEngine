#ifndef OPENGT4_LAUNCHER_UI_H
#define OPENGT4_LAUNCHER_UI_H
#include "launcher.h"

/* The MOON ENGINE front door: our own window, our own look, no game artwork and no
   disc access. Runs before the engine's video window exists, fills the same
   launcher_startup the old screen filled, and returns 1 to run the game,
   0 on cancel, -1 on failure. Portable: SDL only, so Linux, Windows and (later)
   Android share one implementation. Entered with --launcher --launcher-v2. */
int launcher_ui_run(launcher_startup *startup);
#endif
