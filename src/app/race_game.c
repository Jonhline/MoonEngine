/*
 * A race in the game (race_game.h). The physics is the native race of
 * src/core/physics (race_run.c); the player's car answers to the keyboard
 * or a game controller through the physics' host hooks (gt4_phys_pad), as
 * in tests/physics/drive.c: the keyboard as a digital pad, a controller's
 * stick and triggers as an analog one; the other cars keep the AI. The
 * view draws the course and every car where the physics has it.
 */
#include "platform/switch_profile.h"
#include "race_game.h"
#include "fields_conductor.h"
#include "race_run.h"
#include "race_view.h"
#include "race_hud.h"
#include "video_settings.h"
#include "license_session.h"
#include "license_presentation.h"
#include "license_concourse.h"
#include "license_motion.h"
#include "race_pit_crew.h"
#include "license_scene.h"
#include "license_image.h"
#include "car_model.h"
#include "career_license.h"
#include "career_original.h"
#include "career_bspec.h"
#include "fields_bspec.h"
#include "career_parts.h"
#include "logger.h"
#include "race_result.h"
#include "race_mission.h"
#include "render3d.h"
#include "video_gl.h"
#include "dev_overlay.h"
#include "screen_setting.h"
#include "race_pause.h"
#include "race_bspec.h"
#include "race_pitmenu.h"
#include "race_audio.h"
#include "race_smoke.h"
#include "race_leaves.h"
#include "pcm_music.h"
#include "course_motion.h"
#include "course_pregrid.h"
#include "course_tvcam.h"
#include "car.h"
#include "assist_line.h"
#include "specdb.h"
#include "arcade.h"
#include "vol.h"
#include "platform/platform.h"
#include "replay_file.h"
#include "disc_replay.h"
#include "disc_checkpoint.h"
#include "race_split_layout.h"
#include "save.h"
#include "plat_sdl.h"

#include <SDL.h>
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STEP (1.0 / 60)

/* race.field_size: the request the next menu race starts with; 0 (phantoms off, the
   default) keeps the cars the race was given, as the original. */
static int field_size_request = 0;

void race_game_set_field_size(int n)
{
    field_size_request = n;
}

struct race_game {
    char *disc;
    race_view *view;
    gt4_race_run *run;
    int loading, ready, others, laps, time_trial;
    int hud_preparing;
    int world_mode; /* GT Mode Free Run 11 / Practice 12 (race_game_set_world_mode) */
    int tire, manual, assist_asm, assist_tcs;
    int split;
    gt4_race_desc split_options;
    uint8_t split_owned[2][0x4b8], arcade_owned[0x4b8];
    int arcade_owned_on;
    race_hud *hud_2p;
    int camera_2p, view_held_2p, look_back_2p, retry_requested;
    int pause_keys_2p[7];
    float split_eye[2][3], split_rows[2][9], split_projection[2][3];
    int split_camera_valid[2];
    /* B-Spec: the next race's (COMMAND_SPECTATOR) and the pad's buttons
       last frame, its orders sent on a press */
    int bspec, run_bspec;
    menu *bspec_career;
    int bspec_band, bspec_committed;
    uint32_t bspec_buttons, bspec_pressed;
    race_bspec *bspec_ui;                       /* the race's B-Spec display (world+0xE424) */
    race_pitmenu *pitmenu;                      /* the pit menu (world+0xE40C) */
    int pit_car, pit_touched, pit_mode_allowed; /* the car it is up for, a key sent to it (+0xE4) */
    const race_hud_state *bspec_hud;            /* the display's state while it draws (its map) */
    /* the Arcade's QuickTune as the next race takes it (00111DC0) */
    int quick_tuned, quick_power, quick_weight, quick_front_tire, quick_rear_tire, quick_gear;
    double finished;
    unsigned ids[MOON_CONDUCTOR_CARS];
    double acc;
    SDL_GameController *pad;
    gt4_vol *vol; /* the display's assets */
    race_hud *hud;
    race_audio *audio;
    gt4_license_motion *pit_motion[MOON_CONDUCTOR_CARS];
    gt4_pit_crew pit_crew[MOON_CONDUCTOR_CARS];
    license_scene *pit_scene;
    uint32_t pit_random, pit_camera_random;
    int pit_package;
    int bgm_volume, bgm_enabled, se_volume;
    int unit;
    float speed_prev_ms, speed_accel, speed_lag[3], speed_shake_trauma, speed_noise_time;
    float assist_kmh; /* the live speed the assist ribbon is painted from */
    int assist_kmh_set;
    int camera; /* the view (the car model's camera point): 1 behind the car (the start's), 0 on it, 6 the roof */
    int view_held;
    /* Right-mouse free look around the authored chase camera. */
    struct {
        int held;
        float yaw, pitch;      /* radians, applied around the car */
        float return_left;     /* seconds left of the ease-out return */
        float gain;            /* 1 while held, eases to 0 after release */
    } free_look;
    int look_back;
    int ghost_button, ghost_hidden;
    int best_line, best_line_button; /* R3's retained state; Tweaks may force it */
    /* Double-tap brake, gated by the HUD speed: hold the second press to
       select reverse instead of the brake (release restores braking). */
    uint64_t brake_tap_ms;
    int brake_was_down, reverse_tap_armed;
    char hud_course[32]; /* the course whose map the display takes next */
    race_result *result; /* the results over the race, from the player's finish */
    int mission_presentation;
    FILE *mission_test_pad;
    r3d_mesh *trophy[3];
    tex1_image trophy_env[3]; /* each variant's own texture: its reflection map */
    int trophy_tried;
    plat_texture *license_screen;
    gt4_screen_setting screen_setting;
    float license_saturation;
    int license_image_options[4], license_image_valid, license_image_active;
    uint8_t license_image_lut[256][3];
    gt4_ray_hit license_surfaces[3];
    gt4_license_motion *license_motion;
    license_scene *license_scene;
    gt4_license_concourse concourse;
    gt4_license_fade license_fade;
    gt4_driver_car license_car;
    float license_origin[4], license_people[3][4];
    unsigned license_camera_cycle;
    race_pause *pause; /* the pause menu while the race is held */
    int quit;          /* the pause menu chose to leave */
    int replay_back_held;
    SDL_JoystickID pad_id;
    int pause_keys[7]; /* Start, D-pad and both pause confirmation buttons. */
    int confirm_held;
    char hud_names[MOON_CONDUCTOR_CARS][128]; /* 003B4D80 -> 00445450: CAR_NAME short string (+8). */
    char names[MOON_CONDUCTOR_CARS][96];          /* each car's name (CAR_NAME, the StrDB's) */
    gt4_motion_bank *start_cam; /* the course's start camera motions (header +0xD0) */
    int start_cam_tried, start_cam_on;
    gt4_license_motion *license_start_motion;
    float license_start_origin[4];
    int license_start_left, license_start_tried;
    gt4_pregrid *pregrid;
    gt4_tvcam *tvcam; /* the course's TV cameras (REP4 +0xB0) */
    gt4_dive *dive;   /* the replay's Dive mode on the course's camera points */
    int dive_tried, dive_had;
    float dive_prev[3];
    float cue;
    int cue_passed; /* the music's marks (race_audio_music_cue) */
    int tv_on, tv_tried;
    double pregrid_acc;
    int pregrid_tried;
    gt4_license_motion *license_menu_motion;
    unsigned license_menu_ticks;
    uint8_t *license_menu_tape;
    size_t license_menu_steps, license_menu_at;
    int license_menu_playing;
    gt4_race_desc license_menu_desc;
    gt4_license_menu_cycle license_menu_cycle;
    unsigned license_menu_cycles;
    float license_menu_origin[4], license_menu_pos[3], license_menu_rot[9],
        license_menu_wheels[4][3];
    float license_menu_distance;
    int inspect_on, inspect_seeded;
    float inspect_eye[3], inspect_yaw, inspect_pitch, inspect_projection[3];
    double film_clock; /* P: opt-in PC filming rig; never a physics clock */
    struct {
        int on, seeded, mode, target, aim, clean, paused, was_held, aperture, focus_auto;
        float eye[3], rows[9], projection[3], anchor[3], mount[3];
        float original_eye[3], original_rows[9], original_projection[3];
        int original_visible;
        float yaw, pitch, roll, velocity[3], focus, mouse[2];
        plat_texture *sample, *image;
    } cinema;
    /* The replay (func_003F59A0, the session's method 0xC8): the command
       read for the player's car each step goes to the world's recorder
       (+0x240, func_00426C50); the replay runs the same race again with
       those commands. The physics being the same step for step, the race
       comes out the same. */
    uint8_t *tape; /* 0x20 bytes a step */
    uint8_t thumb[GT4_REPLAY_THUMB_W * GT4_REPLAY_THUMB_H *
                  3]; /* the race's picture for a saved replay */
    int thumb_on;
    size_t tape_n, tape_cap, tape_at;
    uint8_t *tape_2p;
    size_t tape_n_2p, tape_cap_2p, tape_at_2p;
    unsigned license_test_frame; /* PC test-controller clock, not session state */
    gt4_logger_point *license_log_points;
    gt4_logger_sampler license_log;
    int license_log_collecting;
    /* the Logger's track B (entry 1: the ghost or the guide) and each
       entry's time when the collection ended */
    gt4_logger_point *log_b_points;
    char logger_loaded_car[2][96];
    int logger_loaded_power[2];
    gt4_logger_sampler log_b;
    uint32_t log_time[2];
    int log_analyzing; /* race_game_logger_analyze: its replay samples and has the ghost */
    size_t log_steps;  /* the analysis' steps so far */
    int photo_held;    /* Photo Drive retains the entire world between shots */
    double license_result_test_clock;
    unsigned license_result_test_sent;
    int replaying, replay_next;
    int legacy_replay_math; /* Native GT4R v1 playback only; live modes use EE math. */
    gt4_disc_replay *disc_replays;
    unsigned disc_segments, disc_segment, disc_segments_2p, disc_segment_2p;
    size_t disc_segment_at_2p[3];
    uint8_t disc_states_2p[3][0x14f4];
    size_t disc_segment_at[3];
    uint8_t disc_states[3][0x14f4];
    int demonstrating;
    struct {
        uint8_t *tape;
        size_t n, cap;
        int thumb_on;
        gt4_logger_sampler log;
    } before_demo;
    int held; /* the replay held (func_003C3B08's message): Select */
    /* the replay's controls (PCSX2, the user's map): Square the mode
       (Normal -> Dive -> Display, labels ReplayModeNormal/Dive/
       NormalDisplay), Circle the music (ReplayModeBGMOn/Off), Cross the
       car's camera, L3 the fast play (ReplayModeAccelerate/NormalSpeed),
       up/down the car watched */
    int rmode; /* 0 Normal, 1 Dive, 2 Display (the race's display) */
    int bgm_off, fast, car_cam, focus;
    float
        zoom; /* Triangle: the TV camera's lens times 4 (ShotManager +0x14C: 4.0 in rv_01, 1.0 elsewhere) */
    int zoom_on;          /* Triangle's flag (the view's +0x1E) */
    int start_events;     /* the start events (4) of the watched car seen */
    float zoom_spring[3]; /* 003852A0's spring: 1 to 4 over some frames */
    const char *label;    /* the plate's rtext key, shown for a while */
    double label_t;       /* its time left on (RaceReplayModeDisplay +0x54's envelope) */
    float label_v;        /* and its light */
    int label_out;
    int pad_held[8];
    long replay_open;  /* the opening's clock at the tape's start (frames), -1 unset */
    double replay_end; /* seconds since the tape ran out */
    /* the last race's lineup, for its replay */
    char last_course[32], last_car[96], last_others[MOON_CONDUCTOR_CARS - 1][96];
    const char *last_other_ptr[MOON_CONDUCTOR_CARS - 1];
    int last_color, last_colors[MOON_CONDUCTOR_CARS - 1], last_nothers, last_has_colors;
    int last_prepared;
    gt4_race_desc last_desc;
    uint8_t disc_entries[MOON_CONDUCTOR_CARS][0x228];
    uint8_t disc_race_options[0x60];
    uint8_t last_owned[MOON_CONDUCTOR_CARS][0x4b8], last_event_row[151];
    uint32_t last_rng;
    int ghost_car;       /* the Time Trial's: car 1 drawn while it drives */
    /* The licence's ghost: the player's car as it was drawn (simulation frame, place,
       rotation, wheels) through the best passed attempt, drawn see-through beside the
       next. Presentation only: no car is added to the simulation. */
    struct {
        float *s; /* LIC_SAMPLE floats each */
        size_t n, cap;
    } lic_rec;
    struct {
        float *s;
        size_t n, cap; /* cap: the buffer's room, kept when it swaps with the recording's */
        uint32_t time;
        char course[64], car[64];
        int on;
    } lic_ghost;
    race_smoke *smoke;   /* the tyres' smoke (the world's +0x31D0) */
    race_leaves *leaves; /* the leaves the wheels throw up (the world's +0x3238) */
    /* The Time Trial's ghost kept for the session (the PS2 keeps the best
       lap's recording while the Time Trial goes on, and its best-lap
       replay can be saved and loaded): the course and car it was driven
       with, the car at the lap's start, the start's remaining thirds and
       step fraction, its time and commands */
    struct {
        int on;
        char course[32], car[96];
        uint8_t state[GT4_GHOST_CAR];
        uint32_t rest, lap;
        float frac;
        uint8_t *tape;
        size_t steps;
    } ghost;
    int power_unit; /* the unit_power option (PS HP BHP CH kW CV PF) */
    /* the replay's opening (race_hud_draw_opening): the player's car as
       the race took it */
    int open_on, open_front, open_rear;
    float open_power;
};

static float speed_lens_scale(int view)
{
    if (view != 1 || !video_settings_speed_fov())
        return 1.f;
    float deg = (float)video_settings_speed_fov_degrees() * video_settings_speed_level();
    if (deg <= 0.f)
        return 1.f;
    if (deg > 30.f)
        deg = 30.f;
    const float rad = 0.017453292f;
    /* focal for 60+deg, relative to the stock 60-degree chase lens:
       1/tan(30) is the stock sx, so the reciprocal below widens, not narrows. */
    return tanf(30.f * rad) / tanf((60.f + deg) * .5f * rad);
}

static float speed_noise(float x, unsigned seed)
{
    float fi = floorf(x);
    int i = (int)fi;
    float f = x - fi;
    unsigned a = (unsigned)i * 0x9e3779b9u + seed, b = (unsigned)(i + 1) * 0x9e3779b9u + seed;
    a ^= a >> 16;
    a *= 0x85ebca6bu;
    a ^= a >> 13;
    b ^= b >> 16;
    b *= 0x85ebca6bu;
    b ^= b >> 13;
    float g0 = (float)(a & 0xffffu) / 32767.5f - 1.f;
    float g1 = (float)(b & 0xffffu) / 32767.5f - 1.f;
    float u = f * f * (3.f - 2.f * f);
    return (g0 * f + (g1 * (f - 1.f) - g0 * f) * u) * 2.f;
}

static void speed_camera_apply(race_game *g, int view, float eye[3], float rows[9], double dt)
{
    if (!g || view != 1 || g->replaying || dt <= 0)
        return;
    gt4_race_hud h;
    float speed = gt4_race_run_hud(g->run, 0, &h) ? h.speed : g->speed_prev_ms;
    float prev = g->speed_prev_ms;
    g->speed_prev_ms = speed;
    float accel = (speed - prev) / (float)dt;
    g->speed_accel += (accel - g->speed_accel) * (1.f - expf((float)-dt / .05f));
    if (video_settings_speed_cam_lag()) {
        float target = fmaxf(-.18f, fminf(.18f, -g->speed_accel * .006f));
        float k = 1.f - expf((float)-dt / .12f);
        for (int i = 0; i < 3; i++) {
            g->speed_lag[i] += (rows[6 + i] * target - g->speed_lag[i]) * k;
            eye[i] += g->speed_lag[i];
        }
    } else {
        for (int i = 0; i < 3; i++)
            g->speed_lag[i] *= .9f;
    }
    if (video_settings_speed_shake()) {
        float bump = fminf(1.f, fabsf(accel) * .01f + fabsf(speed - prev) * .02f);
        g->speed_shake_trauma = fminf(1.f, g->speed_shake_trauma + bump * .05f);
        g->speed_shake_trauma = fmaxf(0.f, g->speed_shake_trauma - (float)dt * .9f);
        float amp = .15f * g->speed_shake_trauma * g->speed_shake_trauma;
        g->speed_noise_time += (float)dt;
        float yaw = speed_noise(g->speed_noise_time * 7.f, 0x1234u) * amp;
        float pitch = speed_noise(g->speed_noise_time * 9.f + 13.f, 0x5678u) * amp;
        float a = yaw * .017453292f, ca = cosf(a), sa = sinf(a);
        for (int k = 0; k < 3; k++) {
            float r = rows[k], f = rows[6 + k];
            rows[k] = ca * r - sa * f;
            rows[6 + k] = sa * r + ca * f;
        }
        a = pitch * .017453292f;
        ca = cosf(a);
        sa = sinf(a);
        for (int k = 0; k < 3; k++) {
            float u = rows[3 + k], f = rows[6 + k];
            rows[3 + k] = ca * u - sa * f;
            rows[6 + k] = sa * u + ca * f;
        }
    }
}

/* Right-mouse free look: orbit the authored chase pose around the car, with
   no roll, and ease back to that pose when the button is released. Nothing
   is touched when the button is not held. */
#define FREE_LOOK_RETURN_SECONDS .4f
#define FREE_LOOK_YAW_PER_PIXEL .004f
#define FREE_LOOK_PITCH_PER_PIXEL .003f
#define FREE_LOOK_PITCH_MIN (-12.f * .017453292f)
#define FREE_LOOK_PITCH_MAX (60.f * .017453292f)
#define FREE_LOOK_CLEARANCE .5f
#define FREE_LOOK_MIN_RADIUS 2.f

static void free_look_rotate_y(float v[3], float a)
{
    float c = cosf(a), s = sinf(a), x = v[0], z = v[2];
    v[0] = c * x + s * z;
    v[2] = -s * x + c * z;
}

static void free_look_rotate_axis(float v[3], const float axis[3], float a)
{
    float c = cosf(a), s = sinf(a), d = axis[0] * v[0] + axis[1] * v[1] + axis[2] * v[2];
    float cx = axis[1] * v[2] - axis[2] * v[1];
    float cy = axis[2] * v[0] - axis[0] * v[2];
    float cz = axis[0] * v[1] - axis[1] * v[0];
    for (int k = 0; k < 3; k++)
        v[k] = v[k] * c + (k == 0 ? cx : k == 1 ? cy : cz) * s + axis[k] * d * (1.f - c);
}

static void free_look_apply(race_game *g, int view, float eye[3], float rows[9], double dt)
{
    if (!g)
        return;
    int can = view == 1 && g->ready && !g->replaying && !g->split && !g->cinema.on &&
              !g->start_cam_on && !g->tv_on && !g->inspect_on && !race_game_is_paused(g);
    if (!can) {
        if (g->free_look.held) {
            g->free_look.held = 0;
            if (!g->cinema.on)
                SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        g->free_look.yaw = g->free_look.pitch = 0;
        g->free_look.gain = g->free_look.return_left = 0;
        return;
    }
    int rmb = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_RMASK) != 0;
    if (rmb && !g->free_look.held) {
        g->free_look.held = 1;
        g->free_look.return_left = 0;
        g->free_look.gain = 1;
        SDL_SetRelativeMouseMode(SDL_TRUE);
        return; /* ignore the cursor's jump into relative mode */
    }
    if (!rmb && g->free_look.held) {
        g->free_look.held = 0;
        if (!g->cinema.on)
            SDL_SetRelativeMouseMode(SDL_FALSE);
        g->free_look.return_left = FREE_LOOK_RETURN_SECONDS;
    }
    if (g->free_look.held) {
        int dx = 0, dy = 0;
        SDL_GetRelativeMouseState(&dx, &dy);
        g->free_look.yaw -= (float)dx * FREE_LOOK_YAW_PER_PIXEL;
        /* Mouse up (dy < 0) looks up: the authored camera rose when the pitch
           grew, so the vertical axis is flipped here only. */
        g->free_look.pitch -= (float)dy * FREE_LOOK_PITCH_PER_PIXEL;
        if (g->free_look.pitch < FREE_LOOK_PITCH_MIN)
            g->free_look.pitch = FREE_LOOK_PITCH_MIN;
        if (g->free_look.pitch > FREE_LOOK_PITCH_MAX)
            g->free_look.pitch = FREE_LOOK_PITCH_MAX;
        g->free_look.gain = 1;
    } else if (g->free_look.return_left > 0) {
        g->free_look.return_left -= (float)dt;
        if (g->free_look.return_left <= 0) {
            g->free_look.return_left = 0;
            g->free_look.yaw = g->free_look.pitch = 0;
            g->free_look.gain = 0;
        } else {
            float t = g->free_look.return_left / FREE_LOOK_RETURN_SECONDS;
            g->free_look.gain = t * t; /* ease out of the orbit, into the authored pose */
        }
    } else
        g->free_look.gain = 0;
    if (g->free_look.gain <= 0 || (!g->free_look.yaw && !g->free_look.pitch))
        return;
    float car[3], rot[9];
    if (!g->run || !gt4_race_run_pose(g->run, 0, car, rot, NULL))
        return;
    float yaw = g->free_look.yaw * g->free_look.gain;
    float pitch = g->free_look.pitch * g->free_look.gain;
    float rel[3] = {eye[0] - car[0], eye[1] - car[1], eye[2] - car[2]};
    free_look_rotate_y(rel, yaw);
    for (int k = 0; k < 3; k++)
        free_look_rotate_y(rows + k * 3, yaw);
    float right[3] = {rows[0], rows[1], rows[2]};
    free_look_rotate_axis(rel, right, pitch);
    for (int k = 0; k < 3; k++)
        free_look_rotate_axis(rows + k * 3, right, pitch);
    float radius = sqrtf(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
    if (radius < FREE_LOOK_MIN_RADIUS && radius > 1e-4f) {
        float scale = FREE_LOOK_MIN_RADIUS / radius;
        for (int k = 0; k < 3; k++)
            rel[k] *= scale;
    }
    for (int k = 0; k < 3; k++)
        eye[k] = car[k] + rel[k];
    if (eye[1] < car[1] + FREE_LOOK_CLEARANCE)
        eye[1] = car[1] + FREE_LOOK_CLEARANCE;
}

/* a licence examination or training (desc.license 1, 2); a driving
   mission (3) races with the common display and results */
#define license_exam(g) ((g)->last_desc.license == 1 || (g)->last_desc.license == 2)
int race_game_mission_active(const race_game *g)
{
    return g && g->run && g->last_prepared && g->last_desc.license == 3;
}

/* the controller's command for the player's car, as the game reads it */
static int seed_bspec(race_game *g);
typedef struct {
    uint8_t flags, device, special;
    int8_t shift;
    int16_t steer, throttle, brake, reverse, handbrake;
} race_command;
static race_command command, command_2p;
static uint32_t player, player_2p;

static race_game *playing; /* the race whose player reads the controller */
static int host_read(void *u, uint32_t car, uint8_t *cmd, const uint8_t *ev)
{
    race_game *g = playing;
    (void)u;
    if (car != player && (!g || !g->split || car != player_2p))
        return 0;
    const race_command *input = car == player ? &command : &command_2p;
    if (g && g->license_menu_playing) {
        if (g->license_menu_at < g->license_menu_steps)
            memcpy(cmd, g->license_menu_tape + 32 * g->license_menu_at++, 32);
        else
            memset(cmd, 0, 32);
        return 1; /* 003EAAC0: teacher recorder in Scene mode 2, no player recording. */
    }
    if (g && g->replaying) {
        size_t *at = car == player ? &g->tape_at : &g->tape_at_2p;
        size_t n = car == player ? g->tape_n : g->tape_n_2p;
        const uint8_t *tape = car == player ? g->tape : g->tape_2p;
        if (*at < n)
            memcpy(cmd, tape + (*at)++ * 0x20, 0x20);
        else {
            memset(cmd, 0, 0x20);
            cmd[1] = 1;
            cmd[3] = ev[3];
        }
        return 1;
    }
    cmd[0] = input->flags;
    cmd[1] = input->device; /* 1 digital pad, 2 analog pad */
    cmd[2] = (uint8_t)input->shift;
    cmd[3] = ev[3];
    memcpy(cmd + 4, &input->steer, 2);
    memcpy(cmd + 8, &input->throttle, 2);
    memcpy(cmd + 0xa, &input->brake, 2);
    /* 00349F48/00348EE8: +C reverse throttle, +7 nitrous request. */
    memcpy(cmd + 0xc, &input->reverse, 2);
    cmd[7] = input->special;
    memcpy(cmd + 0xe, &input->handbrake, 2);
    /* 00426D50: the event block's orders (ev+0x10, ev[3] bytes: the pit's
       and, for a car of kind 0/1, the B-Spec driver's at +0x14/+0x15) */
    memcpy(cmd + 0x10, ev + 0x10, ev[3] <= 0x10 ? ev[3] : 0x10);
    /* Test-only pad tape; normal session rules determine the outcome.
       The recorded bytes never write car state or solver parameters. */
    if (g && g->mission_test_pad) {
        if (fread(cmd, 1, 32, g->mission_test_pad) != 32) {
            memset(cmd, 0, 32);
            cmd[1] = 1;
            cmd[3] = ev[3];
        }
    }
    /* the recording ends with the player's finish (func_003BA260's state 4
       -> func_003BA3E8: the recorders closed, func_00346498 sets +0x38 and
       func_00346138 records no more) */
    if (g && (g->finished < 0 || (g->split && !GT4_U8(player_2p + 0x5b6)))) {
        /* 00426C50: one recorder per human entry. Both clocks keep
           advancing until the two-player result, including neutral inputs. */
        uint8_t **tape = car == player ? &g->tape : &g->tape_2p;
        size_t *n = car == player ? &g->tape_n : &g->tape_n_2p,
               *capacity = car == player ? &g->tape_cap : &g->tape_cap_2p;
        if (*n == *capacity) {
            size_t cap = *capacity ? *capacity * 2 : 60 * 60 * 4;
            uint8_t *t = realloc(*tape, cap * 0x20);
            if (!t)
                return 1;
            *tape = t;
            *capacity = cap;
        }
        memcpy(*tape + (*n)++ * 0x20, cmd, 0x20);
    }
    return 1;
}

static int host_wheel(void *u, uint32_t car, int16_t state[2])
{
    (void)u;
    (void)state;
    return car == player || (playing && playing->split && car == player_2p);
}

static SDL_GameController *rumble_pad;
static int host_write(void *u, uint32_t car, const uint8_t *out)
{
    (void)u;
    int port = car == player ? 0 : playing && playing->split && car == player_2p ? 1 : -1;
    if (port < 0)
        return 0;
    /* 00353EE8 writes out[2], 0x80 centred. Publish only that byte;
       the original force calculation and steering response are untouched. */
    int force_on = playing && !playing->pause && !playing->photo_held && !playing->replaying &&
                   !playing->demonstrating;
    plat_input_set_ffb(port, force_on ? (128.f - out[2]) / 127.f : 0.f);
    int rumble_on =
        force_on && menu_option_number(NULL, port ? "vibration_2p" : "vibration_1p", 1);
    rumble_pad = plat_sdl_pad(port);
    /* Playback is not a driven pad. Send zero as well as gating new output,
       so a live-race pulse cannot remain active when playback takes over. */
    if (rumble_pad)
        SDL_GameControllerRumble(rumble_pad,
                                 rumble_on ? (uint16_t)(out[1] > 127 ? 65535 : out[1] * 516) : 0,
                                 rumble_on && out[0] ? 32768 : 0, 100);
    return 1;
}

race_game *race_game_open(const char *disc)
{
    race_game *g = calloc(1, sizeof *g);
    if (!g)
        return NULL;
    g->disc = malloc(strlen(disc) + 1);
    if (g->disc)
        strcpy(g->disc, disc);
    g->view = race_view_open(disc);
    if (!g->disc || !g->view) {
        race_game_close(g);
        return NULL;
    }
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    {
        const char *err = NULL;
        g->vol = gt4_vol_open(disc, &err);
    }
    g->unit = 1;       /* the option unit_velocity's default (MPH) */
    g->power_unit = 1; /* unit_power: HP */
    g->camera = 1;
    g->bgm_enabled = 1;
    g->pit_mode_allowed = 1;            /* 003BFB90 +50: race options +970 default. */
    g->bgm_volume = g->se_volume = 120; /* menu option defaults */
    return g;
}

#define LIC_SAMPLE 25 /* frame, place 3, rotation 9, wheels 12 */
#define licence_exam_run(g) ((g)->last_prepared && (g)->last_desc.license == 1)
/* a passed licence attempt (gold 2, silver 3, bronze 4) quicker than the one held
   becomes the licence's ghost */
static void keep_licence_ghost(race_game *g)
{
    int32_t result;
    uint32_t time;
    if (!g->run || g->replaying || !licence_exam_run(g) || g->lic_rec.n < 2 ||
        !race_game_license_result(g, &result, &time) || result < 2 || result > 4)
        return;
    int same = g->lic_ghost.on && !strcmp(g->lic_ghost.course, g->last_course) &&
               !strcmp(g->lic_ghost.car, g->last_car);
    if (same && g->lic_ghost.time <= time)
        return;
    /* swap the buffers with their own room: the recording goes on in the old ghost's */
    float *s = g->lic_ghost.s;
    size_t cap = g->lic_ghost.cap;
    g->lic_ghost.s = g->lic_rec.s;
    g->lic_ghost.n = g->lic_rec.n;
    g->lic_ghost.cap = g->lic_rec.cap;
    g->lic_rec.s = s;
    g->lic_rec.cap = s ? cap : 0;
    g->lic_rec.n = 0;
    g->lic_ghost.time = time;
    snprintf(g->lic_ghost.course, sizeof g->lic_ghost.course, "%s", g->last_course);
    snprintf(g->lic_ghost.car, sizeof g->lic_ghost.car, "%s", g->last_car);
    g->lic_ghost.on = 1;
    fprintf(stderr, "ghost: licence attempt %u kept (%zu poses)\n", time, g->lic_ghost.n);
}
/* the run's best lap kept as the session's ghost, when it is better */
static void keep_ghost(race_game *g)
{
    keep_licence_ghost(g);
    const uint8_t *state, *tape;
    uint32_t rest, lap;
    float frac;
    size_t steps;
    uint8_t *copy;
    if (!g->run || !g->time_trial || g->replaying ||
        !gt4_race_run_ghost_get(g->run, &state, &rest, &frac, &lap, &tape, &steps))
        return;
    if (g->ghost.on && !strcmp(g->ghost.course, g->last_course) &&
        !strcmp(g->ghost.car, g->last_car) && g->ghost.lap <= lap)
        return;
    if (!(copy = malloc(steps * 0x20)))
        return;
    memcpy(copy, tape, steps * 0x20);
    free(g->ghost.tape);
    g->ghost.tape = copy;
    g->ghost.steps = steps;
    memcpy(g->ghost.state, state, GT4_GHOST_CAR);
    g->ghost.rest = rest;
    g->ghost.frac = frac;
    g->ghost.lap = lap;
    snprintf(g->ghost.course, sizeof g->ghost.course, "%s", g->last_course);
    snprintf(g->ghost.car, sizeof g->ghost.car, "%s", g->last_car);
    g->ghost.on = 1;
}

int race_game_has_ghost(const race_game *g)
{
    return g && (g->ghost.on || (licence_exam_run(g) && g->lic_ghost.on));
}
/* the licence's result is in: a passed attempt is kept now, so SaveGhost can offer it */
void race_game_keep_licence_ghost(race_game *g)
{
    if (g)
        keep_licence_ghost(g);
}

/* the session's ghost as a best-lap replay (labomode's MODE_SAVE_BESTLAP_REPLAY),
   its commands and car lent */
int race_game_ghost_export(race_game *g, gt4_replay_file *r)
{
    if (!g || !r)
        return 0;
    keep_ghost(g);
    if (licence_exam_run(g) && g->lic_ghost.on) {
        /* P: the licence's ghost as a best-lap file: its poses, one neutral command */
        static uint8_t neutral[0x20] = {0, 1};
        memset(r, 0, sizeof *r);
        r->kind = GT4_REPLAY_BEST_LAP;
        snprintf(r->course, sizeof r->course, "%s", g->lic_ghost.course);
        snprintf(r->car, sizeof r->car, "%s", g->lic_ghost.car);
        r->color = g->last_color;
        r->laps = 1;
        r->best_lap = r->race_time = g->lic_ghost.time;
        r->tape = neutral;
        r->steps = 1;
        r->license_poses = g->lic_ghost.s;
        r->license_pose_count = g->lic_ghost.n;
        snprintf(r->ghost_license, sizeof r->ghost_license, "%s", g->last_desc.license_code);
        return 1;
    }
    if (!g->ghost.on)
        return 0;
    memset(r, 0, sizeof *r);
    r->kind = GT4_REPLAY_BEST_LAP;
    snprintf(r->course, sizeof r->course, "%s", g->ghost.course);
    snprintf(r->car, sizeof r->car, "%s", g->ghost.car);
    r->color = g->last_color;
    r->laps = 1;
    r->tire = g->tire;
    r->manual = g->manual;
    r->assist_asm = g->assist_asm;
    r->assist_tcs = g->assist_tcs;
    r->time_trial = 1;
    r->best_lap = r->race_time = g->ghost.lap;
    r->tape = g->ghost.tape;
    r->steps = g->ghost.steps;
    r->ghost_car = g->ghost.state;
    r->ghost_rest = g->ghost.rest;
    r->ghost_frac = g->ghost.frac;
    return 1;
}

/* a saved best lap as the session's ghost: only for the course and car it
   was driven with (the car as it was is copied into the race whole) */
int race_game_ghost_import(race_game *g, const gt4_replay_file *r)
{
    uint8_t *copy;
    if (g && r && r->kind == GT4_REPLAY_BEST_LAP && r->license_poses && r->license_pose_count > 1) {
        /* a licence's saved ghost: for the same exam, course and car */
        float *s;
        size_t bytes = r->license_pose_count * LIC_SAMPLE * sizeof(float);
        if (!licence_exam_run(g) || strcmp(r->course, g->last_course) ||
            strcmp(r->car, g->last_car) ||
            (r->ghost_license[0] && strcmp(r->ghost_license, g->last_desc.license_code)) ||
            !(s = malloc(bytes)))
            return 0;
        memcpy(s, r->license_poses, bytes);
        free(g->lic_ghost.s);
        g->lic_ghost.s = s;
        g->lic_ghost.n = g->lic_ghost.cap = r->license_pose_count;
        g->lic_ghost.time = r->best_lap;
        snprintf(g->lic_ghost.course, sizeof g->lic_ghost.course, "%s", r->course);
        snprintf(g->lic_ghost.car, sizeof g->lic_ghost.car, "%s", r->car);
        g->lic_ghost.on = 1;
        return 1;
    }
    if (!g || !r || r->kind != GT4_REPLAY_BEST_LAP || !r->ghost_car || !r->steps ||
        strcmp(r->course, g->last_course) || strcmp(r->car, g->last_car) ||
        !(copy = malloc(r->steps * 0x20)))
        return 0;
    memcpy(copy, r->tape, r->steps * 0x20);
    free(g->ghost.tape);
    g->ghost.tape = copy;
    g->ghost.steps = r->steps;
    memcpy(g->ghost.state, r->ghost_car, GT4_GHOST_CAR);
    g->ghost.rest = r->ghost_rest;
    g->ghost.frac = r->ghost_frac;
    g->ghost.lap = r->best_lap;
    snprintf(g->ghost.course, sizeof g->ghost.course, "%s", r->course);
    snprintf(g->ghost.car, sizeof g->ghost.car, "%s", r->car);
    g->ghost.on = 1;
    return 1;
}

void race_game_stop(race_game *g)
{
    if (!g)
        return;
    if (g->cinema.on)
        race_game_cinematic_toggle(g);
    plat_texture_destroy(g->cinema.sample);
    plat_texture_destroy(g->cinema.image);
    memset(&g->cinema, 0, sizeof g->cinema);
    plat_input_set_ffb(0, 0);
    plat_input_set_ffb(1, 0);
    for (int port = 0; port < 2; port++) {
        SDL_GameController *pad = plat_sdl_pad(port);
        if (pad)
            SDL_GameControllerRumble(pad, 0, 0, 0);
    }
    if (g->mission_test_pad) {
        fclose(g->mission_test_pad);
        g->mission_test_pad = NULL;
    }
    g->photo_held = 0;
    g->mission_presentation = GT4_MISSION_RUNNING;
    if (g->split || g->hud_2p) {
        race_hud_close(g->hud);
        g->hud = NULL;
        race_hud_close(g->hud_2p);
        g->hud_2p = NULL;
    }
    if (g->license_motion)
        g->license_camera_cycle = (unsigned)g->concourse.camera_cycle;
    license_scene_close(g->license_scene);
    g->license_scene = NULL;
    for (unsigned i = 0; i < MOON_CONDUCTOR_CARS; i++)
        race_view_set_pit_crew(g->view, i, NULL);
    license_scene_close(g->pit_scene);
    g->pit_scene = NULL;
    for (int i = 5; i >= 0; i--) {
        gt4_license_motion_close(g->pit_motion[i]);
        g->pit_motion[i] = NULL;
    }
    memset(g->pit_crew, 0, sizeof g->pit_crew);
    gt4_license_motion_close(g->license_motion);
    g->license_motion = NULL;
    memset(&g->concourse, 0, sizeof g->concourse);
    keep_ghost(g);
    race_audio_close(g->audio);
    g->audio = NULL;
    race_bspec_close(g->bspec_ui);
    g->bspec_ui = NULL;
    race_pitmenu_close(g->pitmenu);
    g->pitmenu = NULL;
    if (g->loading) /* the loader finishes, what it made dropped */
        while (race_view_poll(g->view, 1) == 0)
            SDL_Delay(5);
    gt4_pregrid_close(g->pregrid);
    g->pregrid = NULL;
    g->pregrid_acc = 0;
    g->pregrid_tried = 0;
    free(g->license_menu_tape);
    g->license_menu_tape = NULL;
    g->license_menu_playing = 0;
    g->license_menu_at = g->license_menu_steps = 0;
    gt4_license_motion_close(g->license_menu_motion);
    g->license_menu_motion = NULL;
    g->license_menu_ticks = 0;
    gt4_tvcam_close(g->tvcam);
    g->tvcam = NULL;
    g->tv_tried = 0;
    gt4_dive_close(g->dive);
    g->dive = NULL;
    g->dive_tried = g->dive_had = 0;
    race_view_clear(g->view);
    race_result_close(g->result);
    race_pause_close(g->pause);
    g->pause = NULL;
    g->quit = 0;
    g->result = NULL;
    for (int i = 0; i < 3; i++) {
        r3d_mesh_destroy(g->trophy[i]);
        g->trophy[i] = NULL;
        free(g->trophy_env[i].rgba);
        memset(&g->trophy_env[i], 0, sizeof g->trophy_env[i]);
    }
    g->trophy_tried = 0;
    gt4_license_motion_close(g->license_start_motion);
    g->license_start_motion = NULL;
    g->license_start_left = 0;
    g->license_start_tried = 0;
    gt4_motion_bank_close(g->start_cam);
    g->start_cam = NULL;
    g->start_cam_tried = g->start_cam_on = 0;
    g->inspect_on = g->inspect_seeded = 0;
    g->film_clock = 0;
    gt4_race_run_close(g->run);
    g->run = NULL;
    g->loading = g->ready = 0;
    g->hud_preparing = 0;
    g->ghost_car = 0;
    if (gt4_wheel_smoke_user == g) {
        gt4_wheel_smoke = NULL;
        gt4_wheel_leaves = NULL;
        gt4_wheel_smoke_user = NULL;
    }
    race_view_set_smoke(g->view, NULL);
    race_smoke_close(g->smoke);
    g->smoke = NULL;
    race_view_set_leaves(g->view, NULL);
    race_leaves_close(g->leaves);
    g->leaves = NULL;
    gt4_phys_pad.read = NULL;
    gt4_phys_pad.wheel_state = NULL;
    gt4_phys_pad.write = NULL;
}

void race_game_close(race_game *g)
{
    if (g) {
        free(g->tape);
        free(g->tape_2p);
    }
    if (playing == g)
        playing = NULL;
    if (!g)
        return;
    free(g->before_demo.tape);
    gt4_disc_replay_close(g->disc_replays);
    race_game_stop(g);
    race_view_close(g->view);
    race_hud_close(g->hud);
    race_hud_close(g->hud_2p);
    gt4_vol_close(g->vol);
    g->pad = NULL; /* platform owns controller handles, including hotplug */
    free(g->ghost.tape);
    free(g->lic_rec.s);
    free(g->lic_ghost.s);
    free(g->disc);
    free(g->license_log_points);
    free(g->log_b_points);
    plat_texture_destroy(g->license_screen);
    screen_setting_close(&g->screen_setting);
    free(g);
}

#include "race_game_license.inc"

static int race_game_start_impl(race_game *g, const char *course, const char *car, int color,
                                const char *const *others, const int *colors, int nothers,
                                const gt4_race_desc *prepared, char *msg, size_t len)
{
    if(!prepared || !prepared->owned_car[0] || prepared->license) g->bspec_career=NULL;
    gt4_race_desc d;
    const char *err = NULL;
    gt4_vol *vol;
    uint8_t *labels;
    size_t n = 0;
    char prepared_labels[MOON_CONDUCTOR_CARS][96];
    const char *prepared_others[MOON_CONDUCTOR_CARS - 1];
    int prepared_colors[MOON_CONDUCTOR_CARS - 1];
    int phantom_n = 0;
    char phantom_names[MOON_MAX_PHANTOMS][128] = {{0}};
    memset(&d, 0, sizeof d);
    msg[0] = 0;
    if (!g)
        return 0;
    race_game_stop(g);
    /* MOption.apply publishes MUnit globally (004365B8 +4C..68), so
       prepared license/mission/career races use the same applied units. */
    if (menu_option_text(NULL, "unit_velocity", NULL))
        race_game_apply_units(g, NULL);
    g->finished = -1;
    if (nothers > MOON_CONDUCTOR_CARS - 1) {
        fprintf(stderr,
                "race: %d opponents requested; the conductor takes %d official opponents, the "
                "rest are phantoms\n",
                nothers, MOON_CONDUCTOR_CARS - 1);
        nothers = MOON_CONDUCTOR_CARS - 1;
    }
    vol = gt4_vol_open(g->disc, &err);
    labels = vol ? gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &n) : NULL;
    if (prepared) {
        d = *prepared;
        course = d.course;
        nothers = d.cars - 1;
        for (int i = 0; i < d.cars; i++) {
            const char *label = labels ? gt4_specdb_label_for_id(labels, n, d.car[i]) : NULL;
            if (!label || strlen(label) >= sizeof prepared_labels[i]) {
                snprintf(msg, len, "unknown prepared car %u", d.car[i]);
                free(labels);
                gt4_vol_close(vol);
                return 0;
            }
            snprintf(prepared_labels[i], sizeof prepared_labels[i], "%s", label);
            if (i)
                prepared_others[i - 1] = prepared_labels[i];
        }
        car = prepared_labels[0];
        others = prepared_others;
    }
    if (!prepared)
        snprintf(d.course, sizeof d.course, "%s", course);
    snprintf(g->hud_course, sizeof g->hud_course, "%s", course);
    if (!prepared) {
        d.laps = getenv("MOON_RACE_LAPS") ? atoi(getenv("MOON_RACE_LAPS"))
                                             : g->laps; /* tests: a shorter race */
        d.tire = g->tire;
        d.time_trial = g->time_trial;
        d.world_mode = g->world_mode;
        if (g->arcade_owned_on)
            d.owned_car[0] = g->arcade_owned;
        if (g->split) {
            d.split_battle = 1;
            d.tire_2p = g->split_options.tire_2p;
            d.manual_2p = g->split_options.manual_2p;
            d.assist_asm_2p = g->split_options.assist_asm_2p;
            d.assist_tcs_2p = g->split_options.assist_tcs_2p;
            d.split_boost = g->split_options.split_boost;
            d.split_handicap = g->split_options.split_handicap;
            d.split_tire_damage = g->split_options.split_tire_damage;
            for (int i = 0; i < 2; i++)
                d.owned_car[i] = g->split_options.owned_car[i];
            d.time_trial = d.bspec = 0;
        }
        if (getenv("MOON_RACE_BSPEC"))
            g->bspec = 1; /* tests: an Arcade race watched in B-Spec */
        d.bspec = g->bspec;
        d.manual = g->manual;
        d.assist_asm = g->assist_asm;
        d.assist_tcs = g->assist_tcs;
        d.quick_tuned = g->quick_tuned;
        d.quick_power = g->quick_power;
        d.quick_weight = g->quick_weight;
        d.quick_front_tire = g->quick_front_tire;
        d.quick_rear_tire = g->quick_rear_tire;
        d.quick_gear = g->quick_gear;
        g->finished = -1;
        for (int i = 0; i <= nothers; i++) {
            const char *label = i ? others[i - 1] : car;
            if (!labels || !gt4_specdb_label(labels, n, label, &d.car[d.cars])) {
                snprintf(msg, len, "unknown car %s", label);
                free(labels);
                gt4_vol_close(vol);
                return 0;
            }
            d.cars++;
        }
    }
    {
        int field_size;
        if (prepared)
            field_size = d.field_size ? d.field_size : d.cars;
        else if (g->split)
            field_size = 2;
        else if (g->time_trial)
            field_size = 1;
        else
            field_size = field_size_request ? field_size_request : d.cars;
        if (field_size < 1 || field_size > MOON_MAX_CARS) {
            snprintf(msg, len, "race.field_size=%d refused: allowed 1..%d", field_size,
                     MOON_MAX_CARS);
            free(labels);
            gt4_vol_close(vol);
            return 0;
        }
        if (!prepared && !g->split && !g->time_trial && nothers > field_size - 1) {
            snprintf(msg, len,
                     "race.field_size=%d refused: %d opponents selected, but the field has %d cars",
                     field_size, nothers, field_size);
            free(labels);
            gt4_vol_close(vol);
            return 0;
        }
        if (field_size < d.cars) {
            snprintf(msg, len,
                     "race.field_size=%d refused: the prepared field has %d cars", field_size,
                     d.cars);
            free(labels);
            gt4_vol_close(vol);
            return 0;
        }
        d.field_size = field_size;
        phantom_n = field_size - d.cars;
        if (phantom_n > MOON_MAX_PHANTOMS) {
            snprintf(msg, len,
                     "race.field_size=%d refused: %d phantoms, this build has %d slots",
                     field_size, phantom_n, MOON_MAX_PHANTOMS);
            free(labels);
            gt4_vol_close(vol);
            return 0;
        }
        if (phantom_n > 0) {
            /* The real arcade menu asks PickUpEnemyCars for five cars at
               offset -0.025, range 0.005 around the player's rating. Use
               the same rated call for the phantoms, with the official cars
               removed from its pool. */
            const float pick_offset = -0.025f, pick_range = 0.005f;
            char used[MOON_CONDUCTOR_CARS + MOON_MAX_PHANTOMS][96];
            int nu = 0, got = 0;
            for (int i = 0; i < d.cars; i++) {
                const char *l = i ? others[i - 1] : car;
                snprintf(used[nu++], sizeof used[0], "%s", l ? l : "");
            }
            const char **pool = calloc(4096, sizeof *pool);
            int npool = 0;
            for (uint32_t id = 0; id < 4096 && pool; id++) {
                const char *l = labels ? gt4_specdb_label_for_id(labels, n, id) : NULL;
                if (!l || !*l)
                    continue;
                int official = 0;
                for (int i = 0; i < d.cars; i++)
                    if (d.car[i] == id)
                        official = 1;
                for (int i = 0; i < nu; i++)
                    if (!strcmp(l, used[i]))
                        official = 1;
                if (!official)
                    pool[npool++] = l;
            }
            gt4_arcade *arc = gt4_arcade_open(vol);
            gt4_car_ratings *ratings = gt4_car_ratings_open(g->disc);
            gt4_arcade_enemy picked[MOON_MAX_PHANTOMS];
            if (arc && ratings && pool) {
                got = gt4_arcade_pick_enemies(arc, gt4_car_ratings_get, ratings, car, pool,
                                              npool, pick_offset, pick_range, phantom_n, 0,
                                              picked);
                if (got < phantom_n) {
                    fprintf(stderr,
                            "race: rated phantom pool supplied %d of %d at offset %.3f range "
                            "%.3f; widening to the nearest ratings\n",
                            got, phantom_n, pick_offset, pick_range);
                    got = gt4_arcade_pick_enemies(arc, gt4_car_ratings_get, ratings, car, pool,
                                                  npool, 0.f, 1.f, phantom_n, 0, picked);
                }
            } else
                fprintf(stderr, "race: phantom ratings unavailable\n");
            if (got > 0) {
                float pr = 0, omin = 1e30f, omax = -1e30f, pmin = 1e30f, pmax = -1e30f;
                uint64_t pk = gt4_arcade_car_key(arc, car);
                if (pk != UINT64_MAX)
                    gt4_car_ratings_get(ratings, &pk, 1, &pr);
                fprintf(stderr,
                        "race: phantom ratings: player=%s %.6f offset=%.3f range=%.3f\n",
                        car ? car : "-", pr, pick_offset, pick_range);
                for (int i = 0; i < d.cars; i++) {
                    const char *l = gt4_specdb_label_for_id(labels, n, d.car[i]);
                    uint64_t k = l ? gt4_arcade_car_key(arc, l) : UINT64_MAX;
                    float r = 0;
                    if (k != UINT64_MAX && gt4_car_ratings_get(ratings, &k, 1, &r)) {
                        fprintf(stderr, "race: phantom ratings: official[%d]=%s %.6f\n", i,
                                l, r);
                        if (r < omin) omin = r;
                        if (r > omax) omax = r;
                    }
                }
                for (int j = 0; j < got; j++) {
                    uint64_t k = gt4_arcade_car_key(arc, picked[j].car);
                    float r = 0;
                    if (k != UINT64_MAX && gt4_car_ratings_get(ratings, &k, 1, &r)) {
                        fprintf(stderr, "race: phantom ratings: phantom[%d]=%s %.6f\n", j,
                                picked[j].car, r);
                        if (r < pmin) pmin = r;
                        if (r > pmax) pmax = r;
                    }
                }
                fprintf(stderr,
                        "race: phantom ratings spread: official %.6f..%.6f phantom %.6f..%.6f\n",
                        omin, omax, pmin, pmax);
            }
            free(pool);
            if (arc) gt4_arcade_close(arc);
            if (ratings) gt4_car_ratings_close(ratings);
            if (got < phantom_n) {
                snprintf(msg, len,
                         "race.field_size=%d refused: %d phantoms requested, %d rated opponents available",
                         field_size, phantom_n, got);
                free(labels);
                gt4_vol_close(vol);
                return 0;
            }
            for (int j = 0; j < got; j++) {
                uint32_t id = 0;
                if (!gt4_specdb_label(labels, n, picked[j].car, &id)) {
                    snprintf(msg, len, "race: phantom car %s has no GENERIC_CAR id",
                             picked[j].car);
                    free(labels);
                    gt4_vol_close(vol);
                    return 0;
                }
                d.phantom_car[j] = id;
                snprintf(d.phantom_label[j], sizeof d.phantom_label[j], "%s", picked[j].car);
                d.phantom_color[j] = picked[j].color;
            }
        }
    }
    free(labels);
    { /* the names the results show: CAR_NAME's first word, a StrDB string */
        size_t nn = 0, ns = 0;
        uint8_t *names =
            vol ? gt4_vol_read(vol, "/specdb/GT4_US2560/CAR_NAME_american.dbt", &nn) : NULL;
        uint8_t *strs =
            vol ? gt4_vol_read(vol, "/specdb/GT4_US2560/american_StrDB.sdb", &ns) : NULL;
        for (int i = 0; i < MOON_CONDUCTOR_CARS; i++) {
            uint8_t row[256];
            const char *s = NULL, *short_name = NULL;
            int row_size = i < d.cars && names && strs
                               ? gt4_specdb_row(names, nn, d.car[i], row, sizeof row) : 0;
            if (row_size >= 4)
                s = gt4_specdb_string(strs, ns,
                                      (uint32_t)row[0] | (uint32_t)row[1] << 8 |
                                          (uint32_t)row[2] << 16 | (uint32_t)row[3] << 24);
            snprintf(g->names[i], sizeof g->names[i], "%s", s ? s : "");
            /* 00445450 reads CAR_NAME +8; results/full names use +0. */
            if (row_size >= 12)
                short_name = gt4_specdb_string(strs, ns,
                    (uint32_t)row[8] | (uint32_t)row[9] << 8 |
                    (uint32_t)row[10] << 16 | (uint32_t)row[11] << 24);
            snprintf(g->hud_names[i], sizeof g->hud_names[i], "%s", short_name ? short_name : "");
        }
        for (int j = 0; j < phantom_n; j++) {
            uint8_t row[256];
            const char *short_name = NULL;
            int row_size = names && strs
                               ? gt4_specdb_row(names, nn, d.phantom_car[j], row, sizeof row) : 0;
            if (row_size >= 12)
                short_name = gt4_specdb_string(
                    strs, ns, (uint32_t)row[8] | (uint32_t)row[9] << 8 |
                                  (uint32_t)row[10] << 16 | (uint32_t)row[11] << 24);
            snprintf(phantom_names[j], sizeof phantom_names[j], "%s",
                     short_name ? short_name : d.phantom_label[j]);
        }
        free(names);
        free(strs);
    }
    gt4_vol_close(vol);
    for (int i = 0; i < MOON_CONDUCTOR_CARS; i++)
        g->ids[i] = i < d.cars ? d.car[i] : 0;
    d.pit_menu = !d.license; /* every race world but the licences' makes the pit menu (003DCA28) */
    if (d.split_battle)
        d.bspec = 0;
    g->split = d.split_battle;
    d.legacy_recording = g->replay_next && !g->demonstrating && g->legacy_replay_math;
    if (!g->replay_next)
        g->legacy_replay_math = 0;
    g->run = gt4_race_run_open(g->disc, &d, msg, len);
    if (!g->run)
        return 0;
    for (int i = 0; i < d.cars; i++)
        gt4_race_run_set_entrant_identity(g->run, i, g->hud_names[i] ? g->hud_names[i] : "",
                                          (uint32_t)(i ? (colors ? colors[i - 1] : 0) : color));
    for (int j = 0; j < phantom_n; j++)
        gt4_race_run_set_entrant_identity(g->run, d.cars + j, phantom_names[j],
                                          (uint32_t)d.phantom_color[j]);
    player = gt4_race_run_car(g->run, 0);
    player_2p = g->split ? gt4_race_run_car(g->run, 1) : 0;
    memset(&command, 0, sizeof command);
    memset(&command_2p, 0, sizeof command_2p);
    const char *mission_pad = getenv("MOON_MISSION_TEST_PAD");
    if (mission_pad && prepared && prepared->license == 3 && !g->replaying)
        g->mission_test_pad = fopen(mission_pad, "rb");
    memset(g->split_camera_valid, 0, sizeof g->split_camera_valid);
    g->camera_2p = 1;
    g->view_held_2p = g->look_back_2p = g->retry_requested = 0;
    memset(g->pause_keys_2p, 0, sizeof g->pause_keys_2p);
    if (d.license && !g->demonstrating)
        license_begin(g, car, d.cars);
    if (!g->replay_next && !g->log_analyzing)
        memset(g->logger_loaded_car, 0, sizeof g->logger_loaded_car);
    /* 0011D1B8 +28..38 isolates the retained attempt during the replay
       scene. Live telemetry (00122FA8 +9C..100) is enabled only below;
       retaining a tape must not retain the previous collection flag. */
    g->license_log_collecting = 0;
    if (!g->replay_next || g->log_analyzing) {
        g->license_log.count = 0;
        g->log_b.count = 0;
        g->log_time[0] = g->log_time[1] = 0;
    }
    if ((d.license || d.time_trial) && !g->demonstrating && (!g->replay_next || g->log_analyzing)) {
        /* 00122FA8 +0x9c..0x100 derives sample spacing from the runway and
           samples entries 0 and 1 (entry 0 again when there is no 1).
           A boundary: collect live physics instead of reconstructing replay.
           An absent demonstration stays absent. */
        if (!g->license_log_points)
            g->license_log_points = calloc(GT4_LOGGER_POINTS, sizeof *g->license_log_points);
        if (!g->log_b_points)
            g->log_b_points = calloc(GT4_LOGGER_POINTS, sizeof *g->log_b_points);
        if (!g->license_log_points || !g->log_b_points) {
            snprintf(msg, len, "cannot allocate licence telemetry");
            race_game_stop(g);
            return 0;
        }
        uint32_t r = GT4_U32(gt4_race_run_conductor(g->run)), c = GT4_U32(r + 0x80),
                 cd = GT4_U32(c + 4), rw = GT4_U32(cd + 0xdc);
        fenv_t env;
        fegetenv(&env);
        gt4_phys_fpu_enter();
        float length = GT4_F32(rw + 0x14);
        gt4_logger_begin(&g->license_log, g->license_log_points, gt4_logger_spacing(length),
                         length);
        gt4_logger_begin(&g->log_b, g->log_b_points, gt4_logger_spacing(length), length);
        fesetenv(&env);
        g->license_log_collecting = 1;
    }
    /* RaceTraining's second entry is the visible guide (003ECB68),
       while RaceLicense clones a ghost (003BDC88). */
    g->ghost_car = (d.time_trial || d.license == 1) && gt4_race_run_cars(g->run) == 2;
    g->lic_rec.n = 0;
    if (g->ghost_car && (!g->replay_next || g->log_analyzing) && g->ghost.on &&
        !strcmp(g->ghost.course, course) && !strcmp(g->ghost.car, car))
        gt4_race_run_ghost_set(g->run, g->ghost.state, g->ghost.rest, g->ghost.frac, g->ghost.lap,
                               g->ghost.tape, g->ghost.steps);
    if (prepared || (d.owned_car[0] || d.owned_car[1])) {
        /* 003BB5A8/0040AA48 finalize paints before presentation; models
           must use entry garage +0x18, not the raw RACES paint qword. */
        uint32_t race = GT4_U32(gt4_race_run_conductor(g->run)), entries = GT4_U32(race + 0x60);
        for (int i = 0; i < d.cars; i++) {
            if (!prepared && !d.owned_car[i]) {
                if (i)
                    prepared_colors[i - 1] = colors ? colors[i - 1] : 0;
                continue;
            }
            uint32_t entry = GT4_U32(GT4_U32(entries + 8) + i * 4);
            int paint = (int8_t)GT4_U8(entry + 0x38);
            if (i)
                prepared_colors[i - 1] = paint;
            else
                color = paint;
            d.event_paint[i] = (uint64_t)(int64_t)(paint + 1);
        }
        colors = prepared_colors;
    }
    /* tests: a PCSX2 capture's moment, frozen (render comparisons) */
    if (getenv("MOON_RACE_SNAPSHOT")) {
        static char snap[MOON_CONDUCTOR_CARS][48];
        static const char *snap_cars[MOON_CONDUCTOR_CARS];
        uint8_t *idi;
        size_t ni = 0;
        if (!gt4_race_run_restore(g->run, getenv("MOON_RACE_SNAPSHOT")))
            fprintf(stderr, "race: cannot read the capture %s\n", getenv("MOON_RACE_SNAPSHOT"));
        /* the captured lineup (race+0x60: the entries, each garage's car key at +0x20) */
        vol = gt4_vol_open(g->disc, &err);
        idi = vol ? gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &ni) : NULL;
        for (int i = 0; i < MOON_CONDUCTOR_CARS && i <= nothers; i++) {
            uint32_t race = GT4_U32(gt4_race_run_conductor(g->run)), ents = GT4_U32(race + 0x60);
            const char *s =
                i < (int)GT4_U32(ents)
                    ? gt4_specdb_label_for_id(idi, ni, GT4_U32(GT4_U32(ents + 0x24 + i * 4) + 0x20))
                    : NULL;
            snprintf(snap[i], sizeof snap[i], "%s", s ? s : (i ? others[i - 1] : car));
            snap_cars[i] = snap[i];
        }
        free(idi);
        gt4_vol_close(vol);
        car = snap_cars[0];
        others = snap_cars + 1;
        colors = NULL;
    }
    if (getenv("MOON_RACE_AUTOPILOT"))
        gt4_car_take_over(player, 5); /* tests: the model drives the player's car too */
    if (g->split && getenv("MOON_RACE_AUTOPILOT_2P"))
        gt4_car_take_over(gt4_race_run_car(g->run, 1),
                          5); /* P: second original model driver for unattended filming */
    if (getenv("MOON_RACE_VIEW"))
        g->camera = atoi(getenv("MOON_RACE_VIEW")); /* tests: the view to start in */
    g->run_bspec = d.bspec; /* the race built B-Spec (the world's +0xCC0..+0xCCC) */
    if (!g->replay_next) {  /* the lineup kept for the replay */
        g->disc_segments = g->disc_segment = g->disc_segments_2p = g->disc_segment_2p = 0;
        g->last_prepared = prepared != NULL || (d.owned_car[0] || d.owned_car[1]);
        if (g->last_prepared) {
            g->last_desc = d;
            if (d.event_row) {
                memmove(g->last_event_row, d.event_row, sizeof g->last_event_row);
                g->last_desc.event_row = g->last_event_row;
            }
            for (int i = 0; i < d.cars; i++)
                if (d.owned_car[i]) {
                    memmove(g->last_owned[i], d.owned_car[i], sizeof g->last_owned[i]);
                    g->last_desc.owned_car[i] = g->last_owned[i];
                }
            /* RaceLicense (003BBF90) supplies the examination entries,
               without the event opponent/paint RNG (0040AA48). */
            if (d.event_rng) {
                g->last_rng = *d.event_rng;
                g->last_desc.event_rng = &g->last_rng;
            }
        }
        snprintf(g->last_course, sizeof g->last_course, "%s", course);
        snprintf(g->last_car, sizeof g->last_car, "%s", car);
        g->last_color = color;
        g->last_nothers = nothers;
        g->last_has_colors = colors != NULL;
        for (int i = 0; i < nothers && i < MOON_CONDUCTOR_CARS - 1; i++) {
            snprintf(g->last_others[i], sizeof g->last_others[i], "%s", others[i]);
            g->last_other_ptr[i] = g->last_others[i];
            g->last_colors[i] = colors ? colors[i] : 0;
        }
    }
    g->replaying = g->replay_next;
    g->replay_next = 0;
    g->tape_at = 0;
    g->tape_at_2p = 0;
    g->license_test_frame = 0;
    g->license_result_test_clock = 0;
    g->license_result_test_sent = 0;
    g->replay_end = -1;
    g->replay_open = -1;
    g->held = 0;
    g->best_line = g->best_line_button = 0;
    g->bgm_off = g->fast = g->car_cam = g->focus = 0;
    g->zoom = 1;
    g->zoom_on = 0;
    memset(g->zoom_spring, 0, sizeof g->zoom_spring);
    g->start_events = 0;
    g->label = NULL;
    g->label_v = 0;
    memset(g->pad_held, 0, sizeof g->pad_held);
    if (!g->replaying) {
        g->tape_n = g->tape_n_2p = 0;
        g->thumb_on = 0;
    }
    g->open_on = 0;
    /* 0039BEC8 +FC..108: license modes 8/9 omit opening captions. */
    if (g->replaying &&
        !g->demonstrating) { /* before the first step: the ratings borrow the physics */
        uint8_t setup[0x178];
        gt4_car_ratings *cr = gt4_car_ratings_open(g->disc);
        g->open_on =
            cr && gt4_race_run_setup(g->run, 0, setup) &&
            gt4_car_ratings_setup(cr, setup, &g->open_power, &g->open_front, &g->open_rear);
        gt4_car_ratings_close(cr);
        if (getenv("MOON_RACE_REPLAY_LOG"))
            fprintf(stderr, "replay opening: %s power %.3f PS tyres %d/%d\n", g->names[0],
                    g->open_power, g->open_front, g->open_rear);
    }
    if(!seed_bspec(g)) { snprintf(msg,len,"B-Spec experience could not be loaded"); return 0; }
    playing = g;
    gt4_phys_pad.read = host_read;
    gt4_phys_pad.wheel_state = host_wheel;
    gt4_phys_pad.write = host_write;
    g->others = nothers;
    if ((g->time_trial || (prepared && d.license == 1)) &&
        !nothers) { /* the ghost: the player's car again */
        const char *ghost = car;
        race_view_set_opponents(g->view, &ghost, &color, 1);
    } else {
        int phantoms = gt4_race_run_phantom_count(g->run);
        if (phantoms > 0) {
            const char *render_others[MOON_MAX_OPPONENTS];
            int render_colors[MOON_MAX_OPPONENTS];
            int n = nothers;
            for (int i = 0; i < n; i++) {
                render_others[i] = others[i];
                render_colors[i] = colors ? colors[i] : 0;
            }
            for (int j = 0; j < phantoms; j++) {
                render_others[n + j] = d.phantom_label[j];
                render_colors[n + j] = d.phantom_color[j];
            }
            race_view_set_opponents(g->view, render_others, render_colors, n + phantoms);
        } else
            race_view_set_opponents(g->view, others, colors, nothers);
    }
    { /* 00390200/00390340 load fitted parts for each owned entry, not
           only P1. TT's duplicated ghost also uses the player's body. */
        char wheel[32] = "", wing[32] = "";
        gt4_vol *pv = gt4_vol_open(g->disc, &err);
        for (int entry = 0; entry < d.cars; entry++) {
            const uint8_t *body = d.owned_car[entry];
            if (entry == 1 && g->time_trial && !nothers)
                body = d.owned_car[0];
            wheel[0] = wing[0] = 0;
            if (pv && body)
                gt4_body_part_labels(pv, body, wheel, wing);
            race_view_set_film_parts(g->view, (unsigned)entry, wheel, wing);
        }
        /* A Time Trial run may describe one entry while the view adds ghost. */
        if ((g->time_trial || (prepared && d.license == 1)) && !nothers && d.cars == 1) {
            wheel[0] = wing[0] = 0;
            if (pv && d.owned_car[0])
                gt4_body_part_labels(pv, d.owned_car[0], wheel, wing);
            race_view_set_film_parts(g->view, 1, wheel, wing);
        }
        if (pv)
            gt4_vol_close(pv);
    }
    if (!race_view_request(g->view, course, car, color)) {
        snprintf(msg, len, "cannot load course %s", course);
        race_game_stop(g);
        return 0;
    }
    g->loading = 1;
    g->acc = 0;
    g->pad = plat_sdl_pad(0);
    rumble_pad = g->pad;
    return 1;
}

int race_game_start(race_game *g, const char *course, const char *car, int color,
                    const char *const *others, const int *colors, int nothers, char *msg,
                    size_t len)
{
    if (g)
        g->pit_mode_allowed = 1;
    return race_game_start_impl(g, course, car, color, others, colors, nothers, NULL, msg, len);
}
int race_game_start_prepared(race_game *g, const gt4_race_desc *d, char *msg, size_t len)
{
    if (!d || !d->event_row || (!d->license && (!d->event_rng || !d->owned_car[0])) ||
        d->cars < 1 || d->cars > 6) {
        snprintf(msg, len, "incomplete prepared career race");
        return 0;
    }
    race_game_set_split(g, NULL);
    if (g)
        g->pit_mode_allowed = 1;
    return race_game_start_impl(g, d->course, NULL, 0, NULL, NULL, d->cars - 1, d, msg, len);
}
int race_game_restart(race_game *g, char *msg, size_t len)
{
    if (!g || !g->last_course[0]) {
        snprintf(msg, len, "no race to restart");
        return 0;
    }
    if (g->last_prepared)
        return race_game_start_impl(g, g->last_desc.course, NULL, 0, NULL, NULL,
                                    g->last_desc.cars - 1, &g->last_desc, msg, len);
    return race_game_start(g, g->last_course, g->last_car, g->last_color, g->last_other_ptr,
                           g->last_has_colors ? g->last_colors : NULL, g->last_nothers, msg, len);
}
int race_game_setting(race_game *g, const uint8_t body[0x4b8])
{
    if (!g || !g->last_prepared || g->last_desc.license || !body)
        return 0;
    uint32_t slot;
    memcpy(&slot, body + 0x488, 4);
    if (slot >= 3)
        return 0;
    /* 003EE3C8 / 00444840: the next attempt rebuilds the player from
    the selected sheet of the garage car; retain the opponent grid. */
    memcpy(g->last_owned[0], body, 0x4b8);
    g->last_desc.owned_car[0] = g->last_owned[0];
    g->last_desc.tuned_car[0] = 0;
    return 1;
}

/* func_003667A8 -> 00395C70 -> 003E75E8: each car's wheels, but the
   Time Trial's ghost (A: whether its copy of the car smokes is not read) */
static void smoke_wheel(void *user, uint32_t car, uint32_t e)
{
    race_game *g = user;
    race_smoke_wheel w;
    if (!g->smoke || !g->run || (g->ghost_car && car == gt4_race_run_car(g->run, 1)))
        return;
    w.wheel = (int)GT4_U32(e);
    w.surface = (int)GT4_U32(e + 4);
    for (int k = 0; k < 3; k++)
        w.pos[k] = GT4_F32(e + 8 + 4 * (uint32_t)k);
    w.shade = GT4_F32(e + 0x18);
    for (int k = 0; k < 4; k++)
        w.plane[k] = GT4_F32(e + 0x1c + 4 * (uint32_t)k);
    w.amount = GT4_F32(e + 0x2c);
    race_smoke_emit(g->smoke, &w);
}

/* func_003667A8 -> 00395C98 -> 003C2160: a wheel on a leaf triangle */
static void leaf_wheel(void *user, uint32_t car, uint32_t e, uint32_t vel)
{
    race_game *g = user;
    race_leaves_wheel w;
    if (!g->leaves || !g->run || (g->ghost_car && car == gt4_race_run_car(g->run, 1)))
        return;
    for (int k = 0; k < 3; k++) {
        w.pos[k] = GT4_F32(e + 8 + 4 * (uint32_t)k);
        w.vel[k] = GT4_F32(vel + 4 * (uint32_t)k);
    }
    w.heading = GT4_F32(e + 0x14);
    w.shade = GT4_F32(e + 0x18);
    for (int k = 0; k < 4; k++)
        w.plane[k] = GT4_F32(e + 0x1c + 4 * (uint32_t)k);
    w.spin = GT4_F32(e + 0x34);
    w.ground = GT4_F32(e + 0x38);
    race_leaves_emit(g->leaves, &w);
}

/* the smoke's classes from the course's Env0 (header +0x80) */
static void start_smoke(race_game *g)
{
    const gt4_course *c = race_view_course(g->view);
    uint32_t env;
    if (!c || c->size < 0x84 || g->smoke)
        return;
    memcpy(&env, c->data + 0x80, 4);
    if (!env || env >= c->size)
        return;
    g->smoke = race_smoke_open(c->data + env, c->size - env);
    if (!g->smoke)
        return;
    race_view_set_smoke(g->view, g->smoke);
    gt4_wheel_smoke = smoke_wheel;
    gt4_wheel_smoke_user = g;
    g->leaves = race_leaves_open(c->data + env, c->size - env);
    race_view_set_leaves(g->view, g->leaves);
    if (g->leaves)
        gt4_wheel_leaves = leaf_wheel;
}

static int prepare_hud(race_game *g);

int race_game_poll(race_game *g)
{
    int r;
    if (!g || !g->loading)
        return g && g->ready ? 1 : -1;
    r = g->hud_preparing ? 1 : race_view_poll(g->view, 0);
    if (r == 0)
        return 0;
    if (r > 0) {
        g->hud_preparing = 1;
        if (!prepare_hud(g))
            return 0; /* One font/upload batch per loading frame, before reveal. */
        g->hud_preparing = 0;
    }
    g->loading = 0;
    if (r < 0) {
        race_game_stop(g);
        return -1;
    }
    g->ready = 1;
    /* 00394ED8 / 003CC7B0: COURSE selects the resident pit package.
       One immutable package, six small independent clock workspaces. */
    if (!g->split && !race_game_license_active(g)) {
        gt4_arcade *catalog = gt4_arcade_open(g->vol);
        g->pit_package = gt4_arcade_course_pit(catalog, g->last_course);
        gt4_arcade_close(catalog);
        size_t bytes = 0;
        uint8_t *package =
            g->pit_package
                ? gt4_vol_read(g->vol, g->pit_package == 2 ? "/pit/pit_002" : "/pit/pit_001",
                               &bytes)
                : NULL;
        g->pit_motion[0] = gt4_license_motion_open_pit(package, bytes);
        free(package);
        g->pit_random = 0xfffffff0u;        /* PitWorld constructor 003CC608 +11c. */
        g->pit_camera_random = 0xfffffff0u; /* 003CF198 +74 seeds camera RNG. */
        if (g->pit_motion[0])
            g->pit_scene = license_scene_open_pit(g->pit_motion[0]);
        if (g->pit_motion[0])
            for (int i = 0; i < gt4_race_run_cars(g->run); i++) {
                if (i)
                    g->pit_motion[i] = gt4_license_motion_clone(g->pit_motion[0]);
                gt4_pit_crew_init(g->pit_crew + i, g->pit_motion[i]);
                gt4_pit_crew_place(g->pit_crew + i, gt4_race_run_car(g->run, i), (unsigned)i,
                                   race_view_car_geometry(g->view, i), g->pit_package == 1,
                                   &g->pit_random);
                race_view_set_pit_crew(g->view, (unsigned)i, g->pit_crew + i);
            }
        if (g->pit_motion[0])
            for (unsigned i = 0; i < (unsigned)gt4_race_run_cars(g->run); i++) {
                gt4_pit_crew *c = g->pit_crew + i;
                float distance = 0;
                if (i) {
                    float dx = c->box[0] - g->pit_crew[i - 1].box[0],
                          dz = c->box[2] - g->pit_crew[i - 1].box[2];
                    distance = dx * dx + dz * dz;
                }
                /* 003CEB90 +38C..44C: shared actors for boxes 15/25 m apart. */
                c->walk_spacing = i ? (distance <= 225 ? 2 : distance <= 625 ? 1 : 0) : 0;
                gt4_pit_walk_init(c->walking, c->car, c->variant, gt4_pit_stage(c->car),
                                  c->walk_spacing, &g->pit_random);
            }
    }
    /* 0033BDB0 +114..124 / 00395B60: section from the built RaceCourse,
       including global objects. Ray queries only read its surface data. */
    uint32_t course = GT4_U32(gt4_race_run_car(g->run, 0) + 8);
    gt4_course_gadgets *objects = race_view_gadgets(g->view, (int32_t)GT4_U32(course + 0xb0));
    if (objects) {
        int section = (int32_t)GT4_U32(course + 0xb0);
        uint32_t rw = GT4_U32(GT4_U32(course + 4) + 0xdc);
        for (unsigned pass = 0; pass < 2; pass++) {
            if (pass && (section < 0 || (unsigned)section >= objects->sections))
                continue;
            unsigned slot = pass ? (unsigned)section + 1 : 0;
            for (unsigned j = 0; j < objects->length[slot]; j++) {
                gt4_course_gadget *object = objects->instance + objects->first[slot] + j;
                gt4_ray_hit hit;
                float pos[3] = {object->pos[0], object->pos[1] + 1, object->pos[2]};
                if (gt4_course_ray_down(rw, &hit, pos))
                    gt4_course_gadget_surface(object, hit.normal, hit.plane, hit.u, hit.v);
            }
        }
    }
    if (g->last_prepared && license_exam(g)) {
        /* 00387FB0 reconstructs the menu Scene after an attempt. Keep
           its initial presentation pose independently of the result's
           concourse handover. A: reset order still needs a menu oracle. */
        gt4_race_run_pose(g->run, 0, g->license_menu_pos, g->license_menu_rot,
                          g->license_menu_wheels);
        gt4_race_run_place(g->run, 0, g->license_menu_origin, g->license_menu_origin + 3);
        g->license_menu_distance = GT4_F32(gt4_race_run_car(g->run, 0) + 0x5a8);
    }
    start_smoke(g);
    return 1;
}

int race_game_active(const race_game *g)
{
    return g && (g->loading || g->ready);
}
unsigned race_game_car_id(const race_game *g, int i)
{
    return g && i >= 0 && i < MOON_CONDUCTOR_CARS ? g->ids[i] : 0;
}
int race_game_player_hud(const race_game *g, gt4_race_hud *hud)
{
    return g && g->run && gt4_race_run_hud(g->run, 0, hud);
}

void race_game_set_career(race_game *g, menu *career)
{
    if (g)
        g->bspec_career = career;
}
static int seed_bspec(race_game *g)
{
    g->bspec_committed = 0;
    if (!g->bspec_career || g->replaying)
        return 1;
    uint8_t experience[0x180], skill[8], style[CAR_BSPEC_STYLE_SIZE];
    if (!menu_bspec_profile(g->bspec_career, g->last_desc.course, experience, skill, 0))
        return 0;
    uint32_t car = gt4_race_run_car(g->run, 0), cond = gt4_race_run_conductor(g->run);
    gt4_phys_fpu_enter();
    g->bspec_band = gt4_career_bspec_band(GT4_F32(GT4_U32(car + CAR_PARAMS) + BSPEC_PARAMS_RATING));
    gt4_career_bspec_seed(experience, skill, g->bspec_band, style);
    /* Original: func_00468710 / func_003F1E60 +0x94, before driver sampling. */
    memcpy(gt4_ptr(GT4_U32(cond) + BSPEC_SESSION_STYLE), style, sizeof style);
    gt4_race_player_styles(cond);
    return 1;
}
int race_game_commit_bspec(race_game *g, menu *career)
{
    if (!g || !g->run || !career || g->replaying)
        return 0;
    if (g->bspec_committed)
        return 1;
    uint8_t experience[0x180], skill[8], style[CAR_BSPEC_STYLE_SIZE];
    if (!menu_bspec_profile(career, g->last_desc.course, experience, skill, 0))
        return 0;
    uint32_t car = gt4_race_run_car(g->run, 0), cond = gt4_race_run_conductor(g->run);
    /* Original: func_00468A40 +0x14 / func_00468A60 +0x34..0x74 */
    int kind = GT4_U8(cond + CONDUCTOR_FIELD_BSPEC_COURSE_KIND) ? 1 : -1;
    int bonus = !g->last_desc.time_trial && g->last_desc.cars >= 2 && GT4_U8(car + CAR_5B6) &&
                (int8_t)GT4_U8(car + CAR_FIELD_POSITION) == 1;
    memcpy(style, gt4_ptr(car + CAR_FIELD_DRIVING_STYLE), sizeof style);
    gt4_career_bspec_commit(experience, skill, g->bspec_band, kind, bonus, style);
    if (!menu_bspec_profile(career, g->last_desc.course, experience, skill, 1))
        return 0;
    g->bspec_committed = 1;
    return 1;
}
int race_game_player_wear(const race_game *g, uint8_t wear[32])
{
    if (!g || !g->run || !wear)
        return 0;
    uint32_t car = gt4_race_run_car(g->run, 0);
    if (!car)
        return 0;
    /* 0010FAA8..0010FB24: four eight-byte copies from car+0x6dc. */
    memcpy(wear, gt4_ptr(car + CAR_ENGINE_WEAR), 32);
    return 1;
}
int race_game_player_distances(const race_game *g, uint32_t *a, uint32_t *b)
{
    if (!g || !g->run || !a || !b)
        return 0;
    uint32_t car = gt4_race_run_car(g->run, 0);
    if (!car)
        return 0;
    /* 0035DDB0: kind 0 subtracts B-spec mileage, kind 1 returns it. */
    gt4_phys_fpu_enter();
    float bdistance = GT4_F32(car + CAR_7B0);
    *a = gt4_career_distance_aspec(GT4_F32(car + CAR_DISTANCE), bdistance);
    *b = gt4_career_distance_integer(bdistance);
    return 1;
}
int race_game_driver_mode(const race_game *g)
{
    if (!g || !g->run)
        return 0;
    return (int)GT4_U32(GT4_U32(GT4_U32(gt4_race_run_conductor(g->run)) + 0x84) + 0xcc8);
}
int race_game_championship_grid(const race_game *g, gt4_race_desc *desc, uint8_t bodies[MOON_CONDUCTOR_CARS][0x4b8])
{
    if (!g || !g->run || !g->last_prepared || !desc || !bodies)
        return 0;
    *desc = g->last_desc;
    memset(bodies, 0, 6 * 0x4b8);
    uint32_t race = GT4_U32(gt4_race_run_conductor(g->run)), entries = GT4_U32(race + 0x60);
    for (int i = 0; i < desc->cars; i++) {
        if (i == 0)
            memcpy(bodies[i], g->last_owned[0], 0x4b8);
        uint32_t entry = GT4_U32(GT4_U32(entries + 8) + i * 4);
        /* 003EEC70 retains the championship entrants between rounds. */
        memcpy(bodies[i], gt4_ptr(entry + 0x20), 0x178);
        desc->owned_car[i] = bodies[i];
        desc->tuned_car[i] = 0;
    }
    return 1;
}
int race_game_finish_order(const race_game *g, uint8_t order[MOON_CONDUCTOR_CARS])
{
    if (!g || !g->run || !order)
        return 0;
    int ranks[6], n = gt4_race_run_cars(g->run);
    /* 003B6FD0 uses stable signed car+0x5b3 ordering. */
    for (int i = 0; i < n; i++) {
        gt4_race_hud hud;
        if (!gt4_race_run_hud(g->run, i, &hud))
            return 0;
        ranks[i] = hud.rank;
        int j = i;
        while (j && ranks[order[j - 1]] > ranks[i]) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = (uint8_t)i;
    }
    return n;
}
int race_game_quick_grid(const race_game *g, menu_quick_race *grid)
{
    if (!g || !g->run || !grid)
        return 0;
    memset(grid, 0, sizeof *grid);
    grid->entries = gt4_race_run_cars(g->run);
    snprintf(grid->course, sizeof grid->course, "%s", g->last_course);
    grid->dry = 1;
    /* 00119918 +0x4f4 copies race+0; +0x4f0 uses cvt.w.s of
       00395400: course+4 -> data+0xdc -> runway+0x14 metres.
       EE cvt.w.s truncates/saturates; host rounding mode is irrelevant. */
    uint32_t race = GT4_U32(gt4_race_run_conductor(g->run));
    uint32_t course = GT4_U32(race + 0x80), data = GT4_U32(course + 4),
             runway = GT4_U32(data + 0xdc);
    grid->laps = (int32_t)GT4_U32(race);
    if (g->last_prepared && license_exam(g)) {
        grid->license = 1;
        grid->entries = 1;
        grid->gold_time = gt4_career_license_time(g->last_event_row, 0);
        grid->silver_time = gt4_career_license_time(g->last_event_row, 1);
        grid->bronze_time = gt4_career_license_time(g->last_event_row, 2);
        /* 0011D400 +0x4c..0x70: logger disabled on IB-6/IB-7;
           005C47A0 reads work +0x4b4, independent of row +0x75. */
        int exam = gt4_career_license_index(g->last_desc.license_code);
        grid->license_disable_logger = exam == 37 || exam == 38;
        grid->license_load_ghost = !g->last_event_row[0x75];
    }
    grid->course_length = gt4_career_course_length(GT4_F32(runway + 0x14));
    for (int i = 0; i < grid->entries; i++) {
        gt4_race_hud hud;
        if (!gt4_race_run_hud(g->run, i, &hud))
            return 0;
        /* quick-license.ad L225..278 presents the student's car only.
           RaceTraining (003ECB68) retains its multi-car grid positions;
           that position is not an index into this one-car summary. */
        int slot = grid->license ? 0 : hud.grid_slot;
        if (slot < 0 || slot >= grid->entries)
            return 0;
        if (!i)
            grid->player = slot;
        snprintf(grid->grid[slot].car, sizeof grid->grid[slot].car, "%s",
                 i ? g->last_others[i - 1] : g->last_car);
        grid->grid[slot].color = i ? g->last_colors[i - 1] : g->last_color;
        grid->grid[slot].time = gt4_career_grid_time(hud.finished, hud.line_time);
        uint32_t car = gt4_race_run_car(g->run, i), params = GT4_U32(car + CAR_PARAMS);
        /* 00112F44 / 00364868: summary of the actual prepared entry,
           not a stock reconstruction using its GENERIC_CAR label. */
        grid->grid[slot].power = (int)gt4_engine_peak_power(params, car + CAR_ENGINE_WEAR);
        grid->grid[slot].weight = (int)GT4_F32(params + PARAMS_MASS) - 70;
    }
    return 1;
}
int race_game_quick_points(const race_game *g, menu_quick_race *grid, const int32_t points[6])
{
    if (!g || !g->run || !grid || !points)
        return 0;
    /* 00119918 builds quick-work rows in grid order; championship points
       (003EEFE0/003EF1E8) remain attached to persistent entry identities. */
    for (int driver = 0; driver < grid->entries; driver++) {
        gt4_race_hud hud;
        if (!gt4_race_run_hud(g->run, driver, &hud) || hud.grid_slot < 0 ||
            hud.grid_slot >= grid->entries)
            return 0;
        grid->total_points[hud.grid_slot] = points[driver];
    }
    return 1;
}

int race_game_quick_results(const race_game *g, menu_quick_race *grid)
{
    menu_quick_race initial;
    uint8_t order[MOON_CONDUCTOR_CARS];
    if (!race_game_quick_grid(g, &initial) || race_game_finish_order(g, order) != initial.entries)
        return 0;
    *grid = initial;
    /* 00114390 walks 003B6FD0's result list, not the initial grid.
       Map driver -> original grid slot before moving the entry. */
    for (int i = 0; i < initial.entries; i++) {
        gt4_race_hud hud;
        if (!gt4_race_run_hud(g->run, order[i], &hud))
            return 0;
        if (hud.grid_slot < 0 || hud.grid_slot >= initial.entries)
            return 0;
        grid->grid[i] = initial.grid[hud.grid_slot];
        if (!order[i])
            grid->player = i;
    }
    return 1;
}
int race_game_classification_order(const race_game *g, uint8_t *ids, int max)
{
    return g && g->run ? gt4_race_run_classification_order(g->run, ids, max) : 0;
}

int race_game_event_position(const race_game *g)
{
    gt4_race_hud player;
    if (!race_game_player_hud(g, &player))
        return -1;
    if (g->run && gt4_race_run_phantom_count(g->run)) {
        int place = gt4_race_run_player_place(g->run);
        if (place >= 0)
            return place;
    }
    int position = 0;
    /* 003B6FD0 stable insertion by signed car+0x5B3, then 00110570:
       player is entry zero, so equal-rank entries follow it. */
    for (int i = 1; i < gt4_race_run_cars(g->run); i++) {
        gt4_race_hud other;
        if (!gt4_race_run_hud(g->run, i, &other))
            return -1;
        if (other.rank < player.rank)
            position++;
    }
    return position;
}
double race_game_finished(const race_game *g)
{
    return g && g->ready ? g->finished : -1;
}
int race_game_license_active(const race_game *g)
{
    return g && g->run && g->last_prepared && license_exam(g);
}
int race_game_license_result(const race_game *g, int32_t *state, uint32_t *time)
{
    return g && g->run && g->last_prepared && license_exam(g) &&
           gt4_race_run_license_result(g->run, state, time);
}
int race_game_license_retry(const race_game *g)
{
    return g && race_result_retry(g->result);
}
const gt4_logger_sampler *race_game_license_log(const race_game *g)
{
    return g && g->license_log.count ? &g->license_log : NULL;
}
#include "race_game_logger.inc"

#include "race_game_replay.inc"

void race_game_set_laps(race_game *g, int laps)
{
    if (g)
        g->laps = laps;
}
/* 00115CB4..00115D74: Power & Speed's end, car 0 through: its time (best
   lap, 0035E3F0) and top speed (car +0x6CC); the test (the laps word) */
int race_game_machine_result(const race_game *g, int *test, uint32_t *time, float *speed)
{
    if (!g || !g->run || g->world_mode != 15 || g->replaying)
        return 0;
    uint32_t car = gt4_race_run_car(g->run, 0);
    if (!GT4_U8(car + CAR_5B6))
        return 0;
    if (test)
        *test = g->laps < 4 ? g->laps : 0;
    if (time)
        *time = GT4_U32(car + CAR_FIELD_BEST_LAP_TIME);
    if (speed)
        *speed = GT4_F32(car + CAR_TOP_SPEED);
    return 1;
}
void race_game_set_time_trial(race_game *g, int on)
{
    if (g)
        g->time_trial = on != 0;
}
void race_game_set_world_mode(race_game *g, int mode)
{
    if (g)
        g->world_mode = mode;
}
/* B-Spec for the next build; the race already built keeps its own. Returns
   1 when the built race differs and must be rebuilt (race_game_restart). */
int race_game_set_bspec(race_game *g, int on)
{
    if (!g)
        return 0;
    g->bspec = on != 0;
    if (!g->last_prepared && !g->last_course[0])
        return 0;
    int changed = g->last_desc.bspec != g->bspec;
    g->last_desc.bspec = g->bspec;
    return changed;
}
int race_game_bspec(const race_game *g)
{
    return g && g->run && g->run_bspec;
}
uint32_t race_game_best_lap(const race_game *g)
{
    gt4_race_hud h;
    /* func_0035E3F0 on the player's car (car+0x71C), as 00112620 reads it
       after the race; the no-time 0x157529FF without one */
    if (!g || !g->run || !gt4_race_run_hud(g->run, 0, &h) || !h.best_lap)
        return 0x157529ffu;
    return h.best_lap;
}
/* 00111DC0 at the Arcade's start: the QuickTune's gearbox (+0x3C0 into
   the entry's +0x3448) and aids (+0x3C4, 00110B90: 2 both, 1 TCS alone)
   always; power, weight, tyres and top speed when IsQuickTuned. Returns
   1 when the race already built must be rebuilt to take them. */
int race_game_quick_tune(race_game *g, int tuned, int power, int weight, int front, int rear,
                         int gear, int transmission, int assist)
{
    if (!g)
        return 0;
    int asm_on = assist == 2, tcs_on = assist >= 1, changed = 0;
    changed |=
        g->manual != (transmission != 0) || g->assist_asm != asm_on || g->assist_tcs != tcs_on;
    g->manual = transmission != 0;
    g->assist_asm = asm_on;
    g->assist_tcs = tcs_on;
    if (tuned) {
        g->quick_tuned = 1;
        g->quick_power = power;
        g->quick_weight = weight;
        g->quick_front_tire = front;
        g->quick_rear_tire = rear;
        g->quick_gear = gear;
        changed = 1;
    }
    return changed;
}
void race_game_quick_clear(race_game *g)
{
    if (g)
        g->quick_tuned = 0;
}
int race_game_gear_range(const race_game *g, int *gear, int *lo, int *hi)
{
    return g && g->run && gt4_race_run_gear_range(g->run, 0, gear, lo, hi);
}

void race_game_set_split(race_game *g, const gt4_race_desc *options)
{
    if (!g)
        return;
    g->split = options != NULL;
    if (options)
        g->split_options = *options;
    else
        memset(&g->split_options, 0, sizeof g->split_options);
    /* Own both buffers across loading, Retry and the menu's lifetime.
       The verified race builder consumes active sheets via 00441248. */
    if (options)
        for (int i = 0; i < 2; i++)
            if (options->owned_car[i]) {
                memmove(g->split_owned[i], options->owned_car[i], sizeof g->split_owned[i]);
                g->split_options.owned_car[i] = g->split_owned[i];
            }
    if (g->split) {
        g->time_trial = g->bspec = 0;
        g->inspect_on = 0;
    }
}
void race_game_set_arcade_owned(race_game *g, const uint8_t *body)
{
    if (!g)
        return;
    g->arcade_owned_on = body != NULL;
    /* arcade.ad GameRoot.doGame passes car_garage to RaceArcade/RaceTT.
       The existing constructor reads its active sheet (00441248). */
    if (body)
        memmove(g->arcade_owned, body, sizeof g->arcade_owned);
}
int race_game_split(const race_game *g)
{
    return g && g->split;
}
int race_game_take_retry(race_game *g)
{
    int pending = g && g->retry_requested;
    if (g)
        g->retry_requested = 0;
    return pending;
}

/* 003D6350 +18..20 copies the option into the player controller.
   The native pregrid is already prepared; report whether it needs a
   rebuild, and retain this choice through prepared Retry. */
int race_game_set_transmission(race_game *g, int manual)
{
    if (!g)
        return 0;
    manual = manual != 0;
    int previous = g->last_prepared ? g->last_desc.manual : g->manual;
    g->manual = manual;
    if (g->last_prepared)
        g->last_desc.manual = manual;
    return previous != manual;
}

void race_game_set_driving(race_game *g, int tire, int manual, int assist_asm, int assist_tcs)
{
    if (!g)
        return;
    g->tire = tire >= 0 ? tire + 1 : 0;
    g->manual = manual;
    g->assist_asm = assist_asm;
    g->assist_tcs = assist_tcs;
}

#include "race_game_controls.inc"

static int watched_car(const race_game *g);

#include "race_game_bspec.inc"

/* P: native diagnostic controls requested by the user, not PS2 camera data.
   Speed 3 m/s (Shift 15), turn 1.5 rad/s and pitch limit are PC choices.
   No course/car collision query; the view starts at the active PS2 camera. */
static void inspect_camera(race_game *g, double dt)
{
    if (!g->inspect_on)
        return;
    if (!g->inspect_seeded) {
        float rows[9];
        if (!race_view_get_camera(g->view, g->inspect_eye, rows, g->inspect_projection))
            return;
        g->inspect_yaw = atan2f(-rows[6], -rows[8]);
        g->inspect_pitch = asinf(fmaxf(-1, fminf(1, -rows[7])));
        g->inspect_seeded = 1;
    }
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float step = (float)fmax(0, fmin(dt, .1));
    g->inspect_yaw += 1.5f * step * (keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT]);
    g->inspect_pitch += 1.5f * step * (keys[SDL_SCANCODE_UP] - keys[SDL_SCANCODE_DOWN]);
    g->inspect_pitch = fmaxf(-1.55f, fminf(1.55f, g->inspect_pitch));
    float sy = sinf(g->inspect_yaw), cy = cosf(g->inspect_yaw), sp = sinf(g->inspect_pitch),
          cp = cosf(g->inspect_pitch);
    float forward[3] = {sy * cp, sp, cy * cp}, right[3] = {-cy, 0, sy};
    float rows[9] = {right[0], 0,           right[2],    -sy * sp,   cp,
                     -cy * sp, -forward[0], -forward[1], -forward[2]};
    float speed = step * ((keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) ? 15.f : 3.f);
    float ahead = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S],
          side = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
    float lift = keys[SDL_SCANCODE_E] - keys[SDL_SCANCODE_Q],
          length = sqrtf(ahead * ahead + side * side + lift * lift);
    if (length > 1)
        speed /= length;
    for (int k = 0; k < 3; k++)
        g->inspect_eye[k] += speed * (forward[k] * ahead + right[k] * side + (k == 1 ? lift : 0));
    race_view_set_camera(g->view, g->inspect_eye, rows, g->inspect_projection[0],
                         g->inspect_projection[1], g->inspect_projection[2]);
    race_view_show_car(g->view, 1);
    g->start_cam_on = 0;
}

/* P: authored PC camera moves for the user's native showcase, enabled only
   by MOON_RACE_FILM=orbit/crane/low. Reads the live pose and changes the
   camera/listener, never the car, controls, simulation speed or replay tape.
   Distances, lens and trajectories are deliberate filming choices, not PS2
   camera data. Clock starts after the race opening (10 seconds). */
static void film_camera(race_game *g, double dt)
{
    if (g->cinema.on)
        return;
    const char *mode = getenv("MOON_RACE_FILM");
    if (!mode || g->split ||
        (strcmp(mode, "orbit") && strcmp(mode, "crane") && strcmp(mode, "low")))
        return;
    g->film_clock += dt;
    float t = (float)fmax(0, g->film_clock - 10), pos[3], rot[9], wheels[4][3];
    if (!gt4_race_run_pose(g->run, 0, pos, rot, wheels))
        return;
    float local[3];
    if (!strcmp(mode, "crane")) {
        float q = fminf(1, t / 24), ease = q * q * (3 - 2 * q);
        local[0] = 4 + 5 * ease;
        local[1] = 1.5f + 20 * ease;
        local[2] = -3 + 10 * ease;
    } else if (!strcmp(mode, "low")) {
        local[0] = 3.6f;
        local[1] = .75f;
        local[2] = -2.2f + 4.4f * (.5f - .5f * cosf(t * .12f));
    } else {
        float angle = -1.3f + t * .11f;
        local[0] = 5.2f * sinf(angle);
        local[1] = 1.4f + .7f * (1 - cosf(t * .1f));
        local[2] = 5.2f * cosf(angle);
    }
    float eye[3], target[3], f[3], rows[9], length = 0;
    for (int k = 0; k < 3; k++) {
        eye[k] = pos[k];
        for (int j = 0; j < 3; j++)
            eye[k] += rot[k * 3 + j] * local[j];
        target[k] = pos[k] + rot[k * 3 + 1] * .65f;
        f[k] = target[k] - eye[k];
        length += f[k] * f[k];
    }
    length = sqrtf(length);
    if (length < .001f)
        return;
    for (int k = 0; k < 3; k++)
        f[k] /= length;
    length = sqrtf(f[0] * f[0] + f[2] * f[2]);
    if (length < .001f)
        return;
    rows[0] = -f[2] / length;
    rows[1] = 0;
    rows[2] = f[0] / length;
    rows[3] = -f[1] * rows[2];
    rows[4] = f[0] * rows[2] - f[2] * rows[0];
    rows[5] = f[1] * rows[0];
    for (int k = 0; k < 3; k++)
        rows[6 + k] = -f[k];
    race_view_set_camera(g->view, eye, rows, 1.5f, 1.95183f, .1f);
    race_view_show_car(g->view, 1);
    g->start_cam_on = 0;
}

static int rolling_phase(race_game *g);
static int tv_camera(race_game *g);
/* the car the cameras follow: the replay's (up/down), B-Spec's (its
   monitor's +0x64), else the player's */
static int watched_car(const race_game *g)
{
    if (g->bspec_ui)
        return race_bspec_selected(g->bspec_ui);
    return g->replaying ? g->focus : 0;
}
static int dive_camera(race_game *g);
#include "race_cinematic.inc"

/* The standing start's camera (CameraPathanim, func_0037D620): its
   clock a second ahead of the start's (the frames to it less 60), the
   motion by the time left (D_00621270: under 1 s the third, under 2 the
   second, else the first, held at its start until 9 s are left), each
   played from its start when chosen; the first in the frame of the
   pole's grid place (func_003FCA08: its heading turned half round), the
   others in the car's (its place and heading). The motion's eye, target
   and up (func_003EBCE8), its field of view (radians) the projection's. */
static void license_start_step(race_game *g)
{
    if (!g->last_prepared || !license_exam(g) || g->replaying)
        return;
    const gt4_course *c = race_view_course(g->view);
    if (!c)
        return;
    if (!g->license_start_tried) {
        g->license_start_tried = 1;
        uint32_t at = 0;
        if (c->size >= 0xd8)
            memcpy(&at, c->data + 0xd4, 4);
        if (at && at < c->size)
            g->license_start_motion = gt4_license_motion_open_camera(c->data + at, c->size - at);
        if (!g->license_start_motion ||
            !gt4_license_motion_select(g->license_start_motion, 3, "replay_start_camera1"))
            return;
        gt4_race_run_place(g->run, 0, g->license_start_origin, g->license_start_origin + 3);
        /* 00374720 selects kind 4 for a live license before start;
           003749A0 passes opening=1 to 0037D338: +60, no grid frame. */
        unsigned kind = g->last_event_row[0x40] + 1;
        int seconds = kind == 2 ? 8 : kind == 3 ? 10 : kind == 4 ? 0 : g->last_event_row[0x41];
        g->license_start_left = seconds * 60 + 60;
    }
    if (g->license_start_motion && --g->license_start_left >= 0)
        gt4_license_motion_step_control(g->license_start_motion, 3, gt4_f32(0x3c888888),
                                        gt4_license_opening_control(g->license_start_left));
}
static int start_camera(race_game *g)
{
    if (g->last_prepared && license_exam(g) && !g->replaying) {
        float look[9], fov, cc, tc;
        /* 00374720 +00..10: leave kind 4 when race+BC reaches zero,
           even though its internal opening clock includes another 60. */
        gt4_race_hud h;
        if (!gt4_race_run_hud(g->run, 0, &h) || h.start_frames <= 0)
            return 0;
        return g->license_start_left > 0 && g->license_start_motion &&
               gt4_license_motion_camera(g->license_start_motion, look, &fov, &cc, &tc) &&
               license_look(g, look, fov, g->license_start_origin);
    }
    /* func_0037D5B0: the motion by the seconds left, D_00621270 for the
       start, D_00621290 for the replay's opening (+0x1D0 set) */
    static const float start_until[3] = {1, 2, 9}, replay_until[5] = {1, 2, 3, 4, 11};
    const float *until = g->replaying ? replay_until : start_until;
    const int motions = g->replaying ? 5 : 3;
    gt4_race_hud h;
    const gt4_course *course = race_view_course(g->view);
    float t, local = 0, look[9], fov, ref[3], yaw, c, s, eye[3], target[3], up[3], rows[9];
    int id = 1;
    if (!course || !gt4_race_run_hud(g->run, 0, &h) || h.rolling)
        return 0;
    /* func_0037D338: the clock +0x18C is the frames to the start less 60,
       for the replay's opening (+0x1D0) plus 60, then one less a frame
       (func_0037D620): the opening runs on a second past the start */
    if (g->replaying) {
        if (g->replay_open < 0) { /* armed with the countdown */
            if (h.start_frames <= 0)
                return 0;
            g->replay_open = (long)h.start_frames + 60 + (long)g->tape_at;
        }
        t = (float)(g->replay_open - (long)g->tape_at) / 60;
        if (t <= 0)
            return 0;
    } else {
        if (h.start_frames <= 60)
            return 0;
        t = (float)(h.start_frames - 60) / 60;
    }
    if (!g->start_cam_tried) {
        /* the course header's Mot1 banks: +0xD0 the start's, +0xD4 the
           replay's opening (func_0037D620 at 0x0037D734 by +0x1D0) */
        unsigned o = g->replaying ? 0xd4 : 0xd0;
        uint32_t at;
        g->start_cam_tried = 1;
        at = course->size >= 0x100
                 ? (uint32_t)(course->data[o] | course->data[o + 1] << 8 |
                              course->data[o + 2] << 16 | (uint32_t)course->data[o + 3] << 24)
                 : 0;
        if (at && at < course->size)
            g->start_cam = gt4_motion_bank_open(course->data + at, course->size - at);
    }
    if (!g->start_cam)
        return 0;
    for (int k = 0; k < motions; k++)
        if (t < until[k]) {
            id = motions - k;
            local = until[k] - t;
            break;
        }
    if (!gt4_motion_bank_camera(g->start_cam, id - 1, local, look, &fov))
        return 0;
    /* func_0037D338: entry+0x3444 bit 0 sets +0x1D1; func_0037D620
       mirrors local x on the car-relative shots, never the grid shot.
       The replay's opening (+0x1D0) skips both: no mirror, every motion
       in the car's frame. */
    if (!g->replaying && id != 1 && (h.grid_slot & 1))
        for (int k = 0; k < 3; k++)
            look[3 * k] = -look[3 * k];
    if (id == 1 && !g->replaying) {
        gt4_grid p = gt4_course_grid(course, 0);
        ref[0] = p.pos[0];
        ref[1] = p.pos[1];
        ref[2] = p.pos[2];
        gt4_course_height(course, p.pos[0], p.pos[2], p.pos[1] + 2, &ref[1]);
        yaw = p.heading + 3.14159265f;
    } else if (!gt4_race_run_place(g->run, 0, ref, &yaw))
        return 0;
    /* world = the reference + Ry(yaw) * local (the view's Ry(-yaw)) */
    c = cosf(yaw);
    s = sinf(yaw);
    for (int k = 0; k < 3; k++) {
        const float *v = look + 3 * k;
        float *o = k == 0 ? eye : k == 1 ? target : up;
        float x = c * v[0] + s * v[2], z = -s * v[0] + c * v[2];
        o[0] = x + (k < 2 ? ref[0] : 0);
        o[1] = v[1] + (k < 2 ? ref[1] : 0);
        o[2] = z + (k < 2 ? ref[2] : 0);
    }
    { /* the view's rows: right, up, back */
        float b[3] = {eye[0] - target[0], eye[1] - target[1], eye[2] - target[2]}, r[3], u[3], n;
        n = sqrtf(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
        if (n < 1e-6f)
            return 0;
        for (int k = 0; k < 3; k++)
            b[k] /= n;
        r[0] = up[1] * b[2] - up[2] * b[1];
        r[1] = up[2] * b[0] - up[0] * b[2];
        r[2] = up[0] * b[1] - up[1] * b[0];
        n = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        if (n < 1e-6f)
            return 0;
        for (int k = 0; k < 3; k++)
            r[k] /= n;
        u[0] = b[1] * r[2] - b[2] * r[1];
        u[1] = b[2] * r[0] - b[0] * r[2];
        u[2] = b[0] * r[1] - b[1] * r[0];
        for (int k = 0; k < 3; k++) {
            rows[k] = r[k];
            rows[3 + k] = u[k];
            rows[6 + k] = b[k];
        }
    }
    if (getenv("MOON_START_CAM_LOG") && h.start_frames % 30 == 0) /* tests */
        fprintf(
            stderr,
            "start cam: %d frames, motion %d at %.3f, ref %.2f %.2f %.2f yaw %.3f, local eye %.2f %.2f %.2f target %.2f %.2f %.2f, fov %.3f\n",
            h.start_frames, id, local, ref[0], ref[1], ref[2], yaw, look[0], look[1], look[2],
            look[3], look[4], look[5], fov);
    {
        float sx = 1 / tanf(fov);
        race_view_set_camera(g->view, eye, rows, sx, sx * 1.30122f, 0.1f);
        race_view_show_car(g->view, 1);
    }
    return 1;
}

/* 00385478 / 0040CA58 / 0040D0A0: copy the driver's presentation inputs.
   The physics' human record is not written; race_view owns its controller. */
static void update_drivers(race_game *g)
{
    for (int i = 0; i <= g->others; i++) {
        uint32_t car = gt4_race_run_car(g->run, i);
        if (!car)
            continue;
        uint32_t params = GT4_U32(car + CAR_PARAMS);
        gt4_driver_animation_input in = {0};
        int transmission[4];
        for (int k = 0; k < 4; k++)
            transmission[k] = (int32_t)GT4_U32(car + CAR_FIELD_UNK_1080 + k * 4);
        in.dt = GT4_F32(car + CAR_DT);
        in.gear = GT4_U8(car + CAR_GEAR);
        in.steering = -GT4_F32(car + CAR_FIELD_UNK_768) * gt4_f32(0x3ecccccc);
        in.steering_fraction = GT4_F32(car + CAR_56C);
        in.throttle = GT4_F32(car + CAR_THROTTLE_REQ);
        in.brake = GT4_F32(car + CAR_BRAKE_PEDAL);
        in.lever_pedal = GT4_F32(car + CAR_THROTTLE); /* 003446A8, type-five lever */
        int count = GT4_U8(params + PARAMS_DRIVE + 2), rpm = (int)GT4_F32(car + CAR_TACHO);
        int up = in.gear == 0 || in.gear == count ? 99999 : (int)gt4_shift_rpm(car);
        int down = in.gear < 2 ? -99999 : GT4_U16(params + 0x260 + in.gear * 2);
        if ((int8_t)GT4_U8(car + CAR_56A) == 1 && in.gear >= 2)
            down = (down * GT4_U8(car + CAR_FIELD_UNK_83)) >> 7;
        in.upshift = (in.gear == 1 ? rpm > up - 1000 && in.throttle > gt4_f32(0x3e4ccccc)
                                   : rpm > up - 300 && in.throttle > gt4_f32(0x3f333333));
        int speed_gear = gt4_gear_for_speed(car, fminf(GT4_F32(car + 0x38), GT4_F32(car + 0x3c)));
        if (in.gear == speed_gear)
            in.downshift = rpm < down - 1000;
        else if (in.throttle > 0)
            in.downshift = rpm < down;
        else if (in.brake == 0)
            in.downshift = rpm < down + 500;
        else {
            in.downshift = rpm < down + 1000;
            in.speed_shift = speed_gear > 0 && speed_gear < in.gear;
        }
        race_view_step_driver(g->view, (unsigned)i, transmission, &in);
    }
}

/* 003D73A0 -> 00344E48 supplies current/previous wheel angles to the
   renderer; its 003D73F8 exposure is read from the course in race_view.
   Read presentation inputs only. No physics fields are changed. */
static void update_wheel_motion(race_game *g, int entry, const float rot[9])
{
    uint32_t car = gt4_race_run_car(g->run, entry);
    if (!car)
        return;
    float current[4] = {0}, previous[4] = {0}, heat[4] = {0};
    for (int k = 0; k < gt4_car_wheels(car) && k < 4; k++) {
        uint32_t w = gt4_car_wheel(car, k);
        float rr[3] = {GT4_F32(w + WHEEL_MOUNT), GT4_F32(w + WHEEL_MOUNT + 8),
                       -GT4_F32(w + WHEEL_MOUNT + 4)};
        float x = rot[0] * rr[0] + rot[3] * rr[1] + rot[6] * rr[2],
              z = rot[2] * rr[0] + rot[5] * rr[1] + rot[8] * rr[2];
        int slot = (z > 0 ? 2 : 0) + (x > 0 ? 0 : 1); /* same render slots as race_run_pose */
        current[slot] = GT4_F32(w + WHEEL_ROTATION);
        previous[slot] = GT4_F32(car + CAR_WHEEL_TURNS + k * 4);
        heat[slot] = GT4_F32(w + 0xa4); /* 003F38D8 brake heat; 003D75F0 reads it */
    }
    race_view_set_brake_heat(g->view, (unsigned)entry, heat);
    race_view_set_wheel_motion(g->view, (unsigned)entry, current, previous,
                               GT4_F32(car + CAR_VIEW_BLEND));
}

#include "race_game_pause.inc"

/* func_0039F0C0: the car at a rank (car+0x5B3), its icon only for a car
   the race drives (func_00366678 3: +0x441 clear); -1 none */
static int icon_at_rank(race_game *g, int rank)
{
    for (int c = 0; c < gt4_race_run_cars(g->run); c++) {
        uint32_t car = gt4_race_run_car(g->run, c);
        if ((int8_t)GT4_U8(car + CAR_FIELD_POSITION) != rank)
            continue;
        return GT4_U8(car + CAR_POSITION + 0x441) ? -1 : c;
    }
    for (int j = 0; j < gt4_race_run_phantom_count(g->run); j++) {
        uint32_t car = gt4_race_run_phantom_car(g->run, j);
        if ((int8_t)GT4_U8(car + CAR_FIELD_POSITION) != rank)
            continue;
        return GT4_U8(car + CAR_POSITION + 0x441) ? -1 : g->others + j + 1;
    }
    return -1;
}

/* The race's events of this step the display shows for the player (car
   0, the display's +0x2E), as func_0039DBB8 reads the queue at race+0xF4:
   a section passed (0xE, func_0039F368) or a lap (0xD, func_0039F200)
   with that car's time (0x10), and its gap (0x11, func_003A04B0) against
   the leader, or the second when leading (func_003A1030 -> 0039F0C0: the
   car of that rank, car+0x5B3), with that car's icon unless it is a
   ghost (func_00366678). Only the events posted this step (count still
   2: func_003B75D8 ages them each frame) */
static void timing_events(race_game *g)
{
    uint32_t race, q;
    int n, cars, lead;
    if (!g->hud || !g->run)
        return;
    race = GT4_U32(gt4_race_run_conductor(g->run));
    q = race + 0xf4;
    n = (int)GT4_U32(q + 0x824);
    { /* func_0039E9B0: each event 4 (the start) of the watched car, or of
           none, with its countdown over: START (003A1488, the race only)
           and the start's sound (D_00621515). Ours stay queued: a new one
           is one more of them */
        int starts = 0, watched = watched_car(g);
        for (int i = 0; i < n && i < 0x100; i++) {
            uint32_t e = q + 8 + (uint32_t)i * 8;
            int32_t who = (int32_t)GT4_U32(e + 4);
            if ((int16_t)GT4_U16(e) == 4 && (who < 0 || who == watched))
                starts++;
        }
        if (starts > g->start_events && gt4_race_run_countdown(g->run, watched) <= 0) {
            if (!g->replaying)
                race_hud_start(g->hud);
            race_audio_start(g->audio);
        }
        g->start_events = starts;
    }
    cars = gt4_race_run_cars(g->run);
    lead = (int8_t)GT4_U8(gt4_race_run_car(g->run, 0) + 0x5b3) == 1;
    for (int i = 0; i < n && i < 0x100; i++) {
        uint32_t e = q + 8 + (uint32_t)i * 8, v = GT4_U32(e + 4);
        int kind = (int16_t)GT4_U16(e);
        if (GT4_U16(e + 2) != 2)
            continue;
        if (kind ==
            0x12) { /* func_003A0308: the player's laps behind the leader, or leading, the second's */
            int second = -1;
            for (int c = 0; c < cars; c++)
                if ((int8_t)GT4_U8(gt4_race_run_car(g->run, c) + 0x5b3) == 2)
                    second = c;
            if (!lead && (v & 15) == 0)
                race_hud_laps_behind(g->hud, (int32_t)v >> 6, icon_at_rank(g, 1));
            else if (lead && second >= 0 && (int)(v & 15) == second)
                race_hud_laps_behind(g->hud, -((int32_t)v >> 6), icon_at_rank(g, 2));
            continue;
        }
        if ((v & 15) != 0)
            continue;
        if (kind == 0x13) { /* func_003A0600 -> 003A3778: the speed x10 above the car */
            race_hud_top_speed(g->hud, (float)((v >> 4) & 0x3fffu) * 0.1f, g->unit == 1);
            continue;
        }
        if (kind == 0xe || kind == 0xd) {
            uint32_t ms = 0x157529FFu;
            for (int k = 0; k < n; k++) { /* the first time message (0x10) of the same car */
                uint32_t f = q + 8 + (uint32_t)k * 8, t = GT4_U32(f + 4);
                int32_t x;
                if ((int16_t)GT4_U16(f) != 0x10 || (t & 15) != 0)
                    continue;
                x = (int32_t)t >> 6; /* func_003A3680 */
                ms = (uint32_t)x + 0x1fffffeu > 0x3fffffcu ? 0x157529FFu : (uint32_t)x;
                break;
            }
            if (kind == 0xe)
                race_hud_section_time(g->hud, (int)((v >> 4) & 15) + 1, ms);
            else
                race_hud_lap_time(g->hud, ms);
        } else if (kind == 0x11) {
            int32_t t = (int32_t)v >> 6;
            if ((uint32_t)t + 0x1fffffeu > 0x3fffffcu)
                continue; /* func_003A3620: no time */
            race_hud_gap(g->hud, t, (int)((v >> 4) & 3),
                         icon_at_rank(g, t < 0 ? 2 : 1)); /* func_003A1030 */
        }
    }
}

/* 003B5E80 +0x1FC..+0x37C consumes consecutive camera positions for
   Doppler. Publish the camera after each simulation frame, before its
   sound requests, even when the PC draws several simulation frames at once.
   Camera construction remains the existing original-derived path. */
static void split_camera(race_game *g, int port)
{
    int view = port ? g->camera_2p : g->camera;
    float point[8];
    float *eye = g->split_eye[port], *rows = g->split_rows[port],
          *projection = g->split_projection[port];
    if (!race_view_camera_point_entry(g->view, port, view, point) ||
        !gt4_race_run_camera(g->run, port, view, point, eye, rows))
        return;
    if (port ? g->look_back_2p : g->look_back) {
        for (int k = 0; k < 3; k++) {
            if (view != 6)
                eye[k] += 2 * (rows[k] * point[0] + rows[6 + k] * point[2]);
            rows[k] = -rows[k];
            rows[6 + k] = -rows[6 + k];
        }
    }
    /* 0033FD88 +1DC..260: viewport/fullscreen height-to-width ratio
       times f23 (0032FDA4..AC: 0x3F999999). 004A7890 scales X/Y.
       A: the inherited single-view base lens still comes from captures. */
    float sx = view == 0 || view == 1 ? 1.73205f : 1 / tanf(atanf(1 / point[6]) * 4 / 3);
    sx *= speed_lens_scale(view);
    float scale = gt4_f32(0x3f999999u);
    projection[0] = sx * .5f * scale;
    projection[1] = sx * 1.30122f * scale;
    projection[2] = .1f;
    g->split_camera_valid[port] = 1;
}
static void split_camera_audio_update(race_game *g)
{
    split_camera(g, 0);
    split_camera(g, 1);
    g->start_cam_on = g->tv_on = 0;
    race_view_set_camera(g->view, g->split_eye[0], g->split_rows[0], g->split_projection[0][0],
                         g->split_projection[0][1], g->split_projection[0][2]);
    /* A: shared audio mix retains player 1's listener; no second music. */
    if (!g->audio)
        g->audio = race_audio_open_replay(g->disc, g->run, race_view_course(g->view), 0);
    if (g->audio)
        for (int i = 0; i < gt4_race_run_cars(g->run); i++)
            race_audio_set_exhaust_count(g->audio, (unsigned)i,
                                         race_view_exhaust_count(g->view, i));
    if (g->audio)
        race_audio_update(g->audio, g->run, g->split_eye[0], g->split_rows[0], g->camera,
                          g->bgm_enabled ? g->bgm_volume : -1, g->se_volume);
}
static void race_camera_audio_update(race_game *g, double dt)
{
    int previous_start = g->start_cam_on, previous_tv = g->tv_on;
    if (g->split) {
        split_camera_audio_update(g);
        return;
    }
    if (!g->inspect_on || !g->inspect_seeded) {
        g->start_cam_on = getenv("MOON_RACE_SNAPSHOT") ? 0 : start_camera(g);
        if (g->replaying)
            g->cue = race_audio_music_cue(g->audio, &g->cue_passed); /* 0033D170, each frame */
        if (g->replaying)
            g->zoom = gt4_tvcam_zoom_step(g->zoom_spring, g->zoom_on); /* 003751F0, each frame */
        g->tv_on = !g->start_cam_on && g->replaying && !g->car_cam && g->rmode == 1 &&
                   dive_camera(g); /* Dive */
        {
            float grade[6];
            gt4_dive_filter(g->dive, grade);
            race_view_set_grade(g->view, g->tv_on && g->dive ? grade : NULL);
        }
        if (!g->tv_on)
            g->tv_on =
                !g->start_cam_on &&
                (rolling_phase(g) || (g->replaying && !g->car_cam) || getenv("MOON_RACE_TV") ||
                 (g->bspec_ui && race_bspec_view(g->bspec_ui) == 2)) &&
                tv_camera(g);
        if (!g->tv_on && g->tvcam)
            gt4_tvcam_reset(g->tvcam);
        if (!g->start_cam_on &&
            !g->tv_on) { /* the game's chase camera on the player's car (its model's camera point 1) */
            float point[8], eye[3], rows[9];
            /* the replay's car camera (Cross, the selector's type 2) follows
           the car watched; A: through the player's model's point */
            int watched = watched_car(g), view = g->replaying ? 1
                                                 : g->bspec_ui
                                                     ? (race_bspec_view(g->bspec_ui) == 0 ? 0 : 1)
                                                     : g->camera;
            if (race_view_camera_point(g->view, view, point) &&
                gt4_race_run_camera(g->run, watched, view, point, eye, rows)) {
                if (!g->replaying && g->look_back && (view == 0 || view == 1 || view == 6)) {
                    /* 0037B3FC..0037B418: reverse pitch/roll and add pi to
                   yaw. Ry(pi)*view flips rows 0/2 and the translated
                   camera offset around the car, without touching physics.
                   Roof 0037B718..0037B738 keeps the point's position and
                   only reverses the orientation. A: full matrix not compared. */
                    for (int k = 0; k < 3; k++) {
                        if (view != 6)
                            eye[k] += 2 * (rows[k] * point[0] + rows[6 + k] * point[2]);
                        rows[k] = -rows[k];
                        rows[6 + k] = -rows[6 + k];
                    }
                }
                /* the projection: the captures' (0x01CE9860) for the views on
               and behind the car; the others their point's own (+0x18, the
               field's half width at unit depth, func_0037B580), the height
               in the same proportion */
                float sx = 1.73205f;
                if (view != 0 && view != 1)
                    /* the point's field (+0x18 its half width at unit depth: 52
                   degrees on the Ka) spread by 4/3, as the roof capture's
                   projection has it (1.44331 for 2.05) */
                    sx = 1 / tanf(atanf(1 / point[6]) * 4 / 3);
                sx *= speed_lens_scale(view);
                speed_camera_apply(g, view, eye, rows, dt);
                free_look_apply(g, view, eye, rows, dt);
                race_view_set_camera(g->view, eye, rows, sx, sx * 1.30122f, 0.1f);
            }
            race_view_show_car(g->view, view != 0);
        }
    }
    if (previous_start != g->start_cam_on || previous_tv != g->tv_on)
        race_audio_camera_cut(g->audio);
    if (!g->inspect_on || !g->inspect_seeded)
        license_camera(g);
    if (!g->split && !g->start_cam_on && (!g->inspect_on || !g->inspect_seeded)) {
        int entry = watched_car(g);
        float look[9], fov, origin[4] = {0};
        /* 003CD0A0 -> 003CCBD0: return previous seed, then store the
           XOR-swapped next value; it advances on each camera evaluation. */
        int random = (int)g->pit_camera_random;
        uint32_t next = g->pit_camera_random * 17u + 17u;
        g->pit_camera_random = next ^ ((next << 16) | (next >> 16));
        if (entry >= 0 && entry < MOON_CONDUCTOR_CARS)
            gt4_pit_camera_pick(g->pit_crew + entry, random);
        if (entry >= 0 && entry < MOON_CONDUCTOR_CARS && gt4_pit_camera(g->pit_crew + entry, look, &fov))
            license_look(g, look, fov, origin);
    }
    inspect_camera(g, dt);
    film_camera(g, dt);
    if (g->cinema.on) {
        race_view_get_camera(g->view, g->cinema.original_eye, g->cinema.original_rows,
                             g->cinema.original_projection);
        g->cinema.original_visible = race_view_car_visible(g->view);
    }
    cinematic_camera(g, dt);
    if (!g->audio)
        g->audio = race_audio_open_replay(g->disc, g->run, race_view_course(g->view), g->replaying);
    if (g->audio)
        for (int i = 0; i < gt4_race_run_cars(g->run); i++)
            race_audio_set_exhaust_count(g->audio, (unsigned)i,
                                         race_view_exhaust_count(g->view, i));
    if (g->audio) {
        float eye[3], rows[9], projection[3];
        if (race_view_get_camera(g->view, eye, rows, projection))
            race_audio_update(
                g->audio, g->run, eye, rows,
                g->start_cam_on || g->tv_on || g->inspect_on || g->cinema.on ||
                        getenv("MOON_RACE_FILM") ||
                        (watched_car(g) < MOON_CONDUCTOR_CARS && g->pit_crew[watched_car(g)].camera_control >= 0)
                    ? -1
                    : g->camera,
                (g->bgm_enabled != g->bgm_off) ? g->bgm_volume : -1, g->se_volume);
    }
}

/* the world's frame for the pit menu (0038AB98): the watched car's stop
   asks for it (0035BC70: its tyres, its fuel; B-Spec's mode from
   world+0xCC8 1 or 3), it goes when the stop no longer waits (0035BCE8),
   the first key stops the stop's own countdown (0035BCC8), the choice goes
   to the car (0035BF08). Unavailable service roles are presentation
   flags (003D3080/003D30D0/003D3120); they do not change service physics. */
static void pit_sound(void *user, const char *name)
{
    race_game *g = user;
    race_audio_menu_sound(g->audio, name, g->se_volume);
}
void race_game_set_pit_mode_change(race_game *g, int allowed)
{
    /* Native session options are kept on the host; captured sessions
       still read their original option block. No physics memory write. */
    if (g)
        g->pit_mode_allowed = allowed != 0;
}
static void pit_frame(race_game *g, double dt)
{
    race_pitmenu_choice c;
    if (g->replaying || !g->run)
        return;
    { /* tests: MOON_RACE_PIT=x:<keys> has the B-Spec driver pit 8 s in, then
           sends the menu Flash key codes ("38,39,13"), one each 0.4 s */
        static double clock, up;
        static int sent, keys;
        const char *t = getenv("MOON_RACE_PIT");
        if (t && g->bspec_ui && sent < 2 && (clock += dt) > 8) {
            sent++;
            bspec_order(g, 6, 1);
        }
        if (t && race_pitmenu_up(g->pitmenu)) {
            int n = 0;
            if (!g->pit_touched)
                g->pit_touched = 1;
            up += dt;
            for (const char *q = strchr(t, ':'); q; q = strchr(q + 1, ','), n++)
                if (n == keys && up > 1 + 0.4 * keys) {
                    race_pitmenu_key(g->pitmenu, atoi(q + 1));
                    keys++;
                    break;
                }
        }
    }
    if (!g->pitmenu && !(g->pitmenu = race_pitmenu_open(g->vol, pit_sound, g)))
        return;
    {
        int i = watched_car(g), v;
        uint32_t car = gt4_race_run_car(g->run, i), cond = gt4_race_run_conductor(g->run),
                 world = GT4_U32(GT4_U32(cond) + 0x84);
        v = gt4_pit_menu_ask(car);
        if (v & 1) {
            race_pitmenu_setup s;
            memset(&s, 0, sizeof s);
            s.tyres = v >> 1 & 1;
            s.feed = v >> 3 & 1;
            s.front = GT4_U8(cond + 0xfb20u + (uint32_t)i * 2);
            s.rear = GT4_U8(cond + 0xfb21u + (uint32_t)i * 2);
            s.bspec = GT4_U32(world + 0xcc8) == 1 || GT4_U32(world + 0xcc8) == 3;
            s.mode = g->pit_mode_allowed;
            uint32_t options = GT4_U32(world + 0x6c);
            /* 0038AB98 +244 -> 003C12A8: original mode-change gate. */
            if (options && options < 0x2000000 - 0x974)
                s.mode = GT4_U32(options + 0x970) != 0;
            if (i == 0 && g->last_prepared && g->last_desc.event_row && g->last_desc.owned_car[0]) {
                s.owned_filter = 1;
                gt4_pit_owned_tires(g->last_owned[0], s.owned);
            }
            for (int entry = 0; entry < gt4_race_run_cars(g->run); entry++)
                gt4_pit_crew_service_enable(g->pit_crew + entry, s.tyres, s.feed);
            race_pitmenu_show(g->pitmenu, &s);
            g->pit_car = i;
            g->pit_touched = 0;
        }
    }
    if (!race_pitmenu_up(g->pitmenu))
        return;
    {
        uint32_t car = gt4_race_run_car(g->run, g->pit_car);
        if (gt4_pit_menu_gone(car)) {
            race_pitmenu_hide(g->pitmenu);
            return;
        }
        if (g->pit_touched == 1) {
            gt4_pit_menu_touch(car);
            g->pit_touched = 2;
        }
        race_pitmenu_update(g->pitmenu, dt);
        if (race_pitmenu_choice_take(g->pitmenu, &c))
            gt4_pit_menu_choose(car, c.front, c.rear, c.feed, c.mode);
    }
}

static void publish_photo_poses(race_game *g)
{
    const gt4_course_gadgets *state = gt4_race_run_objects(g->run);
    uint32_t course = GT4_U32(gt4_race_run_car(g->run, 0) + 8);
    gt4_course_gadgets *draw = race_view_gadgets(g->view, (int32_t)GT4_U32(course + 0xb0));
    if (state && draw && state->count == draw->count)
        for (unsigned i = 0; i < draw->count; i++) {
            const uint8_t *descriptor = draw->instance[i].descriptor;
            draw->instance[i] = state->instance[i];
            draw->instance[i].descriptor = descriptor;
        }
    float pos[3], rot[9], wheels[4][3];
    if (gt4_race_run_pose(g->run, 0, pos, rot, wheels)) {
        race_view_set_pose(g->view, pos, rot);
        race_view_set_wheels(g->view, wheels);
        update_wheel_motion(g, 0, rot);
    }
    for (int i = 0; i <= g->others; i++) {
        float sun, level;
        if (gt4_race_run_shadow(g->run, i, pos, rot, &sun, &level))
            race_view_set_shadow(g->view, i, pos, rot, sun, level);
        else
            race_view_set_shadow(g->view, i, NULL, NULL, 0, 0);
    }
    for (int i = 1; i <= g->others; i++)
        if (gt4_race_run_pose(g->run, i, pos, rot, wheels)) {
            race_view_set_opponent(g->view, i - 1, pos, rot, wheels);
            update_wheel_motion(g, i, rot);
        }
    for (int j = 0; j < gt4_race_run_phantom_count(g->run); j++)
        if (gt4_race_run_phantom_pose(g->run, j, pos, rot, wheels))
            race_view_set_opponent(g->view, g->others + j, pos, rot, wheels);
    if (licence_exam_run(g) && !g->ghost_car && !g->split) {
        unsigned frame = gt4_race_run_frames(g->run);
        float now[LIC_SAMPLE];
        if (!g->replaying && gt4_race_run_pose(g->run, 0, now + 1, now + 4, (float(*)[3])(now + 13))) {
            now[0] = (float)frame;
            if (!g->lic_rec.n || g->lic_rec.s[(g->lic_rec.n - 1) * LIC_SAMPLE] < now[0]) {
                if (g->lic_rec.n == g->lic_rec.cap) {
                    size_t cap = g->lic_rec.cap ? g->lic_rec.cap * 2 : 4096;
                    float *s = realloc(g->lic_rec.s, cap * LIC_SAMPLE * sizeof(float));
                    if (s) {
                        g->lic_rec.s = s;
                        g->lic_rec.cap = cap;
                    }
                }
                if (g->lic_rec.n < g->lic_rec.cap)
                    memcpy(g->lic_rec.s + g->lic_rec.n++ * LIC_SAMPLE, now, sizeof now);
            }
        }
        /* the held attempt at this frame, between its two nearest poses */
        int shown = 0;
        if (!g->replaying && g->lic_ghost.on && g->lic_ghost.n > 1 &&
            !strcmp(g->lic_ghost.course, g->last_course) && !strcmp(g->lic_ghost.car, g->last_car)) {
            const float *s = g->lic_ghost.s;
            size_t lo = 0, hi = g->lic_ghost.n - 1;
            if ((float)frame >= s[0] && (float)frame <= s[hi * LIC_SAMPLE]) {
                while (hi - lo > 1) {
                    size_t mid = (lo + hi) / 2;
                    if (s[mid * LIC_SAMPLE] <= (float)frame)
                        lo = mid;
                    else
                        hi = mid;
                }
                const float *a = s + lo * LIC_SAMPLE, *b = s + hi * LIC_SAMPLE;
                float t = b[0] > a[0] ? ((float)frame - a[0]) / (b[0] - a[0]) : 0, out[LIC_SAMPLE];
                for (int k = 1; k < LIC_SAMPLE; k++)
                    out[k] = a[k] + (b[k] - a[k]) * t;
                race_view_set_ghost(g->view, 0, .55f);
                race_view_set_opponent(g->view, 0, out + 1, out + 4, (float(*)[3])(out + 13));
                if (!g->lic_rec.n || g->lic_rec.n == 1)
                    fprintf(stderr, "ghost: licence attempt %u shown\n", g->lic_ghost.time);
                shown = 1;
            }
        }
        if (!shown)
            race_view_set_opponent(g->view, 0, NULL, NULL, NULL);
    }
    if (g->ghost_car) { /* the ghost, while it drives a lap */
        /* see-through: measured on a PCSX2 frame (Escort, Laguna Seca): its rear
           (64, 66, 72) between the player's same body (13, 13, 36) and the
           road beside it (125, 124, 122) gives 0.53..0.58 by channel; the
           code that blends it is not found */
        race_view_set_ghost(g->view, 0, .55f);
        if (!g->ghost_hidden &&
            (gt4_race_run_ghost_on(g->run) || getenv("MOON_RACE_SNAPSHOT")) &&
            gt4_race_run_pose(g->run, 1, pos, rot, wheels)) {
            race_view_set_opponent(g->view, 0, pos, rot, wheels);
            update_wheel_motion(g, 1, rot);
        } else
            race_view_set_opponent(g->view, 0, NULL, NULL, NULL);
    }
}

void race_game_update(race_game *g, double dt)
{
    if (!g || !g->ready || !g->run || g->photo_held)
        return;
    pause_input(g);
    { /* tests: MOON_RACE_PAUSE=<seconds>[,key,key...] opens the pause
           menu then sends Flash key codes, one each half second */
        const char *t = getenv("MOON_RACE_PAUSE");
        static double clock;
        static int sent;
        if (t) {
            clock += dt;
            double at = atof(t);
            if (!sent && clock >= at) {
                pause_toggle(g);
                sent = 1;
            }
            const char *q = strchr(t, ',');
            for (int k = 1; q && g->pause; k++, q = strchr(q + 1, ','))
                if (sent == k && clock >= at + k * .5) {
                    race_pause_key(g->pause, atoi(q + 1));
                    sent++;
                }
        }
    }
    if (g->pause || g->held) { /* held: no step, no controls; the car sounds quiet */
        cinematic_camera(g, dt);
        if (g->audio) {
            float eye[3], rows[9], projection[3];
            if (race_view_get_camera(g->view, eye, rows, projection))
                race_audio_update(g->audio, g->run, eye, rows,
                                  g->start_cam_on || g->inspect_on ? -1 : g->camera,
                                  (g->bgm_enabled != g->bgm_off) ? g->bgm_volume : -1, 0);
            race_audio_hold(g->audio, dt);
        }
        return;
    }
    {
        gt4_race_hud hud;
        float kmh = gt4_race_run_hud(g->run, 0, &hud) ? hud.speed * 3.6f : 0.f;
        if (!g->assist_kmh_set) {
            g->assist_kmh = kmh;
            g->assist_kmh_set = 1;
        } else
            g->assist_kmh += (kmh - g->assist_kmh) * (1.f - expf((float)-dt / .25f));
        video_settings_speed_frame(kmh, dt);
        moon_debug_overlay_set_race(kmh, video_settings_assist_line(),
                                    (int)(video_settings_speed_level() * 100.f + .5f));
    }
    read_controls(g);
    bspec_frame(g, dt);
    if (!g->split)
        pit_frame(g, dt);
    double camera_dt = dt;
    if (g->bspec_ui)
        dt *= race_bspec_speed(
            g->bspec_ui); /* the monitor's view speed (+0x74, x1..x3). A: how the world applies it not read */
    if (getenv("MOON_RACE_SPEED"))
        dt *= atof(getenv("MOON_RACE_SPEED")); /* tests: the race run faster */
    if (g->replaying && g->fast)
        dt *=
            2; /* A: the replay player's step (+0x74, func_003D9FB8: 1 to 3) taken as one up; L3's own handler not read */
    if (g->label) { /* func_003A9780 */
        if (!g->label_out && (g->label_t -= camera_dt) < 0)
            g->label_out = 1;
        g->label_v = fmaxf(0, fminf(1, g->label_v + (float)camera_dt * (g->label_out ? -4 : 4)));
        if (g->label_out && g->label_v <= 0)
            g->label = NULL;
    }
    g->acc += dt;
    if (g->acc > 0.1 + dt)
        g->acc = 0.1 + dt;
    if (getenv("MOON_RACE_SNAPSHOT"))
        g->acc = 0; /* tests: the captured moment stays */
    int stepped = 0;
    while (g->acc >= STEP) {
        /* 0033C490 +10..34: an event-10 stop is consumed on the next
           world tick before advancing the car. Preserve its final pose. */
        g->mission_presentation = gt4_mission_begin_frame(g->mission_presentation);
        if (g->mission_presentation == GT4_MISSION_STOPPED) {
            fprintf(stderr, "mission stop: frame %u\n", g->license_test_frame);
            g->acc -= STEP;
            break;
        }
        if (g->replaying && g->disc_segment < g->disc_segments &&
            g->tape_at == g->disc_segment_at[g->disc_segment]) {
            /* FreeRun replay entry, 00359D40 +A4..134: reset clock to
               100 thirds and restore the recorded car before its tape.
               A: app frame entry replaces the original session callback. */
            uint32_t cond = gt4_race_run_conductor(g->run), race = GT4_U32(cond);
            if (!g->disc_segment) {
                GT4_U32(cond + CONDUCTOR_FIELD_RACE_CLOCK) = 100;
                GT4_U32(race + 0xbc) = 0;
            }
            fenv_t env;
            fegetenv(&env);
            gt4_phys_fpu_enter();
            disc_checkpoint_restore(cond, 0, g->disc_states[g->disc_segment]);
            GT4_F32(gt4_race_run_car(g->run, 0) + 0x554) = 0;
            fesetenv(&env);
            g->disc_segment++;
        }
        if (g->replaying && g->disc_segment_2p < g->disc_segments_2p &&
            g->tape_at_2p == g->disc_segment_at_2p[g->disc_segment_2p]) {
            fenv_t env;
            fegetenv(&env);
            gt4_phys_fpu_enter();
            disc_checkpoint_restore(gt4_race_run_conductor(g->run), 1,
                                    g->disc_states_2p[g->disc_segment_2p]);
            GT4_F32(gt4_race_run_car(g->run, 1) + 0x554) = 0;
            fesetenv(&env);
            g->disc_segment_2p++;
        }
        /* Controller sampling follows the physics step, including accelerated
           test runs. Ordinary input is still sampled by read_controls. */
        if (getenv("MOON_LICENSE_TEST_BRAKE_AT"))
            read_controls(g);
        if (!SWP_VALUE(SWP_PHYSICS, gt4_race_run_frame(g->run))) {
            fprintf(stderr, "race: the step failed\n");
            race_game_stop(g);
            return;
        }
        g->license_test_frame++;
        license_start_step(g);
        license_step(g);
        if (race_game_mission_active(g) && !g->replaying) {
            uint32_t race = GT4_U32(gt4_race_run_conductor(g->run));
            int previous = g->mission_presentation;
            g->mission_presentation = gt4_mission_presentation(gt4_ptr(race + 0xf4), previous);
            if (g->mission_presentation && g->finished < 0)
                g->finished = 0;
            if (previous != g->mission_presentation)
                fprintf(stderr, "mission presentation: %s frame %u\n",
                        g->mission_presentation == GT4_MISSION_RETURN ? "return" : "board",
                        g->license_test_frame);
        }
        if (g->license_log_collecting)
            logger_sample(g);
        /* 00389B08: arm the end-of-replay fade after the recorded step. */
        if (g->replaying) {
            uint32_t session = GT4_U32(gt4_race_run_conductor(g->run));
            gt4_music_replay_fade(&GT4_F32(session + 0xd4), &GT4_F32(session + 0xd8),
                                  (int32_t)(g->tape_n - g->tape_at), 2,
                                  GT4_U32(session + 0xe0) != 0, 0);
        }
        update_drivers(g);
        timing_events(g);
        race_smoke_update(g->smoke, (float)STEP);   /* 0033C4F0 -> 003E7A58 */
        race_leaves_update(g->leaves, (float)STEP); /* 0033C4F0 -> 003C2780 */
        /* Presentation only, after the service solver. All crews advance
           once per physical step; only the watched car posts one-shots. */
        {
            gt4_pit_crew_sound heard = {0};
            int watched = watched_car(g);
            for (int i = 0; i < gt4_race_run_cars(g->run); i++)
                if (g->pit_motion[i]) {
                    gt4_pit_crew_sound out;
                    /* 003D03E0: 003D0D88 placement precedes 003CFA98 clocks.
                   Retain the previous phase for its seek/history test. */
                    int previous = g->pit_crew[i].stage,
                        next = gt4_pit_stage(gt4_race_run_car(g->run, i));
                    if (next) {
                        g->pit_crew[i].stage = next;
                        gt4_pit_crew_place(g->pit_crew + i, gt4_race_run_car(g->run, i),
                                           (unsigned)i, race_view_car_geometry(g->view, i),
                                           g->pit_package == 1, &g->pit_random);
                        g->pit_crew[i].stage = previous;
                    }
                    gt4_pit_crew_step(g->pit_crew + i, gt4_race_run_car(g->run, i), (float)STEP,
                                      &out);
                    gt4_pit_camera_step(g->pit_crew + i, (float)STEP);
                    heard.refueling |= out.refueling;
                    if (i == watched) {
                        heard.count = out.count;
                        memcpy(heard.effect, out.effect, out.count);
                    }
                }
            race_audio_pit(g->audio, &heard);
            for (unsigned i = 0; i < (unsigned)gt4_race_run_cars(g->run); i++)
                if (g->pit_motion[i]) {
                    gt4_pit_crew *c = g->pit_crew + i;
                    unsigned lane = GT4_U8(c->car + CAR_56B);
                    /* 003CFA98 keeps 0035BD10's command only in phase 1. */
                    int commands = c->stage == 1 ? (lane == 4                ? 1
                                                    : lane == 2 || lane == 3 ? (int)lane
                                                                             : 0)
                                                 : 0;
                    gt4_pit_walk_step(g->pit_crew, (unsigned)gt4_race_run_cars(g->run), i, commands,
                                      (float)STEP, &g->pit_random);
                }
        }
        SWP_VOID(SWP_AUDIO_CONTROL, race_camera_audio_update(g, STEP));
        /* 0033B358: retain adjacent render frames, without advancing during exposure. */
        publish_photo_poses(g);
        race_view_photo_record(g->view);
        SWP_VOID(SWP_AUDIO_CONTROL, race_audio_step(g->audio, g->run));
        stepped++;
        if (getenv(
                "MOON_RACE_REPLAY_LOG")) { /* tests: the player's place by the tape's step, live and replayed */
            size_t at = g->replaying ? g->tape_at : g->tape_n;
            float p3[3], r9[9], w[4][3];
            if (at && at % 300 == 0 && gt4_race_run_pose(g->run, 0, p3, r9, w))
                fprintf(stderr, "replay-log %s %zu %.6f %.6f %.6f\n",
                        g->replaying ? "play" : "live", at, p3[0], p3[1], p3[2]);
        }
        g->acc -= STEP;
        { /* tests: MOON_REPLAY_BUTTONS=<seconds>:<button>,... presses the replay's buttons */
            const char *t = getenv("MOON_REPLAY_BUTTONS");
            for (; g->replaying && t && *t; t = strchr(t, ',') ? strchr(t, ',') + 1 : "")
                if ((size_t)(atof(t) * 60) == g->tape_at && strchr(t, ':'))
                    replay_button(g, atoi(strchr(t, ':') + 1));
        }
        if (g->replaying && g->tape_at >= g->tape_n && (!g->split || g->tape_at_2p >= g->tape_n_2p))
            g->replay_end = g->replay_end < 0 ? 0 : g->replay_end + STEP;
        if (g->finished >= 0)
            g->finished += STEP;
        else if (gt4_race_run_finished(g->run))
            g->finished = 0;
    }
    if (!stepped)
        publish_photo_poses(g);
    /* Snapshot/initialization has no physics tick to publish its camera. */
    if (!stepped &&
        (!g->audio || getenv("MOON_RACE_SNAPSHOT") || (g->cinema.on && !g->cinema.seeded)))
        race_camera_audio_update(g, camera_dt);
}

static int race_game_draw_scene(race_game *g)
{
    if(g && g->run) race_view_daylight_time(g->view,gt4_race_run_frames(g->run)/60.);
    if (!g || !g->ready)
        return 0;
    if (g->split) {
        int drawn = 1;
        /* 0032FCC8 +130..258: horizontal top/bottom, each height/2.
           Models, textures, animation and physics remain shared. */
        for (int i = 0; i < 2; i++) {
            if (!g->split_camera_valid[i])
                split_camera(g, i);
            float *p = g->split_projection[i];
            race_view_set_camera(g->view, g->split_eye[i], g->split_rows[i], p[0], p[1], p[2]);
            drawn &= race_view_draw_player(g->view, i, (i ? g->camera_2p : g->camera) != 0,
                                           i * 224.f, 224);
            int camera = i ? g->camera_2p : g->camera;
            if (camera == 0 || camera == 6) {
                /* 0032FCC8 +17c..1d0 / +280..2d4 -> 00341E50:
                   E2E0 = (200,16/8,240,40,2). Rear camera +18,
                   projection reflection at 0034203c..4c.
                   A: pose adapter uses the installed camera axes. */
                float rear[9];
                memcpy(rear, g->split_rows[i], sizeof rear);
                for (int k = 0; k < 3; k++)
                    rear[6 + k] = -rear[6 + k];
                const float *m = gt4_split_mirror[i];
                race_view_set_camera(g->view, g->split_eye[i], rear, m[4] * m[3] / m[2], m[4],
                                     p[2]);
                race_view_draw_mirror(g->view, i, m[0], i * 224.f + m[1], m[2], m[3]);
                race_view_set_camera(g->view, g->split_eye[i], g->split_rows[i], p[0], p[1], p[2]);
            }
        }
        return drawn;
    }
    for (int i = 0; g->audio && g->run && i < gt4_race_run_cars(g->run) && i < MOON_CONDUCTOR_CARS;
         i++) { /* the backfire's flames */
        float flames[4][3];
        int on = race_audio_afterfire(g->audio, (unsigned)i, flames);
        race_view_set_afterfire(g->view, (unsigned)i, flames, on);
    }
    {
        int licence = g->last_desc.license == 1 || g->last_desc.license == 2;
        int show = video_settings_assist_line() || (licence && g->best_line);
        uint32_t player = g->run ? gt4_race_run_car(g->run, 0) : 0;
        uint32_t lines = player ? GT4_U32(player + CAR_FIELD_DRIVER_MODEL + 0xec) : 0;
        int line_index = player ? GT4_U8(player + CAR_FIELD_DRIVER_MODEL + 0x1c) : 6;
        race_view_set_assist_source(g->view, lines, line_index);
        static int assist_dumped;
        if (lines && !assist_dumped && getenv("MOON_ASSIST_DUMP")) {
            assist_dumped = 1;
            enum { DUMP_MAX = 16384 };
            gt4_ai_line_point *pts = calloc(DUMP_MAX, sizeof *pts);
            if (pts) {
                int active = -1;
                int n = gt4_ai_line_export(lines, line_index, pts, DUMP_MAX, &active);
                fprintf(stderr,
                        "assist dump: course=%s active_line=%d table_lines=%d points=%d\n",
                        g->last_course, active, gt4_ai_line_table_count(lines), n);
                for (int i = 0; i < n && i < 3; i++)
                    fprintf(stderr, "  first[%d] x=%.3f y=%.3f z=%.3f d=%.2f trim=%d r=%.1f kind=%d\n",
                            i, pts[i].x, pts[i].y, pts[i].z, pts[i].distance, pts[i].speed_trim,
                            pts[i].arc_radius, pts[i].kind);
                for (int i = n > 3 ? n - 3 : 0; i < n; i++)
                    fprintf(stderr, "  last[%d] x=%.3f y=%.3f z=%.3f d=%.2f trim=%d r=%.1f kind=%d\n",
                            i, pts[i].x, pts[i].y, pts[i].z, pts[i].distance, pts[i].speed_trim,
                            pts[i].arc_radius, pts[i].kind);
                free(pts);
            }
        }
        int authored = show && licence &&
                       race_view_set_best_line(g->view, g->last_desc.license_code, 1);
        if (authored)
            race_view_set_assist_line(g->view, NULL, 0, 0);
        else if (show && g->last_course[0]) {
            if (!licence)
                race_view_set_best_line(g->view, NULL, 0);
            race_view_set_assist_line(g->view, g->last_course, 1, g->assist_kmh);
        } else {
            race_view_set_best_line(g->view, NULL, 0);
            race_view_set_assist_line(g->view, NULL, 0, 0);
        }
    }
    int drawn = race_view_draw(g->view);
    if (drawn && g->pit_scene) {
        r3d_view view = {0};
        float rows[9], projection[3];
        if (race_view_get_camera(g->view, view.cam, rows, projection)) {
            memcpy(view.r, rows, 12);
            memcpy(view.u, rows + 3, 12);
            for (unsigned k = 0; k < 3; k++)
                view.f[k] = -rows[6 + k];
            view.focal = (448.f * 4.f / 3.f) * .5f * projection[0];
            view.focal_y = 224 * projection[1];
            view.near_clip = projection[2];
            for (int i = 0; i < gt4_race_run_cars(g->run); i++)
                if (g->pit_crew[i].placed)
                    license_scene_draw_pit(g->pit_scene, &view, g->pit_crew + i,
                                           GT4_U32(gt4_race_run_car(g->run, i) + 8),
                                           race_view_course(g->view), i == watched_car(g));
        }
    }
    if (drawn && g->license_scene && g->concourse.placement_ready) {
        r3d_view view = {0};
        float rows[9], projection[3];
        if (race_view_get_camera(g->view, view.cam, rows, projection)) {
            memcpy(view.r, rows, 12);
            memcpy(view.u, rows + 3, 12);
            for (unsigned k = 0; k < 3; k++)
                view.f[k] = -rows[6 + k];
            /* Same native pixel transform as race_view_draw's world/car.
               This adapter is A; scalar lens coefficients are tested C. */
            view.focal = (448.f * 4.f / 3.f) * .5f * projection[0];
            view.focal_y = 224 * projection[1];
            view.near_clip = projection[2];
            /* 00408490 draws authored pitman01 shadows and workman00
               with each actor's ground light context (00409B68). */
            uint32_t car = gt4_race_run_car(g->run, 0);
            license_scene_draw(g->license_scene, &view, g->license_people,
                               GT4_U32(car + CAR_COURSE), race_view_course(g->view),
                               g->license_surfaces);
        }
    }
    if (drawn && g->license_motion && g->license_fade.value < 1.f) {
        /* 0038B5A0 +0x18 draws Scene fade before display/result overlays.
           Original color 00620200 = black. A: 8-bit GL alpha conversion;
           the clock, clamping and cubic factor are tested separately E. */
        float factor = gt4_license_fade_factor(g->license_fade.value);
        uint8_t black[4] = {0, 0, 0, (uint8_t)((1.f - factor) * 255.f)};
        plat_video_set_view(640, 448);
        plat_draw_quad(NULL, 0, 0, 640, 448, 0, 0, 1, 1, black);
    }
    return drawn;
}

int race_game_draw(race_game *g)
{
    if (!g || !g->cinema.on || !g->cinema.aperture || g->split)
        return race_game_draw_scene(g);
    /* P: optional live depth of field, eight GPU samples of the existing
       Photo Travel lens. Scene/physics clocks never advance between samples. */
    float eye[3], rows[9], pr[3], samples[8][4];
    if (!race_view_get_camera(g->view, eye, rows, pr))
        return 0;
    gt4_photo_samples(samples, 8);
    static const float aperture[4] = {0, 2.8f, 5.6f, 11};
    const uint8_t tint[4] = {255, 255, 255, 255};
    int drawn = 0;
    for (int i = 0; i < 8; i++) {
        float shift[2], jitter[2], e[3];
        cinematic_lens(pr, aperture[g->cinema.aperture], g->cinema.focus, samples[i], shift,
                       jitter);
        for (int k = 0; k < 3; k++)
            e[k] = eye[k] + rows[k] * shift[0] + rows[3 + k] * shift[1];
        race_view_set_camera(g->view, e, rows, pr[0], pr[1], pr[2]);
        race_view_photo_jitter(g->view, jitter);
        if (!race_game_draw_scene(g))
            goto done;
        g->cinema.sample = video_grab(g->cinema.sample, 0, 0, 640, 448);
        if (!g->cinema.sample)
            goto done;
        if (i) {
            plat_draw_quad(g->cinema.image, 0, 0, 640, 448, 0, 1, 1, 0, tint);
            video_flush2d();
        }
        plat_draw_quad_blend(g->cinema.sample, 0, 0, 640, 448, 0, 1, 1, 0, tint, 1.f / (i + 1));
        g->cinema.image = video_grab(g->cinema.image, 0, 0, 640, 448);
        if (!g->cinema.image)
            goto done;
    }
    drawn = 1;
done:
    race_view_photo_jitter(g->view, NULL);
    race_view_set_camera(g->view, eye, rows, pr[0], pr[1], pr[2]);
    return drawn;
}

#include "race_game_cameras.inc"

#include "race_game_pregrid.inc"

void race_game_set_unit(race_game *g, int unit)
{
    if (g)
        g->unit = unit;
}
void race_game_set_power_unit(race_game *g, const char *unit)
{
    static const char *const names[7] = {"PS", "HP", "BHP", "CH", "KW", "CV", "PF"};
    if (!g)
        return;
    g->power_unit = 1;
    for (int i = 0; i < 7; i++)
        if (unit && !strcmp(unit, names[i]))
            g->power_unit = i;
    if (unit && !strcmp(unit, "kW"))
        g->power_unit = 4; /* previous native files */
}

void race_game_apply_units(race_game *g, menu *options)
{
    /* GranTurismo4Option unit_velocity: KMPH=0, MPH=1 (005A6978).
       UnitRoot.apply publishes these shared presentation preferences. */
    race_game_set_unit(g, strcmp(menu_option_text(options, "unit_velocity", "MPH"), "KMPH") != 0);
    race_game_set_power_unit(g, menu_option_text(options, "unit_power", "HP"));
}

/* the replay's opening, the tape's seconds in (func_0039BEC8: the mode's
   key from the session, vtable+0x308) */
static void replay_opening(race_game *g)
{
    race_hud_opening o;
    if (!g->open_on || !g->hud)
        return;
    o.car = g->names[0];
    o.power = g->open_power;
    o.unit = g->power_unit;
    o.front = g->open_front;
    o.rear = g->open_rear;
    o.mode = g->time_trial      ? "ARCADE_TIME_TRIAL"
             : g->last_prepared ? "GTMODE_EVENT_RACE"
                                : "ARCADE_SINGLE_RACE";
    o.mode_n = 0;
    race_hud_draw_opening(g->hud, &o, (float)g->tape_at / 60);
}
void race_game_set_audio(race_game *g, int bgm, int enabled, int se)
{
    if (g) {
        g->bgm_volume = bgm;
        g->bgm_enabled = enabled != 0;
        g->se_volume = se;
    }
}

#include "race_game_hud.inc"

/* 0011ABE0: Photo Drive single-car RACE mode 0, or a resident replay.
   This borrows only the renderer; the command tape and solver stay paused. */
race_view *race_game_photo_view(race_game *g)
{
    return g && g->ready && !g->split &&
                   (g->replaying || (g->last_prepared && !g->last_desc.license &&
                                     g->last_desc.cars == 1 && g->last_desc.event_row[0x43] == 0))
               ? g->view
               : NULL;
}
int race_game_photo_select(race_game *g, int ps2key, int release)
{
    /* Retail manual pp24/25/28: Select during replay opens PhotoPause.
       The original attract/replay caller reaches 003C3CE8 at 0010CE14. */
    return g && g->replaying && ps2key == 0xffff && !release && race_game_photo_view(g);
}
void race_game_photo_pause(race_game *g, int on)
{
    if (g) {
        g->photo_held = on != 0;
        race_audio_pause(g->audio, on || g->held);
        if (on) {
            plat_input_set_ffb(0, 0);
            plat_input_set_ffb(1, 0);
        }
    }
}

/* Existing immutable camera queries, shared with PhotoPause. */
int race_game_photo_ground(void *ctx,const float p[3])
{
    race_game *g=ctx;return g&&g->run?gt4_race_run_ground_id(g->run,p):-1;
}
int race_game_photo_wall(void *ctx,float margin,const float from[3],const float to[3],float out[3],int *id)
{
    race_game *g=ctx;return gt4_race_run_camera_wall(g?g->run:NULL,margin,from,to,out,id);
}

int race_game_photo_camera_data(race_game *g, float size[4], float *distance)
{
    if (!g || !g->run || !size || !distance)
        return 0;
    uint32_t car = gt4_race_run_car(g->run, watched_car(g)), params = GT4_U32(car + CAR_PARAMS);
    /* 003F3708/20/38/48, frozen car parameters; 003FDC50 distance. */
    size[0] = GT4_F32(params) + GT4_F32(params + 0x50);
    size[1] = GT4_F32(params + 4) - GT4_F32(params + 0x50);
    size[2] = GT4_F32(params + 8);
    size[3] = GT4_F32(params + 12);
    *distance = GT4_F32(car + CAR_FIELD_COURSE_DISTANCE);
    return 1;
}
