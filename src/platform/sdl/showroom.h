#ifndef MOON_SHOWROOM_H
#define MOON_SHOWROOM_H
#include <stdint.h>
typedef struct showroom showroom;
showroom *showroom_open(const char *disc);
/* The same studio scene with the seamless white room instead of bg.mdl's layers (--studio). */
showroom *showroom_open_studio(const char *disc);
void showroom_close(showroom *s);
void showroom_set_wing(showroom *s, const char *wing);
void showroom_set_wheel(showroom *s, const char *wheel);
int showroom_draw(showroom *s, const char *code, float dt);
/* Request/load only: 1 ready, 0 pending, -1 failed. Does not draw or advance pose. */
int showroom_prepare(showroom *s, const char *code);
/* A normal CarFace, or an explicit script pose: just
   the car and its contact shadow in the widget's rectangle of the 640x480
   UI space, over what the page drew, with the widget's own camera
   (camera_trans, camera_rotate in degrees, view_angle) and lights. */
#include "render3d.h"
#include "car_service.h"
typedef struct {
    int look_valid;
    float look[9];
    float x, y, w, h;
    float eye[3], rotate[3], view_angle;
    r3d_lights lights;
    int pose_valid;
    float car_angle, car_position[3];
    const char *surface; /* the page's root name, for the camera log line */
    const uint8_t *crew_data;
    size_t crew_size;
    uint64_t crew_serial;
    const struct gt4_mot1 *crew_motion;
    float crew_time;
    int crew_loop;
    const gt4_service_model *models;
    int model_count;
    const gt4_service_motion *motions;
    int motion_count;
    float service_geometry[12], additional_scale;
    int service_geometry_valid;
    int service_pass;
    float service_opacity, service_additional_opacity, service_shadow;
    float service_lift; /* CarFace Mot1 "lift", 001570A4..001570D8 */
    int studio_presentation; /* P: PC launcher uses animated showroom lights/reflections. */
} showroom_face;
/* 00156F18 +0xac..0x340: camera-driven fade, lift, light and shadowOpacity. */
void showroom_service_channels(showroom_face *face, const struct gt4_mot1 *motion, float time);
int showroom_draw_face(showroom *s, const char *code, float dt, const showroom_face *face);
/* Hides the current car until the next draw finds it loaded again, which
   then fades it in: used when the showroom page is (re)entered. */
void showroom_hide(showroom *s);
void showroom_color(showroom *s, int delta);
/* Shows paint `index` of the current car (CarRoot's colour chips). */
/* The captain's rig (--studio): a look-at camera and a car placed by hand, in scene
   units, replacing the disc's studio pose. Call it every frame the rig moves. */
void showroom_set_rig(showroom *s, const float eye[3], const float target[3],
                      const float car_offset[3], float car_angle);
void showroom_set_color(showroom *s, int index);
/* P: the car alone over a flat colour (no room, floor shadow or mirror): a mod's dealer
   picture, matted from a black and a white frame. */
void showroom_set_picture(showroom *s, int on, const uint8_t bg[3]);
/* Where the car stands in the studio's world, for a camera aimed at it. */
void showroom_car_position(showroom *s, float out[3]);
/* Request a model and its paint together; a second CarFace owns another s. */
void showroom_set_selection(showroom *s, const char *code, int index);
const char *showroom_caption(showroom *s);
/* Colour index of the car shown (for the race view to load it the same). */
int showroom_color_index(showroom *s);
/* Rotation angle of the presented car (radians); captures reset it. */
void showroom_set_angle(showroom *s, float angle);
/* The studio raster width in pixels (design height stays 448). A wider window makes a wider
   raster, so the scene is aspect-correct and shows more room at the sides - never stretched. */
void showroom_set_width(showroom *s, int width);
/* Hand the showroom back to the game after the studio: drop the captain's rig AND its camera, so a
   game screen never draws with the studio's camera; the game's own CarFace camera is restored. */
/* The launcher camera's lens: vertical field of view in degrees, 0 = the studio's own. */
void showroom_set_rig_fov(showroom *s, float degrees);
/* The launcher room's mood: dark or white, and a light colour (1,1,1 = white light) that
   tints the room, the floor and the lights on the car. */
void showroom_set_room_mood(showroom *s, int dark, const float rgb[3]);
void showroom_clear_rig(showroom *s);
/* After the launcher: the game's showroom uses the disc's studio and its light motion. */
void showroom_use_disc_room(showroom *s);
#endif
