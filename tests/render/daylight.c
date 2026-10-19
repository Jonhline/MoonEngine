#include "platform/platform.h"
#include "plat_sdl.h"
#include "video_gl.h"
#include "daylight.h"
#include "render3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "daylight %d: %s\n", __LINE__, #x);                                    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int frame(int enabled, float intensity, float *average)
{
    video_hdr_options(enabled, 0);
    plat_video_begin_frame();
    video_begin_scene();
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glClearColor(intensity, intensity, intensity, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    video_end_scene();
    int w, h;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    if (!p)
        return 0;
    *average = p[(h / 2 * w + w / 2) * 4];
    free(p);
    plat_video_end_frame();
    return 1;
}
static int shaft_frame(int blocked, int rays, float *average)
{
    const float sunlight[3] = {1, .9f, .7f};
    video_hdr_options(1, rays);
    if (blocked >= 0)
        video_solar_projection(.5f, .5f, 1, sunlight);
    plat_video_begin_frame();
    video_begin_scene();
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(.2f, .2f, .2f, 1);
    glClearDepth(blocked > 0 ? (gl_reversed_z ? 1 : 0) : GL_DEPTH_FAR);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    video_end_scene();
    int w, h;
    uint8_t *p = plat_sdl_read_pixels(&w, &h);
    if (!p)
        return 0;
    *average = p[(h / 2 * w + w / 2) * 4];
    free(p);
    plat_video_end_frame();
    return 1;
}
static int sky_frame(int enabled, int hour, int pixel[3])
{
    gt4_triangle t[2] = {0};
    const float points[6][3] = {{-2, 3, 2}, {2, 3, 2},  {2, 3, -2},
                                {-2, 3, 2}, {2, 3, -2}, {-2, 3, -2}};
    for (int i = 0; i < 2; i++) {
        t[i].texture = -1;
        t[i].state.depth_func = 1;
        t[i].state.depth_write = 1;
        for (int j = 0; j < 3; j++) {
            memcpy(t[i].v[j].p, points[i * 3 + j], 12);
            t[i].v[j].alpha = 1;
            for (int c = 0; c < 3; c++)
                t[i].v[j].rgb[c] = .5f;
        }
    }
    gt4_model model = {.triangles = t, .count = 2};
    r3d_mesh *mesh = r3d_mesh_create(&model);
    if (!mesh)
        return 0;
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = 1;
    view.r[0] = 1;
    view.u[2] = -1;
    view.f[1] = 1;
    view.focal = 320;
    view.focal_y = 224;
    view.unlit = 1;
    moon_daylight d = moon_daylight_at(hour);
    view.daylight = enabled ? 2 : 0;
    memcpy(view.sun_direction, d.sun, 12);
    memcpy(view.sun_color, d.direct, 12);
    memcpy(view.sky_color, d.sky, 12);
    memcpy(view.haze_color, d.haze, 12);
    video_hdr_options(0, 0);
    r3d_begin(0, 0, 640, 448, NULL);
    if (!r3d_begin_target(128))
        return 0;
    r3d_draw(mesh, &view);
    uint8_t data[128 * 128 * 4];
    r3d_end_target(data);
    for (int c = 0; c < 3; c++)
        pixel[c] = data[(64 * 128 + 64) * 4 + c];
    r3d_mesh_destroy(mesh);
    return 1;
}
static int hdr_copy(void)
{
    video_hdr_options(1, 0);
    plat_video_begin_frame();
    video_begin_scene();
    GLint target;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(.25f, .5f, 2, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 16, 16);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    uint8_t pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    printf("HDR to LDR copy=%u/%u/%u\n", pixel[0], pixel[1], pixel[2]);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    video_end_scene();
    plat_video_end_frame();
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return abs(pixel[0] - 64) <= 1 && abs(pixel[1] - 128) <= 1 && pixel[2] == 255;
}
static int edge_frame(int mode, int hdr, int *soft, int *center)
{
    video_scene_quality(mode, 2);
    video_hdr_options(hdr, 0);
    plat_video_begin_frame();
    video_begin_scene();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint program = gl_program(
        "#version 330 core\nconst vec2 p[3]=vec2[3](vec2(-.9,-.9),vec2(.9,-.9),vec2(-.9,.9));void main(){gl_Position=vec4(p[gl_VertexID],0,1);}",
        "#version 330 core\nout vec4 o;void main(){o=vec4(1);}");
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(program);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    video_end_scene();
    int w, h;
    uint8_t *pixels = plat_sdl_read_pixels(&w, &h);
    if (!pixels)
        return 0;
    int white = hdr ? 232 : 255;
    *soft = 0;
    for (int i = 0; i < w * h; i++)
        *soft += pixels[i * 4] > 5 && pixels[i * 4] < white - 5;
    *center = pixels[((h * 3 / 4) * w + w / 4) * 4];
    free(pixels);
    glDeleteProgram(program);
    glDeleteVertexArrays(1, &vao);
    plat_video_end_frame();
    return 1;
}
static int volume_frame(int rays, int ao, int occluder, uint8_t *result)
{
    video_scene_quality(1, 2);
    video_hdr_options(1, rays);
    video_ambient_occlusion(ao);
    plat_video_begin_frame();
    video_begin_scene();
    video_light_volume view = {0};
    view.valid = 1;
    view.right[0] = view.up[1] = view.forward[2] = view.sun[2] = 1;
    view.focal[0] = view.focal[1] = 1;
    view.clip[0] = gl_reversed_z ? -.001001f : 1.002002f;
    view.clip[1] = gl_reversed_z ? .1001001f : -.2002002f;
    for (int i = 0; i < 3; i++) {
        view.shadow_projection[i * 5] = .001f;
        view.shadow_projection[12 + i] = .5f;
    }
    view.shadow_projection[15] = 1;
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture);
    uint8_t texel[4] = {occluder ? 0 : 255, occluder ? 0 : 255, occluder ? 0 : 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    view.shadow = texture;
    video_light_volume_view(&view);
    const float color[3] = {1, 1, 1};
    video_solar_projection(.5f, .5f, 1, color);
    GLuint program = gl_program(
        "#version 330 core\nconst vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));out vec2 uv;void main(){gl_Position=vec4(p[gl_VertexID],0,1);uv=p[gl_VertexID]*.5+.5;}",
        "#version 330 core\nin vec2 uv;out vec4 o;uniform vec2 clip;uniform int reverse;void main(){float distance=uv.x<.5?8.6:9.;float d=clip.x+clip.y/distance;gl_FragDepth=reverse==1?d:d*.5+.5;o=vec4(.2,.2,.2,1);}");
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(program);
    glUniform2fv(glGetUniformLocation(program, "clip"), 1, view.clip);
    glUniform1i(glGetUniformLocation(program, "reverse"), gl_reversed_z != 0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    video_end_scene();
    int w, h;
    uint8_t *data = plat_sdl_read_pixels(&w, &h);
    if (!data)
        return 0;
    memcpy(result, data, 128 * 128 * 4);
    free(data);
    plat_video_end_frame();
    glDeleteProgram(program);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &texture);
    video_ambient_occlusion(0);
    return 1;
}
static int sheen_frame(int hdr, int mask, int rough, float *value)
{
    video_hdr_options(hdr,0);video_ambient_occlusion(0);video_scene_quality(1,2);
    plat_video_begin_frame();video_begin_scene();r3d_begin(0,0,640,448,NULL);
    glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(0,0,0,mask?1:0);
    glClear(GL_COLOR_BUFFER_BIT);
    if(!r3d_begin_target_view(16))return 0;
    glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(.5f,.5f,.5f,1);
    glClear(GL_COLOR_BUFFER_BIT);
    const float tint[3]={1,1,1};if (rough) {
        r3d_view road={0};road.focal=320;road.focal_y=224;
        road.f[1]=rough==1?-1.f:-.1f;
        road.u[1]=rough==1?0:sqrtf(.99f);
        r3d_end_target_add_road(tint,0,&road);
    } else r3d_end_target_add(tint,0);
    video_end_scene();
    int w,h;uint8_t *data=plat_sdl_read_pixels(&w,&h);if(!data)return 0;
    *value=data[((h/2)*w+w/2)*4];free(data);plat_video_end_frame();return 1;
}
int main(int argc, char **argv)
{
    int hdr_negative = argc > 1 && !strcmp(argv[1], "--negative");
    int rays_negative = argc > 1 && !strcmp(argv[1], "--rays-negative");
    int sky_negative = argc > 1 && !strcmp(argv[1], "--sky-negative");
    moon_daylight morning = moon_daylight_at(8), noon = moon_daylight_at(12),
                  evening = moon_daylight_at(16);
    CHECK(morning.sun[0]>0 && evening.sun[0]<0);
    CHECK(noon.sun[1]>morning.sun[1]);
    CHECK(morning.direct[0]/morning.direct[2]>noon.direct[0]/noon.direct[2]);
    CHECK(moon_daylight_at(-10).hour==7 && moon_daylight_at(100).hour==17);
    for (int hour = 7; hour <= 17; hour++) {
        moon_daylight d = moon_daylight_at(hour);
        float norm = 0;
        for (int c = 0; c < 3; c++)
            norm += d.sun[c] * d.sun[c];
        CHECK(fabsf(norm-1)<1e-5 && d.sun[1]>0);
    }
    r3d_daylight_options(1, 16, 2);
    CHECK(r3d_daylight_hour(120)==16);
    r3d_daylight_options(1, 12, 0);
    CHECK(r3d_daylight_hour(36000)==12);
    CHECK(!plat_video_init(&(plat_video_desc){128,128,0,0,0}));
    SDL_HideWindow(plat_sdl_window());
    video_scene_quality(1, 2);
    float a, b, original, restored;
    CHECK(frame(0,.25f,&original));
    CHECK(frame(hdr_negative?0:1,2,&a));
    CHECK(frame(hdr_negative?0:1,4,&b));
    printf("HDR input 2 -> %.0f; 4 -> %.0f\n", a, b);
    CHECK(b>a && a<255);
    CHECK(frame(0,.25f,&restored));
    CHECK(original==restored);
    video_hdr_options(1, 0);
    plat_video_begin_frame();
    video_begin_scene();
    CHECK(video_hdr_active());
    CHECK(r3d_begin_target(16));
    CHECK(!video_hdr_active());
    r3d_end_target(NULL);
    CHECK(video_hdr_active());
    video_end_scene();
    plat_video_end_frame();
    CHECK(hdr_copy());
    int sky[3], morning_sky[3], base_sky[3], restored_sky[3];
    CHECK(sky_frame(0,12,base_sky));
    CHECK(sky_frame(sky_negative?0:1,12,sky));
    CHECK(sky_frame(1,8,morning_sky));
    printf("procedural zenith noon=%d/%d/%d morning=%d/%d/%d\n", sky[0], sky[1], sky[2],
           morning_sky[0], morning_sky[1], morning_sky[2]);
    CHECK(sky[2]>sky[0]+15 && sky[2]>sky[1]);
    CHECK(memcmp(sky,morning_sky,sizeof sky));
    CHECK(sky_frame(0,12,restored_sky));
    CHECK(!memcmp(base_sky,restored_sky,sizeof base_sky));
    float open, blocked, disabled;
    CHECK(shaft_frame(0,rays_negative?0:1,&open));
    CHECK(shaft_frame(1,1,&blocked));
    CHECK(shaft_frame(0,0,&disabled));
    printf("shafts open=%.0f blocked=%.0f disabled=%.0f\n", open, blocked, disabled);
    CHECK(open>blocked+5 && blocked==disabled);
    float stale;
    CHECK(shaft_frame(-1,1,&stale));
    CHECK(stale==disabled);
    int baseline, filtered, center0, center1, hdr_soft, hdr_center;
    CHECK(edge_frame(1,0,&baseline,&center0));
    CHECK(edge_frame(argc>1 && !strcmp(argv[1],"--aa-negative")?1:4,0,&filtered,&center1));
    printf("edge AA intermediate pixels %d -> %d; flat interior %d/%d\n", baseline, filtered,
           center0, center1);
    CHECK(filtered>baseline+20 && center0==center1);
    CHECK(edge_frame(4,1,&hdr_soft,&hdr_center));
    CHECK(hdr_soft>20 && hdr_center>200);
    CHECK(edge_frame(1,0,&filtered,&center1));
    CHECK(filtered==baseline && center0==center1);
    uint8_t volume_lit[65536], volume_blocked[65536], ao_base[65536], ao_shaded[65536];
    int volume_negative = argc > 1 && !strcmp(argv[1], "--volume-negative");
    CHECK(volume_frame(volume_negative?0:2,0,0,volume_lit));
    CHECK(volume_frame(volume_negative?0:2,0,1,volume_blocked));
    int lit = volume_lit[(64 * 128 + 64) * 4], dark = volume_blocked[(64 * 128 + 64) * 4];
    printf("world volume lit/blocked=%d/%d\n", lit, dark);
    CHECK(lit>dark+3);
    CHECK(volume_frame(0,0,0,ao_base));
    CHECK(volume_frame(0,argc>1 && !strcmp(argv[1],"--ao-negative")?0:1,0,ao_shaded));
    int cavity = 0;
    for (int i = 0; i < 128 * 128; i++)
        cavity += ao_shaded[i * 4] + 1 < ao_base[i * 4];
    printf("AO cavity pixels=%d\n", cavity);
    CHECK(cavity>10);
    float sheen,reference,masked;
    CHECK(frame(1,powf((.5f+.055f)/1.055f,2.4f),&reference));
    CHECK(sheen_frame(argc>1 && !strcmp(argv[1],"--sheen-negative")?0:1,1,0,&sheen));
    CHECK(sheen_frame(1,0,0,&masked));
    printf("encoded sheen into HDR=%.0f expected=%.0f mask-off=%.0f\n",sheen,reference,masked);
    CHECK(fabsf(sheen-reference)<=2 && masked==0);
    float rough_normal,rough_grazing,rough_masked,rough_reference;
    CHECK(sheen_frame(1,1,argc>1 && !strcmp(argv[1],"--rough-road-negative")?0:1,&rough_normal));
    CHECK(sheen_frame(1,1,2,&rough_grazing));
    CHECK(sheen_frame(1,0,1,&rough_masked));
    CHECK(frame(1,powf((.5f+.055f)/1.055f,2.4f)*.04f,&rough_reference));
    printf("rough road normal/grazing/masked=%.0f/%.0f/%.0f reference=%.0f\n",
           rough_normal,rough_grazing,rough_masked,rough_reference);
    CHECK(fabsf(rough_normal-rough_reference)<=2);
    CHECK(rough_normal>0 && rough_grazing>rough_normal && rough_grazing<sheen*.5f);
    CHECK(rough_masked==0);
    plat_video_shutdown();
    puts(
        "PASS solar geometry, colour, daytime bounds, deterministic cycle, float headroom and Original restore");
    return 0;
}
