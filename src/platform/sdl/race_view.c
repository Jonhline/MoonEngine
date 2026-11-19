/*
 * Race view: the course's world model and a car on it, drawn by the GPU
 * renderer (render3d.c) with a chase camera. The car stands on grid
 * position 0 until a pose is given (the physics, tests/physics/drive.c);
 * with a pose, its wheels are drawn apart and turned by their steer,
 * rotation and suspension travel.
 */
#include "platform/switch_profile.h"
#include "race_limits.h"
#include "race_view.h"
#ifdef MOON_PC_VULKAN
#include "vk_render.h"
#endif
#include "switch_render.h"
#include "video_gl.h"
#include "vk_render.h"
#ifdef _WIN32
/* Windows calling-convention aliases collide with course field names. */
#undef near
#undef far
#endif
#ifdef MOON_PC_VULKAN
#include "vk_render.h"
#endif
#include "race_lights.h"
#include "car_lamps.h"
#include "car_model.h"
#include "driver_pose.h"
#include "course.h"
#include "assist_line.h"
#include "gpb.h"
#include "render3d.h"
#include "daylight.h"
#include "race_smoke.h"
#include "race_leaves.h"
#include "wheel_motion.h"
#include "race_pit_crew.h"
#include "physics/car.h"
#include "vol.h"
#include "platform/platform.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 640
#define H 448

#define OPPONENTS MOON_MAX_OPPONENTS
static int phantom_draws, car_draws, car_count;
int race_view_phantom_draws(void) { return phantom_draws; }
int race_view_car_draws(void) { return car_draws; }
int race_view_car_count(void) { return car_count; }
/* Debug switches, read once per name and scene: getenv scans the environment
   block and some were read for every car and course object of every frame. */
static const char *rv_env_names[16];
static signed char rv_env_values[16];
static int rv_env_count;
static int rv_env(const char *name)
{
    for (int i = 0; i < rv_env_count; i++)
        if (rv_env_names[i] == name)
            return rv_env_values[i];
    int value = getenv(name) != NULL;
    if (rv_env_count < 16) {
        rv_env_names[rv_env_count] = name;
        rv_env_values[rv_env_count++] = (signed char)value;
    }
    return value;
}
typedef struct {
    char key[128];
    gt4_driver_rig rig;
    gt4_model *skin, *part[GT4_DRIVER_BONES];
    r3d_mesh *skin_mesh, *part_mesh[GT4_DRIVER_BONES];
} driver_asset;
typedef struct {
    driver_asset *asset;
    gt4_driver_skeleton skeleton;
    float pose[GT4_DRIVER_BONES][16], palette[GT4_DRIVER_BONES][16];
    int ready, dirty;
    gt4_driver_animation animation;
    float steering, grasp[2];
    int brake; /* 003446B8: the brake pedal (+0x590) above 0, for the lamps */
} driver_actor;

/* The course, the car, the other cars, and the reflection image the
   showroom's cars use */
typedef struct {
    uint8_t *data;
    gt4_course course;
    gt4_course_vision vision[3];
    gt4_course_surface *surface;
    gt4_model *world, *sky, *scenery, *far;
    car_model *car, *other[OPPONENTS];
    moon_car_lamp lamps[OPPONENTS+1][4];
    unsigned lamp_count[OPPONENTS+1];
    tex1_image *env;
    int env_count;
    r3d_lights lights;
    float light_fade[3][2];
    gt4_model *env_sky;
    float env_turn[2], env_color[2], env_plain[2];
    int fog;
    float fog_distance, fog_max, fog_rgb[3];
    /* the road's sheen (func_003411D0): mask +0x34, reflection +0x24, its sky +0xC8 */
    gt4_model *sheen_mask, *sheen_set, *sheen_sky;
    int sheen_size, sheen_blur, sheen_fog;
    float sheen_rgb[3], sheen_fog_distance, sheen_fog_max, sheen_fog_rgb[3];
    /* the cars' shadows (func_0038F240, func_00398758): colour, alpha, the sun's way times its length, the alpha's factor in the sun and out of it */
    int shadow, time_zone;
    float shadow_rgb[3], shadow_alpha, shadow_way[3], shadow_sun, shadow_shade;
    int gadget_shadow;
    float gadget_shadow_direction[3];
    float afterfire_color[4], afterfire_scale[2];
    float glow_alpha; /* Env0 +0x58: the cars' lamp light (003D7430), 0 by day */
    driver_asset driver[OPPONENTS + 1];
    int drivers;
    driver_actor pilot[OPPONENTS + 1];
    float wheel_exposure;
    float brake_glow_scale; /* 003D75F0: Env0 +4C / +38 */
    gt4_course_gadgets *gadgets;
    gt4_model *photo_map;
    uint8_t photo_environment[0x17f4];
    int photo_environment_on;
    tex1_image *smoke;
    int smoke_count; /* the tyres' smoke texture (course header +0x68) */
    tex1_image *bits;
    int bit_count; /* the spark (0) and the leaves (1..8): course header +0x9C */
} race_asset;

/* A car's meshes: whole, or body (with its dark hull) and wheels apart */
typedef struct {
    r3d_mesh *car, *shadow, *cast, *glow, *hull, *body, *wheel[4], *brake[4], *brake_glow[4],
        *steering[GT4_MODEL_STEERING_MAX];
    float lo[3], hi[3], cast_alpha;
} car_meshes;
typedef struct {
    int on, wheels_on, snapshot_on;
    float snapshot[4][8], interval[4], brake_glow[4], pos[3], rot[9], wheel_pose[4][3], ghost;
} car_pose;
typedef struct {
    int on;
    float pos[3], rot[9], sun, level;
} car_shadow;

/* Renderer double buffers, 0033B358/0033B3B0. Poses are published once per
   solver tick. A: native pose/driver adapter, not the complete EE world cache. */
typedef struct {
    car_pose car[OPPONENTS + 1];
    car_shadow shadow[OPPONENTS + 1];
    driver_actor pilot[OPPONENTS + 1];
    float eye[3], rows[9], projection[3], far_clip;
    int camera, hide;
} photo_frame;

struct race_view {
    char *disc;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    r3d_car_shadow *projected_shadow;
    r3d_mesh *wheel_contact;
    r3d_shadow_map *mapped_shadow;
    r3d_lamp_shadow *lamp_shadow;
    int mapped_shadow_ready;
    double daylight_seconds;
    moon_daylight daylight;
#endif
    SDL_Thread *worker;
    SDL_atomic_t done;
    race_asset *pending;
    char course_request[96], car_request[96];
    int color_request;
    char wheel_request[32],
        wing_request[32]; /* the player's fitted wheel and wing (race_view_set_parts) */
    int photo_request;
    r3d_mesh *photo_map;
    float film_tires[OPPONENTS + 1][4][3];
    uint8_t film_tires_on[OPPONENTS + 1];
    char film_body[OPPONENTS+1][32];
    gt4_course_gadget *film_gadget_base;
    unsigned film_gadget_count;
    char other_request[OPPONENTS][32], other_wheel[OPPONENTS][32], other_wing[OPPONENTS][32];
    int other_color[OPPONENTS], other_count;
    race_asset *now;
    float focal, orbit, distance, ground;
    int pose_on;
    float pose_pos[3], pose_rot[9];
    int wheels_on, wheel_snapshot_on;
    float wheel_snapshot[4][8], wheel_interval[4], wheel_brake_glow[4], wheel_pose[4][3];
    car_shadow shadow[OPPONENTS + 1];
    const gt4_pit_crew *pit_crew[OPPONENTS + 1];
    r3d_mesh *world, *sky_mesh, *scenery, *far, *sheen_mask, *sheen_set, *sheen_sky;
    car_meshes mine, other[OPPONENTS];
    car_pose other_pose[OPPONENTS];
    plat_texture *sky;
    r3d_mesh *env_sky;
    int reflection_size;
    uint8_t *env_faces;
    switch_gpu_image *gpu_env;
    tex1_image env_map;
    int env_ok; /* the cars' reflection of the course */
    int warned;
    int hide_car, gadget_section;
    r3d_mesh **gadgets, **gadget_shadows;
    unsigned gadget_types;
    int grade_on;
    float grade[6]; /* race_view_set_grade */
    gt4_model *best_line;          /* the disc's authored licence ribbon */
    r3d_mesh *best_line_mesh;
    char best_line_code[32];
    int best_line_missing;
    gt4_assist_line assist;        /* the AI line's ribbon, one course at a time */
    r3d_mesh *assist_mesh;
    char assist_course[64];
    float assist_speed;            /* m/s last painted */
    uint64_t assist_paint_ns;      /* last repaint, to keep it off every frame */
    int assist_speed_set;
    uint32_t assist_lines;         /* the guest's lines object (car+0xEC) */
    int assist_index;              /* the line the car is running (car+0x1C) */

    float photo_jitter[2];
    int screen_reflect;
    photo_frame photo_current, photo_previous, photo_saved;
    int photo_history, photo_sampling;
    int cam_on;
    float cam_eye[3], cam_rows[9], cam_sx, cam_sy, cam_near, cam_far; /* race_view_set_camera */
    r3d_sprite_texture *smoke_tex;
    const race_smoke *smoke; /* race_view_set_smoke */
    r3d_sprite_texture *leaf_tex[8];
    const race_leaves *leaves; /* race_view_set_leaves */
    r3d_mesh *strike_halo_main, *strike_core_main;
    r3d_mesh *strike_halo_branch[5], *strike_core_branch[5];
    r3d_mesh *strike_halo_splash[3], *strike_core_splash[3];
    int strike_mesh_ready, strike_mesh_logged;
    int strike_on;
    double strike_age;
    unsigned strike_seed; /* race_view_set_strike */
    r3d_mesh *afterfire[6], *afterfire_ground[6];
    int afterfire_tried; /* race/after_fire.mdl's six frames */
    float flame[OPPONENTS + 1][4][3];
    int flame_on[OPPONENTS + 1]; /* race_view_set_afterfire */
};

static void prepare_afterfire(race_view *v);

static void free_asset(race_asset *a)
{
    if (a) {
        gt4_course_gadgets_close(a->gadgets);
        gt4_course_surface_free(a->surface);
        gt4_model_free(a->photo_map);
        gt4_model_free(a->world);
        gt4_model_free(a->sky);
        gt4_model_free(a->scenery);
        gt4_model_free(a->far);
        gt4_model_free(a->env_sky);
        gt4_model_free(a->sheen_mask);
        gt4_model_free(a->sheen_set);
        gt4_model_free(a->sheen_sky);
        free(a->data);
        car_model_free(a->car);
        for (int i = 0; i < OPPONENTS; i++)
            car_model_free(a->other[i]);
        for (int i = 0; i < a->drivers; i++) {
            gt4_model_free(a->driver[i].skin);
            for (int b = 0; b < GT4_DRIVER_BONES; b++)
                gt4_model_free(a->driver[i].part[b]);
        }
        tex1_free(a->env, a->env_count);
        tex1_free(a->smoke, a->smoke_count);
        tex1_free(a->bits, a->bit_count);
        free(a);
    }
}

/* Worker only: one decoded shared pilot per file, independent skeleton state
   per vehicle. The GL thread uploads and releases these CPU geometry copies. */
static void load_driver(gt4_vol *v, race_asset *a, car_model *car, unsigned entry)
{
    if (!car || (!car->driver_path && !car->driver_data) || entry > OPPONENTS)
        return;
    char key[128];
    snprintf(key, sizeof key, "%s", car->driver_path ? car->driver_path : car->model);
    driver_asset *asset = NULL;
    for (int i = 0; i < a->drivers; i++)
        if (!strcmp(a->driver[i].key, key)) {
            asset = &a->driver[i];
            break;
        }
    if (!asset) {
        if (a->drivers >= OPPONENTS + 1)
            return;
        size_t n = car->driver_size;
        uint8_t *shared = car->driver_path ? gt4_vol_read(v, car->driver_path, &n) : NULL;
        const uint8_t *d = shared ? shared : car->driver_data;
        gt4_driver_rig rig;
        if (!d || !gt4_driver_rig_read(d, n, &rig) || rig.skin_count > 1) {
            free(shared);
            return;
        }
        asset = &a->driver[a->drivers++];
        snprintf(asset->key, sizeof asset->key, "%s", key);
        asset->rig = rig;
        if (rig.skin_count)
            asset->skin = gt4_model_decode_skin_part(d, n, rig.skin_first, NULL);
        for (int b = 0; b < GT4_DRIVER_BONES; b++)
            if (rig.bone[b].model_count == 1) {
                if (b == GT4_LHAND || b == GT4_RHAND)
                    asset->part[b] =
                        gt4_model_decode_hand_part(d, n, rig.bone[b].model_first, NULL);
                if (!asset->part[b])
                    asset->part[b] =
                        gt4_model_decode_skin_part(d, n, rig.bone[b].model_first, NULL);
            }
        free(shared);
    }
    driver_actor *pilot = &a->pilot[entry];
    pilot->asset = asset;
    float channels[64];
    int geometry = asset->skin != NULL;
    for (int b = 0; b < GT4_DRIVER_BONES; b++)
        geometry |= asset->part[b] != NULL;
    pilot->ready =
        geometry && gt4_driver_skeleton_init(asset->rig.dimensions, &pilot->skeleton) &&
        gt4_driver_record_init(&car->driver, channels) &&
        gt4_driver_race_pose(&car->driver, channels, &pilot->skeleton, pilot->pose) &&
        (!asset->rig.slots || gt4_driver_palette(&asset->rig, pilot->pose, pilot->palette));
    if (pilot->ready) {
        pilot->grasp[0] = channels[0x80 / 4];
        pilot->grasp[1] = channels[0x84 / 4];
    }
    fprintf(stderr, "race driver: %s source=%s seated=%d slots=%u\n", car->code, key, pilot->ready,
            asset->rig.slots);
}

/* Original course header (field names as in Nenkai's PDTools
   CourseDataFileGT4): world +04, sky +54, far +58 make the view. The
   environment set +14 (with its sky +5C) is what the cars reflect, not
   part of the view: drawn over the road it left the dark shapes the
   PCSX2 captures do not have (Laguna Seca, s03). Reflection +24 and
   its mask +34 are the road's reflections, +60 the mirror's sky.
   Optional sets must have a valid bounded MDLS; no substituted assets. */
static gt4_model *course_set(const uint8_t *data, size_t size, int field)
{
    size_t at = (size_t)data[field] | (size_t)data[field + 1] << 8 | (size_t)data[field + 2] << 16 |
                (size_t)data[field + 3] << 24;
    if (!at || at > size || size - at < 128 || memcmp(data + at, "MDLS", 4))
        return NULL;
    return gt4_model_decode_world(data + at, size - at, NULL);
}

/* The course's lights for the cars. The scene's car pass (func_00340950
   -> func_0038EFF8) sets them through func_00398210: the environment block
   ("Env0", header +0x80) switches light i on with bit i of +0x10; each
   light is 0x80 bytes from +0xD0, passed to the light setter
   (func_004A5BF8) as parameters 0xD (+0, the way to the light), 0 (+0x10,
   ambient), 1 (+0x20, diffuse) and 4 (+0x30, specular). The colours go
   through func_00397FF8 with the ground's two values under the car:
   ((1 - (1 - sun) * a) * b * level + 1 - b) / Env0 +0x38, where a and b
   are one less +0xAC8 and +0xACC for the ambient, +0xAB4 and +0xABC for
   the diffuse, +0xAC0 and +0xAC4 for the specular (car_light below; the
   division is undone by the frame's doubling). The lights' block the PS2
   sent to VU1 in the captures s03 and s06 holds these numbers (Laguna
   Seca: ambient 0.30 0.30 0.32, diffuse 1.12 1.12 1.11, specular 1 1
   0.99, each over 2 and times 128). The other set, +0xAD0 with bit 11 + i
   (func_00398058), is not the cars'. */
static void course_lights(const uint8_t *data, size_t size, r3d_lights *out, float fade[3][2])
{
    uint32_t env, flags;
    float k[8], scale;
    memset(out, 0, sizeof *out);
    memset(fade, 0, 6 * sizeof(float));
    if (size < 0x84)
        return;
    memcpy(&env, data + 0x80, 4);
    if (!env || env > size || size - env < 0xAD0 || memcmp(data + env, "Env0", 4))
        return;
    memcpy(&flags, data + env + 0x10, 4);
    memcpy(k, data + env + 0xAB0, sizeof k);
    memcpy(&scale, data + env + 0x38, 4);
    for (int i = 0; i < 4; i++) {
        const uint8_t *b = data + env + 0xD0 + i * 0x80;
        if (!(flags >> i & 1))
            continue;
        memcpy(out->direction[out->count], b, 12);
        memcpy(out->ambient[out->count], b + 0x10, 12);
        memcpy(out->diffuse[out->count], b + 0x20, 12);
        memcpy(out->specular[out->count], b + 0x30, 12);
        out->count++;
    }
    fade[0][0] = 1 - k[6];
    fade[0][1] = 1 - k[7]; /* ambient: +0xAC8, +0xACC */
    fade[1][0] = 1 - k[1];
    fade[1][1] = 1 - k[3]; /* diffuse: +0xAB4, +0xABC */
    fade[2][0] = 1 - k[4];
    fade[2][1] = 1 - k[5]; /* specular: +0xAC0, +0xAC4 */
    /* the vertex colour stops at 255, where 128 is one over +0x38 of these units */
    out->vertex_limit = (scale > 0 ? scale : 1) * 255 / 128;
}

/* func_00397FF8: a light's colours for a car over ground of this sun and level */
static void car_light(const r3d_lights *in, const float fade[3][2], float sun, float level,
                      r3d_lights *out)
{
    float f[3];
    *out = *in;
    for (int i = 0; i < 3; i++)
        f[i] = (1 - (1 - sun) * fade[i][0]) * fade[i][1] * level + (1 - fade[i][1]);
    for (int l = 0; l < out->count; l++)
        for (int c = 0; c < 3; c++) {
            out->ambient[l][c] *= f[0];
            out->diffuse[l][c] *= f[1];
            out->specular[l][c] *= f[2];
        }
}

/* The course's fog (func_00397CE0 -> func_00397DA0, Env0 +0x60): the
   colour +0x60, the distance +0x70 and the floor of the GS's F +0x74
   (func_004A40B0). While the scene is drawn the colour is divided by
   +0x38 * +0x3C (func_00397DF8, 2 on every course seen: the frame comes
   out doubled) and kept as a GS byte each (func_004A3F98); here it is
   put back in the doubled picture's units. */
static void course_fog(const uint8_t *data, size_t size, race_asset *a)
{
    uint32_t env;
    float c[3], scale[2], k;
    if (size < 0x84)
        return;
    memcpy(&env, data + 0x80, 4);
    if (!env || env > size || size - env < 0x78 || memcmp(data + env, "Env0", 4))
        return;
    memcpy(c, data + env + 0x60, 12);
    memcpy(&a->fog_distance, data + env + 0x70, 4);
    memcpy(&a->fog_max, data + env + 0x74, 4);
    memcpy(scale, data + env + 0x38, 8);
    k = scale[0] * scale[1];
    if (!(k > 0))
        k = 1;
    for (int i = 0; i < 3; i++) {
        float v = c[i] / k;
        v = v < 0 ? 0 : v > 1 ? 1 : v;
        a->fog_rgb[i] = (float)(int)(v * 255) / 255 * 2;
    }
    a->fog = 1;
}

/* The road's sheen (func_003411D0, the scene's slot +0x378 after the
   world): Env0 +0xC10 the colour and +0xC1C its strength, +0xC20 the GS
   ALPHA of the sum (0x58: Cs * Ad + Cd), +0xC24/+0xC28 the reflection
   picture's size, +0xC2C how often it is blurred, +0xC40 its own fog
   (colour, +0xC50 distance, +0xC54 floor). The sum is made in the frame
   before it is doubled (func_0033E828: the frame times +0x38 through a
   FIX of 255, 255/128), so the colour carries that factor here. */
static void course_sheen(const uint8_t *data, size_t size, race_asset *a)
{
    uint32_t env, w, h, blur, alpha;
    float c[4], f[6], k, scale[2];
    if (size < 0x84)
        return;
    memcpy(&env, data + 0x80, 4);
    if (!env || env > size || size - env < 0xC58 || memcmp(data + env, "Env0", 4))
        return;
    memcpy(c, data + env + 0xC10, 16);
    memcpy(&alpha, data + env + 0xC20, 4);
    memcpy(&w, data + env + 0xC24, 4);
    memcpy(&h, data + env + 0xC28, 4);
    memcpy(&blur, data + env + 0xC2C, 4);
    memcpy(f, data + env + 0xC40, 12);
    memcpy(f + 4, data + env + 0xC50, 8);
    memcpy(scale, data + env + 0x38, 8);
    /* only the sum the courses use, on a square picture (all seen are 64 or 128) */
    if ((alpha & 0xff) != 0x58 || !w || w != h || w > 1024 || blur > 8)
        return;
    a->sheen_mask = course_set(data, size, 0x34);
    if (!a->sheen_mask)
        return;
    a->sheen_set = course_set(data, size, 0x24);
    a->sheen_sky = course_set(data, size, 0xC8);
    if (!a->sheen_set && !a->sheen_sky) {
        gt4_model_free(a->sheen_mask);
        a->sheen_mask = NULL;
        return;
    }
    a->sheen_size = (int)w;
    a->sheen_blur = (int)blur;
    {
        float fix = scale[0] * 128;
        fix = fix < 0 ? 0 : fix > 255 ? 255 : (float)(int)fix;
        for (int i = 0; i < 3; i++)
            a->sheen_rgb[i] = c[i] * c[3] * fix / 128;
    }
    k = scale[0] * scale[1];
    if (!(k > 0))
        k = 1;
    for (int i = 0; i < 3; i++) {
        float v = f[i] / k;
        v = v < 0 ? 0 : v > 1 ? 1 : v;
        a->sheen_fog_rgb[i] = (float)(int)(v * 255) / 255;
    }
    a->sheen_fog_distance = f[4];
    a->sheen_fog_max = f[5];
    a->sheen_fog = 1;
}

/* The cars' reflection colour (func_00397CE0 -> func_004568B0/A0, given
   to pgl at each model, 00455464): VU1 0x215 multiplies the reflection
   layer's colour by +0x34 + +0x34 * +0x30 * min(I.N, 0) / |I|, a Fresnel
   rise toward the outline. The sum goes into the frame before it is
   doubled, so the frame's factor (as in course_sheen) is carried here. */
static void course_env_colour(const uint8_t *data, size_t size, race_asset *a)
{
    uint32_t env;
    float k[3];
    if (size < 0x84)
        return;
    memcpy(&env, data + 0x80, 4);
    if (!env || env > size || size - env < 0x3C || memcmp(data + env, "Env0", 4))
        return;
    memcpy(k, data + env + 0x30, 12);
    if (!isfinite(k[0]) || !isfinite(k[1]) || !isfinite(k[2]))
        return;
    float fix = k[2] * 128;
    fix = fix < 0 ? 0 : fix > 255 ? 255 : (float)(int)fix;
    a->env_color[0] = k[1] * fix / 128;
    a->env_color[1] = k[1] * k[0] * fix / 128;
    a->env_plain[0] = k[1];
    a->env_plain[1] = k[1] * k[0]; /* outside the frame (the results' car icons) */
}

/* The cars' shadows on this course. func_0038F240 (the scene, before each
   car's shadow): Env0 +0xA60 the colour, divided by +0x38 as the frame is
   doubled afterwards, +0xA6C the alpha, and the alpha's factor
   (func_003983E0) (sun * (+0xAB0 - +0xAB8) + +0xAB8) * level, from the
   ground under the car. func_003986E8 -> func_00398758 (with the
   environment): the sun's way from the angles +0xA70 and +0xA74, degrees
   (func_00398438: (sin B cos A, -sin A, cos B cos A) with A, B the angles
   negated), times +0xA78. */
static void course_shadow(const uint8_t *data, size_t size, race_asset *a)
{
    uint32_t env;
    float c[4], t[3], f[3], scale, fix;
    const float d = 3.14159265f / 180;
    if (size < 0x84)
        return;
    memcpy(&env, data + 0x80, 4);
    if (!env || env > size || size - env < 0xABC || memcmp(data + env, "Env0", 4))
        return;
    /* 00398718 -> 00452A90/00452AB0: authored backfire tint and sizes. */
    if (size - env >= 0x17CC) {
        memcpy(a->afterfire_color, data + env + 0x17B4, 16);
        memcpy(a->afterfire_scale, data + env + 0x17C4, 8);
    }
    /* 00340170..19C: author explicitly enables the GDGT shadow pass. */
    if (size - env >= 0xADC) {
        uint32_t flags;
        memcpy(&flags, data + env + 0x10, 4);
        a->gadget_shadow = (flags & 0x800) != 0;
        memcpy(a->gadget_shadow_direction, data + env + 0xAD0, 12);
    }
    memcpy(&a->glow_alpha, data + env + 0x58, 4);
    if (!(a->glow_alpha > 0))
        a->glow_alpha = 0;
    memcpy(c, data + env + 0xA60, 16);
    memcpy(t, data + env + 0xA70, 12);
    memcpy(f, data + env + 0xAB0, 12);
    memcpy(&scale, data + env + 0x38, 4);
    if (!(scale > 0))
        return;
    /* func_004A6100 keeps the colour in the GS's 255; the frame's doubling is a FIX of 255 at most (func_0033E828) */
    fix = scale * 128;
    fix = fix > 255 ? 255 : (float)(int)fix;
    for (int i = 0; i < 3; i++)
        a->shadow_rgb[i] = c[i] / scale * fix / 128;
    a->shadow_alpha = c[3];
    a->shadow_sun = f[0];
    a->shadow_shade = f[2];
    {
        float A = -t[0] * d, B = -t[1] * d;
        a->shadow_way[0] = sinf(B) * cosf(A) * t[2];
        a->shadow_way[1] = -sinf(A) * t[2];
        a->shadow_way[2] = cosf(B) * cosf(A) * t[2];
    }
    a->shadow = 1;
}

static race_asset *load(const char *disc, const char *course, const char *car, int color,
                        const char *wheel, const char *wing, char others[][32],
                        char other_wheels[][32], char other_wings[][32], const int *other_colors,
                        int nothers, int photo, const float tires[][4][3], const uint8_t *tires_on, const char bodies[][32])
{
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(disc, &err);
    if (!v)
        return NULL;
    char path[96];
    size_t size = 0;
    snprintf(path, sizeof path, "/crs/%s", course);
    race_asset *a = calloc(1, sizeof *a);
    if (a) {
        a->data = gt4_vol_read(v, path, &size);
    }
    gt4_photo_location location;
    if (!a || !a->data || !gt4_course_open(&a->course, a->data, size) ||
        (photo == 1   ? !gt4_photo_location_read(a->data, size, &location)
         : photo == 0 ? !a->course.grid_count
                      : 0)) {
        gt4_vol_close(v);
        free_asset(a);
        return NULL;
    }
    if (photo == 1) {
        /* MPhotoMapWindow 00194DB0 +90..124: CRPM +14 is the map MDLS. */
        uint32_t at;
        memcpy(&at, location.data + 0x14, 4);
        if (at && at < location.size)
            a->photo_map = gt4_model_decode_world(location.data + at, location.size - at, NULL);
        if (a->photo_map)
            for (int i = 0; i < a->photo_map->count; i++)
                for (int k = 0; k < 3; k++) {
                    gt4_vertex *v = &a->photo_map->triangles[i].v[k];
                    v->p[1] = -v->p[2];
                    v->p[2] = 0;
                }
    }
    a->world = gt4_model_decode_world(a->data + a->course.world, size - a->course.world, NULL);
    if (!a->world || !a->world->count) {
        gt4_vol_close(v);
        free_asset(a);
        return NULL;
    }
    /* 00396E10 ->004580F0 ->00396F00: the camera's nearest authored
       vision node selects world models; never draw all LODs together. */
    for (unsigned pass = 0; pass < 3; pass++) {
        uint32_t at;
        memcpy(&at, a->data + 8 + pass * 4, 4);
        if (at && (at >= size || !gt4_course_vision_open(a->vision + pass, a->data + at, size - at,
                                                         a->world->source_models))) {
            fprintf(stderr, "invalid course vision list %u\n", pass);
            gt4_vol_close(v);
            free_asset(a);
            return NULL;
        }
    }
    if (photo != 1)
        a->gadgets = gt4_course_gadgets_open(&a->course);
    a->sky = course_set(a->data, size, 0x54);
    a->far = course_set(a->data, size, 0x58);
    a->surface = gt4_course_surface_create(&a->course);
    course_lights(a->data, size, &a->lights, a->light_fade);
    course_fog(a->data, size, a);
    course_sheen(a->data, size, a);
    course_env_colour(a->data, size, a);
    course_shadow(a->data, size, a);
    /* 003D73F8: course header +80 points at Env0; +EA0 is wheel
       exposure. Missing source data disables blur instead of inventing it. */
    {
        uint32_t env;
        memcpy(&env, a->data + 0x80, 4);
        if (env && env < size && size - env >= 0xEA4 && !memcmp(a->data + env, "Env0", 4))
        {
            memcpy(&a->wheel_exposure, a->data + env + 0xEA0, 4);
            float num, den;
            memcpy(&num, a->data + env + 0x4C, 4);
            memcpy(&den, a->data + env + 0x38, 4);
            a->brake_glow_scale = den != 0 ? num / den : 0;
            uint32_t flags; memcpy(&flags,a->data+env+0x10,4);
            a->time_zone=flags & 0x80000 ? 2 : flags & 0x8000 ? 1 : 0;
        }
    }
    { /* the smoke's Tex1 (course header +0x68, the world's +0x31D0 +0x34): four frames side by side */
        uint32_t t;
        memcpy(&t, a->data + 0x68, 4);
        if (t && t < size && size - t >= 16 && !memcmp(a->data + t, "Tex1", 4)) {
            int n = tex1_decode(a->data + t, size - t, &a->smoke);
            a->smoke_count = n > 0 ? n : 0;
            if (n <= 0)
                a->smoke = NULL;
        }
        /* the particles' Tex1 (header +0x9C, the world's D_00621B44) */
        memcpy(&t, a->data + 0x9c, 4);
        if (t && t < size && size - t >= 16 && !memcmp(a->data + t, "Tex1", 4)) {
            int n = tex1_decode(a->data + t, size - t, &a->bits);
            a->bit_count = n > 0 ? n : 0;
            if (n <= 0)
                a->bits = NULL;
        }
    }
    a->scenery = course_set(a->data, size, 0x14); /* the environment, for the reflections only */
    a->env_sky = course_set(a->data, size, 0x5C);
    { /* func_0044E528: the environment's sky turned by Env0 +0x48 about x, +0x28 about y (degrees) */
        uint32_t env;
        memcpy(&env, a->data + 0x80, 4);
        if (env && env < size && size - env >= 0x4C && !memcmp(a->data + env, "Env0", 4)) {
            memcpy(&a->env_turn[0], a->data + env + 0x48, 4);
            memcpy(&a->env_turn[1], a->data + env + 0x28, 4);
        }
    }
    if ((car && *car) || nothers) {
        car_model_tables *t = car_model_tables_open(v);
        int zone = 0;
        { /* 00387FB0: every car's time of day (+0x1680, callback 7) from Env0 +0x10:
             bit 0x80000 gives 2, else bit 0x8000 1 (night: route5, hongkong, sarthe24...),
             else 0. Only the race's cars (RaceEntry, 003B4B38) set it (004513B8 from
             00390C90); every other car is drawn through 00451A00 with the draw object's
             constructor 0 (00450A04) and a pose whose light (slot +0x78, 00459D60) is 0:
             Photo Travel keeps daytime and no lamp light. */
            uint32_t env, flags = 0;
            memcpy(&env, a->data + 0x80, 4);
            if (photo != 1 && env && env < size && size - env >= 0x14 &&
                !memcmp(a->data + env, "Env0", 4))
                memcpy(&flags, a->data + env + 0x10, 4);
            zone = flags & 0x80000 ? 2 : flags & 0x8000 ? 1 : 0;
        }
        /* 00388290: the pace car (label D_0061870C) always has 1, lamps lit by day too */
#define CAR_ZONE(label)                                                                            \
    car_model_tables_time_zone(t, photo != 1 && !strcmp(label, "r34_gtr_v_gtcolor_pc_01") ? 1 : zone)
        if (car && *car)
            CAR_ZONE(car);
        /* Photo Travel has a static car without RaceCar's driver/LOD path.
           A: high menu geometry adapter until PhotoCar's draw selection is tested. */
        if (car && *car)
            a->car = photo == 2 && tires_on[0]
                         ? car_model_load_film_body(v, t, car, color, nothers + 1, 0, wheel, wing,
                                                     tires[0],bodies[0])
                     : photo == 1
                         ? car_model_load_parts(v, t, car, color, wheel, wing)
                         : car_model_load_race_parts(v, t, car, color, nothers + 1, 0, wheel, wing);
        for (int i = 0; i < nothers && i < OPPONENTS; i++)
            if (others[i][0] && (CAR_ZONE(others[i]), 1))
                a->other[i] =
                    photo == 2 && tires_on[i + 1]
                        ? car_model_load_film_body(v, t, others[i], other_colors[i], nothers + 1,
                                                    (unsigned)i + 1, other_wheels[i],
                                                    other_wings[i], tires[i + 1],bodies[i+1])
                        : car_model_load_race_parts(v, t, others[i], other_colors[i], nothers + 1,
                                                    (unsigned)i + 1, other_wheels[i],
                                                    other_wings[i]);
        load_driver(v, a, a->car, 0);
        for (int i = 0; i < nothers; i++)
            load_driver(v, a, a->other[i], (unsigned)i + 1);
#undef CAR_ZONE
        if (photo == 1 && a->car) {
            gt4_model_free(a->car->glow);
            a->car->glow = NULL;
        }
        car_model_tables_close(t);
    }
    if (photo && !a->car) {
        gt4_vol_close(v);
        free_asset(a);
        return NULL;
    }
    size_t package_size = 0, env_size = 0;
    uint8_t *package = gt4_vol_read(v, "/projects/arcade/US/CarRoot.gpb", &package_size);
    uint8_t *env =
        package ? gpb_load(package, package_size, "image/arcade/car/env0.png", &env_size) : NULL;
    a->env_count = env ? tex1_decode(env, env_size, &a->env) : 0;
    free(env);
    free(package);
    gt4_vol_close(v);
    for(unsigned entry=0;entry<=OPPONENTS;entry++) {
        const car_model *model=entry?a->other[entry-1]:a->car;
        if(model)a->lamp_count[entry]=moon_car_lamps(model->car,a->lamps[entry]);
    }
    return a;
}

static int worker(void *ptr)
{
    race_view *v = ptr;
    SWP_LOAD_BEGIN(v->course_request);
    v->pending =
        load(v->disc, v->course_request, v->car_request, v->color_request, v->wheel_request,
             v->wing_request, v->other_request, v->other_wheel, v->other_wing, v->other_color,
             v->other_count, v->photo_request, v->film_tires, v->film_tires_on,v->film_body);
    SWP_LOAD_END();
    SDL_AtomicSet(&v->done, 1);
    return 0;
}

static void drop_car(car_meshes *m)
{
    r3d_mesh_destroy(m->car);
    r3d_mesh_destroy(m->shadow);
    r3d_mesh_destroy(m->cast);
    r3d_mesh_destroy(m->glow);
    r3d_mesh_destroy(m->hull);
    r3d_mesh_destroy(m->body);
    for (int i = 0; i < 4; i++) {
        r3d_mesh_destroy(m->wheel[i]);
        r3d_mesh_destroy(m->brake[i]);
        r3d_mesh_destroy(m->brake_glow[i]);
    }
    for (int i = 0; i < GT4_MODEL_STEERING_MAX; i++)
        r3d_mesh_destroy(m->steering[i]);
    memset(m, 0, sizeof *m);
}

static void drop_meshes(race_view *v)
{
    for (unsigned i = 0; i < v->gadget_types; i++) {
        r3d_mesh_destroy(v->gadgets[i]);
        r3d_mesh_destroy(v->gadget_shadows ? v->gadget_shadows[i] : NULL);
    }
    free(v->gadgets);
    free(v->gadget_shadows);
    v->gadget_shadows = NULL;
    v->gadgets = NULL;
    v->gadget_types = 0;
    if (v->now)
        for (int i = 0; i < v->now->drivers; i++) {
            driver_asset *d = &v->now->driver[i];
            r3d_mesh_destroy(d->skin_mesh);
            d->skin_mesh = NULL;
            for (int b = 0; b < GT4_DRIVER_BONES; b++) {
                r3d_mesh_destroy(d->part_mesh[b]);
                d->part_mesh[b] = NULL;
            }
        }
    r3d_mesh_destroy(v->world);
    v->world = NULL;
    r3d_mesh_destroy(v->photo_map);
    v->photo_map = NULL;
    r3d_mesh_destroy(v->sky_mesh);
    v->sky_mesh = NULL;
    r3d_mesh_destroy(v->scenery);
    v->scenery = NULL;
    r3d_mesh_destroy(v->far);
    v->far = NULL;
    r3d_mesh_destroy(v->env_sky);
    v->env_sky = NULL;
    v->env_ok = 0;
    switch_gpu_image_destroy(v->gpu_env);
    v->gpu_env = NULL;
    r3d_mesh_destroy(v->sheen_mask);
    v->sheen_mask = NULL;
    r3d_mesh_destroy(v->sheen_set);
    v->sheen_set = NULL;
    r3d_mesh_destroy(v->sheen_sky);
    v->sheen_sky = NULL;
    drop_car(&v->mine);
    for (int i = 0; i < OPPONENTS; i++)
        drop_car(&v->other[i]);
    r3d_sprite_texture_destroy(v->smoke_tex);
    v->smoke_tex = NULL;
    for (int i = 0; i < 8; i++) {
        r3d_sprite_texture_destroy(v->leaf_tex[i]);
        v->leaf_tex[i] = NULL;
    }
    r3d_mesh_destroy(v->strike_halo_main);
    r3d_mesh_destroy(v->strike_core_main);
    v->strike_halo_main = v->strike_core_main = NULL;
    for (int i = 0; i < 5; i++) {
        r3d_mesh_destroy(v->strike_halo_branch[i]);
        r3d_mesh_destroy(v->strike_core_branch[i]);
        v->strike_halo_branch[i] = v->strike_core_branch[i] = NULL;
    }
    for (int i = 0; i < 3; i++) {
        r3d_mesh_destroy(v->strike_halo_splash[i]);
        r3d_mesh_destroy(v->strike_core_splash[i]);
        v->strike_halo_splash[i] = v->strike_core_splash[i] = NULL;
    }
    v->strike_mesh_ready = 0;
    v->strike_on = 0;
}

static void make_car(car_meshes *m, const car_model *a)
{
    if (!a)
        return;
    m->car = r3d_mesh_create(a->car);
    m->shadow = r3d_mesh_create(a->shadow);
    m->cast = r3d_mesh_create(a->cast);
    m->glow = r3d_mesh_create(a->glow);
    /* P: use the authored volume's maximum opacity for the replacement
       silhouette; retaining its height fade would require a different receiver.
       Original: func_004524B8 uses model-1 vertex alpha. */
    m->cast_alpha = 0;
    if (a->cast)
        for (int t = 0; t < a->cast->count; t++)
            for (int j = 0; j < 3; j++)
                m->cast_alpha = fmaxf(m->cast_alpha, a->cast->triangles[t].v[j].alpha);

    int steering_parts = 0;
    for (unsigned i = 0; i < a->car->steering_count; i++)
        if (a->steering[i]) {
            gt4_model part = *a->steering[i];
            part.textures = a->car->textures;
            part.ntextures = a->car->ntextures;
            m->steering[i] = r3d_mesh_create_shared(&part, m->car);
            steering_parts += m->steering[i] != NULL;
        }
    /* Static fallback and posed body omit only uploaded steering parts.
       Textures are shared; the split and all uploads happen once at load. */
    if (steering_parts) {
        gt4_model rest = *a->car;
        rest.count = 0;
        rest.triangles = malloc((size_t)a->car->count * sizeof *rest.triangles);
        if (rest.triangles) {
            for (int i = 0; i < a->car->count; i++) {
                unsigned tag = a->car->triangles[i].attachment;
                if (tag < 5 || !m->steering[tag - 5])
                    rest.triangles[rest.count++] = a->car->triangles[i];
            }
            if (rest.count < a->car->count) {
                r3d_mesh *mesh = r3d_mesh_create_shared(&rest, m->car);
                if (mesh) {
                    r3d_mesh_destroy(m->car);
                    m->car = mesh;
                } else
                    for (int i = 0; i < GT4_MODEL_STEERING_MAX; i++) {
                        r3d_mesh_destroy(m->steering[i]);
                        m->steering[i] = NULL;
                    }
            }
            free(rest.triangles);
        } else
            for (int i = 0; i < GT4_MODEL_STEERING_MAX; i++) {
                r3d_mesh_destroy(m->steering[i]);
                m->steering[i] = NULL;
            }
    }
    for (int k = 0; k < 3; k++) {
        m->lo[k] = 1e30f;
        m->hi[k] = -1e30f;
    }
    for (int t = 0; t < a->car->count; t++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++) {
                float c = a->car->triangles[t].v[j].p[k];
                if (c < m->lo[k])
                    m->lo[k] = c;
                if (c > m->hi[k])
                    m->hi[k] = c;
            }
    if (a->hull)
        m->hull = r3d_mesh_create(a->hull);
    if (a->body_count > 0) {
        gt4_model body = *a->car;
        body.count = 0;
        body.triangles = malloc((size_t)a->body_count * sizeof *body.triangles);
        if (body.triangles) {
            for (int i = 0; i < a->body_count; i++) {
                unsigned tag = a->car->triangles[i].attachment;
                if (!tag || (tag >= 5 && !m->steering[tag - 5]))
                    body.triangles[body.count++] = a->car->triangles[i];
            }
            m->body = r3d_mesh_create_shared(&body, m->car);
            free(body.triangles);
        }
        for (int i = 0; i < 4; i++)
            if (a->wheel[i])
                m->wheel[i] = r3d_mesh_create(a->wheel[i]);
        for (int i = 0; i < 4; i++)
            if (a->brake[i]) {
                gt4_model brake = *a->brake[i];
                brake.textures = a->car->textures;
                brake.ntextures = a->car->ntextures;
                m->brake[i] = r3d_mesh_create_shared(&brake, m->car);
                if (a->brake_glow[i]) {
                    gt4_model glow = *a->brake_glow[i];
                    glow.textures = a->car->textures;
                    glow.ntextures = a->car->ntextures;
                    m->brake_glow[i] = r3d_mesh_create_shared(&glow, m->car);
                }
            }
    }
}

/* Main thread (GL context current): the meshes of a loaded course and car. */
static void install(race_view *v, race_asset *a)
{
    drop_meshes(v);
    free(v->film_gadget_base);v->film_gadget_base=NULL;v->film_gadget_count=0;
    free_asset(v->now);
    v->now = a;
    v->gadget_section = -1;
    if (a->gadgets) {
        v->gadget_types = a->gadgets->types;
        v->gadgets = calloc(v->gadget_types, sizeof *v->gadgets);
        v->gadget_shadows = calloc(v->gadget_types, sizeof *v->gadget_shadows);
        if (v->gadgets && v->gadget_shadows)
            for (unsigned i = 0; i < v->gadget_types; i++) {
                v->gadgets[i] =
                    r3d_mesh_create_shared(a->gadgets->model[i], i ? v->gadgets[0] : NULL);
                v->gadget_shadows[i] = r3d_mesh_create_shared(a->gadgets->shadow[i], v->gadgets[0]);
                gt4_model_free(a->gadgets->shadow[i]);
                a->gadgets->shadow[i] = NULL;
                gt4_model_free(a->gadgets->model[i]);
                a->gadgets->model[i] = NULL;
            }
        else {
            free(v->gadgets);
            free(v->gadget_shadows);
            v->gadgets = v->gadget_shadows = NULL;
            v->gadget_types = 0;
        }
        fprintf(stderr, "course objects: %u models, %u placements, %u sections\n",
                a->gadgets->types, a->gadgets->count, a->gadgets->sections);
    }
    v->focal = 224 / tanf(20 * 3.14159265f / 180); /* 40 degrees vertical, a native choice */
    v->orbit = 0;
    v->distance = 6.5f;
    gt4_grid g = gt4_course_grid(&a->course, 0);
    v->ground = g.pos[1];
    gt4_course_height(&a->course, g.pos[0], g.pos[2], g.pos[1] + 2, &v->ground);
    v->world = r3d_mesh_create(a->world);
    v->photo_map = r3d_mesh_create(a->photo_map);
    gt4_model_free(a->photo_map);
    a->photo_map = NULL;
    v->sky_mesh = r3d_mesh_create(a->sky);
    v->scenery = r3d_mesh_create(a->scenery);
    v->far = r3d_mesh_create(a->far);
    v->env_sky = r3d_mesh_create(a->env_sky);
    v->sheen_mask = r3d_mesh_create(a->sheen_mask);
    v->sheen_set = r3d_mesh_create(a->sheen_set);
    v->sheen_sky = r3d_mesh_create(a->sheen_sky);
    make_car(&v->mine, a->car);
    v->smoke_tex = a->smoke_count > 0 ? r3d_sprite_texture_create(&a->smoke[0]) : NULL;
    for (int i = 0; i < 8; i++)
        v->leaf_tex[i] = a->bit_count > i + 1 ? r3d_sprite_texture_create(&a->bits[i + 1]) : NULL;
    memset(v->wheel_interval, 0, sizeof v->wheel_interval);
    memset(v->wheel_brake_glow, 0, sizeof v->wheel_brake_glow);
    memset(v->other_pose, 0, sizeof v->other_pose);
    for (int i = 0; i < OPPONENTS; i++) {
        make_car(&v->other[i], a->other[i]);
        v->other_pose[i].on = 0;
    }
    for (int i = 0; i < a->drivers; i++) {
        driver_asset *d = &a->driver[i];
        d->skin_mesh = r3d_mesh_create(d->skin);
        r3d_mesh *textures = d->skin_mesh;
        for (int b = 0; b < GT4_DRIVER_BONES; b++)
            if (d->part[b]) {
                d->part_mesh[b] = r3d_mesh_create_shared(d->part[b], textures);
                if (!textures)
                    textures = d->part_mesh[b];
                if (d->part_mesh[b]) {
                    gt4_model_free(d->part[b]);
                    d->part[b] = NULL;
                }
            }
        if (d->skin_mesh) {
            gt4_model_free(d->skin);
            d->skin = NULL;
        }
    }
    fprintf(stderr, "course: world=%d triangles, %d grid positions\n", a->world->count,
            a->course.grid_count);
    /* Uploaded meshes own GPU data. Keep the course/RNW4 buffer for physics
       and surface queries, but release the large decoded CPU model copies. */
    if (v->world) {
        gt4_model_free(a->world);
        a->world = NULL;
    }
    if (v->sky_mesh) {
        gt4_model_free(a->sky);
        a->sky = NULL;
    }
    if (v->scenery) {
        gt4_model_free(a->scenery);
        a->scenery = NULL;
    }
    if (v->far) {
        gt4_model_free(a->far);
        a->far = NULL;
    }
    if (v->env_sky) {
        gt4_model_free(a->env_sky);
        a->env_sky = NULL;
    }
    if (v->sheen_mask) {
        gt4_model_free(a->sheen_mask);
        a->sheen_mask = NULL;
    }
    if (v->sheen_set) {
        gt4_model_free(a->sheen_set);
        a->sheen_set = NULL;
    }
    if (v->sheen_sky) {
        gt4_model_free(a->sheen_sky);
        a->sheen_sky = NULL;
    }
}

race_view *race_view_open(const char *disc)
{
    race_view *v = calloc(1, sizeof *v);
    if (!v)
        return NULL;
    v->disc = strdup(disc);
    if (!v->disc) {
        free(v);
        return NULL;
    }
    v->distance = 6.5f;
    return v;
}

static void best_line_free(race_view *v)
{
    if (!v)
        return;
    r3d_mesh_destroy(v->best_line_mesh);
    v->best_line_mesh = NULL;
    gt4_model_free(v->best_line);
    v->best_line = NULL;
    v->best_line_code[0] = 0;
    v->best_line_missing = 0;
}

static void assist_line_free(race_view *v)
{
    if (!v)
        return;
    r3d_mesh_destroy(v->assist_mesh);
    v->assist_mesh = NULL;
    gt4_assist_line_free(&v->assist);
    v->assist_course[0] = 0;
    v->assist_speed_set = 0;
}

void race_view_close(race_view *v)
{
    if (!v)
        return;
    best_line_free(v);
    assist_line_free(v);
    if (v->worker) {
        SDL_WaitThread(v->worker, NULL);
        free_asset(v->pending);
    }
    for (int f = 0; f < 6; f++) {
        r3d_mesh_destroy(v->afterfire[f]);
        r3d_mesh_destroy(v->afterfire_ground[f]);
    }
    drop_meshes(v);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    r3d_car_shadow_destroy(v->projected_shadow);
    r3d_mesh_destroy(v->wheel_contact);
    r3d_shadow_map_destroy(v->mapped_shadow);
    r3d_lamp_shadow_destroy(v->lamp_shadow);
#endif
    free(v->film_gadget_base);v->film_gadget_base=NULL;v->film_gadget_count=0;
    free_asset(v->now);
    plat_texture_destroy(v->sky);
    free(v->env_faces);
    free(v->env_map.rgba);
    free(v->disc);
    free(v);
}

/* the player's car's fitted wheel and wing for the next request (an
   owned car's; NULL or empty the stock ones) */
void race_view_set_parts(race_view *v, const char *wheel, const char *wing)
{
    if (!v || v->worker)
        return;
    snprintf(v->wheel_request, sizeof v->wheel_request, "%s",
             wheel && strlen(wheel) < sizeof v->wheel_request ? wheel : "");
    snprintf(v->wing_request, sizeof v->wing_request, "%s",
             wing && strlen(wing) < sizeof v->wing_request ? wing : "");
}
/* CDTL 0033A1B0 +78..A8 calls 003B5AF8/003B5B80 for fitted wheel/wing.
   Apply every stored car, not just the active profile's player. */
int race_view_set_film_parts(race_view *v, unsigned entry, const char *wheel, const char *wing)
{
    if (!v || v->worker || entry > OPPONENTS || !wheel || !wing || strlen(wheel) >= 32 ||
        strlen(wing) >= 32 || strchr(wheel, '/') || strchr(wheel, '\\') || strchr(wing, '/') ||
        strchr(wing, '\\'))
        return 0;
    char *w = entry ? v->other_wheel[entry - 1] : v->wheel_request,
         *g = entry ? v->other_wing[entry - 1] : v->wing_request;
    strcpy(w, wheel);
    strcpy(g, wing);
    return 1;
}
int race_view_set_film_body(race_view *v,unsigned entry,const char *body)
{
    if(!v || v->worker || entry>OPPONENTS || !body || strlen(body)>=32 || strchr(body, '/') || strchr(body, 92))return 0;
    strcpy(v->film_body[entry],body);return 1;
}
/* GDGT stream was validated by the film reader; retain a second boundary
   check here because this API is also used by native scene tools. */
int race_view_set_film_gadgets(race_view *v,const uint8_t *data,size_t size)
{
    if(!v || !v->now || !data || size<3 || data[0])return 0;
    gt4_course_gadgets *g=v->now->gadgets;
    unsigned count=data[1]|(unsigned)data[2]<<8;
    if(count!=(g?g->count:0))return 0;
    size_t at=3;
    for(unsigned i=0;i<count;i++){
        if(at>=size)return 0;
        if(data[at++]){
            if(at>=size)return 0;
            if(data[at++]){
                if(size-at<16)return 0;
                for(int k=0;k<3;k++){float x;memcpy(&x,data+at+4+k*4,4);if(!isfinite(x))return 0;}
                at+=16;
            }
        }
    }
    if(at!=size)return 0;
    if(!count)return 1;
    if(!v->film_gadget_base){
        v->film_gadget_base=malloc(count*sizeof *g->instance);if(!v->film_gadget_base)return 0;
        memcpy(v->film_gadget_base,g->instance,count*sizeof *g->instance);v->film_gadget_count=count;
    }
    if(v->film_gadget_count!=count)return 0;
    memcpy(g->instance,v->film_gadget_base,count*sizeof *g->instance);
    at=3;
    for(unsigned i=0;i<count;i++){
        gt4_course_gadget *p=g->instance+i;
        if(!data[at++]){p->active=0;continue;}
        if(!data[at++])continue;
        p->age=data[at++];
        for(int k=0;k<3;k++)p->angles[k]=(float)(int8_t)data[at++] * 3.141592502593994f / 128;
        memcpy(p->pos,data+at,12);at+=12;
        memset(p->velocity,0,sizeof p->velocity);memset(p->angular,0,sizeof p->angular);
        gt4_course_gadget_rotation(p->angles,p->rot);
    }
    return 1;
}
int race_view_set_film_tires(race_view *v, unsigned entry, const float tires[4][3])
{
    if (!v || v->worker || entry > OPPONENTS)
        return 0;
    if (tires)
        for (int i = 0; i < 4; i++)
            if (!isfinite(tires[i][0]) || !isfinite(tires[i][1]) || !isfinite(tires[i][2]) ||
                tires[i][0] <= 0 || tires[i][2] <= 0 || tires[i][1] < tires[i][2])
                return 0;
    v->film_tires_on[entry] = tires != NULL;
    if (tires)
        memcpy(v->film_tires[entry], tires, sizeof v->film_tires[entry]);
    return 1;
}
int race_view_request(race_view *v, const char *course, const char *car, int color)
{
    if (!v || v->worker || !course || !*course || strlen(course) > 80 || strchr(course, '/') ||
        strchr(course, '\\'))
        return 0;
    if (car && strlen(car) >= sizeof v->car_request)
        return 0;
    strcpy(v->course_request, course);
    snprintf(v->car_request, sizeof v->car_request, "%s", car ? car : "");
    v->color_request = color;
    v->photo_request = 0;
    SDL_AtomicSet(&v->done, 0);
    v->worker = SDL_CreateThread(worker, "race view loader", v);
    return v->worker != NULL;
}

int race_view_poll(race_view *v, int cancel)
{
    if (!v || !v->worker)
        return -1;
    if (!SDL_AtomicGet(&v->done))
        return 0;
    SDL_WaitThread(v->worker, NULL);
    v->worker = NULL;
    race_asset *a = v->pending;
    v->pending = NULL;
    if (!a)
        return -1;
    if (cancel)
        free_asset(a);
    else {
        install(v, a);
#ifdef __SWITCH__
        prepare_afterfire(v);
#endif
#ifdef MOON_PC_VULKAN
        if (!moon_vk_render_prepare_geometry())
            return -1;
#endif
    }
    return 1;
}

int race_view_request_photo(race_view *v, const char *course, const char *car, int color)
{
    if (!v || v->worker || !course || strncmp(course, "ss_", 3) ||
        strlen(course) >= sizeof v->course_request || strchr(course, '/') || strchr(course, '\\') ||
        !car || !*car || strlen(car) >= sizeof v->car_request)
        return 0;
    strcpy(v->course_request, course);
    strcpy(v->car_request, car);
    v->color_request = color;
    v->other_count = 0;
    v->photo_request = 1;
    SDL_AtomicSet(&v->done, 0);
    v->worker = SDL_CreateThread(worker, "Photo Travel loader", v);
    return v->worker != NULL;
}

int race_view_request_film(race_view *v, const char *course, const char *car, int color)
{
    /* 00335248 reconstructs the stored scene, also on normal race courses.
       CDTL wheel/wing and DRVR snapshots are applied separately.
       A: body LOD selection still uses the native race model loader. */
    if (!v || v->worker || !course || !*course || strlen(course) >= sizeof v->course_request ||
        strchr(course, '/') || strchr(course, '\\') || !car || !*car ||
        strlen(car) >= sizeof v->car_request)
        return 0;
    strcpy(v->course_request, course);
    strcpy(v->car_request, car);
    v->color_request = color;
    v->photo_request = 2;
    v->photo_history = 0;
    v->photo_sampling = 0;
    v->hide_car = 0;
    SDL_AtomicSet(&v->done, 0);
    v->worker = SDL_CreateThread(worker, "Photo film loader", v);
    return v->worker != NULL;
}

void race_view_clear(race_view *v)
{
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    r3d_car_lamp_frame(NULL,0);
#endif
    if (v) {
        v->photo_history = 0;
        v->photo_sampling = 0;
        drop_meshes(v);
        free(v->film_gadget_base);v->film_gadget_base=NULL;v->film_gadget_count=0;
    free_asset(v->now);
        v->now = NULL;
        v->pose_on = v->wheels_on = 0;
        best_line_free(v);
        assist_line_free(v);
    }
}

/* The disc's own licence ribbon (/crs/bestline/<region><code>), authored
   in world coordinates with vertex colours: blue acceleration, red braking.
   Loaded only while wanted; OFF means no read, no mesh, no allocation. */
int race_view_set_best_line(race_view *v, const char *code, int on)
{
    if (!v)
        return 0;
    if (!on || !code || !*code) {
        if (v->best_line_mesh || v->best_line)
            best_line_free(v);
        return 0;
    }
    if (!strcmp(v->best_line_code, code) && (v->best_line_mesh || v->best_line_missing))
        return v->best_line_mesh != NULL;
    best_line_free(v);
    snprintf(v->best_line_code, sizeof v->best_line_code, "%s", code);
    static const char *const regions[] = {"us", "jp", "uk", "kr"};
    const char *err = NULL;
    gt4_vol *vol = gt4_vol_open(v->disc, &err);
    if (!vol)
        return 0;
    for (unsigned r = 0; r < sizeof regions / sizeof *regions && !v->best_line; r++) {
        char path[64];
        snprintf(path, sizeof path, "/crs/bestline/%s%s", regions[r], code);
        size_t size = 0;
        uint8_t *bytes = gt4_vol_read(vol, path, &size);
        if (!bytes)
            continue;
        gt4_model_stats stats;
        gt4_model *m = gt4_model_decode_stats(bytes, size, -1, 0, &stats);
        free(bytes);
        if (!m) {
            fprintf(stderr, "assist line: %s did not decode (op %d at 0x%zx)\n", path,
                    stats.fail_op, stats.fail_offset);
            continue;
        }
        v->best_line = m;
        v->best_line_mesh = r3d_mesh_create(m);
        fprintf(stderr, "assist line: loaded %s, %d triangles\n", path, m->count);
    }
    gt4_vol_close(vol);
    if (!v->best_line_mesh) {
        v->best_line_missing = 1;
        fprintf(stderr, "assist line: no authored best line for %s\n", code);
    }
    return v->best_line_mesh != NULL;
}

void race_view_set_assist_source(race_view *v, uint32_t lines, int index)
{
    if (!v)
        return;
    v->assist_lines = lines;
    v->assist_index = index;
}

/* Repaint only when the speed moved enough to matter, at most every 50 ms:
   the gradient is a function of the live speed, not of the frame rate. */
static void assist_paint(race_view *v, float speed_kmh)
{
    if (!v->assist_mesh || !v->assist.model)
        return;
    float speed = speed_kmh > 0 ? speed_kmh / 3.6f : 0;
    uint64_t now = plat_time_ns();
    if (v->assist_speed_set && fabsf(speed - v->assist_speed) < .15f)
        return;
    if (v->assist_paint_ns && now - v->assist_paint_ns < 50000000ull)
        return;
    gt4_assist_line_paint(&v->assist, speed);
    r3d_mesh_update_colours(v->assist_mesh, v->assist.model);
    v->assist_speed = speed;
    v->assist_speed_set = 1;
    v->assist_paint_ns = now;
}

/* The line built from the game's own AI course line, cached by course name.
   The points come from the loaded race's lines object; nothing is generated
   or recorded. */
int race_view_set_assist_line(race_view *v, const char *course, int on, float speed_kmh)
{
    if (!v)
        return 0;
    if (!on || !course || !*course || !v->assist_lines) {
        assist_line_free(v);
        return 0;
    }
    if (v->assist_mesh && !strcmp(v->assist_course, course)) {
        assist_paint(v, speed_kmh);
        return 1;
    }
    assist_line_free(v);
    snprintf(v->assist_course, sizeof v->assist_course, "%s", course);
    if (!v->now)
        return 0;
    enum { AI_LINE_MAX = 16384 };
    gt4_ai_line_point *points = calloc(AI_LINE_MAX, sizeof *points);
    if (!points)
        return 0;
    int active = -1;
    int n = gt4_ai_line_export(v->assist_lines, v->assist_index, points, AI_LINE_MAX, &active);
    int built = n > 1 && gt4_assist_line_build(&v->now->course, points, n, &v->assist);
    free(points);
    if (!built) {
        fprintf(stderr, "assist line: no usable AI line for %s (active %d, %d points)\n", course,
                active, n);
        return 0;
    }
    gt4_assist_line_paint(&v->assist, speed_kmh > 0 ? speed_kmh / 3.6f : 0);
    v->assist_mesh = r3d_mesh_create(v->assist.model);
    v->assist_speed = speed_kmh > 0 ? speed_kmh / 3.6f : 0;
    v->assist_speed_set = 1;
    v->assist_paint_ns = plat_time_ns();
    fprintf(stderr, "assist line: %s AI line %d, %d points, %d triangles, %.2f ms\n", course,
            active, v->assist.count, v->assist.triangles, v->assist.build_ms);
    return v->assist_mesh != NULL;
}

int race_view_set_opponents(race_view *v, const char *const *cars, const int *colors, int n)
{
    if (!v || v->worker || n < 0 || n > OPPONENTS)
        return 0;
    for (int i = 0; i < n; i++)
        if (cars[i] && strlen(cars[i]) >= sizeof v->other_request[i])
            return 0;
    for (int i = 0; i < n; i++) {
        snprintf(v->other_request[i], sizeof v->other_request[i], "%s", cars[i] ? cars[i] : "");
        v->other_color[i] = colors ? colors[i] : 0;
    }
    memset(v->other_wheel, 0, sizeof v->other_wheel);
    memset(v->other_wing, 0, sizeof v->other_wing);
    v->other_count = n;
    return 1;
}

void race_view_set_opponent(race_view *v, int i, const float pos[3], const float rot[9],
                            const float wheels[4][3])
{
    if (!v || i < 0 || i >= OPPONENTS)
        return;
    car_pose *p = &v->other_pose[i];
    p->on = pos != NULL;
    p->wheels_on = wheels != NULL;
    p->snapshot_on = 0;
    if (pos) {
        memcpy(p->pos, pos, sizeof p->pos);
        memcpy(p->rot, rot, sizeof p->rot);
    }
    if (wheels)
        memcpy(p->wheel_pose, wheels, sizeof p->wheel_pose);
}

void race_view_set_ghost(race_view *v, int i, float alpha)
{
    if (v && i >= 0 && i < OPPONENTS)
        v->other_pose[i].ghost = alpha;
}

void race_view_camera(race_view *v, float orbit, float zoom)
{
    if (v) {
        v->orbit += orbit;
        v->distance = fmaxf(3, fminf(15, v->distance + zoom));
    }
}

int race_view_camera_point_entry(const race_view *v, int entry, int i, float out[8])
{
    if (!v || !v->now || entry < 0 || entry > OPPONENTS)
        return 0;
    car_model *m = entry ? v->now->other[entry - 1] : v->now->car;
    if (!m || i < 0 || i >= m->cam_points)
        return 0;
    memcpy(out, m->cam_point[i], 8 * sizeof *out);
    return 1;
}

int race_view_camera_point(const race_view *v, int i, float out[8])
{
    if (!v || !v->now || !v->now->car || i < 0 || i >= v->now->car->cam_points)
        return 0;
    memcpy(out, v->now->car->cam_point[i], 8 * sizeof *out);
    return 1;
}

void race_view_show_car(race_view *v, int on)
{
    if (v)
        v->hide_car = !on;
}
int race_view_car_visible(const race_view *v)
{
    return v && !v->hide_car;
}
void race_view_set_grade(race_view *v, const float grade[6])
{
    if (v) {
        v->grade_on = grade != NULL;
        if (grade)
            memcpy(v->grade, grade, sizeof v->grade);
    }
}
void race_view_set_afterfire(race_view *v, unsigned entry, const float flames[4][3], int on)
{
    if (!v || entry > OPPONENTS)
        return;
    v->flame_on[entry] = on;
    if (on)
        memcpy(v->flame[entry], flames, sizeof v->flame[entry]);
}

/* 00452AC8: body flame (model 0) and ground glow (model 1), with
   Env0 +17B4 colour / +17C4,+17C8 scale. 00452F28 selects time*6's
   integer branch and uploads its fractional tween through 00456850. */
static r3d_view body_place(const car_model *a, const r3d_view *car);
/* Same six immutable branches as the lazy draw path. Switch prepares them
   before readiness, so the first gear-change flame cannot decode mid-frame. */
static void prepare_afterfire(race_view *v)
{
    if (v->afterfire_tried) return;
    SWP_LOAD_BEGIN("after-fire");
    if (!v->afterfire_tried) {
        v->afterfire_tried = 1;
        const char *err = NULL;
        gt4_vol *vol = gt4_vol_open(v->disc, &err);
        size_t size = 0;
        uint8_t *bytes = vol ? gt4_vol_read(vol, "/race/after_fire.mdl", &size) : NULL;
        for (int f = 0; bytes && f < 6; f++) {
            gt4_model *m = gt4_model_decode_afterfire(bytes, size, 0, f);
            if (m) {
                v->afterfire[f] = r3d_mesh_create(m);
                gt4_model_free(m);
            }
            m = gt4_model_decode_afterfire(bytes, size, 1, f);
            if (m) {
                v->afterfire_ground[f] = r3d_mesh_create(m);
                gt4_model_free(m);
            }
        }
        free(bytes);
        if (vol)
            gt4_vol_close(vol);
        if (!v->afterfire[0])
            fprintf(stderr, "race: no after fire model\n");
    }
    SWP_LOAD_END();
}
static void draw_afterfire(race_view *v, const car_model *a, const r3d_view *car,
                           const float flames[4][3], const car_shadow *shadow)
{
    if (!a || !a->exhaust_points)
        return;
    prepare_afterfire(v);
    r3d_view body = body_place(a, car);
    for (int pass = 0; pass < 2; pass++) {
    if (pass && (!shadow || !shadow->on)) continue;
    if (pass) {
        body = *car;
        memcpy(body.pos, shadow->pos, sizeof body.pos);
        memcpy(body.rot, shadow->rot, sizeof body.rot);
        /* 00452418: local ground offset; projection bias handled below. */
        for (int r = 0; r < 3; r++) body.pos[r] += body.rot[r * 3 + 1] * .02f;
    }
    for (unsigned k = 0; k < a->exhaust_points; k++) {
        const float *q = flames[k & 3], *e = a->exhaust[k];
        if (!(q[0] > 0))
            continue;
        float phase = q[0] * 6;
        int frame = (int)phase;
        float tween = phase - frame;
        if (frame > 5)
            frame = 5;
        r3d_mesh *mesh = pass ? v->afterfire_ground[frame] : v->afterfire[frame];
        if (!mesh)
            continue;
        const float rad = 0.01745329252f;
        float yaw = atan2f(e[3], e[5]), pitch = -asinf(fmaxf(-1, fminf(1, e[4]))),
              roll = q[2] * rad, s = q[1] * v->now->afterfire_scale[pass];
        if (pass) pitch = roll = 0;
        float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch), cr = cosf(roll),
              sr = sinf(roll);
        float ry[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy}, rx[9] = {1, 0, 0, 0, cp, -sp, 0, sp, cp},
              rz[9] = {cr, -sr, 0, sr, cr, 0, 0, 0, 1}, yx[9], local[9];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                yx[r * 3 + c] = 0;
                for (int i = 0; i < 3; i++)
                    yx[r * 3 + c] += ry[r * 3 + i] * rx[i * 3 + c];
            }
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) {
                local[r * 3 + c] = 0;
                for (int i = 0; i < 3; i++)
                    local[r * 3 + c] += yx[r * 3 + i] * rz[i * 3 + c];
                local[r * 3 + c] *= s;
            }
        r3d_view w = body;
        for (int r = 0; r < 3; r++) {
            w.pos[r] = body.pos[r];
            for (int c = 0; c < 3; c++) {
                w.rot[r * 3 + c] = 0;
                for (int i = 0; i < 3; i++)
                    w.rot[r * 3 + c] += body.rot[r * 3 + i] * local[i * 3 + c];
                w.pos[r] += body.rot[r * 3 + c] * (pass && c == 1 ? 0 : e[c]);
            }
        }
        w.brake = 0;
        w.depth_bias = pass ? .005f : 0;
        w.grasp = tween;
        w.unlit = 1;
        w.color_scale = 1;
        w.authored_color = 1;
        memcpy(w.color_mul, v->now->afterfire_color, sizeof w.color_mul);
        w.vertex_alpha_set = 1;
        w.vertex_alpha = v->now->afterfire_color[3];
        r3d_draw(mesh, &w);
    }
    }
}

unsigned race_view_exhaust_count(const race_view *v, int entry)
{
    if (!v || !v->now || entry < 0 || entry > OPPONENTS)
        return 0;
    const car_model *car = entry ? v->now->other[entry - 1] : v->now->car;
    return car ? car->exhaust_count : 0;
}
const gt4_course *race_view_course(const race_view *v)
{
    return v && v->now ? &v->now->course : NULL;
}
const car_model *race_view_car_model(const race_view *v, int entry)
{
    if (!v || !v->now || entry < 0 || entry > OPPONENTS)
        return NULL;
    return entry ? v->now->other[entry - 1] : v->now->car;
}
const gt4_driver_car *race_view_car_geometry(const race_view *v, int entry)
{
    if (!v || !v->now || entry < 0 || entry > OPPONENTS)
        return NULL;
    const car_model *c = entry ? v->now->other[entry - 1] : v->now->car;
    return c ? &c->driver : NULL;
}

void race_view_set_camera_far(race_view *v,float far_clip)
{
    if(v && isfinite(far_clip) && far_clip>v->cam_near)v->cam_far=far_clip;
}
float race_view_camera_far(const race_view *v)
{
    return v && v->cam_far>v->cam_near?v->cam_far:20000;
}
void race_view_set_camera(race_view *v, const float eye[3], const float rows[9], float sx, float sy,
                          float near_clip)
{
    if (!v)
        return;
    v->cam_on = eye != NULL;
    if (eye) {
        memcpy(v->cam_eye, eye, sizeof v->cam_eye);
        memcpy(v->cam_rows, rows, sizeof v->cam_rows);
        v->cam_sx = sx;
        v->cam_sy = sy;
        v->cam_near = near_clip;
        v->cam_far = 20000;
    }
}

int race_view_get_camera(const race_view *v, float eye[3], float rows[9], float projection[3])
{
    if (!v || !v->cam_on)
        return 0;
    memcpy(eye, v->cam_eye, sizeof v->cam_eye);
    memcpy(rows, v->cam_rows, sizeof v->cam_rows);
    projection[0] = v->cam_sx;
    projection[1] = v->cam_sy;
    projection[2] = v->cam_near;
    return 1;
}

void race_view_set_shadow(race_view *v, int entry, const float pos[3], const float rot[9],
                          float sun, float level)
{
    if (!v || entry < 0 || entry > OPPONENTS)
        return;
    car_shadow *s = &v->shadow[entry];
    s->on = pos != NULL;
    if (pos) {
        memcpy(s->pos, pos, sizeof s->pos);
        memcpy(s->rot, rot, sizeof s->rot);
        s->sun = sun;
        s->level = level;
    }
}

void race_view_set_pose(race_view *v, const float pos[3], const float rot[9])
{
    if (!v)
        return;
    v->pose_on = pos != NULL;
    if (pos) {
        memcpy(v->pose_pos, pos, sizeof v->pose_pos);
        memcpy(v->pose_rot, rot, sizeof v->pose_rot);
    }
}

void race_view_set_wheels(race_view *v, const float wheels[4][3])
{
    if (!v)
        return;
    v->wheels_on = wheels != NULL;
    v->wheel_snapshot_on = 0;
    if (wheels)
        memcpy(v->wheel_pose, wheels, sizeof v->wheel_pose);
}

void race_view_set_opponent_wheel_spin(race_view *v, int i, float degrees)
{
    if (!v || i < 0 || i >= OPPONENTS)
        return;
    for (int k = 0; k < 4; k++)
        v->other_pose[i].interval[k] = degrees;
}

void race_view_set_wheel_motion(race_view *v, unsigned entry, const float current[4],
                                const float previous[4], float interpolation)
{
    if (!v || !v->now || entry > OPPONENTS)
        return;
    float *interval = entry ? v->other_pose[entry - 1].interval : v->wheel_interval;
    for (int i = 0; i < 4; i++)
        interval[i] = current && previous
                          ? gt4_wheel_motion_degrees(current[i], previous[i], interpolation,
                                                     v->now->wheel_exposure)
                          : 0;
}
/* 003D75F0 -> 003F3A28 (interpolation 0): the wheel's brake heat (+A4)
   squared, times Env0 +4C / +38; 00390C90 -> 00451550 makes it the
   brake's wheel block +28, the alpha its blended layers are drawn under. */
void race_view_set_brake_heat(race_view *v, unsigned entry, const float heat[4])
{
    if (!v || !v->now || entry > OPPONENTS)
        return;
    float *glow = entry ? v->other_pose[entry - 1].brake_glow : v->wheel_brake_glow;
    for (int i = 0; i < 4; i++)
        glow[i] = heat ? heat[i] * heat[i] * v->now->brake_glow_scale : 0;
}
/* CPRM +3C angles / +48 hub XYZ, stride +34. 00452008 +4C..CC
   passes both vectors to 004514A0/00451518 without axis conversion. */
void race_view_set_wheel_snapshot(race_view *v, unsigned entry, const float wheels[4][8])
{
    if (!v || entry > OPPONENTS)
        return;
    if (entry) {
        car_pose *p = &v->other_pose[entry - 1];
        p->snapshot_on = wheels != NULL;
        p->wheels_on = wheels != NULL;
        if (wheels)
            memcpy(p->snapshot, wheels, sizeof p->snapshot);
    } else {
        v->wheel_snapshot_on = wheels != NULL;
        v->wheels_on = wheels != NULL;
        if (wheels)
            memcpy(v->wheel_snapshot, wheels, sizeof v->wheel_snapshot);
    }
}
/* 00339CF8 +104..114 restores the two recorded finger closure values.
   003B1248 publishes hand bones; the hand part renderer consumes grasp. */
void race_view_set_driver_grasp(race_view *v, unsigned entry, const float grasp[2])
{
    if (v && v->now && entry <= OPPONENTS && grasp)
        memcpy(v->now->pilot[entry].grasp, grasp, 8);
}
int race_view_set_driver_snapshot(race_view *v, unsigned entry, const gt4_driver_skeleton *s)
{
    if (!v || !v->now || entry > OPPONENTS)
        return 0;
    driver_actor *p = &v->now->pilot[entry];
    if (!s) {
        p->ready = 0;
        return 1;
    }
    if (!p->asset)
        return 1;
    float pose[20][16], palette[20][16];
    memcpy(palette, p->palette, sizeof palette);
    if (!gt4_driver_skeleton_matrices(s, pose) ||
        (p->asset->rig.slots && !gt4_driver_palette(&p->asset->rig, pose, palette)))
        return 0;
    p->skeleton = *s;
    memcpy(p->pose, pose, sizeof pose);
    memcpy(p->palette, palette, sizeof palette);
    p->animation.initialized = 0;
    p->dirty = 0;
    p->ready = 1;
    return 1;
}
int race_view_set_driver_frame(race_view *v, unsigned entry, const float channels[64])
{
    if (!v || !v->now || entry > OPPONENTS || !channels)
        return 0;
    driver_actor *p = &v->now->pilot[entry];
    const car_model *c = entry ? v->now->other[entry - 1] : v->now->car;
    if (!c || !isfinite(channels[0x34 / 4]))
        return 0;
    if (!p->asset) {
        p->steering = channels[0x34 / 4];
        return 1;
    }
    gt4_driver_skeleton next = p->skeleton;
    float pose[20][16], palette[20][16];
    memcpy(palette, p->palette, sizeof palette);
    if (!gt4_driver_race_pose(&c->driver, channels, &next, pose) ||
        (p->asset->rig.slots && !gt4_driver_palette(&p->asset->rig, pose, palette)))
        return 0;
    p->skeleton = next;
    memcpy(p->pose, pose, sizeof pose);
    memcpy(p->palette, palette, sizeof palette);
    p->steering = channels[0x34 / 4];
    p->grasp[0] = channels[0x80 / 4];
    p->grasp[1] = channels[0x84 / 4];
    p->ready = 1;
    return 1;
}
int race_view_brake_lamp(const race_view *v, unsigned entry)
{
    return v && v->now && entry <= OPPONENTS ? v->now->pilot[entry].brake : 0;
}
int race_view_step_driver(race_view *v, unsigned entry, const int transmission[4],
                          const gt4_driver_animation_input *input)
{
    if (!v || !v->now || entry > OPPONENTS || !transmission || !input)
        return 0;
    driver_actor *p = &v->now->pilot[entry];
    const car_model *c = entry ? v->now->other[entry - 1] : v->now->car;
    p->brake = input->brake > 0;
    if (!c || !isfinite(input->steering))
        return 0;
    if (!p->animation.initialized &&
        !gt4_driver_animation_init(&p->animation, &c->driver, transmission, input->gear)) {
        /* Ordinary wheel rotation does not require a drawable character. */
        if (gt4_driver_control_type(&c->driver) != 0)
            return 0;
        float limit = c->driver.shift_type == 1 ? 1.570796251296997f : 8.377579689025879f;
        p->steering = fminf(limit, fmaxf(-limit, input->steering));
        return 1;
    }
    if (!gt4_driver_animation_step(&p->animation, &c->driver, input))
        return 0;
    p->steering = p->animation.channels[0x34 / 4];
    p->dirty = p->ready;
    return 1;
}

/* Car placement and chase camera: the pose when set, otherwise grid
   position 0 on the road height under it. */
static void place(const race_view *v, float pos[3], float rot[9], float cam[3], float target[3])
{
    if (v->pose_on) {
        /* forward is -z of the model: minus the third column */
        float fx = -v->pose_rot[2], fz = -v->pose_rot[8], len = sqrtf(fx * fx + fz * fz), h;
        if (len < 1e-4f) {
            fx = 0;
            fz = 1;
            len = 1;
        }
        fx /= len;
        fz /= len;
        h = atan2f(fx, fz) + v->orbit;
        memcpy(pos, v->pose_pos, 3 * sizeof *pos);
        memcpy(rot, v->pose_rot, 9 * sizeof *rot);
        cam[0] = pos[0] - sinf(h) * v->distance;
        cam[1] = pos[1] + 2.2f;
        cam[2] = pos[2] - cosf(h) * v->distance;
        target[0] = pos[0] + fx * 2;
        target[1] = pos[1] + .8f;
        target[2] = pos[2] + fz * 2;
        return;
    }
    gt4_grid g = gt4_course_grid(&v->now->course, 0);
    float ground = v->ground;
    float fx = sinf(g.heading), fz = cosf(g.heading), yaw = g.heading + 3.14159265f, sn = sinf(yaw),
          cs = cosf(yaw);
    float r[9] = {cs, 0, sn, 0, 1, 0, -sn, 0, cs};
    memcpy(rot, r, sizeof r);
    pos[0] = g.pos[0];
    pos[1] = ground;
    pos[2] = g.pos[2];
    cam[0] = g.pos[0] - sinf(g.heading + v->orbit) * v->distance;
    cam[1] = ground + 2.2f;
    cam[2] = g.pos[2] - cosf(g.heading + v->orbit) * v->distance;
    target[0] = g.pos[0] + fx * 2;
    target[1] = ground + .8f;
    target[2] = g.pos[2] + fz * 2;
}

static float dot3(const float *a, const float *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void look_at(r3d_view *r, const float cam[3], const float target[3])
{
    float f[3], len;
    for (int k = 0; k < 3; k++) {
        r->cam[k] = cam[k];
        f[k] = target[k] - cam[k];
    }
    len = sqrtf(dot3(f, f));
    for (int k = 0; k < 3; k++)
        r->f[k] = f[k] / len;
    r->r[0] = -r->f[2];
    r->r[1] = 0;
    r->r[2] = r->f[0];
    len = sqrtf(dot3(r->r, r->r));
    for (int k = 0; k < 3; k++)
        r->r[k] /= len;
    r->u[0] = r->r[1] * r->f[2] - r->r[2] * r->f[1];
    r->u[1] = r->r[2] * r->f[0] - r->r[0] * r->f[2];
    r->u[2] = r->r[0] * r->f[1] - r->r[1] * r->f[0];
}

/* A wheel's placement from the car's: turned by its steer about the model's
   up axis and by its rotation about the axle (Ry * Rx), its hub lifted. */
static void wheel_place(const car_model *a, const float pose[3], const float *snapshot, int i,
                        const r3d_view *car, r3d_view *w)
{
    float cy = cosf(pose[0]), sy = sinf(pose[0]), cx = cosf(pose[1]), sx = sinf(pose[1]);
    float local[9] = {cy, sy * sx, sy * cx, 0, cx, -sx, -sy, cy * sx, cy * cx};
    float hub[3] = {a->hub[i][0], a->hub[i][1] + pose[2], a->hub[i][2]};
    if (snapshot) {
        /* 00454110 +28..50: hub, RotateY(steer), RotateZ(camber);
           004541A0 +08: RotateX(spin). Hub-to-rim width is already baked
           into the PC wheel mesh. 004514A0 clamps Y after interpolation. */
        float cy = cosf(snapshot[4]), sy = sinf(snapshot[4]), cz = cosf(snapshot[5]),
              sz = sinf(snapshot[5]), cx = cosf(snapshot[3]), sx = sinf(snapshot[3]);
        float m[9] = {
            cy * cz,  -cy * sz * cx + sy * sx, cy * sz * sx + sy * cx, sz, cz * cx, -cz * sx,
            -sy * cz, sy * sz * cx + cy * sx,  -sy * sz * sx + cy * cx};
        memcpy(local, m, sizeof m);
        memcpy(hub, snapshot, 12);
        hub[1] = gt4_photo_wheel_height(hub[1], a->wheel_limit[i / 2], snapshot[7]);
    }
    *w = *car;
    for (int r = 0; r < 3; r++) {
        w->pos[r] = car->pos[r];
        for (int c = 0; c < 3; c++) {
            w->rot[r * 3 + c] = 0;
            for (int k = 0; k < 3; k++)
                w->rot[r * 3 + c] += car->rot[r * 3 + k] * local[k * 3 + c];
            w->pos[r] += car->rot[r * 3 + c] * hub[c];
        }
    }
}
void race_view_set_pit_crew(race_view *v, unsigned entry, const gt4_pit_crew *c)
{
    if (v && entry <= OPPONENTS)
        v->pit_crew[entry] = c;
}
static void car_draw(r3d_mesh *mesh, const r3d_view *view);
static int draw_pit_tire(const gt4_pit_crew *c, unsigned role, const r3d_view *car, r3d_mesh *mesh)
{
    if (!c || c->stage < 1 || !c->placed)
        return 0;
    const char *labels[] = {"tire0", "tire1"};
    float markers[2][16];
    if (gt4_license_motion_frames_blend(c->motion, role, c->blend[role], labels, 2, markers) != 2)
        return 0;
    unsigned cars = GT4_U32(GT4_U32(c->car + 4) + 0xf8a0);
    int model = gt4_pit_actor_model(c, role, cars);
    for (unsigned tire = 0; tire < 2; tire++) {
        if (!gt4_pit_tire_visible(c->stage, tire))
            continue;
        float matrix[16];
        if (!gt4_pit_tire_draw_matrix(c, role, tire, model, markers[tire], matrix))
            continue;
        r3d_view w = *car;
        memcpy(w.pos, matrix + 12, 12);
        for (unsigned r = 0; r < 3; r++)
            for (unsigned col = 0; col < 3; col++)
                w.rot[r * 3 + col] = matrix[col * 4 + r] * ((role & 1) && col != 1 ? -1.f : 1.f);
        /* car_model's resident wheel already contains the right-side mirror.
     Cancel 003D1488's final right-side half-turn before reusing it. */
        w.wheel_blur = 0;
        w.wheel_interval = 0;
        car_draw(mesh, &w);
    }
    /* 003D2AF8 / 003CF7C8: at approach/departure the car still owns
    the mounted wheel; only stage 3 replaces that draw with both props. */
    return c->stage == 3;
}

/* A car: its shadow, its dark hull (hides the far wheels and the
   underbody), then the body with its wheels turned, or the whole model */
static r3d_view body_place(const car_model *a, const r3d_view *car)
{
    r3d_view body = *car;
    float factory[9] = {1, 0, 0, 0, a->body_cosine, a->body_sine, 0, -a->body_sine, a->body_cosine};
    for (int row = 0; row < 3; row++) {
        body.pos[row] = car->pos[row] + car->rot[row * 3 + 1] * a->body_lift;
        for (int column = 0; column < 3; column++) {
            body.rot[row * 3 + column] = 0;
            for (int k = 0; k < 3; k++)
                body.rot[row * 3 + column] += car->rot[row * 3 + k] * factory[k * 3 + column];
        }
    }
    return body;
}
static r3d_view pit_body_place(const car_model *a, const r3d_view *car, const gt4_pit_crew *pit,
                               int baked)
{
    r3d_view out = *car;
    float root[16];
    if (!pit || !pit->stage || !gt4_pit_car_matrix(pit, root))
        return baked ? out : body_place(a, car);
    /* 004595C8/00459610 draw the body from the live reference frame,
       which rises on the jack. car_model baked the showroom's fixed
       suspension lift/tilt into its vertices: undo that fixed transform
       here before applying the original live parent, without physics writes. */
    const float inverse[9] = {
        1, 0, 0, 0, a->body_cosine, -a->body_sine, 0, a->body_sine, a->body_cosine};
    for (unsigned r = 0; r < 3; r++) {
        out.pos[r] = root[12 + r];
        for (unsigned c = 0; c < 3; c++) {
            out.rot[r * 3 + c] = 0;
            for (unsigned k = 0; k < 3; k++)
                out.rot[r * 3 + c] +=
                    root[k * 4 + r] * (baked ? inverse[k * 3 + c] : (k == c ? 1.f : 0.f));
        }
        if (baked)
            out.pos[r] -= out.rot[r * 3 + 1] * a->body_lift;
    }
    return out;
}
static void pit_mounted_place(const gt4_pit_crew *pit, unsigned role, r3d_view *w)
{
    float m[16];
    if (pit && pit->stage && gt4_pit_mounted_matrix(pit, role, m))
        memcpy(w->pos, m + 12, 12);
}
/* the lights the cars are drawn with this frame (the course's, or none) */
static const r3d_lights *car_lights;
static void car_draw(r3d_mesh *mesh, const r3d_view *view)
{
    if (car_lights)
        r3d_draw_lit(mesh, view, car_lights);
    else
        r3d_draw(mesh, view);
}
/* A car's shadow, as the scene draws it before the car (func_00340950 ->
   func_003907A0 -> func_00451D18), in the shadow's own frame (the car's
   place, its heading and the ground's slope), raised 0.02 (the pose's
   +0x80, func_003D7448) and 0.008 nearer in depth (func_00452418):
   - func_004524B8: model 1, the car's dark volume, laid flat along the
     sun (x += y * way.x, z += y * way.z, y = 0, the way taken into the
     shadow's frame), the shadow's colour added, its alpha times the
     ground's factor. The race's model (/car/lod) fades that volume's
     alpha with height, 77 of 128 at the ground down to 0 at the roof;
   - func_00452660: model 2, the soft blot under the car, with the alpha
     as it is.
   Both blend (Cs - Cd) * As + Cd without writing depth. RNW4 lamps
   select one flat volume per sampled light (0038F168/00451EE0). Without the
   race's frame (the course viewer) the shadow is laid on the surface
   under the car and the ground counts as lit. */
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
#include "race_view_car_shadow.inc"
#include "race_view_shadow_map.inc"
#include "race_view_lamp_shadow.inc"
#endif
static void draw_shadow(race_view *owner, const race_asset *env, const car_meshes *m,
                        const gt4_course_surface *surface, const r3d_view *car,
                        const car_shadow *from, const car_model *model, int posed,
                        const float wheels[4][3], const float snapshot[4][8], const gt4_pit_crew *pit,
                        int lamp_limit)
{
    r3d_view shadow = *car;
    shadow.shadow = 1;
    float ground, normal[3], sun = 1, level = 1;
    if (from && from->on) {
        memcpy(shadow.pos, from->pos, sizeof shadow.pos);
        memcpy(shadow.rot, from->rot, sizeof shadow.rot);
        sun = from->sun;
        level = from->level;
    } else if (gt4_course_surface_at(surface, car->pos[0], car->pos[2], car->pos[1] + 2, &ground,
                                     normal)) {
        shadow.pos[1] = ground;
        float back[3] = {car->rot[2], car->rot[5], car->rot[8]}, d = dot3(back, normal);
        for (int k = 0; k < 3; k++)
            back[k] -= normal[k] * d;
        float len = sqrtf(dot3(back, back));
        if (len > 1e-6f) {
            for (int k = 0; k < 3; k++)
                back[k] /= len;
            float right[3] = {normal[1] * back[2] - normal[2] * back[1],
                              normal[2] * back[0] - normal[0] * back[2],
                              normal[0] * back[1] - normal[1] * back[0]};
            for (int k = 0; k < 3; k++) {
                shadow.rot[k * 3] = right[k];
                shadow.rot[k * 3 + 1] = normal[k];
                shadow.rot[k * 3 + 2] = back[k];
            }
        }
    }
    for (int k = 0; k < 3; k++)
        shadow.pos[k] += shadow.rot[k * 3 + 1] * .02f;
    shadow.depth_bias = .008f;
    /* 00451D18 +17C: after the shadow, model 3 in the same frame when the course's Env0
       +0x58 is above 0 (slot +0x78, 003D7430): colour scale (1, 1, 1, that) and no offset
       (004A60E0/004A6100), its own blend (func_00452998). The P shadow paths below replace
       only the shadow, so the light is drawn before them there. */
    r3d_view glow = shadow;
    memset(glow.shadow_rgb, 0, sizeof glow.shadow_rgb);
    glow.shadow_alpha = env ? env->glow_alpha : 0;
    int lit = m->glow && glow.shadow_alpha > 0;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    if (lit && (r3d_shadow_map_covers(car->pos) || (r3d_car_shadows_enabled() && env && env->shadow)))
        r3d_draw(m->glow, &glow), lit = 0;
    /* Original: func_004524B8 draws the authored contact geometry.
       P: the depth-map path replaces that hard polygonal blot, rather than
       drawing it on top of the filtered geometry shadow. */
    if (car->daylight && r3d_car_shadows_enabled())
        draw_ambient_contacts(owner, m, model, car, &shadow, posed, wheels, snapshot);
    if (r3d_shadow_map_covers(car->pos)) return;
    if (r3d_car_shadows_enabled() && env && env->shadow &&
        draw_projected_shadow(owner, env, m, model, car, &shadow, sun, level,
                              posed, wheels, snapshot, pit))
        return;
#endif
    race_lamp_shadows lamps={0};
    if (env && surface && m->cast) {
        int triangle=-1; float ground_y;
        gt4_course_surface_triangle(surface,car->pos[0],car->pos[2],car->pos[1]+5,
                                    &ground_y,NULL,&triangle);
        race_lights_shadows(&env->course,triangle,car->pos,
                            env->photo_environment_on ? env->photo_environment : NULL,
                            sizeof env->photo_environment,&lamps);
    }
    if (lamps.count && !rv_env("MOON_NO_CAST_SHADOW")) {
        for (int i=0;lamps.count>=2 && i<lamps.count && i<lamp_limit;i++) {
            r3d_view flat=shadow;
            float way[3]={0};
            for(int c=0;c<3;c++) for(int k=0;k<3;k++)
                way[c]+=shadow.rot[k*3+c]*lamps.lamp[i].way[k];
            for(int k=0;k<3;k++)
                flat.rot[k*3+1]=shadow.rot[k*3]*way[0]+shadow.rot[k*3+2]*way[2];
            memcpy(flat.shadow_rgb,lamps.lamp[i].rgb,sizeof flat.shadow_rgb);
            flat.shadow_alpha=lamps.lamp[i].alpha;
            flat.depth_bias=.005f; /* 00452580: biased lamp pass, not solar .008 */
            if (flat.shadow_alpha>0) r3d_draw(m->cast,&flat);
        }
        /* The contact layer retains Env0's colour, independent of each lamp. */
        if (env) {memcpy(shadow.shadow_rgb,env->shadow_rgb,sizeof shadow.shadow_rgb);shadow.shadow_alpha=env->shadow_alpha;}
    } else
    if (env && env->shadow) {
        memcpy(shadow.shadow_rgb, env->shadow_rgb, sizeof shadow.shadow_rgb);
        float alpha = env->shadow_alpha *
                      (sun * (env->shadow_sun - env->shadow_shade) + env->shadow_shade) * level;
        if (m->cast && alpha > 0 && !rv_env("MOON_NO_CAST_SHADOW")) {
            r3d_view flat = shadow;
            float way[3] = {0, 0, 0};
            for (int c = 0; c < 3; c++)
                for (int k = 0; k < 3; k++)
                    way[c] += shadow.rot[k * 3 + c] * env->shadow_way[k];
            for (int k = 0; k < 3; k++)
                flat.rot[k * 3 + 1] = shadow.rot[k * 3] * way[0] + shadow.rot[k * 3 + 2] * way[2];
            flat.shadow_alpha = alpha;
            r3d_draw(m->cast, &flat);
        }
        shadow.shadow_alpha = env->shadow_alpha;
    }
    r3d_draw(m->shadow, &shadow);
    if (lit)
        r3d_draw(m->glow, &glow);
}

static void draw_driver(const driver_actor *pilot, const car_model *a, const r3d_view *car,
                        const gt4_pit_crew *pit);

/* 12-car Tsukuba, 1280x960 Vulkan, MOON_VIDEO_PROFILE=1 frame buckets:
   baseline 25.1 ms (present 18.1); graphics.car_shadows=0 29.9 ms (22.5);
   graphics.material_shading=0 25.0 ms (18.2); 640x480 18.9 ms (13.2).
   No per-car layer dominated; brake_glow has no store/env toggle. LOD not shipped. */
static void draw_car(race_view *owner, const race_asset *env, const car_meshes *m, const car_model *a,
                     const gt4_course_surface *surface, const r3d_view *seen,
                     const car_shadow *from, int posed, const float wheels[4][3],
                     const float snapshot[4][8], const float interval[4],
                     const float brake_glow[4], int wheel_samples, float steering,
                     const gt4_pit_crew *pit, const driver_actor *pilot)
{
    car_draws++;
    r3d_lights lights;
    r3d_view lit = *seen;
    const r3d_view *car = &lit;
    if (env) {
        lit.env_color[0] = env->env_color[0];
        lit.env_color[1] = env->env_color[1];
    }
    uint64_t shadow_start = plat_time_ns();
    if (!seen->ghost)
        draw_shadow(owner, env, m, surface, car, from, a, posed, wheels, snapshot, pit, wheel_samples>1?4:2); /* the ghost casts none (PCSX2, Laguna Seca TT) */
    video_profile_bucket(VIDEO_BUCKET_SHADOW, plat_time_ns() - shadow_start);
    int triangle = -1;
    float ground;
    /* A: immutable surface adapter selects the triangle beneath the drawn
       pose. Solver formulas/parameters and its contact cache stay untouched. */
    if (surface && env &&
        env->data[env->course.runway + 0x40] + env->data[env->course.runway + 0x41])
        gt4_course_surface_triangle(surface, seen->pos[0], seen->pos[2], seen->pos[1] + 5, &ground,
                                    NULL, &triangle);
    int on = from && from->on;
    if (env && race_lights_sample_environment(
                   &env->course, triangle, seen->pos, on ? from->sun : 1, on ? from->level : 1,
                   env->photo_environment_on ? env->photo_environment : NULL,
                   sizeof env->photo_environment, &lights))
        car_lights = &lights;
    else if (env && env->lights.count) {
        car_light(&env->lights, env->light_fade, on ? from->sun : 1, on ? from->level : 1, &lights);
        car_lights = &lights;
    } else
        car_lights = NULL;
    /* The GS window stream retains its authored depth writes. Populate the
       cockpit first, so the blended glass covers the pilot and steering
       wheel instead of rejecting them later. Same depth dependency as brake/rim below.
       Native pass ordering; full PS2 car/driver command sequence still A. */
    if (pilot)
        draw_driver(pilot, a, car, pit);
    r3d_view body = pit_body_place(a, car, pit, 0);
    for (unsigned i = 0; i < a->car->steering_count; i++)
        if (m->steering[i]) {
            float matrix[16];
            if (!gt4_model_steering_matrix(a->car, i, steering, matrix))
                continue;
            r3d_view part = body;
            for (int row = 0; row < 3; row++) {
                part.pos[row] = body.pos[row];
                for (int k = 0; k < 3; k++)
                    part.pos[row] += body.rot[row * 3 + k] * matrix[12 + k];
                for (int col = 0; col < 3; col++) {
                    part.rot[row * 3 + col] = 0;
                    for (int k = 0; k < 3; k++)
                        part.rot[row * 3 + col] += body.rot[row * 3 + k] * matrix[col * 4 + k];
                }
            }
            car_draw(m->steering[i], &part);
        }
    if (m->hull)
        r3d_draw(m->hull, car);
    uint64_t body_start = plat_time_ns();
    if (m->body && posed) {
        r3d_view body = pit_body_place(a, car, pit, 1);
        car_draw(m->body, &body);
        video_profile_bucket(VIDEO_BUCKET_BODY, plat_time_ns() - body_start);
        uint64_t wheels_start = plat_time_ns();
        /* each wheel's brake before the wheel, as the race body's MDLS calls
           its brake (callbacks 11..14) before its wheel (15..18): the rim's
           see-through texels write depth, so a brake drawn after it would
           not show between the spokes (Legacy B4, user report) */
        for (int i = 0; i < 4; i++) {
            if (m->brake[i]) {
                float pose[3] = {wheels[i][0], 0, wheels[i][2]};
                r3d_view w;
                float brake_snapshot[8];
                if (snapshot) {
                    memcpy(brake_snapshot, snapshot[i], sizeof brake_snapshot);
                    brake_snapshot[3] = 0;
                }
                wheel_place(a, pose, snapshot ? brake_snapshot : NULL, i, car, &w);
                pit_mounted_place(pit, (unsigned)(i ^ 1), &w);
                car_draw(m->brake[i], &w);
                /* 00453798: the blended layers under the block +28 alpha;
                   at 0 they add nothing (GS 0x48) or keep the scene (0x44) */
                if (m->brake_glow[i] && brake_glow && brake_glow[i] > 0) {
                    w.vertex_alpha_set = 1;
                    w.vertex_alpha = brake_glow[i];
                    car_draw(m->brake_glow[i], &w);
                }
            }
            if (m->wheel[i]) {
                if (draw_pit_tire(pit, (unsigned)(i ^ 1), car, m->wheel[i]))
                    continue;
                r3d_view w;
                wheel_place(a, wheels[i], snapshot ? snapshot[i] : NULL, i, car, &w);
                float degrees = interval ? interval[i] : 0;
                pit_mounted_place(pit, (unsigned)(i ^ 1), &w);
                w.wheel_blur =
                    wheel_samples > 1 ? wheel_samples : gt4_wheel_motion_blurred(degrees);
                w.wheel_interval = gt4_wheel_motion_radians(degrees);
                car_draw(m->wheel[i], &w);
            }
        }
        video_profile_bucket(VIDEO_BUCKET_WHEELS, plat_time_ns() - wheels_start);
    } else {
        car_draw(m->car, car);
        video_profile_bucket(VIDEO_BUCKET_BODY, plat_time_ns() - body_start);
    }
}
static void draw_driver(const driver_actor *pilot, const car_model *a, const r3d_view *car,
                        const gt4_pit_crew *pit)
{
    if (!pilot->ready || !pilot->asset)
        return;
    r3d_view body = pit_body_place(a, car, pit, 0);
    if (pilot->asset->skin_mesh) {
        body.skin_palette = pilot->palette;
        r3d_draw(pilot->asset->skin_mesh, &body);
    }
    body.skin_palette = NULL;
    for (int b = 0; b < GT4_DRIVER_BONES; b++)
        if (pilot->asset->part_mesh[b]) {
            r3d_view bone = body;
            const float *m = pilot->pose[b];
            bone.grasp = b == GT4_LHAND ? pilot->grasp[0] : b == GT4_RHAND ? pilot->grasp[1] : 0;
            for (int row = 0; row < 3; row++) {
                bone.pos[row] = body.pos[row];
                for (int k = 0; k < 3; k++)
                    bone.pos[row] += body.rot[row * 3 + k] * m[12 + k];
                for (int column = 0; column < 3; column++) {
                    bone.rot[row * 3 + column] = 0;
                    for (int k = 0; k < 3; k++)
                        bone.rot[row * 3 + column] += body.rot[row * 3 + k] * m[column * 4 + k];
                }
            }
            r3d_draw(pilot->asset->part_mesh[b], &bone);
        }
}

static plat_texture *sky(void)
{
    uint8_t *px = malloc((size_t)W * H * 4);
    if (!px)
        return NULL;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint8_t *p = px + ((size_t)y * W + x) * 4;
            float k = (float)y / H;
            p[0] = (uint8_t)(150 - 40 * k);
            p[1] = (uint8_t)(175 - 45 * k);
            p[2] = (uint8_t)(200 - 50 * k);
            p[3] = 255;
        }
    plat_texture *t = plat_texture_create(W, H, px);
    free(px);
    return t;
}

/* The cars' reflection of the course, every frame (func_0044E660 with
   func_0044E528): from 0.8 above the car (the reflection camera,
   func_0044EEA8), the environment's sky (+0x5C, turned) and the
   environment set (+0x14). The PS2 renders three 120-degree views and
   remaps them with a mesh (func_0044EB10, D_006A7D00); as the showroom
   does for its studio, the six faces of a cube are rendered here and each
   sphere-map texel looks up its reflected direction, as seen by the
   view's camera, in them. */
enum { ENV_FACE = 128, ENV_SIZE = 128 };
static void course_reflection(race_view *v, const float centre[3], const r3d_view *eye)
{
    static const float axes[6][2][3] = {{{1, 0, 0}, {0, 1, 0}},  {{-1, 0, 0}, {0, 1, 0}},
                                        {{0, 0, 1}, {0, 1, 0}},  {{0, 0, -1}, {0, 1, 0}},
                                        {{0, 1, 0}, {0, 0, -1}}, {{0, -1, 0}, {0, 0, 1}}};
    const float d = 3.14159265f / 180, ax = v->now->env_turn[0] * d, ay = v->now->env_turn[1] * d;
    const float cx = cosf(ax), sx = sinf(ax), cy = cosf(ay), sy = sinf(ay);
    const float turn[9] = {cy, 0, sy, sx * sy, cx, -sx * cy, -cx * sy, sx, cx * cy}; /* Rx * Ry */
    if (!v->env_sky && !v->scenery)
        return;
    int env_size = ENV_SIZE;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    env_size = r3d_reflection_size();
#endif
    /* P: resize only on a quality change, retaining the same six views/remap. */
    if (v->reflection_size != env_size) {
#ifndef MOON_DEKO3D
        r3d_gpu_environment(0, 0, 0);
#endif
        switch_gpu_image_destroy(v->gpu_env);
        v->gpu_env = NULL;
        free(v->env_faces);
        v->env_faces = NULL;
        free(v->env_map.rgba);
        memset(&v->env_map, 0, sizeof v->env_map);
        v->env_ok = 0;
        v->reflection_size = env_size;
        fprintf(stderr, "race reflections: face=%d map=%d\n", env_size, env_size);
    }
    int gpu = 0;
    /* PC and native Switch retain the six views and remap on the GPU.
       CPU composition remains available for independent comparisons. */
#if defined(__SWITCH__)
    int resident_environment = 1;
#else
    int resident_environment = r3d_resident_reflections();
#endif
    if (resident_environment && !v->gpu_env)
        v->gpu_env = switch_gpu_env_create(env_size, env_size);
    gpu = resident_environment && v->gpu_env != NULL;
    if (!gpu && !v->env_faces && !(v->env_faces = malloc((size_t)6 * env_size * env_size * 4)))
        return;
    if (!gpu && !v->env_map.rgba) {
        if (!(v->env_map.rgba = malloc((size_t)env_size * env_size * 4)))
            return;
        v->env_map.width = v->env_map.height = v->env_map.tex_w = v->env_map.tex_h = env_size;
        v->env_map.max_u = v->env_map.max_v = env_size - 1;
    }
    for (int face = 0; face < 6; face++) {
        const float *ff = axes[face][0], *uu = axes[face][1];
        r3d_view f = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
        f.focal = 320;
        f.focal_y = 224;
        f.near_clip = .05f;
        f.unlit = 1; /* as the GS has it in memory: not doubled */
        f.daylight=eye->daylight;
        f.authored_environment=eye->authored_environment;
        f.sky_seconds=eye->sky_seconds;
        memcpy(f.sun_direction,eye->sun_direction,12);
        memcpy(f.sun_color,eye->sun_color,12);
        memcpy(f.sky_color,eye->sky_color,12);
        memcpy(f.haze_color,eye->haze_color,12);
        memcpy(f.f, ff, 12);
        memcpy(f.u, uu, 12);
        f.r[0] = ff[1] * uu[2] - ff[2] * uu[1];
        f.r[1] = ff[2] * uu[0] - ff[0] * uu[2];
        f.r[2] = ff[0] * uu[1] - ff[1] * uu[0];
        if (!r3d_begin_target(env_size))
            return;
        if (v->env_sky) {
            r3d_view s = f;
            /* Reflection source has the retained half-intensity convention. */
            s.daylight=r3d_daylight_enabled() && (!v->now->time_zone || r3d_night_sky_enabled())?3:0;
            memcpy(s.rot, turn, sizeof turn);
            r3d_draw(v->env_sky, &s);
            r3d_clear_depth();
        }
        f.cam[0] = centre[0];
        f.cam[1] = centre[1] + .8f;
        f.cam[2] = centre[2];
        r3d_draw(v->scenery, &f);
        if (gpu)
            switch_gpu_env_capture(v->gpu_env, face);
        else
            r3d_end_target(v->env_faces + (size_t)face * env_size * env_size * 4);
    }
    if (gpu) {
        switch_gpu_env_remap(v->gpu_env, eye->r, eye->u, eye->f);
        v->env_ok = 1;
        return;
    }
    for (int y = 0; y < env_size; y++)
        for (int x = 0; x < env_size; x++) {
            /* sphere-map texel -> view-space reflection -> world direction (the showroom's) */
            float mx = ((x + .5f) / env_size - .5f) * 2, my = -((y + .5f) / env_size - .5f) * 2,
                  m2 = mx * mx + my * my;
            if (m2 > 1) {
                float k = 1 / sqrtf(m2);
                mx *= k;
                my *= k;
                m2 = 1;
            }
            float nz = sqrtf(1 - m2), R[3] = {2 * nz * mx, 2 * nz * my, 2 * nz * nz - 1}, D[3];
            for (int k = 0; k < 3; k++)
                D[k] = R[0] * eye->r[k] + R[1] * eye->u[k] - R[2] * eye->f[k];
            int best = 0;
            float big = -1;
            for (int face = 0; face < 6; face++) {
                const float *ff = axes[face][0];
                float dd = D[0] * ff[0] + D[1] * ff[1] + D[2] * ff[2];
                if (dd > big) {
                    big = dd;
                    best = face;
                }
            }
            const float *ff = axes[best][0], *uu = axes[best][1];
            float rr[3] = {ff[1] * uu[2] - ff[2] * uu[1], ff[2] * uu[0] - ff[0] * uu[2],
                           ff[0] * uu[1] - ff[1] * uu[0]};
            float qx = (D[0] * rr[0] + D[1] * rr[1] + D[2] * rr[2]) / big,
                  qy = (D[0] * uu[0] + D[1] * uu[1] + D[2] * uu[2]) / big;
            /* filtered between the face's four nearest pixels: the PS2 remaps
           its views drawn as textures on the mesh (its TEX1 not read) */
            float fx = fminf(env_size - 1, fmaxf(0, env_size / 2 + qx * env_size / 2 - .5f)),
                  fy = fminf(env_size - 1, fmaxf(0, env_size / 2 - qy * env_size / 2 - .5f));
            int x0 = (int)fx, y0 = (int)fy, x1 = x0 + 1 < env_size ? x0 + 1 : x0,
                y1 = y0 + 1 < env_size ? y0 + 1 : y0;
            float wx = fx - x0, wy = fy - y0;
            const uint8_t *face = v->env_faces + (size_t)best * env_size * env_size * 4;
            uint8_t *out = v->env_map.rgba + ((size_t)y * env_size + x) * 4;
            for (int c = 0; c < 3; c++) {
                float a = face[(y0 * env_size + x0) * 4 + c] * (1 - wx) +
                          face[(y0 * env_size + x1) * 4 + c] * wx;
                float b = face[(y1 * env_size + x0) * 4 + c] * (1 - wx) +
                          face[(y1 * env_size + x1) * 4 + c] * wx;
                out[c] = (uint8_t)(a * (1 - wy) + b * wy + .5f);
            }
            out[3] = 255;
        }
    v->env_ok = 1;
    r3d_environment_changed();
}

/* The road's sheen, after the world and before the cars (func_003411D0):
   the frame's alpha cleared (func_00342370) and the mask set drawn into
   it (it writes alpha alone, against the world's depth); the reflection's
   sky (+0xC8, as the sky: about the eye, no fog) and set (+0x24, with its
   own fog) drawn from the same eye into a small picture, in the GS's own
   colours; that picture blurred and added over the view through the mask
   (r3d_end_target_add). */
static void road_sheen(race_view *v, const r3d_view *world)
{
    const race_asset *a = v->now;
    r3d_view m = *world, t = *world;
    if (!v->sheen_mask || a->sheen_size <= 0 || rv_env("MOON_NO_SHEEN"))
        return;
    r3d_clear_alpha(0);
    m.fog = 0;
    m.daylight = 0; /* A coverage mask is not a relit surface. */
    r3d_draw(v->sheen_mask, &m);
    if (!r3d_begin_target_view(a->sheen_size))
        return;
    t.color_scale = 1;
    t.daylight = 0; /* Retained encoded sheen source, converted once at composition. */
    if (v->sheen_sky) {
        r3d_view s = t;
        memset(s.cam, 0, sizeof s.cam);
        s.near_clip = .01f;
        s.fog = 0;
        r3d_draw(v->sheen_sky, &s);
        r3d_clear_depth();
    }
    t.fog = a->sheen_fog;
    t.fog_distance = a->sheen_fog_distance;
    t.fog_max = a->sheen_fog_max;
    memcpy(t.fog_rgb, a->sheen_fog_rgb, sizeof t.fog_rgb);
    if (v->sheen_set)
        r3d_draw(v->sheen_set, &t);
    if (world->daylight)
        r3d_end_target_add_road(a->sheen_rgb, a->sheen_blur, world);
    else
        r3d_end_target_add(a->sheen_rgb, a->sheen_blur);
}

/* 003E4A28: odd GDGT model, shadow projection then object placement.
   Original homogeneous W is constant for a directional light, so its divide
   belongs in this read-only affine adapter, not in the object's physics. */
static int gadget_shadow_view(const race_asset *a, const gt4_course_gadget *g,
                              const r3d_view *object, r3d_view *out)
{
    if (!a->gadget_shadow)
        return 0;
    float m[16];
    gt4_course_gadget_shadow_matrix(g, a->gadget_shadow_direction, m);
    if (!isfinite(m[15]) || m[15] == 0)
        return 0;
    *out = *object;
    for (int r = 0; r < 3; r++) {
        out->pos[r] =
            ((m[r] * object->pos[0] + m[4 + r] * object->pos[1]) + m[8 + r] * object->pos[2]) +
            m[12 + r];
        out->pos[r] /= m[15];
        for (int c = 0; c < 3; c++)
            out->rot[r * 3 + c] = ((m[r] * object->rot[c] + m[4 + r] * object->rot[3 + c]) +
                                   m[8 + r] * object->rot[6 + c]) /
                                  m[15];
    }
    out->shadow = 1;
    out->unlit = 1;
    out->daylight = 0;
    out->modern_light = 0;
    memset(out->shadow_rgb, 0, sizeof out->shadow_rgb);
    out->shadow_alpha =
        gt4_course_gadget_shadow_alpha(g->sun, g->shade, a->shadow_sun, a->shadow_shade);
    out->depth_bias = .005f; /* 0049AB58, bits 3BA3D70A. */
    return out->shadow_alpha > 0;
}

static int split_mode;
static int split_entry; /* active drawing pass, never a simulation index */
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
static void update_car_lamps(race_view *v,const float pos[3],const float rot[9],int pregrid)
{
    r3d_car_lamp lights[R3D_CAR_LAMPS];unsigned count=0;
    if(!pregrid && r3d_car_lamps_enabled())for(unsigned entry=0;entry<=OPPONENTS;entry++) {
        const car_model *model=entry?v->now->other[entry-1]:v->now->car;
        if (!model ||
            (entry && (!v->other_pose[entry - 1].on || v->other_pose[entry - 1].ghost > 0)))
            continue;
        int on=v->now->time_zone || !strcmp(model->code,"r34_gtr_v_gtcolor_pc_01");
        int brake=v->now->pilot[entry].brake;
        const float *p=entry?v->other_pose[entry-1].pos:pos,*r=entry?v->other_pose[entry-1].rot:rot;
        for(unsigned i=0;i<v->now->lamp_count[entry] && count<R3D_CAR_LAMPS;i++) {
            const moon_car_lamp *lamp=&v->now->lamps[entry][i];
            if(!on && !(lamp->rear && brake))continue;
            r3d_car_lamp *out=&lights[count++];memset(out,0,sizeof *out);
            for(int k=0;k<3;k++) {
                out->position[k]=p[k];
                for(int c=0;c<3;c++){out->position[k]+=r[k*3+c]*lamp->pos[c];out->direction[k]+=r[k*3+c]*lamp->direction[c];}
                out->color[k]=lamp->color[k]*(lamp->rear?(brake?3.f:.35f):100.f);
            }
            out->color[3]=lamp->rear?0:.02f; /* cutoff above the downward optical axis */
            out->position[3]=lamp->rear?8.f:60.f;
            out->direction[3]=cosf((lamp->rear?65.f:25.f)*.0174532925199433f);
        }
    }
    r3d_car_lamp_frame(lights,count);
}
#endif
/* Custom Modes: the strike is real geometry.  Resident tubes are created
   once and only their vertex positions change each frame. */
#define STRIKE_SEGMENTS 16
#define STRIKE_POINTS (STRIKE_SEGMENTS + 1)
#define STRIKE_BRANCHES 5
#define STRIKE_BRANCH_POINTS 4
#define STRIKE_MESH_SEGMENTS 11
#define STRIKE_MESH_POINTS (STRIKE_MESH_SEGMENTS + 1)
#define STRIKE_SPLASH_COUNT 3
typedef struct {
    float p[STRIKE_POINTS][3];
    int n;
} strike_path;

static unsigned strike_rand(unsigned *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* A fixed tube: 4-sided cross-section, 8 triangles per segment. */
static r3d_mesh *strike_mesh_new(int segments, int blend, int depth_write, const float rgb[3])
{
    gt4_model model = {0};
    int tris = segments * 8;
    gt4_triangle *triangles = calloc((size_t)tris, sizeof *triangles);
    r3d_mesh *mesh;
    if (!triangles)
        return NULL;
    model.count = tris;
    model.triangles = triangles;
    for (int i = 0; i < tris; i++) {
        gt4_triangle *t = triangles + i;
        t->texture = -1;
        t->blend_enabled = 1;
        t->state.blend = (uint8_t)blend;
        t->state.color_mask = 0xffffffff;
        t->state.depth_test = 1;
        t->state.depth_write = (uint8_t)depth_write;
        t->state.depth_func = 2;
        t->state.cull = 0;
        for (int v = 0; v < 3; v++) {
            t->v[v].rgb[0] = rgb[0];
            t->v[v].rgb[1] = rgb[1];
            t->v[v].rgb[2] = rgb[2];
            t->v[v].normal[1] = 1;
            t->v[v].alpha = 1;
        }
    }
    mesh = r3d_mesh_create(&model);
    free(triangles);
    return mesh;
}

static void strike_meshes_free(race_view *v)
{
    r3d_mesh_destroy(v->strike_halo_main);
    r3d_mesh_destroy(v->strike_core_main);
    v->strike_halo_main = v->strike_core_main = NULL;
    for (int i = 0; i < STRIKE_BRANCHES; i++) {
        r3d_mesh_destroy(v->strike_halo_branch[i]);
        r3d_mesh_destroy(v->strike_core_branch[i]);
        v->strike_halo_branch[i] = v->strike_core_branch[i] = NULL;
    }
    for (int i = 0; i < STRIKE_SPLASH_COUNT; i++) {
        r3d_mesh_destroy(v->strike_halo_splash[i]);
        r3d_mesh_destroy(v->strike_core_splash[i]);
        v->strike_halo_splash[i] = v->strike_core_splash[i] = NULL;
    }
    v->strike_mesh_ready = 0;
}

static int strike_meshes_init(race_view *v)
{
    static const float halo_rgb[3] = {.65f, .80f, 1.f};
    static const float core_rgb[3] = {.97f, .99f, 1.f};
    int verts, tris;
    if (v->strike_mesh_ready)
        return 1;
    v->strike_halo_main = strike_mesh_new(STRIKE_MESH_SEGMENTS, 0x44, 0, halo_rgb);
    v->strike_core_main = strike_mesh_new(STRIKE_MESH_SEGMENTS, 0x48, 1, core_rgb);
    for (int i = 0; i < STRIKE_BRANCHES; i++) {
        v->strike_halo_branch[i] =
            strike_mesh_new(STRIKE_BRANCH_POINTS - 1, 0x44, 0, halo_rgb);
        v->strike_core_branch[i] =
            strike_mesh_new(STRIKE_BRANCH_POINTS - 1, 0x48, 1, core_rgb);
    }
    for (int i = 0; i < STRIKE_SPLASH_COUNT; i++) {
        v->strike_halo_splash[i] =
            strike_mesh_new(STRIKE_BRANCH_POINTS - 1, 0x44, 0, halo_rgb);
        v->strike_core_splash[i] =
            strike_mesh_new(STRIKE_BRANCH_POINTS - 1, 0x48, 1, core_rgb);
    }
    if (!v->strike_halo_main || !v->strike_core_main) {
        strike_meshes_free(v);
        return 0;
    }
    for (int i = 0; i < STRIKE_BRANCHES; i++)
        if (!v->strike_halo_branch[i] || !v->strike_core_branch[i]) {
            strike_meshes_free(v);
            return 0;
        }
    for (int i = 0; i < STRIKE_SPLASH_COUNT; i++)
        if (!v->strike_halo_splash[i] || !v->strike_core_splash[i]) {
            strike_meshes_free(v);
            return 0;
        }
    verts = 2 * (STRIKE_MESH_SEGMENTS + STRIKE_BRANCHES * (STRIKE_BRANCH_POINTS - 1) +
                 STRIKE_SPLASH_COUNT * (STRIKE_BRANCH_POINTS - 1)) * 24;
    tris = 2 * (STRIKE_MESH_SEGMENTS + STRIKE_BRANCHES * (STRIKE_BRANCH_POINTS - 1) +
                STRIKE_SPLASH_COUNT * (STRIKE_BRANCH_POINTS - 1)) * 8;
    if (!v->strike_mesh_logged) {
        v->strike_mesh_logged = 1;
        fprintf(stderr, "custom modes: strike mesh verts=%d tris=%d\n", verts, tris);
    }
    v->strike_mesh_ready = 1;
    return 1;
}

static void strike_to_view(const r3d_view *world, const float p[3], float out[3])
{
    float d[3] = {p[0] - world->cam[0], p[1] - world->cam[1], p[2] - world->cam[2]};
    out[0] = d[0] * world->r[0] + d[1] * world->r[1] + d[2] * world->r[2];
    out[1] = d[0] * world->u[0] + d[1] * world->u[1] + d[2] * world->u[2];
    out[2] = -(d[0] * world->f[0] + d[1] * world->f[1] + d[2] * world->f[2]);
}

/* Sample the fractal channel down to the tube's fixed vertex count. */
static void strike_sample(const strike_path *src, float dst[][3], int count)
{
    for (int i = 0; i < count; i++) {
        float t = src->n > 1 ? (float)i * (float)(src->n - 1) / (float)(count - 1) : 0.f;
        int j = (int)t;
        float f = t - (float)j;
        if (j >= src->n - 1) {
            memcpy(dst[i], src->p[src->n - 1], sizeof dst[i]);
        } else
            for (int k = 0; k < 3; k++)
                dst[i][k] = src->p[j][k] * (1.f - f) + src->p[j + 1][k] * f;
    }
}

/* Four-sided tube positions for one path.  The triangle order matches the
   fixed model built by strike_mesh_new(). */
static void strike_tube_positions(const float p[][3], int n, float top_r, float bottom_r,
                                  float out[][3])
{
    float ring[STRIKE_MESH_POINTS][4][3];
    int q = 0;
    for (int i = 0; i < n; i++) {
        float t[3], side[3], up[3], ref[3] = {0, 1, 0}, r, len;
        if (i == 0)
            for (int k = 0; k < 3; k++) t[k] = p[1][k] - p[0][k];
        else if (i == n - 1)
            for (int k = 0; k < 3; k++) t[k] = p[n - 1][k] - p[n - 2][k];
        else
            for (int k = 0; k < 3; k++) t[k] = p[i + 1][k] - p[i - 1][k];
        len = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
        if (len < 1e-5f) {
            t[0] = 0; t[1] = 1; t[2] = 0;
            len = 1;
        }
        for (int k = 0; k < 3; k++) t[k] /= len;
        if (fabsf(t[1]) > .9f) {
            ref[0] = 1; ref[1] = 0; ref[2] = 0;
        }
        side[0] = t[1] * ref[2] - t[2] * ref[1];
        side[1] = t[2] * ref[0] - t[0] * ref[2];
        side[2] = t[0] * ref[1] - t[1] * ref[0];
        len = sqrtf(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);
        if (len < 1e-5f) {
            side[0] = 1; side[1] = 0; side[2] = 0;
            len = 1;
        }
        for (int k = 0; k < 3; k++) side[k] /= len;
        up[0] = side[1] * t[2] - side[2] * t[1];
        up[1] = side[2] * t[0] - side[0] * t[2];
        up[2] = side[0] * t[1] - side[1] * t[0];
        r = top_r + (bottom_r - top_r) * (n > 1 ? (float)i / (float)(n - 1) : 0.f);
        for (int s = 0; s < 4; s++) {
            float a = (float)s * 1.5707963f, c = cosf(a), sn = sinf(a);
            for (int k = 0; k < 3; k++)
                ring[i][s][k] = p[i][k] + side[k] * c * r + up[k] * sn * r;
        }
    }
    for (int i = 0; i + 1 < n; i++)
        for (int s = 0; s < 4; s++) {
            const float *a = ring[i][s], *b = ring[i][(s + 1) & 3];
            const float *c = ring[i + 1][s], *d = ring[i + 1][(s + 1) & 3];
            memcpy(out[q++], a, sizeof out[0]);
            memcpy(out[q++], b, sizeof out[0]);
            memcpy(out[q++], c, sizeof out[0]);
            memcpy(out[q++], b, sizeof out[0]);
            memcpy(out[q++], d, sizeof out[0]);
            memcpy(out[q++], c, sizeof out[0]);
        }
}

static void strike_draw_tube(race_view *v, const r3d_view *view, r3d_mesh *mesh, const float p[][3],
                             int n, float top_r, float bottom_r, float alpha)
{
    float positions[264][3];
    int count = (n - 1) * 24;
    (void)v;
    if (!mesh || count <= 0 || count > 264)
        return;
    strike_tube_positions(p, n, top_r, bottom_r, positions);
    if (r3d_mesh_update_positions(mesh, positions, (unsigned)count, alpha))
        r3d_draw(mesh, view);
}

/* Fractal midpoint displacement: 16 segments from the sky point to the base.
   Each level halves the horizontal offset of the previous one. */
static void strike_build_main(strike_path *s, const float base[3], float height, unsigned *seed)
{
    float amp = 5.f + (float)(strike_rand(seed) % 4);
    memset(s, 0, sizeof *s);
    s->n = STRIKE_POINTS;
    s->p[0][0] = base[0] + (((float)(strike_rand(seed) % 201) / 100.f) - 1.f) * 3.f;
    s->p[0][1] = base[1] + height;
    s->p[0][2] = base[2] + (((float)(strike_rand(seed) % 201) / 100.f) - 1.f) * 3.f;
    memcpy(s->p[STRIKE_SEGMENTS], base, sizeof s->p[STRIKE_SEGMENTS]);
    for (int step = STRIKE_SEGMENTS / 2; step >= 1; step >>= 1) {
        for (int i = step; i < STRIKE_SEGMENTS; i += step * 2) {
            int a = i - step, b = i + step;
            s->p[i][0] = (s->p[a][0] + s->p[b][0]) * .5f +
                         (((float)(strike_rand(seed) % 201) / 100.f) - 1.f) * amp;
            s->p[i][1] = (s->p[a][1] + s->p[b][1]) * .5f;
            s->p[i][2] = (s->p[a][2] + s->p[b][2]) * .5f +
                         (((float)(strike_rand(seed) % 201) / 100.f) - 1.f) * amp;
        }
        amp *= .5f;
    }
}

static int strike_build_branches(strike_path *b, int max, const strike_path *main, float height,
                                 unsigned *seed)
{
    int count = 3 + (int)(strike_rand(seed) % 3);
    if (count > max)
        count = max;
    for (int j = 0; j < count; j++) {
        int at = 4 + (int)(strike_rand(seed) % 9);
        const float *start = main->p[at];
        float len = height * (.20f + (float)(strike_rand(seed) % 21) * .01f);
        float ang = (float)(strike_rand(seed) % 628) * .01f;
        float dir[3] = {cosf(ang) * .8f, -1.f, sinf(ang) * .8f};
        float norm = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        b[j].n = STRIKE_BRANCH_POINTS;
        for (int k = 0; k < 3; k++)
            dir[k] /= norm;
        memcpy(b[j].p[0], start, sizeof b[j].p[0]);
        for (int k = 1; k < STRIKE_BRANCH_POINTS; k++) {
            float f = (float)k / (float)(STRIKE_BRANCH_POINTS - 1);
            float jitter = (1.f - f) * (1.f + (float)(strike_rand(seed) % 100) * .02f);
            b[j].p[k][0] = start[0] + dir[0] * len * f +
                           (((float)(strike_rand(seed) % 201) / 100.f) - 1.f) * jitter;
            b[j].p[k][1] = start[1] + dir[1] * len * f;
            b[j].p[k][2] = start[2] + dir[2] * len * f +
                           (((float)(strike_rand(seed) % 201) / 100.f) - 1.f) * jitter;
        }
    }
    return count;
}

/* Short burst tubes radiating from the car roof. */
static void strike_build_splash(strike_path *s, const float base[3], int index, unsigned *seed)
{
    float ang = 2.0943951f * (float)index + (float)(strike_rand(seed) % 100) * .01f;
    float len = 2.5f + (float)(strike_rand(seed) % 4);
    float rise = .4f + (float)(strike_rand(seed) % 60) * .01f;
    memset(s, 0, sizeof *s);
    s->n = STRIKE_BRANCH_POINTS;
    memcpy(s->p[0], base, sizeof s->p[0]);
    for (int k = 1; k < STRIKE_BRANCH_POINTS; k++) {
        float f = (float)k / (float)(STRIKE_BRANCH_POINTS - 1);
        s->p[k][0] = base[0] + cosf(ang) * len * f;
        s->p[k][1] = base[1] + rise * len * f;
        s->p[k][2] = base[2] + sinf(ang) * len * f;
    }
}

/* Custom Modes: one strike in the race world, landing on the player's car. */
static void strike_draw(race_view *v, const r3d_view *world, const float pos[3],
                        const float rot[9])
{
    strike_path mainline, branches[STRIKE_BRANCHES], splash[STRIKE_SPLASH_COUNT];
    float base[3], right[3], up[3], back[3], bv[3];
    unsigned seed;
    double t = v->strike_age;
    float height, life, flick, gain, base_depth, xscale;
    float core_px, halo_px, core_r, halo_r;
    int nbranch, i;
    float mainpts[STRIKE_MESH_POINTS][3], bpts[STRIKE_BRANCH_POINTS][3];
    float spts[STRIKE_BRANCH_POINTS][3];
    r3d_view strike;
    if (!v || !world || !pos || !rot || !v->strike_on || t < 0 || t >= 1.0)
        return;
    if (!strike_meshes_init(v))
        return;
    /* pose_car writes the car's columns: right, up, back. */
    right[0] = rot[0]; right[1] = rot[3]; right[2] = rot[6];
    up[0] = rot[1]; up[1] = rot[4]; up[2] = rot[7];
    back[0] = rot[2]; back[1] = rot[5]; back[2] = rot[8];
    for (i = 0; i < 3; i++)
        base[i] = pos[i] + up[i] * 1.35f; /* the car's roof */
    strike_to_view(world, base, bv);
    base_depth = -bv[2];
    xscale = world->focal / 320.f * video_projection_x();
    if (base_depth < .1f || xscale < .001f)
        return;
    seed = v->strike_seed ^ (((unsigned)(t * 60.0) + 1u) * 2654435761u);
    core_px = 18.f + (float)(strike_rand(&seed) % 13);
    halo_px = 60.f + (float)(strike_rand(&seed) % 51);
    core_r = core_px * base_depth / (2.f * 640.f * xscale);
    halo_r = halo_px * base_depth / (2.f * 640.f * xscale);
    height = 45.f + (float)(strike_rand(&seed) % 26);
    strike_build_main(&mainline, base, height, &seed);
    nbranch = strike_build_branches(branches, STRIKE_BRANCHES, &mainline, height, &seed);
    for (i = 0; i < STRIKE_SPLASH_COUNT; i++)
        strike_build_splash(&splash[i], base, i, &seed);
    if (getenv("MOON_STRIKE_TRACE")) {
        float dx = base[0] - (pos[0] + up[0] * 1.35f);
        float dy = base[1] - (pos[1] + up[1] * 1.35f);
        float dz = base[2] - (pos[2] + up[2] * 1.35f);
        fprintf(stderr,
                "custom modes: strike trace base_car_dist=%.2f base_depth=%.1f "
                "core_px=%.1f halo_px=%.1f\n",
                sqrtf(dx * dx + dy * dy + dz * dz), base_depth, core_px, halo_px);
    }
    life = t < .15 ? 1.f : 1.f - (float)((t - .15) / .85);
    if (life < 0.f)
        life = 0.f;
    {
        int phase = (int)(t * 17.f) % 4;
        flick = t < .15 ? 1.f : phase == 0 ? .35f : phase == 1 ? .8f : phase == 2 ? .55f : 1.f;
    }
    gain = life * flick;
    if (gain <= .01f)
        return;
    strike_sample(&mainline, mainpts, STRIKE_MESH_POINTS);
    strike = *world;
    memset(strike.pos, 0, sizeof strike.pos);
    memset(strike.rot, 0, sizeof strike.rot);
    strike.rot[0] = strike.rot[4] = strike.rot[8] = 1;
    strike.unlit = 1;
    strike.color_scale = 1;
    strike.skin_palette = NULL;
    /* Halo first (alpha blended), then the hot additive core. */
    strike_draw_tube(v, &strike, v->strike_halo_main, mainpts, STRIKE_MESH_POINTS,
                     halo_r * .55f, halo_r, .78f * gain);
    for (i = 0; i < nbranch; i++) {
        for (int k = 0; k < STRIKE_BRANCH_POINTS; k++)
            memcpy(bpts[k], branches[i].p[k], sizeof bpts[k]);
        strike_draw_tube(v, &strike, v->strike_halo_branch[i], bpts, STRIKE_BRANCH_POINTS,
                         halo_r * .35f, halo_r * .18f, .78f * gain);
    }
    for (i = 0; i < STRIKE_SPLASH_COUNT; i++) {
        for (int k = 0; k < STRIKE_BRANCH_POINTS; k++)
            memcpy(spts[k], splash[i].p[k], sizeof spts[k]);
        strike_draw_tube(v, &strike, v->strike_halo_splash[i], spts, STRIKE_BRANCH_POINTS,
                         halo_r * .80f, halo_r * .25f, .78f * gain);
    }
    strike_draw_tube(v, &strike, v->strike_core_main, mainpts, STRIKE_MESH_POINTS,
                     core_r * .55f, core_r, .95f * gain);
    for (i = 0; i < nbranch; i++) {
        for (int k = 0; k < STRIKE_BRANCH_POINTS; k++)
            memcpy(bpts[k], branches[i].p[k], sizeof bpts[k]);
        strike_draw_tube(v, &strike, v->strike_core_branch[i], bpts, STRIKE_BRANCH_POINTS,
                         core_r * .30f, core_r * .15f, .95f * gain);
    }
    for (i = 0; i < STRIKE_SPLASH_COUNT; i++) {
        for (int k = 0; k < STRIKE_BRANCH_POINTS; k++)
            memcpy(spts[k], splash[i].p[k], sizeof spts[k]);
        strike_draw_tube(v, &strike, v->strike_core_splash[i], spts, STRIKE_BRANCH_POINTS,
                         core_r * .80f, core_r * .20f, .95f * gain);
    }
}

static int draw_scene_box(race_view *v, int pregrid, int menu, int photo, float x, float y, float w,
                          float h)
{
    uint64_t scene_start = plat_time_ns();
    phantom_draws = 0;
    car_draws = 0;
    car_count = v ? v->other_count + 1 : 0;
    rv_env_count = 0;
    if (!v || !v->now)
        return 0;
    if (!r3d_available()) {
        if (!v->warned++)
            fprintf(stderr, "race view: needs the GPU renderer\n");
        return 0;
    }
    for (unsigned entry = 0; entry <= OPPONENTS; entry++) {
        driver_actor *p = &v->now->pilot[entry];
        if (pregrid || ((int)entry == split_entry && v->hide_car) ||
            (entry && !v->other_pose[entry - 1].on))
            continue;
        if (p->dirty) {
            race_view_set_driver_frame(v, entry, p->animation.channels);
            p->dirty = 0;
        }
    }
    const uint8_t tint[4] = {255, 255, 255, 255};
    if (!v->sky)
        v->sky = sky();
    plat_draw_quad(v->sky, x, y, w, h, 0, 0, 1, 1, tint);
    float pos[3], rot[9], cam[3], target[3];
    place(v, pos, rot, cam, target);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    update_car_lamps(v,pos,rot,pregrid);
#endif
    r3d_view world = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    world.focal = v->focal;
    world.unlit = 1;
    look_at(&world, cam, target);
    /* The course comes out doubled: textures are authored at half
       intensity (texels up to 127). Against PCSX2's screenshots of the
       same moments: Laguna Seca's textured sky (s03, colours ~128,
       texels <= 127) is 2.00x the GS's texel * colour / 128 in every
       channel, and Tsukuba's untextured sky (b_reta) twice its vertex
       colours (71 -> 142). Where the game doubles (its vertex program or
       a pass over the frame) is not read yet. */
    world.color_scale = 2;
    world.fog = v->now->fog && !rv_env("MOON_NO_FOG");
    world.fog_distance = v->now->fog_distance;
    world.fog_max = v->now->fog_max;
    memcpy(world.fog_rgb, v->now->fog_rgb, sizeof world.fog_rgb);
    if (v->cam_on) { /* the game's camera: its axes are the rotation's rows, looking down -z */
        for (int k = 0; k < 3; k++) {
            world.cam[k] = v->cam_eye[k];
            world.r[k] = v->cam_rows[k];
            world.u[k] = v->cam_rows[3 + k];
            world.f[k] = -v->cam_rows[6 + k];
        }
        /* the game's scales are of the 4:3 picture: in the renderer's pixels of
           the 640x448 view (whose width it widens by 640 / (448 * 4/3)) */
        world.focal = v->cam_sx * (menu ? 320 : (H * 4.f / 3) / 2);
        world.focal_y = v->cam_sy * H / 2;
        world.near_clip = v->cam_near;
        world.far_clip = race_view_camera_far(v);
    }
    memcpy(world.jitter, v->photo_jitter, sizeof world.jitter);
    world.screen_reflect = v->screen_reflect;
    world.modern_light = 1;

#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    if(!r3d_daylight_enabled() || (v->now->time_zone && !r3d_night_sky_enabled()))video_solar_projection(0,0,0,world.fog_rgb);
    if ((r3d_daylight_enabled() || r3d_modern_lighting_enabled()) && (!v->now->time_zone || r3d_night_sky_enabled())) {
        /* 00398210: primary enabled Env0 +D0 directional light. Source sky
           geometry/textures retain sun and moon positions; no synthetic orbit. */
        float direction[3];
        for(int c=0;c<3;c++)direction[c]=v->now->lights.count?v->now->lights.direction[0][c]:-v->now->shadow_way[c];
        v->daylight=moon_daylight_authored(direction,v->now->time_zone!=0);
        world.authored_environment=1;
        world.daylight=r3d_modern_lighting_enabled()?1:0;
        world.sky_seconds=(float)v->daylight_seconds;
        world.authored_night =
            moon_authored_night_weight(v->now->time_zone != 0, v->daylight.sun[1]);
        memcpy(world.sun_direction,v->daylight.sun,12);
        memcpy(world.sun_color,v->daylight.direct,12);
        memcpy(world.sky_color,v->daylight.sky,12);
        memcpy(world.haze_color,v->daylight.haze,12);
        float forward=dot3(world.sun_direction,world.f);
        float sx=.5f,sy=.5f;
        if(forward>.001f){sx+=dot3(world.sun_direction,world.r)*world.focal/640/forward*video_projection_x();sy+=dot3(world.sun_direction,world.u)*(world.focal_y>0?world.focal_y:world.focal)/448/forward;}
        if (!v->screen_reflect)
            video_solar_projection(sx,sy,!split_mode && forward>.001f?1:0,world.sun_color);
    }
#endif
    r3d_view car = world;
    car.unlit = 0;
    car.color_scale = 0;
    memcpy(car.rot, rot, sizeof rot);
    memcpy(car.pos, pos, sizeof pos);
    if (!pregrid)
        course_reflection(v, pos, &world);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    v->mapped_shadow_ready = 0;
    r3d_shadow_map_bind(NULL);
    if (!pregrid) {
        moon_vk_gpu_mark("shadow");
        prepare_lamp_shadow(v, &car);
        prepare_shadow_map(v, &car, split_entry);
        moon_vk_gpu_mark("shadow_end");
    }
#endif

    /* the Dive's filter on the course, not the cars: rp_04 (PCSX2) shows
       its fence at (1, 0.6, 0.35) of the grey and the cars' paint as it
       is (A: the pass that does it is not read) */
    if (!menu && v->grade_on)
        memcpy(world.grade, v->grade, sizeof world.grade);
    moon_vk_gpu_mark("track");
    if (v->gpu_env && v->env_ok
#if !defined(__SWITCH__)
        && r3d_resident_reflections()
#endif
    ) {
        if (photo)
            r3d_begin_ui_environment(x, y, w, h, NULL);
        else
            r3d_begin(x, y, w, h, NULL);
        r3d_gpu_environment(video_texture_id(switch_gpu_image_texture(v->gpu_env)), v->reflection_size,
                            v->reflection_size);
    } else
        if (photo)
        r3d_begin_ui_environment(x, y, w, h,
                                 v->env_ok               ? &v->env_map
                                 : v->now->env_count > 0 ? v->now->env
                                                         : NULL);
    else
        r3d_begin(x, y, w, h, v->env_ok ? &v->env_map : v->now->env_count > 0 ? v->now->env : NULL);
    if (v->sky_mesh) {
        /* A sky centred on the eye keeps its authored angular size and
           orientation while the camera travels through the course. */
        r3d_view s = world;
        memset(s.cam, 0, sizeof s.cam);
        s.near_clip = .01f;
        s.daylight=r3d_daylight_enabled() && (!v->now->time_zone || r3d_night_sky_enabled())?2:0;
        s.fog = 0; /* func_00396B80: the sky with the fog at distance 0 and floor 1, none */
        r3d_draw(v->sky_mesh, &s);
        /* The dome (some 40 m across) is behind everything: the distant
           set (+0x58, kilometres away, the hills on the horizon of
           the PCSX2 captures) draws over it. The original's way of
           doing it is not read yet; the picture is the captures'. */
        r3d_clear_depth();
    }
    r3d_draw(v->far, &world);
    r3d_view selected = world;
    unsigned pass = v->screen_reflect ? 1 : split_mode ? 2 : 0;
    selected.model_list = gt4_course_vision_select(v->now->vision + pass, world.cam, NULL);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    if (!pregrid) r3d_shadow_map_bind(v->mapped_shadow_ready ? v->mapped_shadow : NULL);
#endif
    r3d_draw(v->world, &selected);
#ifndef MOON_DEKO3D
    road_sheen(v, &world);
#endif /* P: native Switch forward shading replaces the road reflection passes. */
    if (v->best_line_mesh || v->assist_mesh) {
        r3d_view line = world;
        line.unlit = 1;
        line.color_scale = 0;
        line.model_list = NULL;
        r3d_draw(v->best_line_mesh ? v->best_line_mesh : v->assist_mesh, &line);
    }
    /* 003E5338 / 003E4A28: paired shadow and normal GDGT models in the object frame,
       Env0 lighting from the original per-object RNW4 light bytes.
       Geometry/textures stay resident; each draw changes only its pose. */
    uint64_t lights_start = plat_time_ns();
    if (v->gadgets && v->now->gadgets && !rv_env("MOON_TEST_HIDE_GADGETS")) {
        gt4_course_gadgets *g = v->now->gadgets;
        for (unsigned pass = 0; pass < 2; pass++) {
            if (pass && (v->gadget_section < 0 || (unsigned)v->gadget_section >= g->sections))
                continue;
            unsigned slot = pass ? (unsigned)v->gadget_section + 1 : 0;
            for (unsigned j = 0; j < g->length[slot]; j++) {
                gt4_course_gadget *p = g->instance + g->first[slot] + j;
                if (!gt4_course_gadget_visible(p))
                    continue;
                r3d_view object = world;
                memcpy(object.pos, p->pos, 12);
                memcpy(object.rot, p->rot, 36);
                object.unlit = 0;
                object.color_scale = 0;
                r3d_view shadow;
                if (v->gadget_shadows && v->gadget_shadows[p->type] &&
                    !rv_env("MOON_TEST_HIDE_GADGET_SHADOWS") &&
                    gadget_shadow_view(v->now, p, &object, &shadow))
                    r3d_draw(v->gadget_shadows[p->type], &shadow);
                r3d_lights lights;
                car_light(&v->now->lights, v->now->light_fade, p->sun, p->shade, &lights);
                r3d_draw_lit(v->gadgets[p->type], &object, &lights);
            }
        }
    }
    video_profile_bucket(VIDEO_BUCKET_LIGHTS, plat_time_ns() - lights_start);
    video_profile_bucket(VIDEO_BUCKET_TRACK, lights_start - scene_start);
    car_lights = NULL;
    /* 00340950 +564/+59C: camera car requests 16, other cars 1. */
    car.brake = v->now->pilot[0].brake;
    moon_vk_gpu_mark("track_end");
    moon_vk_gpu_mark("car");
    uint64_t car_start = plat_time_ns();
    if (!pregrid && v->now->car && (split_entry || !v->hide_car)) {
        draw_car(v, v->now, &v->mine, v->now->car, v->now->surface, &car, &v->shadow[0],
                 v->pose_on && v->wheels_on, v->wheel_pose,
                 v->wheel_snapshot_on ? v->wheel_snapshot : NULL, v->wheel_interval,
                 v->wheel_brake_glow,
                 split_entry == 0 ? 16 : 1, v->now->pilot[0].steering, v->pit_crew[0],
                 &v->now->pilot[0]);
    }
    for (int i = 0; !pregrid && i < OPPONENTS; i++) {
        const car_pose *p = &v->other_pose[i];
        if (!v->now->other[i] || !p->on || (split_entry == i + 1 && v->hide_car))
            continue;
        r3d_view o = car;
        memcpy(o.rot, p->rot, sizeof o.rot);
        memcpy(o.pos, p->pos, sizeof o.pos);
        o.brake = v->now->pilot[i + 1].brake;
        /* the ghost: car and driver's depth first, then their nearest
           surface blended over the scene */
        o.ghost_alpha = p->ghost;
        for (int pass = p->ghost > 0; pass <= (p->ghost > 0 ? 2 : 0); pass++) {
            o.ghost = pass;
            draw_car(v, v->now, &v->other[i], v->now->other[i], v->now->surface, &o, &v->shadow[i + 1],
                     p->wheels_on, p->wheel_pose, p->snapshot_on ? p->snapshot : NULL, p->interval,
                     p->brake_glow,
                     split_entry == i + 1 ? 16 : 1, v->now->pilot[i + 1].steering,
                     v->pit_crew[i + 1], &v->now->pilot[i + 1]);
            if (i >= 5)
                phantom_draws++;
        }
    }
    video_profile_bucket(VIDEO_BUCKET_CAR, plat_time_ns() - car_start);
    moon_vk_gpu_mark("car_end");
    uint64_t tail_start = plat_time_ns();
    /* the backfire's flames after the cars, added over them */
    if (!pregrid && v->now->car && (split_entry || !v->hide_car) && v->flame_on[0])
        draw_afterfire(v, v->now->car, &car, v->flame[0], &v->shadow[0]);
    for (int i = 0; !pregrid && i < OPPONENTS; i++) {
        const car_pose *p = &v->other_pose[i];
        if (!v->now->other[i] || !p->on || p->ghost > 0 || !v->flame_on[i + 1])
            continue;
        r3d_view o = car;
        memcpy(o.rot, p->rot, sizeof o.rot);
        memcpy(o.pos, p->pos, sizeof o.pos);
        draw_afterfire(v, v->now->other[i], &o, v->flame[i + 1], &v->shadow[i + 1]);
    }
    if (!pregrid &&
        v->leaves) { /* func_003C2838 from the cars' draw (0033EAB8): each leaf, its picture */
        static race_leaves_quad leaves[128];
        static float corners[4 * 9];
        float rows[9];
        for (int k = 0; k < 3; k++) {
            rows[k] = world.r[k];
            rows[3 + k] = world.u[k];
            rows[6 + k] = -world.f[k];
        }
        int n = race_leaves_quads(v->leaves, world.cam, rows, leaves, 128);
        for (int q = 0; q < n; q++) {
            const r3d_sprite_texture *t = leaves[q].picture >= 0 && leaves[q].picture < 8
                                              ? v->leaf_tex[leaves[q].picture]
                                              : NULL;
            if (!t)
                continue;
            for (int c = 0; c < 4; c++) {
                float *o = corners + c * 9;
                memcpy(o, leaves[q].xyz[c], 3 * sizeof(float));
                memcpy(o + 3, leaves[q].uv[c], 2 * sizeof(float));
                memcpy(o + 5, leaves[q].rgb, 3 * sizeof(float));
                o[8] = 1;
            }
            r3d_draw_sprites(t, &world, corners, 1, 2);
        }
    }
    if (!pregrid && v->smoke && v->smoke_tex) { /* func_003E7CC8 from the cars' draw (0033F2E8) */
        static race_smoke_quad quads[256 * 3];
        static float corners[256 * 3 * 4 * 9];
        float rows[9];
        for (int k = 0; k < 3; k++) {
            rows[k] = world.r[k];
            rows[3 + k] = world.u[k];
            rows[6 + k] = -world.f[k];
        }
        int n = race_smoke_quads(v->smoke, world.cam, rows, quads, 256 * 3);
        for (int q = 0; q < n; q++)
            for (int c = 0; c < 4; c++) {
                float *o = corners + ((size_t)q * 4 + c) * 9;
                memcpy(o, quads[q].xyz[c], 3 * sizeof(float));
                memcpy(o + 3, quads[q].uv[c], 2 * sizeof(float));
                memcpy(o + 5, quads[q].rgba, 4 * sizeof(float));
            }
        r3d_draw_sprites(v->smoke_tex, &world, corners, n, race_smoke_blend(v->smoke) == 0x48);
    }
    if (!pregrid && v->strike_on)
        strike_draw(v, &world, pos, rot);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    if(!v->screen_reflect && !split_mode)r3d_shadow_map_volume(&world);
#endif
    r3d_end();
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    r3d_shadow_map_bind(NULL);
#endif
    video_profile_bucket(VIDEO_BUCKET_TRACK, plat_time_ns() - tail_start);
    return v->now->car != NULL;
}

gt4_course_gadgets *race_view_gadgets(race_view *v, int section)
{
    if (!v || !v->now)
        return NULL;
    v->gadget_section = section;
    return v->now->gadgets;
}

void race_view_set_smoke(race_view *v, const race_smoke *s)
{
    if (v)
        v->smoke = s;
}
void race_view_set_leaves(race_view *v, const race_leaves *l)
{
    if (v)
        v->leaves = l;
}

void race_view_set_strike(race_view *v, int on, double age, unsigned serial)
{
    if (!v)
        return;
    v->strike_on = on && age >= 0 && age < 1.05;
    v->strike_age = age;
    v->strike_seed = serial ? serial : 1u;
}

static int draw_scene(race_view *v, int pregrid, int menu)
{
    return draw_scene_box(v, pregrid, menu, 0, 0, 0, W, menu ? 480 : H);
}
double race_view_daylight_seconds(const race_view *v)
{
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    return v ? v->daylight_seconds : 0;
#else
    (void)v;
    return 0;
#endif
}
void race_view_daylight_time(race_view *v,double seconds)
{
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    if(v)v->daylight_seconds=seconds;
#else
    (void)v;(void)seconds;
#endif
}
int race_view_draw(race_view *v)
{
    split_entry = 0;
    return draw_scene(v, 0, 0);
}
int race_view_draw_player(race_view *v, int entry, int show, float y, float h)
{
    if (!v || entry < 0 || entry > OPPONENTS)
        return 0;
    int hidden = v->hide_car;
    split_mode = 1;
    split_entry = entry;
    v->hide_car = !show;
    int drawn = draw_scene_box(v, 0, 0, 0, 0, y, W, h);
    split_mode = 0;
    split_entry = 0;
    v->hide_car = hidden;
    return drawn;
}
int race_view_draw_mirror(race_view *v, int entry, float x, float y, float w, float h)
{
    if (!v || entry < 0 || entry > OPPONENTS)
        return 0;
    int hidden = v->hide_car, reflection = v->screen_reflect;
    split_entry = entry;
    v->hide_car = 1;
    v->screen_reflect = 1;
    int ok = draw_scene_box(v, 0, 0, 0, x, y, w, h);
    split_entry = 0;
    v->hide_car = hidden;
    v->screen_reflect = reflection;
    return ok;
}
static void photo_get(const race_view *v, photo_frame *f)
{
    memset(f, 0, sizeof *f);
    car_pose *c = &f->car[0];
    c->on = v->pose_on;
    c->wheels_on = v->wheels_on;
    c->snapshot_on = v->wheel_snapshot_on;
    memcpy(c->snapshot, v->wheel_snapshot, sizeof c->snapshot);
    memcpy(c->pos, v->pose_pos, sizeof c->pos);
    memcpy(c->rot, v->pose_rot, sizeof c->rot);
    memcpy(c->wheel_pose, v->wheel_pose, sizeof c->wheel_pose);
    memcpy(c->interval, v->wheel_interval, sizeof c->interval);
    memcpy(c->brake_glow, v->wheel_brake_glow, sizeof c->brake_glow);
    memcpy(f->car + 1, v->other_pose, sizeof v->other_pose);
    memcpy(f->shadow, v->shadow, sizeof v->shadow);
    if (v->now)
        memcpy(f->pilot, v->now->pilot, sizeof f->pilot);
    f->camera = v->cam_on;
    f->hide = v->hide_car;
    memcpy(f->eye, v->cam_eye, sizeof f->eye);
    memcpy(f->rows, v->cam_rows, sizeof f->rows);
    f->projection[0] = v->cam_sx;
    f->projection[1] = v->cam_sy;
    f->projection[2] = v->cam_near;
    f->far_clip = race_view_camera_far(v);
}
static void photo_put(race_view *v, const photo_frame *f)
{
    const car_pose *c = &f->car[0];
    v->pose_on = c->on;
    v->wheels_on = c->wheels_on;
    v->wheel_snapshot_on = c->snapshot_on;
    memcpy(v->wheel_snapshot, c->snapshot, sizeof c->snapshot);
    memcpy(v->pose_pos, c->pos, sizeof c->pos);
    memcpy(v->pose_rot, c->rot, sizeof c->rot);
    memcpy(v->wheel_pose, c->wheel_pose, sizeof c->wheel_pose);
    memcpy(v->wheel_interval, c->interval, sizeof c->interval);
    memcpy(v->wheel_brake_glow, c->brake_glow, sizeof c->brake_glow);
    memcpy(v->other_pose, f->car + 1, sizeof v->other_pose);
    memcpy(v->shadow, f->shadow, sizeof v->shadow);
    if (v->now)
        memcpy(v->now->pilot, f->pilot, sizeof f->pilot);
    v->cam_on = f->camera;
    v->hide_car = f->hide;
    memcpy(v->cam_eye, f->eye, sizeof f->eye);
    memcpy(v->cam_rows, f->rows, sizeof f->rows);
    v->cam_sx = f->projection[0];
    v->cam_sy = f->projection[1];
    v->cam_near = f->projection[2];
    v->cam_far = f->far_clip;
}
void race_view_photo_record(race_view *v)
{
    if (!v || v->photo_sampling)
        return;
    v->photo_previous = v->photo_current;
    photo_get(v, &v->photo_current);
    if (!v->photo_history)
        v->photo_previous = v->photo_current;
    v->photo_history = 1;
}
void race_view_photo_begin(race_view *v)
{
    if (v && !v->photo_sampling) {
        photo_get(v, &v->photo_saved);
        v->photo_sampling = 1;
    }
}
void race_view_photo_end(race_view *v)
{
    if (v && v->photo_sampling) {
        photo_put(v, &v->photo_saved);
        v->photo_sampling = 0;
    }
}
void race_view_photo_sample(race_view *v, float time, float eye[3], float rows[9])
{
    if (!v || !v->photo_sampling)
        return;
    photo_frame f = v->photo_saved;
    if (v->photo_history && time > 0) {
        for (int i = 0; i <= OPPONENTS; i++) {
            car_pose *a = &f.car[i];
            const car_pose *b = &v->photo_previous.car[i];
            if (a->on && b->on) {
                gt4_photo_lerp(a->pos, f.car[i].pos, b->pos, 3, time);
                gt4_photo_lerp(a->rot, f.car[i].rot, b->rot, 9, time);
                if (a->snapshot_on && b->snapshot_on)
                    for (int wheel = 0; wheel < 4; wheel++) {
                        float *q = a->snapshot[wheel];
                        const float *p = b->snapshot[wheel];
                        gt4_photo_lerp(q, q, p, 3, time);
                        /* 0045AAA0 +70..8C overrides interpolated wheel spin
                       with current spin minus the recorded angular delta. */
                        q[3] -= q[6] * time;
                        for (int k = 4; k < 6; k++)
                            q[k] = gt4_photo_angle(q[k] * 57.29577951308232f,
                                                   p[k] * 57.29577951308232f, time) *
                                   .01745329252f;
                    }
                gt4_photo_lerp(&a->wheel_pose[0][0], &f.car[i].wheel_pose[0][0],
                               &b->wheel_pose[0][0], 12, time);
            }
            if (f.shadow[i].on && v->photo_previous.shadow[i].on) {
                gt4_photo_lerp(f.shadow[i].pos, f.shadow[i].pos, v->photo_previous.shadow[i].pos, 3,
                               time);
                gt4_photo_lerp(f.shadow[i].rot, f.shadow[i].rot, v->photo_previous.shadow[i].rot, 9,
                               time);
            }
        }
        /* 0033B3B0 uses the camera's two nine-float frames. A: apply the
           stored camera delta to the user's edited camera, preserving its tool pose. */
        if (v->photo_current.camera && v->photo_previous.camera) {
            for (int k = 0; k < 3; k++)
                f.eye[k] += (v->photo_previous.eye[k] - v->photo_current.eye[k]) * time;
            for (int k = 0; k < 9; k++)
                f.rows[k] += (v->photo_previous.rows[k] - v->photo_current.rows[k]) * time;
        }
    }
    photo_put(v, &f);
    if (v->now && v->photo_history && time > 0)
        for (int i = 0; i <= OPPONENTS; i++) {
            const driver_actor *a = &v->photo_saved.pilot[i], *b = &v->photo_previous.pilot[i];
            float channels[64];
            if (!a->animation.initialized && !b->animation.initialized && a->ready && b->ready) {
                gt4_driver_skeleton skeleton;
                if (gt4_driver_skeleton_blend(&skeleton, &a->skeleton, &b->skeleton, time))
                    race_view_set_driver_snapshot(v, (unsigned)i, &skeleton);
                float grasp[2];
                gt4_photo_lerp(grasp, a->grasp, b->grasp, 2, time);
                race_view_set_driver_grasp(v, (unsigned)i, grasp);
            }
            if (a->animation.initialized && b->animation.initialized) {
                gt4_photo_lerp(channels, a->animation.channels, b->animation.channels, 64, time);
                race_view_set_driver_frame(v, (unsigned)i, channels);
            }
        }
    if (eye)
        memcpy(eye, v->cam_eye, 12);
    if (rows)
        memcpy(rows, v->cam_rows, 36);
}

void race_view_photo_jitter(race_view *v, const float j[2])
{
    if (v) {
        if (j)
            memcpy(v->photo_jitter, j, 8);
        else
            memset(v->photo_jitter, 0, 8);
    }
}
int race_view_photo_pose(const race_view *v, float p[3], float r[9])
{
    if (!v || !v->pose_on)
        return 0;
    memcpy(p, v->pose_pos, 12);
    memcpy(r, v->pose_rot, 36);
    return 1;
}
int race_view_draw_photo(race_view *v, float x, float y, float w, float h)
{
    return draw_scene_box(v, 0, 1, 1, x, y, w, h);
}
int race_view_draw_photo_map(race_view *v, const gt4_photo_mode *p, float x, float y, float w,
                             float h)
{
    if (!v || !v->photo_map || !p || p->location.map_length <= 0 || w <= 0 || h <= 0)
        return 0;
    /* A: orthographic map adapter. Authored MDLS retained, x/z becomes
       a flat UI plane at z=-1; native polygon highlighting remains F. */
    float scale = 2 / p->location.map_length;
    r3d_view view = {0};
    view.rot[0] = scale;
    view.rot[4] = scale * w / h;
    view.pos[0] = -p->eye[0] * scale;
    view.pos[1] = p->eye[2] * scale * w / h;
    view.pos[2] = -1;
    view.r[0] = 1;
    view.u[1] = 1;
    view.f[2] = -1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    view.color_scale = 2;
    view.near_clip = .01f;
    r3d_begin_ui(x, y, w, h);
    r3d_draw(v->photo_map, &view);
    r3d_end();
    return 1;
}
/* SceneViewFace (0012BC18), the pregrid's 640x480 world-only pass.
   001119A0 +0x24..+0x80 removes the race object before entering the menu.
   Cars and their cube reflection passes are unnecessary in this mode. */
int race_view_draw_pregrid(race_view *v)
{
    return draw_scene(v, 1, 1);
}
/* 0011D1B8 +0x44..0x60 keeps RaceLicense in Scene mode 2;
   0012BFE0 +0x28..0x7C draws its full scene (slots 348/358/360).
   The car/wheels/driver/reflections are retained, unlike 001119A0's
   empty Arcade menu. A: inherits the existing SceneViewFace pixel adapter. */
int race_view_draw_license_menu(race_view *v)
{
    return draw_scene(v, 0, 1);
}

/* The icon maker's other picture (func_00393D20's perspective branch, the
   96x64 one 00393790 draws for B-Spec's current_car with mode 0): the car
   turned at least 158 degrees about y (00393CF0 the rest) and 2 about x,
   seen through a narrow lens (half angle about 0.157, 004A59B8), lit as
   the side view. A: 22 degrees off its nose on its left (180 - 158), 5
   up, as the race's gap icon in a PCSX2 print (nearly head-on, the front
   at the picture's left),
   the size in the box from the Race Monitor print (00393CF0's own turn
   not read) */
void race_view_draw_icon_front_alpha(race_view *v, int i, float x, float y, float w, float h,
                                     float opacity)
{
    const car_model *a;
    const car_meshes *m;
    if (!v || !v->now || i < 0 || i > OPPONENTS || !r3d_available() || w <= 0 || h <= 0)
        return;
    a = i == 0 ? v->now->car : v->now->other[i - 1];
    m = i == 0 ? &v->mine : &v->other[i - 1];
    if (!a || !m->car)
        return;
    float c3[3], R = 0;
    for (int k = 0; k < 3; k++) {
        c3[k] = (m->lo[k] + m->hi[k]) * .5f;
        float e = (m->hi[k] - m->lo[k]) * .5f;
        R += e * e;
    }
    R = sqrtf(R);
    if (!(R > 0))
        return;
    const float yaw = -22 * 3.14159265f / 180, pitch = 5 * 3.14159265f / 180;
    const float t = tanf(0.157f), dist = R / t * 0.55f;
    float d[3] = {sinf(yaw) * cosf(pitch), sinf(pitch),
                  -cosf(yaw) * cosf(pitch)}; /* from the car to the eye */
    r3d_view c = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    for (int k = 0; k < 3; k++) {
        c.cam[k] = c3[k] + d[k] * dist;
        c.f[k] = -d[k];
    }
    { /* r = f x up, u = r x f */
        float up[3] = {0, 1, 0};
        c.r[0] = c.f[1] * up[2] - c.f[2] * up[1];
        c.r[1] = c.f[2] * up[0] - c.f[0] * up[2];
        c.r[2] = c.f[0] * up[1] - c.f[1] * up[0];
        float n = sqrtf(c.r[0] * c.r[0] + c.r[1] * c.r[1] + c.r[2] * c.r[2]);
        for (int k = 0; k < 3; k++)
            c.r[k] /= n;
        c.u[0] = c.r[1] * c.f[2] - c.r[2] * c.f[1];
        c.u[1] = c.r[2] * c.f[0] - c.r[0] * c.f[2];
        c.u[2] = c.r[0] * c.f[1] - c.r[1] * c.f[0];
    }
    /* the picture is 96x64 (00393B70), stretched to whatever box shows
       it (the gap's 64x32, func_003A6678) */
    c.focal = 320 * (64.f / 96) / t;
    c.focal_y = 224 / t;
    c.near_clip = dist - R * 2 > 0.1f ? dist - R * 2 : 0.1f;
    c.env_color[0] = v->now->env_plain[0];
    c.env_color[1] = v->now->env_plain[1];
    r3d_lights lights;
    memset(&lights, 0, sizeof lights);
    {
        float e[3] = {-10.5f, .5f, -1}, n = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
        for (int j = 0; j < 3; j++) {
            lights.direction[0][j] = (e[0] * c.r[j] + e[1] * c.u[j] - e[2] * c.f[j]) / n;
            lights.ambient[0][j] = .8f;
            lights.diffuse[0][j] = 1.5f;
            lights.specular[0][j] = 1;
        }
        lights.count = 1;
        lights.vertex_limit = 255.f / 128;
    }
    r3d_begin_ui(x, y, w, h);
    if (m->hull)
        r3d_draw_lit(m->hull, &c, &lights);
    r3d_draw_lit(m->car, &c, &lights);
    r3d_end_layer(opacity, 0);
}

void race_view_draw_icon_front(race_view *v, int i, float x, float y, float w, float h)
{
    race_view_draw_icon_front_alpha(v, i, x, y, w, h, 1);
}

/* A car's icon (the game's CarIconMaker, func_00393D20, the side view the
   results show): the model from its side, facing left, without
   perspective (a far camera), its length or height at 0.85 of the box
   (func_004A7890), its height's middle at the box's (func_004A7844). */
void race_view_draw_icon(race_view *v, int i, float x, float y, float w, float h)
{
    const car_model *a;
    const car_meshes *m;
    const float D = 200;
    if (!v || !v->now || i < 0 || i > OPPONENTS || !r3d_available() || w <= 0 || h <= 0)
        return;
    a = i == 0 ? v->now->car : v->now->other[i - 1];
    m = i == 0 ? &v->mine : &v->other[i - 1];
    if (!a || !m->car)
        return;
    float len = m->hi[2] - m->lo[2], ht = m->hi[1] - m->lo[1];
    if (!(len > 0 && ht > 0))
        return;
    /* The icon's reflection picture (func_00393D20, 0x00393E88..0x00393F40):
       a 128x128 target cleared black, its top 64 rows filled in eight bands
       of 8 from white to black ((7 - i) / 7), copied and bound as the
       model's environment (vtable +0xA8 -> func_004568F0 slot 0) */
    static uint8_t gradient[128 * 128 * 4];
    static tex1_image environment;
    if (!environment.rgba) {
        for (int y = 0; y < 128; y++)
            for (int x = 0; x < 128; x++) {
                uint8_t *p = gradient + (y * 128 + x) * 4;
                int c = y < 64 ? (int)((7 - y / 8) / 7.f * 255 + .5f) : 0;
                p[0] = p[1] = p[2] = (uint8_t)c;
                p[3] = 255;
            }
        environment.width = environment.height = environment.tex_w = environment.tex_h = 128;
        environment.rgba = gradient;
    }
    float k = .85f * fminf(w / len, h / ht); /* box units per metre */
    r3d_view c = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    c.cam[0] = -D;
    c.cam[1] = (m->lo[1] + m->hi[1]) * .5f;
    c.cam[2] = 0;
    c.r[0] = 0;
    c.r[1] = 0;
    c.r[2] = 1;
    c.u[0] = 0;
    c.u[1] = 1;
    c.u[2] = 0;
    c.f[0] = 1;
    c.f[1] = 0;
    c.f[2] = 0;
    c.focal = 320 * 2 * k / w * D;
    c.focal_y = 224 * 2 * k / h * D;
    c.near_clip = D - 50;
    /* the model's reflection colour as the race leaves it (00455464), in
       the icon's own target: not the frame, so not doubled */
    c.env_color[0] = v->now->env_plain[0];
    c.env_color[1] = v->now->env_plain[1];
    /* func_00394A10: light 1 alone, ambient 0.8, diffuse 1.5, specular 1
       (D_00621480/90/A0), its way (1, 0.5, -10.5) (D_00621470) normalized
       and taken through the modelview it is set under (func_004ABEC0):
       the side view's RotY(90) then its scale, s in the 2x target's pixels
       a metre across and 1 in depth. Read here as a direction again. */
    r3d_lights lights;
    memset(&lights, 0, sizeof lights);
    {
        float s = 2 * k, e[3] = {-10.5f * s, .5f * s, -1},
              n = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
        for (int j = 0; j < 3; j++) {
            lights.direction[0][j] = (e[0] * c.r[j] + e[1] * c.u[j] - e[2] * c.f[j]) / n;
            lights.ambient[0][j] = .8f;
            lights.diffuse[0][j] = 1.5f;
            lights.specular[0][j] = 1;
        }
        lights.count = 1;
        lights.vertex_limit = 255.f / 128; /* the icon's target is not doubled */
    }
    r3d_begin_ui_environment(x, y, w, h, &environment);
    if (m->hull)
        r3d_draw_lit(m->hull, &c, &lights);
    r3d_draw_lit(m->car, &c, &lights);
    r3d_end();
}

int race_view_photo_environment(race_view *v, const uint8_t *data, size_t size)
{
    /* 0033A150/0033A838 copy the complete Env0 parameter record (17F4).
    Reuse the renderer's existing course environment boundary, never mutate
    the borrowed course. A: lighting/reflection output still needs PS2 pixels. */
    if (!v || !v->now || !data || size != 0x17f4 || memcmp(data, "Env0", 4))
        return 0;
    memcpy(v->now->photo_environment, data, size);
    v->now->photo_environment_on = 1;
    uint8_t framed[0x90 + 0x17f4] = {0};
    uint32_t offset = 0x90;
    memcpy(framed + 0x80, &offset, 4);
    memcpy(framed + 0x90, data, size);
    course_lights(framed, sizeof framed, &v->now->lights, v->now->light_fade);
    course_fog(framed, sizeof framed, v->now);
    course_sheen(framed, sizeof framed, v->now);
    course_env_colour(framed, sizeof framed, v->now);
    course_shadow(framed, sizeof framed, v->now);
    v->env_ok = 0;
    return 1;
}
