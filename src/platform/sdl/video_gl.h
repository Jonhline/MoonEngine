/* Internal interface between the OpenGL video (video_gl.c) and the 3D
   renderer of the SDL backend. */
#ifndef MOON_VIDEO_GL_H
#define MOON_VIDEO_GL_H

#include "platform/platform.h"
#ifdef MOON_DEKO3D
/* Native Switch IDs, never OpenGL objects. */
typedef unsigned GLuint;
unsigned gl_program_count(void);
int video_read_texture(const plat_texture *texture, uint8_t *rgba);
void video_texture_dimensions(const plat_texture *texture, int *width, int *height);
float video_read_depth(int x, int y);
#else
#include "gl.h"
#endif

/* Draws pending 2D quads (before 3D, captures and state changes). */
void video_flush2d(void);
/* Bound frontend rendering to its native 60 Hz; race timing is separate. */
void video_set_menu_pacing(int enabled);
/* Frames whose pacing deadline had already passed when they began (the debug
   overlay's "late" reading). Lifetime count; 0 before the first frame. */
uint64_t video_frame_late_count(void);
/* Opt-in offline movie export: one original 60 Hz update per real frame. */
int video_export_fixed_step(void);
/* Menu 2D maps to the whole PS2 framebuffer; race HUD has wide layout. */
void video_set_menu_projection(int enabled);
GLuint video_texture_id(const plat_texture *t);
GLuint video_draw_framebuffer(void);
/* Native or FSR 1 ultra quality / quality / balanced / performance (0..4). */
int video_set_upscale(int mode);
/* Original display ratio: 0 = 4:3, 1 = 16:9. */
void video_set_aspect(int widescreen);
/* The studio fills the window; the game keeps the disc's 4:3. */
void video_set_fill_window(int enabled);
int video_fill_window(void);
/* Sense-of-speed post pass (PC only): radial blur amount 0..100, vignette 0/1. */
void video_set_speed_post(int blur, int amount, int vignette);
/* Current offscreen frame target (the main FBO), for diagnostics. */
void video_target_size(int *width, int *height);
/* Current 3D scene target, for diagnostics. */
void video_scene_target_size(int *width, int *height);
float video_projection_x(void);
int video_fsr_available(void);
/* Android-only: shrink the 3D scene target (0 = full, 1 = 75%, 2 = 50%,
   3 = 35%). The existing end-of-scene blit scales it back; UI stays native. */
void video_set_scene_render_scale(int level);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
void video_scene_quality(int antialiasing, int render_scale);
#endif
typedef struct {
    float camera[3], right[3], up[3], forward[3], sun[3];
    float focal[2], clip[2], shadow_projection[16];
    GLuint shadow;
    int valid;
} video_light_volume;
void video_light_volume_view(const video_light_volume *view);
void video_ambient_occlusion(int enabled);
void video_hdr_options(int enabled, int rays);
int video_hdr_active(void);
void video_solar_projection(float x,float y,float visible,const float color[3]);
void video_begin_scene(void);
void video_end_scene(void);
/* UI filtering: 0 linear, 1 sharp cubic, 2 nearest. Text is separate. */
void video_ui_quality(int filter, float text_sharpness);
int video_set_vsync(int enabled);
void video_performance_options(int low_latency, int fps_limit);
/* A rectangle of the virtual screen as a GL viewport in frame pixels. */
#ifndef __SWITCH__
int video_window_to_ui(int x, int y, float *u, float *v);
#endif
void video_viewport(float x, float y, float w, float h, int *vx, int *vy, int *vw, int *vh);
/* Copies what the frame holds in a rectangle of the virtual screen into t
   (created or resized as needed; NULL makes one). Drawn back with
   plat_draw_quad4 and v from 1 (top) to 0: GL rows run bottom up. */
float video_view_height(void);
plat_texture *video_capture_frame(plat_texture *t);
plat_texture *video_grab(plat_texture *t, float x, float y, float w, float h);
/* A grab's pixels as the texture holds them (rows bottom up), to keep it on disk, and the
   texture made again from them: screen is the grab's full-frame mark. */
uint8_t *video_grab_pixels(const plat_texture *t, int *w, int *h, int *screen);
plat_texture *video_grab_restore(int w, int h, const uint8_t *rgba, int screen);

/* ResultLicense 004A7D90/004A8CB8. Resident GPU capture and cached CLUT. */
void video_license_image(const plat_texture *t, float x, float y, float w, float h,
                         const uint8_t lut[256][3], const uint8_t mix[3][3]);
/* A clipped 2D HUD inside a race viewport. NULL panel via h=0 restores. */
void video_ui_panel(float y, float h);
#endif
