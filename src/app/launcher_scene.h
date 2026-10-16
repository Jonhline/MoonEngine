#ifndef MOON_LAUNCHER_SCENE_H
#define MOON_LAUNCHER_SCENE_H
#include <stdint.h>
enum {
    LAUNCHER_WIDTH = 1244,
    LAUNCHER_HEIGHT = 700,
    LAUNCHER_PANEL_X = 172,
    LAUNCHER_INTERVAL_MS = 16,
    LAUNCHER_SHOT_SECONDS = 12,
    LAUNCHER_SHOTS = 18
};
typedef struct launcher_scene launcher_scene;
/* P: private live preview using the native renderer and the user's disc. */
launcher_scene *launcher_scene_open(const char *disc, const char *saves, int backend, int fixed);
void launcher_scene_close(launcher_scene *scene);
int launcher_scene_copy(launcher_scene *scene, uint32_t *pixels, int selection);
int launcher_scene_frames(const launcher_scene *scene);
const char *launcher_scene_caption(const launcher_scene *scene);
void launcher_scene_report(const launcher_scene *scene);
void launcher_scene_pause(launcher_scene *scene, int paused);
void launcher_scene_seek(launcher_scene *scene, float seconds);
struct gt4_vol;
int launcher_scene_saved_vehicle(const char *saves, struct gt4_vol *vol, char code[96], int *color,
                                 char wheel[32], char wing[32]);
int launcher_scene_saved_car(const char *saves, char code[96], int *color);
float launcher_scene_transition(float seconds);
void launcher_scene_camera(float seconds, int fixed, float look[9], float *fov);
/* -1 for an ordinary invocation; otherwise the preview worker's exit code. */
int launcher_scene_entry(int argc, char **argv);
#endif
