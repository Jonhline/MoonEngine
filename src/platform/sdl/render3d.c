#if !defined(__SWITCH__) && !defined(__ANDROID__)
static int texture_smoothing;
void r3d_texture_smoothing(int enabled)
{
    texture_smoothing = enabled != 0;
}
#endif
#include "platform/switch_profile.h"
#include "render3d.h"
#include "car_service.h"
#include "video_gl.h"
#include "fsr.h"
#ifdef MOON_PC_VULKAN
#include "vk_render.h"
#endif
#include "render3d_frustum.h"
#include "render3d_visibility.h"
#include "wheel_motion.h"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
#include "switch_quality.h"
#endif
#ifdef __SWITCH__
#include "platform/platform.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* vertex: position 3, normal 3, uv 2, colour 3, ambient 3, specular 3, shine + alpha 2 */
#define VFLOATS 19
#define SKIN_VFLOATS 27
#define UPLOAD_TRIANGLES 2048

typedef struct {
    int first, count, texture, reflection, flat, specular, wheel_blur_texture, lamp, night_lens;
    gt4_draw_state state;
    uint8_t blend_enabled;
    float lo[3], hi[3];
    unsigned source_model;
    unsigned bone_mask;
    int batch_end;
    float weight_lo, weight_hi;
} r3d_group;

typedef struct {
    GLuint id;
    int width, height, tex_w, tex_h, native, scale;
    uint8_t color_component, texture_function, wrap_u, wrap_v, nearest, mip_count, minify;
    int min_u, max_u, min_v, max_v;
    GLuint wheel_tex[2], wheel_fbo[2];
    float wheel_angle;
    int wheel_samples, wheel_size;
    int filtering_applied;
    float anisotropy_max, anisotropy_original;
} r3d_texture;

struct r3d_mesh {
    GLuint vao, vbo;
    r3d_group *groups;
    int ngroups;
    r3d_visibility visibility;
    unsigned source_models;
    int *model_ranges;
    r3d_texture *textures;
    int ntextures;
    int vertices;
    unsigned *texture_refs;
    int skinned;
    int morph, morph_color;
    float grasp_scale;
    float wheel_screen[2];
    int wheel_screen_valid;
};

static GLuint program, dest_tex, dest_fbo, env_tex;
static int dest_w, dest_h, env_w, env_h, env_dirty, failed, vp[4];
static const tex1_image *env_source;
static unsigned environment_revision;
#ifndef MOON_DEKO3D
static GLuint gpu_environment;
static int gpu_environment_w, gpu_environment_h;
#endif
static float projection_x = 1;
/* Debug switches are read once per r3d_begin: getenv scans the environment
   block and was called for every mesh and draw. Tests change some between
   frames. */
typedef struct {
    unsigned frame;
    signed char value;
} r3d_env_slot;
static unsigned r3d_env_frame = 1;
static inline int r3d_env_flag(const char *name, r3d_env_slot *slot)
{
    if (slot->frame != r3d_env_frame) {
        slot->value = getenv(name) != NULL;
        slot->frame = r3d_env_frame;
    }
    return slot->value;
}
static inline int r3d_env_one(const char *name, r3d_env_slot *slot)
{
    if (slot->frame != r3d_env_frame) {
        const char *value = getenv(name);
        slot->value = value && *value == '1';
        slot->frame = r3d_env_frame;
    }
    return slot->value;
}
static GLuint wheel_program, wheel_vao;
static struct {
    GLint points, extent, source, scanmask, copy;
} WU;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
/* Diagnostic choices are sampled at the view boundary, not for every mesh. */
static struct {
    int cache_materials, batch_opaque, spatial, texture_filter, car_shadows, material_shading, reflection_quality, shadow_map_quality, shadow_filter, modern_lighting, daylight, solar_hour, solar_cycle;
#if !defined(__ANDROID__)
    r3d_shadow_map *shadow_map;
#endif
} pc_submission;
void r3d_texture_filtering(int mode)
{
    pc_submission.texture_filter = mode >= 0 && mode <= 4 ? mode : 0;
}
static int night_sky_enabled;
void r3d_night_sky(int enabled){night_sky_enabled=enabled!=0;}
int r3d_night_sky_enabled(void){return night_sky_enabled;}
float r3d_solar_hour(double seconds,int authored_night){
    if(!night_sky_enabled)return r3d_daylight_hour(seconds);
    const double rates[]={0,1./3600,1./60};
    double start=pc_submission.solar_hour<0?(authored_night?22:12):pc_submission.solar_hour;
    return (float)fmod(start+fmax(0,seconds)*rates[pc_submission.solar_cycle],24);
}
static int car_lamps_enabled, car_lamp_count;
static r3d_car_lamp car_lamp_frame[R3D_CAR_LAMPS];
void r3d_car_lamps(int enabled){car_lamps_enabled=enabled!=0;}
int r3d_car_lamps_enabled(void){return car_lamps_enabled;}
void r3d_car_lamp_frame(const r3d_car_lamp *lamps,unsigned count){
    r3d_lamp_shadow_bind(NULL);
    car_lamp_count=lamps?(count>R3D_CAR_LAMPS?R3D_CAR_LAMPS:count):0;
    if(car_lamp_count)memcpy(car_lamp_frame,lamps,car_lamp_count*sizeof *lamps);
}
static void shadow_map_receiver(const r3d_view *view);
static void lamp_shadow_receiver(int count);
void r3d_daylight_options(int enabled, int hour, int cycle)
{
    pc_submission.daylight = enabled != 0;
    pc_submission.solar_hour = hour >= -1 && hour < 24 ? hour : 12;
    pc_submission.solar_cycle = cycle >= 0 && cycle <= 2 ? cycle : 0;
}
int r3d_daylight_enabled(void) { return pc_submission.daylight; }
float r3d_daylight_hour(double seconds)
{
    const double rates[] = {0, 1.0/3600, 1.0/60};
    double hour = (pc_submission.solar_hour<0?12:fmin(17,fmax(7,pc_submission.solar_hour))) + fmax(0,seconds)*rates[pc_submission.solar_cycle];
    /* Reflect at the daylight boundaries: no discontinuous jump into night. */
    hour = fmod(hour-7,20);
    return (float)(7+(hour<=10?hour:20-hour));
}
int r3d_modern_lighting_enabled(void) { return pc_submission.modern_lighting; }
void r3d_modern_lighting(int enabled)
{
    pc_submission.modern_lighting = enabled != 0;
}
void r3d_shadow_map_quality(int mode)
{
    pc_submission.shadow_map_quality = mode >= 0 && mode <= 3 ? mode : 0;
}
void r3d_shadow_filter(int fast)
{
    pc_submission.shadow_filter = fast != 0;
}
int r3d_shadow_map_size(void)
{
    return pc_submission.shadow_map_quality ? 256 << pc_submission.shadow_map_quality : 0;
}
void r3d_reflection_quality(int mode)
{
    pc_submission.reflection_quality = mode >= 0 && mode <= 2 ? mode : 0;
}
int r3d_reflection_size(void)
{
    return 128 << pc_submission.reflection_quality;
}
void r3d_material_shading(int enabled)
{
    pc_submission.material_shading = enabled != 0;
}
void r3d_car_shadows(int enabled)
{
    pc_submission.car_shadows = enabled != 0;
}
int r3d_car_shadows_enabled(void)
{
    return pc_submission.car_shadows;
}
static int scene_spatial = 1, scene_resident_reflections = 1;
void r3d_scene_options(int spatial, int resident_reflections)
{
    scene_spatial = spatial != 0;
    scene_resident_reflections = resident_reflections != 0;
}
int r3d_resident_reflections(void)
{
    static r3d_env_slot cpu;
    return scene_resident_reflections && !r3d_env_flag("MOON_REFLECTION_CPU", &cpu);
}
#endif
static r3d_stats frame_stats;
static unsigned measure_light_frames, measure_light_draws, measure_light_sum, measure_light_max;
static r3d_totals completed_work;
static void accumulate_work(r3d_totals *total, const r3d_stats *view)
{
    total->draws += view->draws;
    total->triangles += view->triangles;
    total->merged_groups += view->merged_groups;
    total->culled_groups += view->culled_groups;
    total->texture_updates += view->texture_updates;
    total->texture_hits += view->texture_cache_hits;
    total->state_updates += view->state_updates;
    total->state_hits += view->state_cache_hits;
    total->visibility_tests += view->visibility_tests;
    total->spatial_rejected += view->spatial_rejected;
    total->readbacks += view->readbacks;
}
r3d_totals r3d_work_totals(void)
{
    r3d_totals total = completed_work;
    accumulate_work(&total, &frame_stats);
    return total;
}
r3d_stats r3d_frame_stats(void)
{
    return frame_stats;
}
static struct {
    GLint rot, pos, cam, r, u, f, focal, jitter, clip, mode, flat, reflect, specular;
    GLint nlights, local_lights, ldir, lamb, ldif, lspe, lspec_scale, lvertex, shadow_alpha, shadow_rgb, mirror,
        cscale, untex, envk;
    GLint tex, has_tex, texsize, size, wrap, region, cc, tfx, native, nearest, mip_count, minify, filtering;
    GLint cloud_cache, cloud_volume, cloud_shadow, cloud_origin, linear_environment, environment_linear;
    GLint fade, atest, afunc, aref, pass, date, date_bit, dest, envtex, envsize, viewport, focal_px;
    GLint skinned, palette, morph, morph_color, colorop, colormul, coloradd, valpha;
    GLint fog, fogcol, grade_rgb, grade;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    GLint material_shading, skip_material_light, measure_no_env, measure_no_spec;
    GLint night_sky, authored_night;
    GLint car_lamp_count,car_lamp_position,car_lamp_direction,car_lamp_color;
    GLint lamp_lens;
    GLint lamp_shadow_mask, lamp_shadow_map, lamp_shadow_projection;
    GLint sky_seconds;
    GLint authored_environment;
    GLint shadow_on, shadow_map, shadow_projection, modern_lighting, hdr, daylight, sun_direction, sun_color, sky_color, haze_color;
#endif
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    GLint switch_fast;
#endif
} U;

static const char *vs =
    "#version 330 core\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    "uniform int u_switch_fast;\n"
#endif
    "layout(location=0) in vec3 a_pos; layout(location=1) in vec3 a_normal; layout(location=2) in vec2 a_uv;\n"
    "layout(location=3) in vec3 a_rgb; layout(location=4) in vec3 a_amb; layout(location=5) in vec3 a_spec;\n"
    "layout(location=6) in vec2 a_shine_alpha;\n"
    "layout(location=7) in vec4 a_bone; layout(location=8) in vec4 a_weight;\n"
    "layout(location=9) in vec3 a_morph_pos; layout(location=10) in vec3 a_morph_normal; layout(location=11) in vec2 a_morph_uv;layout(location=12) in vec4 a_morph_rgba;\n"
    "uniform float u_morph; uniform int u_morph_color; uniform int u_colorop; uniform vec3 u_colormul,u_coloradd; uniform float u_valpha;\n"
    "uniform int u_skinned; uniform mat4 u_palette[20];\n"
    "uniform mat3 u_rot; uniform vec3 u_pos, u_cam, u_r, u_u, u_f; uniform vec2 u_focal, u_jitter, u_clip;\n"
    "uniform int u_mode, u_flat, u_reflect, u_mirror, u_specular_only; uniform float u_shadow_alpha, u_cscale; uniform vec3 u_shadow_rgb; uniform vec2 u_envk;\n"
    "uniform int u_nlights; uniform vec3 u_ldir[4],u_lamb[4],u_ldif[4],u_lspe[4]; uniform float u_lspec_scale, u_lvertex;\n"
    "out float v_floor; out vec3 v_light; out vec3 v_spec; out vec2 v_envuv; out vec2 v_uv; out float v_alpha;\n"
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    "out vec3 v_material_normal; out vec3 v_material_ray; out vec3 v_material_rgb; out vec3 v_material_amb; out vec3 v_material_spec;\n"
    "out float v_material_shine;\n"
    "out vec3 v_shadow_world;\n"
#endif
    /* the GS interpolates F across the screen as it does the colours */
    "uniform vec3 u_fog; noperspective out float v_fog;\n"
    "const vec3 L[3]=vec3[3](vec3(.57735,.57735,.57735),vec3(0.,.9759,-.2182),vec3(-1.,0.,0.));\n"
    "const vec3 D=vec3(1.,60./255.,216./255.), S=vec3(239./255.,108./255.,198./255.);\n"
    "void main(){\n"
    "  vec3 p=a_pos, normal=a_normal, rgb=a_rgb; float alpha=a_shine_alpha.y; vec2 uv=a_uv;\n"
    "  if(u_morph>0.){p=mix(p,a_morph_pos,u_morph);normal=mix(normal,a_morph_normal,u_morph);uv=mix(uv,a_morph_uv,u_morph);if(u_morph_color==1){rgb=mix(rgb,a_morph_rgba.rgb,u_morph);alpha=mix(alpha,a_morph_rgba.a,u_morph);}}\n"
    "  if(u_skinned==1 && dot(a_weight,vec4(1.))>0.){\n"
    "    mat4 bone=u_palette[int(a_bone.x)]*a_weight.x+u_palette[int(a_bone.y)]*a_weight.y+\n"
    "              u_palette[int(a_bone.z)]*a_weight.z+u_palette[int(a_bone.w)]*a_weight.w;\n"
    "    p=(bone*vec4(p,1.)).xyz; normal=mat3(bone)*normal; }\n"
    "  vec3 w=u_rot*p+u_pos; v_floor=w.y; if(u_mirror==1) w.y=-w.y; w-=u_cam; float x=dot(w,u_r), y=dot(w,u_u), z=dot(w,u_f);\n"
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    "  v_shadow_world=u_rot*p+u_pos;\n"
    "  v_material_normal=u_rot*normal; if(u_mirror==1)v_material_normal.y=-v_material_normal.y;\n"
    "  v_material_ray=w;v_material_rgb=a_rgb;v_material_amb=a_amb;v_material_spec=a_spec;v_material_shine=a_shine_alpha.x;\n"
#endif
    /* u_clip: the z coefficients of the depth for the near/far planes (depth_coeffs) */
    "  gl_Position=vec4(x*u_focal.x, y*u_focal.y, z*u_clip.x+u_clip.y, z);\n"
    "  gl_Position.xy+=u_jitter*z;\n"
    /* VU1 (the vertex loops at 0x3D8..): w limited to the fog distance
       (MINI with vf24.w), times 1/w, times 255 - max*255 (vf25.w) plus
       max*255 + 0.1 (vf26.w); FTOI4 keeps the whole part */
    "  { float fz=max(z,1e-6); v_fog=u_fog.x<0.?1.:floor(min(255.,u_fog.y+u_fog.x*min(fz,u_fog.z)/fz))/255.; }\n"
    "  v_uv=uv; v_alpha=u_mode==2?min(alpha*u_shadow_alpha,1.):alpha*u_valpha;\n"
    /* the course (unlit): a kind 4 layer has no colour of its own (0049EB90
       loads no ambient/diffuse block), only the lights' specular. Which
       lights the course gives it is not read: none here, so it adds
       nothing (its vertex colour, 1.0 doubled, made the grid's Cs * Ad + Cd
       tyre marks white at Tsukuba, user's PCSX2 report) */
    "  if(u_mode!=0){ v_light=u_mode==1?(u_specular_only==1?vec3(0.):rgb*u_cscale):a_rgb+u_shadow_rgb; if(u_colorop==1) v_light=v_light*u_colormul+u_coloradd; v_spec=vec3(0.); v_envuv=vec2(.5); return; }\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    /* P: flat non-reflective vertices do not consume their normal. */
    "  vec3 wn=vec3(0.,1.,0.); if(u_switch_fast==0 || u_flat==0 || u_reflect==1) wn=normalize(u_rot*normal);\n"
    "  vec3 vdir=-u_f; if(u_mirror==1) wn.y=-wn.y; float dsum=0., ssum=0.;\n"
    /* Explicit scene lights overwrite both default-light terms below.
       Flat materials overwrite them too (0049EB90 material context). */
    "  if(u_switch_fast==0 || (u_nlights<=0 && u_flat==0)){\n"
#else
    "  vec3 wn=normalize(u_rot*normal), vdir=-u_f; if(u_mirror==1) wn.y=-wn.y; float dsum=0., ssum=0.;\n"
#endif
    "  for(int l=0;l<3;l++){ float d=dot(wn,L[l]); dsum+=D[l]*max(0.,d); vec3 h=L[l]+vdir; float hl=length(h);\n"
    "    if(d>0. && hl>1e-6) ssum+=S[l]*pow(max(0.,dot(wn,h)/hl),a_shine_alpha.x); }\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    "  }\n"
    /* VU1 0x215 sphere coordinates are consumed only by reflection layers. */
    "  v_envuv=vec2(.5); if(u_switch_fast==0 || u_reflect==1)\n"
#endif
    /* the reflection's place in the environment image, per vertex as
       VU1 0x215 makes its STQ (the GS interpolates it): the view ray
       mirrored on the normal, as a sphere map */
    "  { vec3 nn=vec3(dot(wn,u_r),dot(wn,u_u),-dot(wn,u_f)); vec3 i=normalize(vec3(x,y,-max(z,1e-6)));\n"
    "    vec3 r=i-2.*dot(i,nn)*nn; r.z+=1.; float den=2.*length(r);\n"
    "    v_envuv=den>1e-8?vec2(.5+r.x/den,.5-r.y/den):vec2(.5); }\n"
    "  v_light=u_reflect==1?a_rgb:50./255.*a_amb+a_rgb*dsum;\n"
    "  if(u_flat==1) v_light=a_rgb;\n"
    /* Original: VU1 0x215 uses the authored environment colour, not the
       surface-light blocks loaded by func_0049E918 +0xA0..0xBC. */
    "  v_spec=u_flat==1 || u_reflect==1?vec3(0.):a_spec*ssum*0.18;\n"
    "  if(u_nlights>0 && u_flat==0 && u_reflect==0){ vec3 amb=vec3(0.),dif=vec3(0.),spe=vec3(0.);\n"
    "    for(int l=0;l<4;l++){ if(l>=u_nlights) break; amb+=u_lamb[l]; if(length(u_ldir[l])<1e-6) continue;\n"
    "      vec3 ld=normalize(u_ldir[l]); float d=max(0.,dot(wn,ld)); vec3 h=ld+vdir;\n"
    "      float s=d>0. && length(h)>1e-6?pow(max(0.,dot(wn,normalize(h))),a_shine_alpha.x):0.;\n"
    /* VU1 0x23 / 0x52 / 0x83 (picked at 0049EA18 by the material's shine
       below 6, below 12, or more): N.H to the 4th, 8th or 16th power */
    "      if(u_lvertex>0.){ float e=length(h)>1e-6?max(0.,dot(wn,normalize(h))):0.; e*=e; e*=e;\n"
    "        if(a_shine_alpha.x>=6.) e*=e; if(a_shine_alpha.x>=12.) e*=e; s=e; }\n"
    "      dif+=u_ldif[l]*d; spe+=u_lspe[l]*s; }\n"
    "    v_light=a_amb*amb+a_rgb*dif; v_spec=a_spec*spe*u_lspec_scale;\n"
    /* the same programs: the three terms summed into the vertex colour,
       limited to its 255; the texel multiplies all of it */
    /* Original: func_0049EB90 +0x84..0x94 loads the specular block alone.
       Select that contribution before vertex saturation discards v_spec. */
    "    if(u_lvertex>0.){ vec3 lit=a_spec*spe; if(u_specular_only==0) lit+=a_amb*amb+a_rgb*dif;\n"
    "      v_light=min(max(lit,vec3(0.)),vec3(u_lvertex)); v_spec=vec3(0.); }\n"
    "  }\n"
    /* 0049D1A0 -> 0049EB90: kind 4 loads light directions and material
       specular colours (3A0/3B0), without the ambient/diffuse colour block.
       These authored additive masks must not light the diffuse surface again. */
    "  if(u_specular_only==1 && u_lvertex<=0.){ v_light=v_spec; v_spec=vec3(0.); }\n"
    /* VU1 0x215 (the reflection layer): no lights, the vertex colour times
       205 + 206 * min(I.N, 0) / |I| */
    "  if(u_reflect==1 && u_envk!=vec2(0.)){ v_light=a_rgb*(u_envk.x+u_envk.y*min(dot(w,wn),0.)/max(length(w),1e-6)); v_spec=vec3(0.); }\n"
    "}\n";

static const char *fs =
    "#version 330 core\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    "uniform int u_switch_fast;\n"
#endif
    "in float v_floor; uniform int u_mirror; in vec3 v_light; in vec3 v_spec; in vec2 v_envuv; in vec2 v_uv; in float v_alpha; out vec4 o;\n"
    "noperspective in float v_fog; uniform vec3 u_fogcol, u_grade_rgb, u_grade;\n"
    "uniform sampler2D u_tex, u_dest, u_envtex; uniform int u_has_tex, u_cc, u_tfx, u_native, u_nearest, u_mip_count, u_minify;\n"
    "uniform float u_authored_night;\n"
    "uniform vec2 u_texsize; uniform ivec2 u_size, u_wrap, u_envsize; uniform ivec4 u_region;\n"
    "uniform int u_atest, u_afunc, u_pass, u_date, u_date_bit, u_reflect; uniform float u_aref, u_untex, u_fade; uniform vec2 u_focal_px;\n"
    "uniform vec4 u_viewport;\n"
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
#include "render3d_material_shader.inc"
#include "render3d_shadow_shader.inc"
#include "render3d_modern_shader.inc"
#include "render3d_atmosphere_shader.inc"
#include "render3d_daylight_shader.inc"
#include "render3d_car_lamps_shader.inc"
#endif
    "int texel(int at,int size,int mode,int mn,int mx){\n"
    "  if(mode==0) return at-size*int(floor(float(at)/float(size)));\n"
    "  if(mode==3) at=(at&mn)|mx; else if(mode==2) at=clamp(at,mn,mx);\n"
    "  return clamp(at,0,size-1); }\n"
    "uniform int u_filtering;\n"
    "vec4 fetch(int x,int y){ return texelFetch(u_tex,ivec2(x,y),0); }\n"
    /* A: host derivatives select the footprint; GS Q/L/K selection is pending.
       Address each tap at its selected authored level, including REGION_CLAMP. */
    "vec4 mipSample(int level){ ivec2 sz=max(u_size>>level,ivec2(1)); ivec4 reg=u_region>>level;\n"
    "  vec2 p=v_uv*u_texsize/exp2(float(level))-.5; ivec2 q=ivec2(floor(p)); vec2 f=fract(p);\n"
    "  int x=texel(q.x,sz.x,u_wrap.x,reg.x,reg.y), y=texel(q.y,sz.y,u_wrap.y,reg.z,reg.w);\n"
    "  int nx=texel(q.x+1,sz.x,u_wrap.x,reg.x,reg.y), ny=texel(q.y+1,sz.y,u_wrap.y,reg.z,reg.w);\n"
    "  if(u_minify<4){q=ivec2(floor(p+.5));return texelFetch(u_tex,ivec2(texel(q.x,sz.x,u_wrap.x,reg.x,reg.y),texel(q.y,sz.y,u_wrap.y,reg.z,reg.w)),level);}\n"
    "  return mix(mix(texelFetch(u_tex,ivec2(x,y),level),texelFetch(u_tex,ivec2(nx,y),level),f.x),\n"
    "             mix(texelFetch(u_tex,ivec2(x,ny),level),texelFetch(u_tex,ivec2(nx,ny),level),f.x),f.y); }\n"
    "void main(){\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    /* No derivative/logarithm is needed without authored mip selection. */
    "  float lod=0.; if(u_switch_fast==0 || (u_has_tex==1 && u_mip_count>0 && u_minify>=2)){\n"
    "    vec2 dx=dFdx(v_uv*u_texsize),dy=dFdy(v_uv*u_texsize); lod=log2(max(max(length(dx),length(dy)),1e-8)); }\n"
#else
    "  vec2 dx=dFdx(v_uv*u_texsize),dy=dFdy(v_uv*u_texsize); float lod=log2(max(max(length(dx),length(dy)),1e-8));\n"
#endif
    "  if(u_mirror==1 && v_floor<0.) discard; vec4 t=vec4(1.); float ta=128.;\n"
    "  if(u_has_tex==1){\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    /* Base-only simple wrapping can use the hardware's bilinear sample.
       Native mip, nearest and region-addressed cases retain the reference. */
    "    if(u_switch_fast==1 && !(u_mip_count>0 && u_minify>=2) && u_nearest==0 && u_wrap.x<2 && u_wrap.y<2)\n"
    "      t=textureLod(u_tex,v_uv*u_texsize/vec2(u_size),0.); else {\n"
#endif
    "    float fx=v_uv.x*u_texsize.x-.5, fy=v_uv.y*u_texsize.y-.5; int tx=int(floor(fx)), ty=int(floor(fy));\n"
    "    fx-=floor(fx); fy-=floor(fy);\n"
    "    int nx=texel(tx+1,u_size.x,u_wrap.x,u_region.x,u_region.y), ny=texel(ty+1,u_size.y,u_wrap.y,u_region.z,u_region.w);\n"
    "    tx=texel(tx,u_size.x,u_wrap.x,u_region.x,u_region.y); ty=texel(ty,u_size.y,u_wrap.y,u_region.z,u_region.w);\n"
    /* TEX1 MMAG/MMIN 0: the texel the point falls in (in an upscaled
       texture, the middle of that texel's block) */
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    /* P: native taps retain region addressing; ordinary repeat/clamp surfaces
       opt into hardware derivatives, authored/generated mips and anisotropy. */
    "    if(u_filtering>0 && u_wrap.x<2 && u_wrap.y<2){ vec2 sz=vec2(u_size);\n"
    "      t=textureGrad(u_tex,v_uv*u_texsize/sz,dx/sz,dy/sz); } else\n"
#endif
    "    if(u_mip_count>0 && u_minify>=2 && lod>0.){ float l=clamp(lod,0.,float(u_mip_count));\n"
    "      if(u_minify==2 || u_minify==4)t=mipSample(int(floor(l+.5)));\n"
    "      else {int low=int(floor(l));t=mix(mipSample(low),mipSample(min(low+1,u_mip_count)),fract(l));} }\n"
    "    else if(u_nearest>0){ ivec2 c=ivec2(floor(v_uv*u_texsize/float(u_nearest)))*u_nearest+u_nearest/2;\n"
    "      t=fetch(texel(c.x,u_size.x,u_wrap.x,u_region.x,u_region.y),texel(c.y,u_size.y,u_wrap.y,u_region.z,u_region.w)); }\n"
    "    else if(u_wrap.x<2 && u_wrap.y<2) t=texture(u_tex,v_uv*u_texsize/vec2(u_size));\n"
    "    else t=mix(mix(fetch(tx,ty),fetch(nx,ty),fx),mix(fetch(tx,ny),fetch(nx,ny),fx),fy);\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    "    }\n"
#endif
    "    if(u_native==1) ta=t.a*255.;\n"
    "  }\n"
    "  float na=v_alpha;\n"
    "  if(u_has_tex==1 && u_cc==1){ na=ta/128.; if(u_tfx==0) na*=v_alpha; else if(u_tfx==2) na+=v_alpha; }\n"
    "  na=floor(clamp(na*128.,0.,255.)+.5)/128.;\n"
    "  if(u_atest==1){ float a=na*128.; bool p=false;\n"
    "    if(u_afunc==1)p=true; else if(u_afunc==2)p=a<u_aref; else if(u_afunc==3)p=a<=u_aref; else if(u_afunc==4)p=a==u_aref;\n"
    "    else if(u_afunc==5)p=a>=u_aref; else if(u_afunc==6)p=a>u_aref; else if(u_afunc==7)p=a!=u_aref;\n"
    "    if(u_pass==2){ if(p) discard; } else if(!p) discard; }\n"
    "  if(u_date==1){ bool set=texelFetch(u_dest,ivec2(gl_FragCoord.xy),0).a>=.999; if(set!=(u_date_bit==1)) discard; }\n"
    "  vec3 light=v_light,spec=v_spec;vec2 envuv=v_envuv;\n"
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    "  materialSample(light,spec,envuv);\n"
#endif
    "  vec3 src;vec3 reflectedEnvironment=vec3(0.);\n"
    "  if(u_reflect==1){\n"
    "    vec3 e=vec3(0.);\n"
    "    if(u_envsize.x>0){\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    "      if(u_switch_fast==1){ vec2 halftexel=.5/vec2(u_envsize);\n"
    "        e=textureLod(u_envtex,clamp(envuv,halftexel,vec2(1.)-halftexel),0.).rgb; } else {\n"
#endif
    "      float eu=envuv.x, ev=envuv.y;\n"
    "      float ex=clamp(eu*float(u_envsize.x)-.5,0.,float(u_envsize.x-1)), ey=clamp(ev*float(u_envsize.y)-.5,0.,float(u_envsize.y-1));\n"
    "      int x0=int(ex), y0=int(ey), x1=min(x0+1,u_envsize.x-1), y1=min(y0+1,u_envsize.y-1); float fx=ex-float(x0), fy=ey-float(y0);\n"
    "      e=mix(mix(texelFetch(u_envtex,ivec2(x0,y0),0).rgb,texelFetch(u_envtex,ivec2(x1,y0),0).rgb,fx),\n"
    "            mix(texelFetch(u_envtex,ivec2(x0,y1),0).rgb,texelFetch(u_envtex,ivec2(x1,y1),0).rgb,fx),fy);\n"
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    "      }\n"
#endif
    "    }\n"
    "    reflectedEnvironment=e;src=e*light;\n"
    "  } else src=min(vec3(1.),t.rgb*light*(u_has_tex==1?1.:u_untex)+spec);\n"
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    "  vec3 retainedNight=src;bool linearOutput=false;\n"
    "  vec2 occlusion=u_shadow_on>0?shadowVisibility():vec2(1.,0.);float visibility=occlusion.x;\n"
    "  if(u_modern_lighting==1 && u_mode==0 && u_flat==0 && u_nlights>0){src=modernShade(t.rgb,visibility,occlusion.y,src,reflectedEnvironment);linearOutput=u_hdr==1;}else if(u_daylight==0)src*=visibility;\n"
#endif
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    "  if(u_daylight>0 && u_reflect==1)src*=1.-occlusion.y*.65;\n"
    "  if(u_daylight>0){src=daylightShade(src,t.rgb,visibility,occlusion.y);linearOutput=u_hdr==1;}\n"
    "  if(u_hdr==1 && !linearOutput)src=modernLinear(max(src,vec3(0.)));\n"
#endif
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    "  if(u_daylight==1 && u_authored_night>0.)src=mix(src,u_hdr==1?(u_authored_environment==1 && u_mode==1?authoredSceneLinear(modernLinear(max(retainedNight,vec3(0.)))):modernLinear(max(retainedNight,vec3(0.)))):retainedNight,clamp(u_authored_night,0.,1.));\n"
    "  if(u_car_lamp_count>0 && u_daylight<2 && u_mode!=2 && u_reflect==0)src=carLampShade(src,t.rgb);\n"
    "  src=mix(u_hdr==1?modernLinear(u_fogcol):u_fogcol,src,v_fog);\n"
#else
    "  src=mix(u_fogcol,src,v_fog);\n"
#endif
    "  if(u_grade.z>0.){ float l=dot(src,vec3(.299,.587,.114));\n"
    "    src=mix(src,clamp(mix(vec3(l),src,u_grade.x)*u_grade_rgb+u_grade.y,0.,1.),u_grade.z); }\n"
    "  o=vec4(src,clamp(na,0.,1.)*u_fade);\n"
    "}\n";

int r3d_available(void)
{
    if (program)
        return 1;
    if (failed)
        return 0;
#ifdef __SWITCH__
    uint64_t compile_started = plat_time_ns();
#endif
    program = gl_program(vs, fs);
#ifdef __SWITCH__
    fprintf(stderr, "Switch shader: model compile/link %.2f ms, success=%d\n",
            (plat_time_ns() - compile_started) / 1e6, program != 0);
    fprintf(
        stderr,
        "Switch renderer: adapted-gl state_cache=%d shader_fast=%d draw_state_cache=1 generated_mips=0 anisotropy=1\n",
        getenv("MOON_SWITCH_UNCACHED") == NULL, getenv("MOON_SWITCH_SHADER_REFERENCE") == NULL);
#endif
    if (!program) {
        failed = 1;
        return 0;
    }
#define UNI(field, name) U.field = glGetUniformLocation(program, name)
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    UNI(switch_fast, "u_switch_fast");
#endif
    UNI(rot, "u_rot");
    UNI(pos, "u_pos");
    UNI(cam, "u_cam");
    UNI(r, "u_r");
    UNI(u, "u_u");
    UNI(f, "u_f");
    UNI(focal, "u_focal");
    UNI(jitter, "u_jitter");
    UNI(clip, "u_clip");
    UNI(mode, "u_mode");
    UNI(flat, "u_flat");
    UNI(reflect, "u_reflect");
    UNI(nlights, "u_nlights");
    UNI(local_lights, "u_local_lights");
    UNI(ldir, "u_ldir");
    UNI(lamb, "u_lamb");
    UNI(ldif, "u_ldif");
    UNI(lspe, "u_lspe");
    UNI(lspec_scale, "u_lspec_scale");
    UNI(lvertex, "u_lvertex");
    UNI(shadow_alpha, "u_shadow_alpha");
    UNI(shadow_rgb, "u_shadow_rgb");
    UNI(cscale, "u_cscale");
    UNI(untex, "u_untex");
    UNI(mirror, "u_mirror");
    UNI(envk, "u_envk");
    UNI(specular, "u_specular_only");
    UNI(tex, "u_tex");
    UNI(has_tex, "u_has_tex");
    UNI(texsize, "u_texsize");
    UNI(size, "u_size");
    UNI(wrap, "u_wrap");
    UNI(region, "u_region");
    UNI(cc, "u_cc");
    UNI(tfx, "u_tfx");
    UNI(native, "u_native");
    UNI(nearest, "u_nearest");
    UNI(mip_count, "u_mip_count");
    UNI(filtering, "u_filtering");
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    UNI(material_shading, "u_material_shading");
    UNI(skip_material_light, "u_skip_material_light");
    UNI(measure_no_env, "u_measure_no_env");
    UNI(measure_no_spec, "u_measure_no_spec");
    UNI(hdr,"u_hdr");
    UNI(daylight, "u_daylight");
    UNI(authored_environment,"u_authored_environment");
    UNI(sky_seconds, "u_sky_seconds");
    UNI(cloud_cache,"u_cloud_cache");UNI(cloud_volume,"u_cloud_volume");
    UNI(linear_environment,"u_linear_environment");UNI(environment_linear,"u_environment_linear");
    UNI(cloud_shadow,"u_cloud_shadow");UNI(cloud_origin,"u_cloud_origin");
    UNI(sun_direction, "u_sun_direction");
    UNI(sun_color, "u_sun_color");
    UNI(sky_color, "u_sky_color");
    UNI(night_sky,"u_night_sky");
    UNI(authored_night, "u_authored_night");
    UNI(car_lamp_count,"u_car_lamp_count");UNI(car_lamp_position,"u_car_lamp_position");
    UNI(car_lamp_direction,"u_car_lamp_direction");UNI(car_lamp_color,"u_car_lamp_color");
    UNI(lamp_lens, "u_lamp_lens");
    UNI(lamp_shadow_mask, "u_lamp_shadow_mask");
    UNI(lamp_shadow_map, "u_lamp_shadow_map");
    UNI(lamp_shadow_projection, "u_lamp_shadow_projection");
    UNI(haze_color, "u_haze_color");
    UNI(modern_lighting, "u_modern_lighting");
    UNI(shadow_on, "u_shadow_on");
    UNI(shadow_map, "u_shadow_map");
    UNI(shadow_projection, "u_shadow_projection");
#endif
    UNI(minify, "u_minify");
    UNI(fade, "u_fade");
    UNI(atest, "u_atest");
    UNI(afunc, "u_afunc");
    UNI(aref, "u_aref");
    UNI(pass, "u_pass");
    UNI(date, "u_date");
    UNI(date_bit, "u_date_bit");
    UNI(dest, "u_dest");
    UNI(envtex, "u_envtex");
    UNI(envsize, "u_envsize");
    UNI(viewport, "u_viewport");
    UNI(focal_px, "u_focal_px");
    UNI(skinned, "u_skinned");
    UNI(palette, "u_palette");
    UNI(morph, "u_morph");
    UNI(morph_color, "u_morph_color");
    UNI(colorop, "u_colorop");
    UNI(colormul, "u_colormul");
    UNI(valpha, "u_valpha");
    UNI(coloradd, "u_coloradd");
    UNI(fog, "u_fog");
    UNI(fogcol, "u_fogcol");
    UNI(grade_rgb, "u_grade_rgb");
    UNI(grade, "u_grade");
#undef UNI
    return 1;
}

static GLuint upload(int w, int h, const uint8_t *rgba, const uint8_t *native)
{
    uint8_t *px = malloc((size_t)w * h * 4);
    if (!px)
        return 0;
    for (int i = 0; i < w * h; i++) {
        memcpy(px + i * 4, rgba + i * 4, 3);
        px[i * 4 + 3] = native ? native[i] : rgba[i * 4 + 3];
    }
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    free(px);
    return id;
}

static int same_group(const r3d_group *g, const gt4_triangle *t)
{
    return g->source_model == t->source_model && g->texture == t->texture &&
           g->wheel_blur_texture == t->wheel_blur_texture && g->blend_enabled == t->blend_enabled &&
           g->flat == (t->kind == 0 || t->kind == 5) && g->specular == (t->kind == 4) &&
           g->lamp == t->lamp && g->night_lens == t->night_lens &&
           !memcmp(&g->state, &t->state, sizeof g->state);
}

/* P: preserve 00455708 draw state while omitting redundant GL applications.
   Textures have a separate cache; no state survives a mesh submission or a
   second alpha/ghost pass. */
static int same_draw_state(const r3d_group *a, const r3d_group *b)
{
    return a->blend_enabled == b->blend_enabled && a->reflection == b->reflection &&
           a->flat == b->flat && a->specular == b->specular && a->lamp == b->lamp &&
           a->night_lens == b->night_lens && !memcmp(&a->state, &b->state, sizeof a->state);
}

static int same_opaque_group(const r3d_group *a, const r3d_group *b)
{
    return !b->blend_enabled && a->source_model == b->source_model && a->texture == b->texture &&
           a->reflection == b->reflection && a->flat == b->flat && a->specular == b->specular &&
           a->wheel_blur_texture == b->wheel_blur_texture && a->lamp == b->lamp &&
           a->night_lens == b->night_lens && !memcmp(&a->state, &b->state, sizeof a->state);
}

r3d_mesh *r3d_mesh_create_shared(const gt4_model *m, const r3d_mesh *shared)
{
    if (!m || !r3d_available())
        return NULL;
#ifdef __SWITCH__
    uint64_t upload_started = plat_time_ns();
#endif
    if (shared && shared->ntextures != m->ntextures)
        return NULL;
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    /* A child may reference a texture its parent geometry did not use.
       Upload that child independently rather than sampling an absent handle. */
    for (int i = 0; shared && i < m->count; i++) {
        int indices[2] = {m->triangles[i].texture, m->triangles[i].wheel_blur_texture - 1};
        for (int k = 0; k < 2; k++)
            if (shared && indices[k] >= 0 && indices[k] < m->ntextures &&
                !shared->textures[indices[k]].id)
                shared = NULL;
    }
#endif
    r3d_mesh *mesh = calloc(1, sizeof *mesh);
    if (!mesh)
        return NULL;
    int group_capacity = 128;
    mesh->skinned = m->skin != NULL;
    int stride = mesh->skinned ? SKIN_VFLOATS : VFLOATS;
    mesh->morph = m->morph != NULL;
    mesh->morph_color=m->morph_color;
    mesh->grasp_scale = m->grasp_scale;
    if (mesh->morph) {
        if (mesh->skinned || !isfinite(mesh->grasp_scale) || mesh->grasp_scale <= 0 ||
            mesh->grasp_scale > 1) {
            free(mesh);
            return NULL;
        }
        stride += 12;
    }
    /* Stream into the final VBO with a bounded scratch buffer. A million
       course triangles otherwise need another 228 MB temporary CPU copy. */
    float *v = malloc((size_t)UPLOAD_TRIANGLES * 3 * stride * sizeof(float));
    mesh->groups = calloc(group_capacity, sizeof *mesh->groups);
    if (shared) {
        mesh->textures = shared->textures;
        mesh->texture_refs = shared->texture_refs;
        (*mesh->texture_refs)++;
    } else {
        mesh->textures = calloc((size_t)(m->ntextures ? m->ntextures : 1), sizeof *mesh->textures);
        mesh->texture_refs = malloc(sizeof *mesh->texture_refs);
        if (mesh->texture_refs)
            *mesh->texture_refs = 1;
    }
    mesh->ntextures = m->ntextures;
    if (!v || !mesh->groups || !mesh->textures || !mesh->texture_refs) {
        free(v);
        r3d_mesh_destroy(mesh);
        return NULL;
    }
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)m->count * 3 * stride * sizeof(float)), NULL,
                 GL_STATIC_DRAW);
    for (int i = 0; i < m->count; i++) {
        const gt4_triangle *t = &m->triangles[i];
        float e1[3], e2[3], face[3];
        for (int k = 0; k < 3; k++) {
            e1[k] = t->v[1].p[k] - t->v[0].p[k];
            e2[k] = t->v[2].p[k] - t->v[0].p[k];
        }
        face[0] = e1[1] * e2[2] - e1[2] * e2[1];
        face[1] = e1[2] * e2[0] - e1[0] * e2[2];
        face[2] = e1[0] * e2[1] - e1[1] * e2[0];
        for (int j = 0; j < 3; j++) {
            const gt4_vertex *x = &t->v[j];
            float *o = v + ((size_t)(i % UPLOAD_TRIANGLES) * 3 + j) * stride;
            const float *n = x->normal;
            float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (nl < 1e-6f)
                n = face; /* as the software renderer: face normal */
            memcpy(o, x->p, 12);
            memcpy(o + 3, n, 12);
            memcpy(o + 6, x->uv, 8);
            memcpy(o + 8, x->rgb, 12);
            memcpy(o + 11, x->ambient, 12);
            memcpy(o + 14, x->specular, 12);
            o[17] = x->shine;
            o[18] = x->alpha;
            if (mesh->skinned)
                for (int k = 0; k < 4; k++) {
                    const gt4_skin_vertex *s = &m->skin[i].v[j];
                    if (s->bone[k] >= 20 || !isfinite(s->weight[k]) || s->weight[k] < 0) {
                        free(v);
                        r3d_mesh_destroy(mesh);
                        return NULL;
                    }
                    o[19 + k] = s->bone[k];
                    o[23 + k] = s->weight[k];
                }
            if (mesh->morph) {
                const gt4_morph_vertex *x = &m->morph[i].v[j];
                memcpy(o + 19, x->p, 12);
                memcpy(o + 22, x->normal, 12);
                memcpy(o + 25, x->uv, 8);
                memcpy(o+27,x->rgb,12);o[30]=x->alpha;
            }
        }
        r3d_group *g = mesh->ngroups ? &mesh->groups[mesh->ngroups - 1] : NULL;
        /* Bound long batches so off-screen course sections can be rejected.
           Keep authored order: masks/blending cannot be sorted by material. */
        if (g && g->count < 384 && same_group(g, t))
            g->count += 3;
        else {
            if (mesh->ngroups == group_capacity) {
                group_capacity *= 2;
                void *q = realloc(mesh->groups, (size_t)group_capacity * sizeof *mesh->groups);
                if (!q) {
                    free(v);
                    r3d_mesh_destroy(mesh);
                    return NULL;
                }
                mesh->groups = q;
            }
            g = &mesh->groups[mesh->ngroups++];
            memset(g, 0, sizeof *g);
            g->source_model = t->source_model;
            g->first = i * 3;
            g->count = 3;
            g->texture = t->texture;
            g->state = t->state;
            g->wheel_blur_texture = t->wheel_blur_texture;
            g->blend_enabled = t->blend_enabled;
            g->reflection = t->texture == -2;
            g->flat = t->kind == 0 || t->kind == 5;
            g->specular = t->kind == 4;
            g->lamp = t->lamp;
            g->night_lens = t->night_lens;
            for (int k = 0; k < 3; k++) {
                g->lo[k] = 1e30f;
                g->hi[k] = -1e30f;
            }
            g->weight_lo = 1e30f;
            g->weight_hi = 0;
        }
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++) {
                g->lo[k] = fminf(g->lo[k], t->v[j].p[k]);
                g->hi[k] = fmaxf(g->hi[k], t->v[j].p[k]);
                if (mesh->morph) {
                    float x = m->morph[i].v[j].p[k];
                    g->lo[k] = fminf(g->lo[k], x);
                    g->hi[k] = fmaxf(g->hi[k], x);
                }
            }
        if (mesh->skinned)
            for (int j = 0; j < 3; j++) {
                const gt4_skin_vertex *s = &m->skin[i].v[j];
                float sum = 0;
                for (int k = 0; k < 4; k++) {
                    sum += s->weight[k];
                    if (s->weight[k] > 0)
                        g->bone_mask |= 1u << s->bone[k];
                }
                if (sum == 0)
                    g->bone_mask |= 0x80000000u;
                else {
                    if (!isfinite(sum)) {
                        free(v);
                        r3d_mesh_destroy(mesh);
                        return NULL;
                    }
                    g->weight_lo = fminf(g->weight_lo, sum);
                    g->weight_hi = fmaxf(g->weight_hi, sum);
                }
            }
        if (i % UPLOAD_TRIANGLES == UPLOAD_TRIANGLES - 1 || i + 1 == m->count) {
            size_t first = (size_t)(i / UPLOAD_TRIANGLES) * UPLOAD_TRIANGLES,
                   count = i % UPLOAD_TRIANGLES + 1;
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(first * 3 * stride * sizeof(float)),
                            (GLsizeiptr)(count * 3 * stride * sizeof(float)), v);
        }
    }
    const int sizes[9] = {3, 3, 2, 3, 3, 3, 2, 4, 4};
    for (int a = 0, at = 0; a < (mesh->skinned ? 9 : 7); at += sizes[a++]) {
        glVertexAttribPointer(a, sizes[a], GL_FLOAT, GL_FALSE, stride * sizeof(float),
                              (void *)(size_t)(at * sizeof(float)));
        glEnableVertexAttribArray(a);
    }
    if (mesh->morph)
        for (int a = 9, at = 19; a < 13; a++) {
            int size = a == 12 ? 4 : a == 11 ? 2 : 3;
            glVertexAttribPointer(a, size, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                                  (void *)(size_t)(at * sizeof(float)));
            glEnableVertexAttribArray(a);
            at += size;
        }
    free(v);
    for (int i = 0; !shared && i < m->ntextures; i++) {
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
        int used = 0;
        for (int g = 0; g < mesh->ngroups && !used; g++)
            used = mesh->groups[g].texture == i || mesh->groups[g].wheel_blur_texture == i + 1;
        if (!used)
            continue;
#endif
        const tex1_image *im = &m->textures[i];
        r3d_texture *t = &mesh->textures[i];
        t->scale = 1;
        if (!t->id)
            t->id = upload(im->width, im->height, im->rgba, im->native_alpha);
        t->width = im->width;
        t->height = im->height;
        t->tex_w = im->tex_w;
        t->tex_h = im->tex_h;
        t->native = im->native_alpha != NULL;
        t->color_component = im->color_component;
        t->mip_count = im->mip_count;
        t->minify = im->minify;
        if (im->mip_count && t->id) {
            /* Region-cropped UI storage is retained on the CPU. GPU mip
               dimensions use the original TEX0.TW/TH, as MIPTBP requires. */
            uint8_t *base = calloc((size_t)im->tex_w * im->tex_h, 4);
            if (!base) {
                r3d_mesh_destroy(mesh);
                return NULL;
            }
            for (int y = 0; y < im->height; y++)
                for (int x = 0; x < im->width; x++) {
                    uint8_t *dst = base + ((size_t)y * im->tex_w + x) * 4;
                    memcpy(dst, im->rgba + ((size_t)y * im->width + x) * 4, 3);
                    dst[3] = im->native_alpha ? im->native_alpha[y * im->width + x]
                                              : im->rgba[(y * im->width + x) * 4 + 3];
                }
            glBindTexture(GL_TEXTURE_2D, t->id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, im->tex_w, im->tex_h, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, base);
            free(base);
            t->width = im->tex_w;
            t->height = im->tex_h;
            for (unsigned level = 1; level <= im->mip_count; level++) {
                int w = im->tex_w >> level, h = im->tex_h >> level;
                glTexImage2D(GL_TEXTURE_2D, (GLint)level, GL_RGBA8, w ? w : 1, h ? h : 1, 0,
                             GL_RGBA, GL_UNSIGNED_BYTE, im->mip_rgba[level - 1]);
            }
            glTexParameteri(GL_TEXTURE_2D, 0x813D, im->mip_count); /* GL_TEXTURE_MAX_LEVEL */
        }
        t->filtering_applied = -1;
        t->anisotropy_max = t->anisotropy_original = 1;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
        int can_anisotropy = 0;
#ifdef MOON_PC_VULKAN
        can_anisotropy = moon_vk_render_active();
#endif
        if (can_anisotropy || SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
            glGetFloatv(0x84FF, &t->anisotropy_max);
            t->anisotropy_original = can_anisotropy ? 1 : fminf(t->anisotropy_max, 8);
        }
#endif
        t->nearest = !im->magnify && !im->minify;
        t->texture_function = im->texture_function;
        t->wrap_u = im->wrap_u;
        t->wrap_v = im->wrap_v;
        t->min_u = im->min_u;
        t->max_u = im->max_u;
        t->min_v = im->min_v;
        t->max_v = im->max_v;
        if (im->wrap_u < 2 && im->wrap_v < 2 && t->id) {
            glBindTexture(GL_TEXTURE_2D, t->id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            im->wrap_u ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            im->wrap_v ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
            /* P: keep authored mip uploads above, omit PC-generated levels
               and anisotropy. Native GS mip selection remains in the shader. */
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#else
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            if (!im->mip_count)
                glGenerateMipmap(GL_TEXTURE_2D);
            if (SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
                GLfloat max = 1;
                glGetFloatv(0x84FF, &max);
                glTexParameterf(GL_TEXTURE_2D, 0x84FE, fminf(max, 8));
            }
#endif
        }
    }
    mesh->source_models = m->source_models ? m->source_models : 1;
    mesh->model_ranges = calloc((size_t)mesh->source_models * 2, sizeof(int));
    if (!mesh->model_ranges) {
        r3d_mesh_destroy(mesh);
        return NULL;
    }
    for (int i = 0; i < mesh->ngroups; i++) {
        unsigned model = mesh->groups[i].source_model;
        if (model >= mesh->source_models) {
            r3d_mesh_destroy(mesh);
            return NULL;
        }
        int *range = mesh->model_ranges + model * 2;
        if (range[1] == 0)
            range[0] = i;
        range[1] = i + 1;
    }
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
    /* Material descriptions are immutable after creation. Precompute compatible
       ranges once; visibility and model-list boundaries are still checked per view. */
    for (int i = mesh->ngroups - 1; i >= 0; i--) {
        r3d_group *g = mesh->groups + i;
        g->batch_end = i + 1;
        if (i + 1 < mesh->ngroups) {
            const r3d_group *next = g + 1;
            if (next->first == g->first + g->count && same_opaque_group(g, next))
                g->batch_end = next->batch_end;
        }
    }
#endif
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
    if (!mesh->skinned && !mesh->morph && mesh->ngroups >= 64)
        r3d_visibility_build(&mesh->visibility, mesh->groups[0].lo, sizeof *mesh->groups,
                             (unsigned)mesh->ngroups);
#endif
    mesh->ntextures = m->ntextures;
    mesh->vertices = m->count * 3;
#ifdef __SWITCH__
    int resident = 0;
    for (int i = 0; i < mesh->ntextures; i++)
        resident += mesh->textures[i].id != 0;
    fprintf(
        stderr,
        "Switch mesh: %d triangles, textures resident=%d/%d, prepare/upload %.2f ms, shared=%d\n",
        m->count, resident, m->ntextures, (plat_time_ns() - upload_started) / 1e6, shared != NULL);
#endif
    return mesh;
}
r3d_mesh *r3d_mesh_create(const gt4_model *m)
{
    return r3d_mesh_create_shared(m, NULL);
}

void r3d_mesh_destroy(r3d_mesh *mesh)
{
    if (!mesh)
        return;
    if (!mesh->texture_refs || --*mesh->texture_refs == 0) {
        for (int i = 0; mesh->textures && i < mesh->ntextures; i++) {
            glDeleteTextures(1, &mesh->textures[i].id);
            glDeleteTextures(2, mesh->textures[i].wheel_tex);
            glDeleteFramebuffers(2, mesh->textures[i].wheel_fbo);
        }
        free(mesh->textures);
        free(mesh->texture_refs);
    }
    if (mesh->vbo)
        glDeleteBuffers(1, &mesh->vbo);
    if (mesh->vao)
        glDeleteVertexArrays(1, &mesh->vao);
    r3d_visibility_free(&mesh->visibility);
    free(mesh->groups);
    free(mesh->model_ranges);
    free(mesh);
}

void r3d_begin(float x, float y, float w, float h, const tex1_image *environment)
{
    accumulate_work(&completed_work, &frame_stats);
    memset(&frame_stats, 0, sizeof frame_stats);
    r3d_env_frame++;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
    static r3d_env_slot reference_env, repeat_env, no_batch_env, flat_env;
    int reference = r3d_env_flag("MOON_RENDER_REFERENCE", &reference_env);
    pc_submission.cache_materials = !reference;
#ifdef MOON_PC_VULKAN
    if (moon_vk_render_active() && r3d_env_flag("MOON_VK_REPEAT_COMMANDS", &repeat_env))
        pc_submission.cache_materials = 0;
#endif
    pc_submission.batch_opaque = !reference && !r3d_env_flag("MOON_RENDER_NO_BATCH", &no_batch_env);
    pc_submission.spatial = scene_spatial && !r3d_env_flag("MOON_FLAT_VISIBILITY", &flat_env);
#endif

#ifndef MOON_DEKO3D
    gpu_environment = 0;
#endif
    video_flush2d();
    if (!r3d_available())
        return;
    projection_x = x <= 0 && w >= 640 ? video_projection_x() : 1.f;
    video_viewport(x, y, w, h, &vp[0], &vp[1], &vp[2], &vp[3]);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp[0], vp[1], vp[2], vp[3]);
    glDepthMask(GL_TRUE);
    glClearDepth(GL_DEPTH_FAR);
    glClear(GL_DEPTH_BUFFER_BIT);
    /* destination alpha starts at 1 (128), as in the software renderer */
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (environment != env_source || env_dirty) {
        env_dirty = 0;
        environment_revision++;
        if (environment && env_tex && env_w == environment->width && env_h == environment->height) {
            glBindTexture(GL_TEXTURE_2D, env_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, env_w, env_h, GL_RGBA, GL_UNSIGNED_BYTE,
                            environment->rgba);
        } else {
            if (env_tex)
                glDeleteTextures(1, &env_tex);
            env_tex = 0;
            env_w = env_h = 0;
            if (environment) {
                env_tex = upload(environment->width, environment->height, environment->rgba, NULL);
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
                /* The reference uses texelFetch; the adapted path uses base
                   bilinear sampling. Neither needs generated mip levels. */
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif
                env_w = environment->width;
                env_h = environment->height;
            }
        }
        env_source = environment;
    }
    int fw = vp[0] + vp[2], fh = vp[1] + vp[3];
    if (!dest_tex || fw > dest_w || fh > dest_h) {
        if (!dest_tex)
            glGenTextures(1, &dest_tex);
        glBindTexture(GL_TEXTURE_2D, dest_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fw, fh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        dest_w = fw;
        dest_h = fh;
    }
}

/* CarFace's two resident layers, 00217058 / 00216E78. A regular layer
   starts with the page; the reflected layer starts black and adds RGB.
   The target matches the destination viewport, without GPU readback. */
static struct {
    GLuint fbo, color, depth, copy, vao, accum_fbo, accum_color;
    GLint fb, viewport[4], scissor[4], image;
    int w, h, accum_w, accum_h, clipped, active;
    float projection;
} layer;
int r3d_begin_layer(int copy_page)
{
    if (layer.active || !r3d_available() || vp[2] <= 0 || vp[3] <= 0)
        return 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &layer.fb);
    memcpy(layer.viewport, vp, sizeof vp);
    layer.projection = projection_x;
    layer.clipped = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, layer.scissor);
    if (!layer.fbo) {
        glGenFramebuffers(1, &layer.fbo);
        glGenTextures(1, &layer.color);
        glGenRenderbuffers(1, &layer.depth);
    }
    int width = vp[2], height = vp[3];
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    /* P: CarFace layers use the same Switch 3D budget as race/showroom.
       Keep the page viewport for composition, including its clipping. */
    switch_scene_dimensions(vp[2], vp[3], &width, &height);
#endif
    if (layer.w != width || layer.h != height) {
        layer.w = width;
        layer.h = height;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, layer.color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, layer.w, layer.h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindRenderbuffer(GL_RENDERBUFFER, layer.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, gl_reversed_z ? 0x8CAC : GL_DEPTH_COMPONENT24,
                              layer.w, layer.h);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, layer.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, layer.color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, layer.depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, layer.fb);
        return 0;
    }
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClearDepth(GL_DEPTH_FAR);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (copy_page) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, layer.fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, layer.fbo);
        glBlitFramebuffer(vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3], 0, 0, layer.w, layer.h,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, layer.fbo);
    }
    vp[0] = vp[1] = 0;
    vp[2] = layer.w;
    vp[3] = layer.h;
    glViewport(0, 0, layer.w, layer.h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, layer.w, layer.h);
    layer.active = 1;
    return 1;
}
void r3d_end_layer(float opacity, int additive)
{
    r3d_end_layer_sample(opacity, additive, 0, 1);
}
void r3d_end_layer_sample(float opacity, int additive, int index, int samples)
{
    if (!layer.active)
        return;
    r3d_end();
    glBindFramebuffer(GL_FRAMEBUFFER, layer.fb);
    memcpy(vp, layer.viewport, sizeof vp);
    projection_x = layer.projection;
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    if (layer.clipped) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(layer.scissor[0], layer.scissor[1], layer.scissor[2], layer.scissor[3]);
    } else
        glDisable(GL_SCISSOR_TEST);
    if (!layer.copy) {
        layer.copy = gl_program(
            "#version 330 core\nout vec2 uv; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.-1.,0.,1.);}\n",
            "#version 330 core\nin vec2 uv; out vec4 o; uniform sampler2D image; void main(){o=texture(image,uv);}\n");
        glGenVertexArrays(1, &layer.vao);
        layer.image = glGetUniformLocation(layer.copy, "image");
    }
    if (layer.copy) {
        GLuint source = layer.color;
        if (samples > 1) {
            if (!layer.accum_fbo) {
                glGenFramebuffers(1, &layer.accum_fbo);
                glGenTextures(1, &layer.accum_color);
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer.accum_color);
            if (layer.accum_w != layer.w || layer.accum_h != layer.h) {
                layer.accum_w = layer.w;
                layer.accum_h = layer.h;
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, layer.w, layer.h, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, layer.accum_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   layer.accum_color, 0);
            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, layer.w, layer.h);
            glDisable(GL_DEPTH_TEST);
            if (!index) {
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            glUseProgram(layer.copy);
            glBindVertexArray(layer.vao);
            glBindTexture(GL_TEXTURE_2D, layer.color);
            glUniform1i(layer.image, 0);
            glEnable(GL_BLEND);
            glBlendColor(0, 0, 0, gt4_service_blend_fix(1.f / (index + 1)) / 128.f);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_ZERO, GL_ONE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDisable(GL_BLEND);
            glBindVertexArray(0);
            glBindFramebuffer(GL_FRAMEBUFFER, layer.fb);
            glViewport(vp[0], vp[1], vp[2], vp[3]);
            if (layer.clipped) {
                glEnable(GL_SCISSOR_TEST);
                glScissor(layer.scissor[0], layer.scissor[1], layer.scissor[2], layer.scissor[3]);
            }
            if (index + 1 < samples) {
                layer.active = 0;
                return;
            }
            source = layer.accum_color;
        }
        /* 004A2808 +0x1c..0x50 truncates FIX in PS2 FPU mode. */
        float coefficient = gt4_service_blend_fix(opacity) / 128.f;
        glUseProgram(layer.copy);
        glBindVertexArray(layer.vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, source);
        glUniform1i(layer.image, 0);
        glEnable(GL_BLEND);
        glBlendColor(0, 0, 0, coefficient);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_CONSTANT_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_CONSTANT_ALPHA,
                            GL_ZERO, GL_ONE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_BLEND);
        glBindVertexArray(0);
    }
    layer.active = 0;
}

/* Everything drawn so far stays behind what follows (a sky dome). */
void r3d_clear_depth(void)
{
    if (!r3d_available())
        return;
    glDepthMask(GL_TRUE);
    glClearDepth(GL_DEPTH_FAR);
    glClear(GL_DEPTH_BUFFER_BIT);
}

/* An offscreen square of size x size with its own depth (environment
   maps): drawing goes there until r3d_end_target, which reads it back. */
static GLuint target_fbo, target_color, target_depth;
static int target_size, target_saved_fb, target_saved_vp[4];
static float target_saved_projection;
int r3d_begin_target(int size)
{
    video_flush2d();
    if (!r3d_available() || size <= 0 || size > 2048)
        return 0;
    if (size != target_size) {
        if (!target_fbo) {
            glGenFramebuffers(1, &target_fbo);
            glGenTextures(1, &target_color);
            glGenRenderbuffers(1, &target_depth);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, target_color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindRenderbuffer(GL_RENDERBUFFER, target_depth);
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            gl_reversed_z ? 0x8CAC /* GL_DEPTH_COMPONENT32F */ : GL_DEPTH_COMPONENT24, size, size);
        target_size = size;
    }
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    target_saved_fb = (int)video_draw_framebuffer();
#else
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target_saved_fb);
#endif
    memcpy(target_saved_vp, vp, sizeof vp);
    target_saved_projection = projection_x;
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target_color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, target_saved_fb);
        return 0;
    }
    vp[0] = vp[1] = 0;
    vp[2] = vp[3] = size;
    projection_x = 1;
    glViewport(0, 0, size, size);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, size, size);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearDepth(GL_DEPTH_FAR);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!dest_tex || size > dest_w || size > dest_h) { /* the destination-alpha copy */
        if (!dest_tex)
            glGenTextures(1, &dest_tex);
        glBindTexture(GL_TEXTURE_2D, dest_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size > dest_w ? size : dest_w,
                     size > dest_h ? size : dest_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (size > dest_w)
            dest_w = size;
        if (size > dest_h)
            dest_h = size;
    }
    return 1;
}

#ifndef MOON_DEKO3D
void r3d_gpu_environment(unsigned texture, int width, int height)
{
    gpu_environment = texture;
    environment_revision++;
    gpu_environment_w = width;
    gpu_environment_h = height;
}
void r3d_copy_target(unsigned destination_fbo, int destination_x)
{
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, target_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination_fbo);
    glBlitFramebuffer(0, 0, target_size, target_size, destination_x, 0, destination_x + target_size,
                      target_size, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}
#endif

void r3d_end_target(uint8_t *rgba)
{
    if (!target_size)
        return;
    r3d_end();
    if (rgba) {
        frame_stats.readbacks++;
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, target_size, target_size, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        for (int y = 0; y < target_size / 2; y++) { /* GL rows go up; the image's go down */
            uint8_t *a = rgba + (size_t)y * target_size * 4,
                    *b = rgba + (size_t)(target_size - 1 - y) * target_size * 4;
            for (int x = 0; x < target_size * 4; x++) {
                uint8_t t = a[x];
                a[x] = b[x];
                b[x] = t;
            }
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, target_saved_fb);
    memcpy(vp, target_saved_vp, sizeof vp);
    projection_x = target_saved_projection;
    glViewport(vp[0], vp[1], vp[2], vp[3]);
}

void r3d_begin_ui(float x, float y, float w, float h)
{
    r3d_begin_ui_environment(x, y, w, h, NULL);
}
void r3d_begin_ui_environment(float x, float y, float w, float h, const tex1_image *environment)
{
    video_flush2d();
    GLint parent[4];
    GLboolean clipped = glIsEnabled(GL_SCISSOR_TEST);
    if (clipped)
        glGetIntegerv(GL_SCISSOR_BOX, parent);
    r3d_begin(x, y, w, h, environment);
    /* ModelFace uses its authored widget projection (00217058), even
       when its extended viewport spans the screen. Menu composition
       already receives the display transform; no race aspect correction. */
    projection_x = 1.f;
    if (clipped) {
        int left = vp[0] > parent[0] ? vp[0] : parent[0];
        int bottom = vp[1] > parent[1] ? vp[1] : parent[1];
        int right = vp[0] + vp[2] < parent[0] + parent[2] ? vp[0] + vp[2] : parent[0] + parent[2];
        int top = vp[1] + vp[3] < parent[1] + parent[3] ? vp[1] + vp[3] : parent[1] + parent[3];
        glScissor(left, bottom, right > left ? right - left : 0, top > bottom ? top - bottom : 0);
    }
}

void r3d_environment_changed(void)
{
    env_dirty = 1;
}

void r3d_end(void)
{
    if (getenv("MOON_MEASURE_LIGHTS") && measure_light_draws) {
        if (++measure_light_frames % 60 == 0)
            fprintf(stderr, "render: lights max=%u avg=%.2f draws=%u\n", measure_light_max,
                    (double)measure_light_sum / measure_light_draws, measure_light_draws);
        measure_light_draws = measure_light_sum = measure_light_max = 0;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
}

/* (A-B)*C+D with A, B, D in {Cs, Cd, 0} and C in {As, Ad, FIX}: the
   coefficient of Cs and of Cd is a*C+b; map it to GL factors. */
static void set_blend(const r3d_group *g)
{
    if (!g->blend_enabled) {
        glDisable(GL_BLEND);
        return;
    }
    unsigned e = g->state.blend, A = e & 3, B = (e >> 2) & 3, C = (e >> 4) & 3, D = (e >> 6) & 3;
    int sa = (A == 0) - (B == 0), sb = D == 0, da = (A == 1) - (B == 1), db = D == 1;
    GLenum c = C == 0 ? GL_SRC_ALPHA : C == 1 ? GL_DST_ALPHA : GL_CONSTANT_ALPHA;
    GLenum c1 = C == 0   ? GL_ONE_MINUS_SRC_ALPHA
                : C == 1 ? GL_ONE_MINUS_DST_ALPHA
                         : GL_ONE_MINUS_CONSTANT_ALPHA;
    int sneg = 0, dneg = 0;
    GLenum sf, df;
#define FACTOR(a, b, out, neg)                                                                     \
    if ((a) == 0)                                                                                  \
        out = (b) ? GL_ONE : GL_ZERO;                                                              \
    else if ((a) == 1 && !(b))                                                                     \
        out = c;                                                                                   \
    else if ((a) == -1 && (b))                                                                     \
        out = c1;                                                                                  \
    else if ((a) == -1) {                                                                          \
        out = c;                                                                                   \
        neg = 1;                                                                                   \
    } else                                                                                         \
        out = GL_ONE; /* 1 + C: not expressible, nearest */
    FACTOR(sa, sb, sf, sneg)
    FACTOR(da, db, df, dneg)
#undef FACTOR
    GLenum eq = GL_FUNC_ADD;
    if (sneg && !dneg)
        eq = GL_FUNC_REVERSE_SUBTRACT;
    else if (dneg && !sneg)
        eq = GL_FUNC_SUBTRACT;
    else if (sneg && dneg) {
        sf = GL_ZERO;
        df = GL_ZERO;
    }
    glEnable(GL_BLEND);
    glBlendColor(0, 0, 0, fminf(1, g->state.fixed_alpha / 128.f));
    /* colour by the equation; the alpha channel stores the source coefficient */
    glBlendEquationSeparate(eq, GL_FUNC_ADD);
    glBlendFuncSeparate(sf, df, GL_ONE, GL_ZERO);
}

/* clip z = view z * k[0] + k[1] (w = view z): classic [-1, 1] depth, or
   reversed [0, 1] with the near plane at 1 and the far at 0 */
static void depth_coeffs(float n, float f, float k[2])
{
    if (gl_reversed_z) {
        k[0] = -n / (f - n);
        k[1] = n * f / (f - n);
    } else {
        k[0] = (f + n) / (f - n);
        k[1] = -2.f * f * n / (f - n);
    }
}

static void set_depth(const gt4_draw_state *s, int depth_write)
{
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (!s->depth_test)
        glDepthFunc(GL_ALWAYS);
    else if (s->depth_func == 0)
        glDepthFunc(GL_NEVER);
    else if (s->depth_func == 1)
        glDepthFunc(GL_ALWAYS);
    /* Model command 25 (00456000) -> 004A26F0 +00..+1C:
       D_00629DD8 maps API indices 0,1,2,3 to GS ZTST 0,1,3,2.
       2 is strict; 3 includes equality, independently of ZMSK.
       Do not add an estimated offset to read-only passes. */
    else if (s->depth_func == 2)
        glDepthFunc(GL_DEPTH_FUNC_LESS);
    else
        glDepthFunc(GL_DEPTH_FUNC_LEQUAL);
    glDepthMask(depth_write ? GL_TRUE : GL_FALSE);
}

static void set_masks(uint32_t mask, int color, int alpha)
{
    glColorMask(color && ((mask & 0xff) != 0xff), color && (((mask >> 8) & 0xff) != 0xff),
                color && (((mask >> 16) & 0xff) != 0xff), alpha && (((mask >> 24) & 0xff) != 0xff));
}

/* 00453BB0, 00453A58 and 004A6668: render two rotations in alternating
   scratch rows, then bilinearly reduce the 2S strip into S. SCANMSK is
   004A5FE0 +80 (field parity 0 here); it is not alpha blending. Separate
   resident scratch/result images avoid sampling the active attachment.
   Raster subpixels and GL filtering still need a GS image comparison (A). */
static int wheel_accumulate(r3d_texture *t, int samples, float angle)
{
    if (t->wheel_samples == samples && t->wheel_angle == angle && t->wheel_tex[1]) {
        frame_stats.wheel_cache_hits++;
        return 1;
    }
    if (!wheel_program) {
        wheel_program = gl_program(
            "#version 330 core\n"
            "uniform vec3 points[4];uniform vec2 extent;out vec2 uv;\n"
            "void main(){int i=gl_VertexID;uv=vec2(i==1||i==2?1.:0.,i>=2?1.:0.);"
            "gl_Position=vec4(points[i].xy/extent*2.-1.,0.,1.);}\n",
            "#version 330 core\n"
            "uniform sampler2D source;uniform int scanmask,copy_pass;in vec2 uv;out vec4 color;\n"
            "void main(){if(scanmask==1&&(int(gl_FragCoord.y)&1)==0)discard;"
            "vec2 p=copy_pass==1?vec2(uv.x,(1.+2.*uv.y)/3.):uv;"
            "color=texture(source,p);}\n");
        if (!wheel_program)
            return 0;
        WU.points = glGetUniformLocation(wheel_program, "points");
        WU.extent = glGetUniformLocation(wheel_program, "extent");
        WU.source = glGetUniformLocation(wheel_program, "source");
        WU.scanmask = glGetUniformLocation(wheel_program, "scanmask");
        WU.copy = glGetUniformLocation(wheel_program, "copy_pass");
        glGenVertexArrays(1, &wheel_vao);
    }
    gt4_wheel_accum_plan p = gt4_wheel_accumulation(t->tex_w, samples, angle);
    int size = p.size;
    if (size <= 0)
        return 0;
    GLint draw, read, viewport[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLfloat clear[4];
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
    glActiveTexture(GL_TEXTURE0);
    if (!t->wheel_tex[0]) {
        glGenTextures(2, t->wheel_tex);
        glGenFramebuffers(2, t->wheel_fbo);
    }
    if (t->wheel_size != size)
        for (int k = 0; k < 2; k++) {
            glBindTexture(GL_TEXTURE_2D, t->wheel_tex[k]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, k ? size : 3 * size, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t->wheel_fbo[k]);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   t->wheel_tex[k], 0);
        }
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColorMask(1, 1, 1, 1);
    glUseProgram(wheel_program);
    glBindVertexArray(wheel_vao);
    glUniform1i(WU.source, 0);
    for (int stage = 0; stage < p.stages; stage++) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t->wheel_fbo[0]);
        glViewport(0, 0, size, 3 * size);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        GLuint source = stage ? t->wheel_tex[1] : t->id;
        glBindTexture(GL_TEXTURE_2D, source);
        /* Bilinear, base level only: 00453BB0 +128/+138 TEX1 MMIN/MMAG=1.
           Restore the source's filtering after use; other cars share assets. */
        GLint min_filter, mag_filter;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &mag_filter);
        GLint wrap_s, wrap_t;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap_s);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &wrap_t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        stage || t->wrap_u ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        stage || t->wrap_v ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform2f(WU.extent, (float)size, (float)(3 * size));
        glUniform1i(WU.copy, 0);
        for (int k = 0; k < 2; k++) {
            float xy[8], points[12] = {0};
            gt4_wheel_accum_quad(&p.draw[stage * 2 + k], xy);
            for (int v = 0; v < 4; v++) {
                points[v * 3] = xy[v * 2];
                points[v * 3 + 1] = xy[v * 2 + 1];
            }
            glUniform3fv(WU.points, 4, points);
            glUniform1i(WU.scanmask, k);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            frame_stats.wheel_passes++;
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t->wheel_fbo[1]);
        glViewport(0, 0, size, size);
        glBindTexture(GL_TEXTURE_2D, t->wheel_tex[0]);
        const float points[12] = {0,           0,           0, (float)size, 0,           0,
                                  (float)size, (float)size, 0, 0,           (float)size, 0};
        glUniform3fv(WU.points, 4, points);
        glUniform2f(WU.extent, (float)size, (float)size);
        glUniform1i(WU.copy, 1);
        glUniform1i(WU.scanmask, 0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        frame_stats.wheel_passes++;
    }
    t->wheel_size = size;
    t->wheel_samples = samples;
    t->wheel_angle = angle;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    return 1;
}

#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
#include "render3d_cloud_cache.inc"
#include "render3d_environment_cache.inc"
#endif
/* The window rectangle a group can cover: its box projected as the vertex
   shader does, two pixels wider, clipped to the viewport. DATE reads the
   copied alpha only under the group's own fragments, so copying this
   rectangle instead of the whole viewport gives the same image. A box
   reaching behind the eye keeps the whole viewport. MOON_DATE_FULL: old copy. */
static void date_rect(const r3d_view *view, const r3d_group *g, int skinned, int rect[4])
{
    rect[0] = vp[0];
    rect[1] = vp[1];
    rect[2] = vp[0] + vp[2];
    rect[3] = vp[1] + vp[3];
    static r3d_env_slot full;
    if (r3d_env_flag("MOON_DATE_FULL", &full))
        return;
    float lo[3], hi[3];
    const float *bound_lo = g->lo, *bound_hi = g->hi;
    if (skinned) {
        r3d_skin_bounds(g->lo, g->hi, g->bone_mask, g->weight_lo, g->weight_hi, view->skin_palette,
                        lo, hi);
        bound_lo = lo;
        bound_hi = hi;
    }
    float fx = view->focal / 320 * projection_x,
          fy = (view->focal_y > 0 ? view->focal_y : view->focal) / 224;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (int corner = 0; corner < 8; corner++) {
        float p[3], w[3];
        for (int k = 0; k < 3; k++)
            p[k] = corner & (1 << k) ? bound_hi[k] : bound_lo[k];
        for (int r = 0; r < 3; r++)
            w[r] = view->rot[r * 3] * p[0] + view->rot[r * 3 + 1] * p[1] +
                   view->rot[r * 3 + 2] * p[2] + view->pos[r];
        if (view->mirror)
            w[1] = -w[1];
        for (int k = 0; k < 3; k++)
            w[k] -= view->cam[k];
        float x = w[0] * view->r[0] + w[1] * view->r[1] + w[2] * view->r[2],
              y = w[0] * view->u[0] + w[1] * view->u[1] + w[2] * view->u[2],
              z = w[0] * view->f[0] + w[1] * view->f[1] + w[2] * view->f[2];
        if (!(z > 1e-3f) || !isfinite(x) || !isfinite(y))
            return;
        float sx = x * fx / z + view->jitter[0], sy = y * fy / z + view->jitter[1];
        x0 = fminf(x0, sx);
        x1 = fmaxf(x1, sx);
        y0 = fminf(y0, sy);
        y1 = fmaxf(y1, sy);
    }
    float px0 = vp[0] + (x0 + 1) * .5f * vp[2] - 2, px1 = vp[0] + (x1 + 1) * .5f * vp[2] + 2,
          py0 = vp[1] + (y0 + 1) * .5f * vp[3] - 2, py1 = vp[1] + (y1 + 1) * .5f * vp[3] + 2;
    if (px0 > rect[0])
        rect[0] = px0 < rect[2] ? (int)floorf(px0) : rect[2];
    if (px1 < rect[2])
        rect[2] = px1 > rect[0] ? (int)ceilf(px1) : rect[0];
    if (py0 > rect[1])
        rect[1] = py0 < rect[3] ? (int)floorf(py0) : rect[3];
    if (py1 < rect[3])
        rect[3] = py1 > rect[1] ? (int)ceilf(py1) : rect[1];
}
static void draw_mesh(r3d_mesh *mesh, const r3d_view *view, const r3d_lights *lights)
{
    if (!mesh || !program)
        return;
    SWP_PUSH(SWP_GL);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    int cloud_ready=cloud_cache_prepare(view);
    int environment_ready=environment_cache_prepare(view);
#endif
    float planes[6][4];
    r3d_frustum(view, projection_x, planes);
    static r3d_env_slot no_cull;
    int cull = !r3d_env_flag("MOON_DISABLE_CULL", &no_cull);
    float wheel_angle = view->wheel_interval;
    if (view->wheel_blur > 1) {
        /* 004522D8 +A8..+C0: exp(-20 * screen displacement). The PC
           camera projection supplies screen XY here; its equivalence to
           the original vf24..27 projection is still A, not an EE proof. */
        float delta[3], screen[2];
        for (int k = 0; k < 3; k++)
            delta[k] = view->pos[k] - view->cam[k];
        float z = delta[0] * view->f[0] + delta[1] * view->f[1] + delta[2] * view->f[2];
        if (z > 0) {
            screen[0] = (delta[0] * view->r[0] + delta[1] * view->r[1] + delta[2] * view->r[2]) *
                        view->focal / 320 * projection_x / z;
            screen[1] = (delta[0] * view->u[0] + delta[1] * view->u[1] + delta[2] * view->u[2]) *
                        (view->focal_y > 0 ? view->focal_y : view->focal) / 224 / z;
            if (mesh->wheel_screen_valid) {
                float dx = screen[0] - mesh->wheel_screen[0],
                      dy = screen[1] - mesh->wheel_screen[1];
                wheel_angle *= expf(-20 * sqrtf(dx * dx + dy * dy));
            }
            memcpy(mesh->wheel_screen, screen, sizeof screen);
            mesh->wheel_screen_valid = 1;
        }
    }
    if (view->wheel_blur > 1 && isfinite(view->wheel_interval)) {
        for (int i = 0; i < mesh->ngroups; i++) {
            r3d_group *g = &mesh->groups[i];
            int texture = g->texture;
            if (!g->wheel_blur_texture || texture < 0 || texture >= mesh->ntextures)
                continue;
            if (cull && !r3d_bounds_visible(planes, g->lo, g->hi))
                continue;
            wheel_accumulate(&mesh->textures[texture], view->wheel_blur, wheel_angle);
        }
    }
    glUseProgram(program);
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    static r3d_env_slot shader_reference;
    glUniform1i(U.switch_fast, !r3d_env_flag("MOON_SWITCH_SHADER_REFERENCE", &shader_reference));
#endif
    int skinned = mesh->skinned && view->skin_palette != NULL;
    glUniform1i(U.skinned, skinned);
    glUniform1i(U.morph_color,mesh->morph_color);
    glUniform1f(U.morph, mesh->morph && isfinite(view->grasp)
                             ? fminf(1, fmaxf(0, view->grasp)) * mesh->grasp_scale
                             : 0);
    glUniform1i(U.colorop, view->authored_color);
    glUniform3fv(U.colormul, 1, view->color_mul);
    glUniform1f(U.valpha, view->vertex_alpha_set ? view->vertex_alpha : 1.f);
    glUniform3fv(U.coloradd, 1, view->color_add);
    if (skinned)
        glUniformMatrix4fv(U.palette, 20, GL_FALSE, &view->skin_palette[0][0]);
    int n = lights ? lights->count : 0;
    n = n < 0 ? 0 : n > 4 ? 4 : n;
    const char *measure_lights = getenv("MOON_MEASURE_LIGHTS");
    if (measure_lights) {
        int cap = atoi(measure_lights);
        if (cap >= 0 && cap < n)
            n = cap;
    }
    measure_light_draws++;
    measure_light_sum += (unsigned)n;
    if ((unsigned)n > measure_light_max)
        measure_light_max = (unsigned)n;
    glUniform1i(U.nlights, n);
    glUniform1i(U.local_lights, n ? lights->authored_local : 0);
    glUniform1f(U.lvertex, n ? lights->vertex_limit : 0);
    if (n) {
        glUniform3fv(U.ldir, n, &lights->direction[0][0]);
        glUniform3fv(U.lamb, n, &lights->ambient[0][0]);
        glUniform3fv(U.ldif, n, &lights->diffuse[0][0]);
        glUniform3fv(U.lspe, n, &lights->specular[0][0]);
        glUniform1f(U.lspec_scale, lights->specular_scale > 0 ? lights->specular_scale : 1);
    }
    glBindVertexArray(mesh->vao);
    /* rot is row-major; GLSL mat3 is column-major */
    glUniformMatrix3fv(U.rot, 1, GL_TRUE, view->rot);
    glUniform3fv(U.pos, 1, view->pos);
    glUniform3fv(U.cam, 1, view->cam);
    glUniform3fv(U.r, 1, view->r);
    glUniform3fv(U.u, 1, view->u);
    glUniform3fv(U.f, 1, view->f);
    float fy = view->focal_y > 0 ? view->focal_y : view->focal;
    glUniform2f(U.focal, view->focal / 320 * projection_x, fy / 224);
    glUniform2f(U.jitter, view->jitter[0], view->jitter[1]);
    {
        float k[2];
        depth_coeffs((view->near_clip > 0 ? view->near_clip : .2f) + view->depth_bias,
                     (view->far_clip > view->near_clip ? view->far_clip : 20000) + view->depth_bias, k);
        glUniform2f(U.clip, k[0], k[1]);
    }
    glUniform1i(U.mode, view->shadow ? 2 : view->unlit ? 1 : 0);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
    glUniform1i(U.material_shading, pc_submission.material_shading && !view->shadow && !view->unlit);
    glUniform1i(U.skip_material_light, getenv("MOON_MEASURE_SKIP_MATERIAL_LIGHT") != NULL);
    shadow_map_receiver(view);
    glUniform1i(U.hdr,video_hdr_active());
    glUniform1i(U.daylight, view->daylight);
    glUniform1i(U.authored_environment,view->authored_environment);
    cloud_cache_bind(cloud_ready,view);
    environment_cache_bind(environment_ready);
    glUniform1f(U.sky_seconds, view->sky_seconds);
    glUniform1i(U.night_sky,night_sky_enabled);
    glUniform1f(U.authored_night, view->authored_night);
    glUniform3fv(U.sun_direction,1,view->sun_direction);
    glUniform3fv(U.sun_color,1,view->sun_color);
    glUniform3fv(U.sky_color,1,view->sky_color);
    glUniform3fv(U.haze_color,1,view->haze_color);
    int lamp_count=car_lamps_enabled && !view->ghost && !view->shadow?car_lamp_count:0;
    glUniform1i(U.car_lamp_count,lamp_count);
    lamp_shadow_receiver(lamp_count);
    float positions[R3D_CAR_LAMPS][4],directions[R3D_CAR_LAMPS][4],colors[R3D_CAR_LAMPS][4];
    for(int i=0;i<lamp_count;i++) {
        memcpy(positions[i],car_lamp_frame[i].position,16);memcpy(directions[i],car_lamp_frame[i].direction,16);memcpy(colors[i],car_lamp_frame[i].color,16);
    }
    if(lamp_count){glUniform4fv(U.car_lamp_position,lamp_count,positions[0]);glUniform4fv(U.car_lamp_direction,lamp_count,directions[0]);glUniform4fv(U.car_lamp_color,lamp_count,colors[0]);}

    glUniform1i(U.modern_lighting, pc_submission.modern_lighting && view->modern_light && !view->ghost && !view->shadow);
    glUniform1i(U.measure_no_env, getenv("MOON_MEASURE_NO_ENV") != NULL);
    glUniform1i(U.measure_no_spec, getenv("MOON_MEASURE_NO_SPEC") != NULL);
#endif
    glUniform1f(U.shadow_alpha, view->shadow_alpha > 0 ? view->shadow_alpha : .55f);
    glUniform3fv(U.shadow_rgb, 1, view->shadow_rgb);
    glUniform2fv(U.envk, 1, view->env_color);
    glUniform1f(U.cscale, view->color_scale > 0 ? view->color_scale : 1.f);
    /* the GS: a textured fragment is texel * colour / 128, an untextured
       one the colour itself (128 is half the framebuffer's range); the
       course's colours are in units of 128 */
    glUniform1f(U.untex, view->unlit ? 128.f / 255.f : 1.f);
    glUniform1i(U.mirror, view->mirror != 0);
    glUniform3fv(U.grade_rgb, 1, view->grade);
    glUniform3f(U.grade, view->grade[3], view->grade[4], view->grade[5]);
    { /* 004A40B0: the VU1 constants from the distance and the floor */
        float m = fminf(fmaxf(view->fog_max * 255.f, 0), 255);
        if (view->fog)
            glUniform3f(U.fog, 255.f - m, m + .1f, fmaxf(view->fog_distance, 0));
        else
            glUniform3f(U.fog, -1, 0, 0);
    }
    float fog_scale = view->unlit && view->color_scale > 0 ? view->color_scale : 1.f;
    glUniform2f(U.focal_px, view->focal * projection_x, fy);
    glUniform4f(U.viewport, (float)vp[0], (float)vp[1], (float)vp[2], (float)vp[3]);
    glUniform1i(U.tex, 0);
    glUniform1i(U.dest, 1);
    glUniform1i(U.envtex, 2);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, dest_tex);
    glActiveTexture(GL_TEXTURE2);
#ifndef MOON_DEKO3D
    glBindTexture(GL_TEXTURE_2D, gpu_environment ? gpu_environment : env_tex);
    glUniform2i(U.envsize, gpu_environment ? gpu_environment_w : env_w,
                gpu_environment ? gpu_environment_h : env_h);
#else
    glBindTexture(GL_TEXTURE_2D, env_tex);
    glUniform2i(U.envsize, env_w, env_h);
#endif
    glEnable(GL_CULL_FACE);
    glFrontFace((view->mirror != 0) != (view->screen_reflect != 0)
                    ? GL_CW
                    : GL_CCW); /* 00341E50 projection reflection, plus optional floor reflection */
    /* 00396F00 +34..+78 submits only the Vls0 list, in its stored order.
       Distinct detailed/coarse meshes may occupy the same surface. */
    unsigned count =
        view->model_list ? (unsigned)view->model_list[0] | (unsigned)view->model_list[1] << 8 : 1;
    /* P: reuse texture bindings/uniforms only inside this mesh submission.
       DATE snapshots and draw order (00396F00 +34..78) are unchanged. */
    const r3d_texture *last_texture = NULL;
    GLuint last_texture_id = 0;
    int texture_valid = 0;
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    static r3d_env_slot uncached;
    int cache_textures = !r3d_env_flag("MOON_SWITCH_UNCACHED", &uncached);
    int batch_opaque = cache_textures;
#else
    int cache_textures = pc_submission.cache_materials;
    int batch_opaque = pc_submission.batch_opaque;
#endif
    const r3d_group *last_draw_state = NULL;
    GLint date_read_fb = 0, date_draw_fb = 0;
    int date_framebuffer_known = 0;
    static r3d_env_slot no_reflect_env, only_reflect_env;
    int no_reflect = r3d_env_flag("MOON_NO_REFLECT", &no_reflect_env);
    int only_reflect = r3d_env_flag("MOON_ONLY_REFLECT", &only_reflect_env);
    int spatial = 0;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
    /* Authored course lists already restrict model ranges. A global spatial
       query is useful for unrestricted meshes, especially reflection scenery. */
    spatial =
        cull && pc_submission.spatial && mesh->visibility.count && !skinned && !view->model_list;
#endif
    for (unsigned entry = 0; entry < count; entry++) {
        int first = 0, end = mesh->ngroups;
        if (view->model_list) {
            unsigned model = (unsigned)view->model_list[2 + entry * 2] |
                             (unsigned)view->model_list[3 + entry * 2] << 8;
            if (model >= mesh->source_models)
                continue;
            first = mesh->model_ranges[model * 2];
            end = mesh->model_ranges[model * 2 + 1];
        }
        if (spatial) {
            r3d_visibility_select(&mesh->visibility, planes, mesh->groups[0].lo,
                                  sizeof *mesh->groups, (unsigned)first, (unsigned)end);
            frame_stats.visibility_tests += mesh->visibility.tests;
            frame_stats.spatial_rejected += mesh->visibility.rejected;
        }
        for (int i = first; i < end; i++) {
            const r3d_group *g = &mesh->groups[i];
            if (g->lamp && (g->lamp == 2) != (view->brake != 0))
                continue; /* the brake lamps' branch */
            if (spatial && !mesh->visibility.visible[i]) {
                frame_stats.culled_groups++;
                continue;
            }
            if (cull && !spatial) {
                frame_stats.visibility_tests++;
                float lo[3], hi[3];
                const float *bound_lo = g->lo, *bound_hi = g->hi;
                if (skinned) {
                    r3d_skin_bounds(g->lo, g->hi, g->bone_mask, g->weight_lo, g->weight_hi,
                                    view->skin_palette, lo, hi);
                    bound_lo = lo;
                    bound_hi = hi;
                }
                if (!r3d_bounds_visible(planes, bound_lo, bound_hi)) {
                    frame_stats.culled_groups++;
                    continue;
                }
            }
            const gt4_draw_state *s = &g->state;
            int vertices = g->count;
            /* P: combine consecutive visible opaque slices of the same
               resident VBO. Keep primitive order and model-list boundaries.
               Multipass alpha-fail, DATE, blending, skin and ghost stay separate. */
            int single_pass = !s->alpha_test;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
            /* KEEP discards failing fragments without a second pass. Joining
               adjacent ranges preserves both primitive and fragment order. */
            single_pass |= s->alpha_fail == 0;
#endif
            if (batch_opaque && !skinned && !view->ghost && !g->blend_enabled && single_pass &&
                !s->destination_test)
                while (i + 1 < end) {
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST)
                    if (i + 1 >= g->batch_end)
                        break;
#endif
                    const r3d_group *next = mesh->groups + i + 1;
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
                    if (next->first != g->first + vertices || !same_opaque_group(g, next))
                        break;
#endif
                    if (cull) {
                        if (!spatial)
                            frame_stats.visibility_tests++;
                        if (spatial ? !mesh->visibility.visible[i + 1]
                                    : !r3d_bounds_visible(planes, next->lo, next->hi))
                            break;
                    }
                    vertices += next->count;
                    frame_stats.merged_groups++;
                    i++;
                }
            if (g->reflection && no_reflect)
                continue;
            if (!g->reflection && only_reflect)
                continue;

            int texture = view->wheel_blur == 1 && g->wheel_blur_texture ? g->wheel_blur_texture - 1
                                                                         : g->texture;
            r3d_texture *t =
                texture >= 0 && texture < mesh->ntextures ? &mesh->textures[texture] : NULL;
            const r3d_texture *source_texture = t;
            r3d_texture accumulated;
            if (t && g->wheel_blur_texture && view->wheel_blur > 1 &&
                t->wheel_samples == view->wheel_blur && t->wheel_angle == wheel_angle) {
                accumulated = *t;
                accumulated.mip_count = 0;
                accumulated.id = t->wheel_tex[1];
                accumulated.filtering_applied = -1;
                accumulated.anisotropy_original = 1;
                accumulated.width = accumulated.height = accumulated.tex_w = accumulated.tex_h =
                    t->wheel_size;
                accumulated.scale = 1;
                accumulated.nearest = 0;
                accumulated.wrap_u = accumulated.wrap_v = 1;
                t = &accumulated;
            }
            int state_changed = 1;
            state_changed = !cache_textures || view->ghost || !last_draw_state ||
                            !same_draw_state(last_draw_state, g);
            if (state_changed) {
                last_draw_state = g;
                frame_stats.state_updates++;
            } else
                frame_stats.state_cache_hits++;
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
            /* P: blend coverage is not a material class. Authored paint decals,
               glass and lamps retain their alpha while using the same lighting
               as their corresponding opaque material. */
            /* View uniforms are set once above. Material state equality already
               includes the lens tags, so this value changes with that state. */
            if (state_changed)
                glUniform1i(U.lamp_lens, g->night_lens || g->lamp == 2);
#endif
            if (state_changed) {
                /* the draw's own fog colour (a GS colour, doubled with the course's) or the scene's */
                if (s->fog_override)
                    glUniform3f(U.fogcol, s->fog_rgb[0] / 255.f * fog_scale,
                                s->fog_rgb[1] / 255.f * fog_scale,
                                s->fog_rgb[2] / 255.f * fog_scale);
                else
                    glUniform3f(U.fogcol, view->fog_rgb[0], view->fog_rgb[1], view->fog_rgb[2]);
                /* Original: func_00453698 callback 0 selects a model branch;
                   it does not replace every material with an unlit material.
                   Preserve the decoded material kind for both brake states. */
                glUniform1i(U.flat, g->flat || view->shadow || view->unlit);
                glUniform1i(U.specular, g->specular && !view->shadow);
                glUniform1i(U.reflect, g->reflection);
            }
            int texture_changed = 1;
            GLuint texture_id = t ? t->id : 0;
            texture_changed = !cache_textures || !texture_valid || source_texture != last_texture ||
                              texture_id != last_texture_id;
            if (texture_changed) {
                last_texture = source_texture;
                last_texture_id = texture_id;
                texture_valid = 1;
                frame_stats.texture_updates++;
            } else
                frame_stats.texture_cache_hits++;
            if (texture_changed) {
                glUniform1i(U.has_tex, t != NULL);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, t ? t->id : 0);
            }
            if (t && texture_changed) {
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
                int filtering = pc_submission.texture_filter;
                int ordinary_wrap = t->wrap_u < 2 && t->wrap_v < 2;
                glUniform1i(U.filtering, ordinary_wrap ? filtering : 0);
                if (t->filtering_applied != filtering && ordinary_wrap) {
                    static const float levels[] = {1, 1, 4, 8, 16};
                    float wanted = filtering ? levels[filtering] : t->anisotropy_original;
                    if (t->anisotropy_max > 1)
                        glTexParameterf(GL_TEXTURE_2D, 0x84FE, fminf(t->anisotropy_max, wanted));
                    t->filtering_applied = filtering;
                }
#endif
                int k = t->scale; /* addressing in the upscaled texture's texels */
                glUniform2f(U.texsize, (float)(t->tex_w * k), (float)(t->tex_h * k));
                glUniform2i(U.size, t->width * k, t->height * k);
                glUniform2i(U.wrap, t->wrap_u, t->wrap_v);
                glUniform4i(U.region, t->min_u * k, t->max_u * k + k - 1, t->min_v * k,
                            t->max_v * k + k - 1);
                glUniform1i(U.cc, t->color_component);
                glUniform1i(U.tfx, t->texture_function);
                glUniform1i(U.native, t->native);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
                glUniform1i(U.nearest, !texture_smoothing && t->nearest ? k : 0);
#else
                glUniform1i(U.nearest, t->nearest ? k : 0);
#endif
                glUniform1i(U.mip_count, t->mip_count);
                glUniform1i(U.minify, t->minify);
            }
            if (state_changed) {
                if (s->cull == 0)
                    glDisable(GL_CULL_FACE);
                else {
                    glEnable(GL_CULL_FACE);
                    glCullFace(s->cull == 1 ? GL_BACK : GL_FRONT);
                }
                glUniform1i(U.atest, s->alpha_test);
                glUniform1i(U.afunc, s->alpha_func);
                glUniform1f(U.aref, s->alpha_ref);
                glUniform1i(U.date, s->destination_test);
                glUniform1i(U.date_bit, s->destination_bit);
            }
            if (s->destination_test) {
                /* 00455708, commands 29/31: DATE reads the preceding draws'
               framebuffer alpha. The PC MSAA target must be resolved first:
               glCopyTexSubImage2D rejects a multisampled read framebuffer,
               leaving stale masks (squares above the Ka's rear lamps).
               Blit to the resident single-sample mask; no CPU readback. */
                GLint read_fb, draw_fb;
                /* DATE temporarily changes these bindings and restores them
                   below. Query the actual nested target once per submission,
                   not twice for every destination-tested material. */
                if (!cache_textures || !date_framebuffer_known) {
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &date_read_fb);
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &date_draw_fb);
                    date_framebuffer_known = 1;
                }
                read_fb = date_read_fb;
                draw_fb = date_draw_fb;

                if (!dest_fbo)
                    glGenFramebuffers(1, &dest_fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest_fbo);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       dest_tex, 0);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)draw_fb);
                int rect[4];
                date_rect(view, g, skinned, rect);
                glBlitFramebuffer(rect[0], rect[1], rect[2], rect[3], rect[0], rect[1], rect[2],
                                  rect[3], GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)read_fb);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)draw_fb);
            }
            if (view->ghost == 1 && g->blend_enabled)
                continue;
            if (state_changed) {
                set_blend(g);
                /* pass 1: fragments passing the alpha test (all of them without a test) */
                glUniform1i(U.pass, 1);
                set_depth(s, s->depth_write);
                set_masks(s->color_mask, 1, 1);
                /* the ghost's own see-through layers: their share of the colour
           (by the source alpha) scaled with it */
                glUniform1f(U.fade, view->ghost == 2 && g->blend_enabled ? view->ghost_alpha : 1.f);
            }
            if (view->ghost == 1)
                set_masks(0, 0, 0);
            else if (view->ghost == 2 && !g->blend_enabled) {
                glEnable(GL_BLEND);
                glBlendColor(0, 0, 0, view->ghost_alpha);
                glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
                glBlendFuncSeparate(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_ONE,
                                    GL_ZERO);
                glDepthFunc(GL_DEPTH_FUNC_LEQUAL);
                glDepthMask(GL_FALSE);
            }
            glDrawArrays(GL_TRIANGLES, g->first, vertices);
            frame_stats.draws++;
            frame_stats.triangles += vertices / 3;
            if (view->ghost)
                continue;
            /* pass 2: failing fragments, written as the fail action says */
            if (s->alpha_test && s->alpha_fail != 0) {
                int color = s->alpha_fail != 2, alpha = s->alpha_fail == 1,
                    depth = s->depth_write && s->alpha_fail == 2;
                glUniform1i(U.pass, 2);
                set_depth(s, depth);
                set_masks(s->color_mask, color, alpha);
                glDrawArrays(GL_TRIANGLES, g->first, vertices);
                frame_stats.draws++;
                frame_stats.triangles += vertices / 3;
                /* Fail actions changed pass, depth and channel masks. */
                last_draw_state = NULL;
            }
        }
    }
    glActiveTexture(GL_TEXTURE0);
    SWP_POP();
}

void r3d_draw(r3d_mesh *mesh, const r3d_view *view)
{
    draw_mesh(mesh, view, NULL);
}
int r3d_mesh_update_positions(r3d_mesh *m, const float (*p)[3], unsigned count, float alpha)
{
    if (!m || !p || m->skinned || m->morph || m->ngroups != 1 || count > 276 ||
        count != (unsigned)m->vertices)
        return 0;
    float stream[276 * VFLOATS];
    memset(stream, 0, sizeof stream);
    r3d_group *g = m->groups;
    for (unsigned j = 0; j < 3; j++) {
        g->lo[j] = INFINITY;
        g->hi[j] = -INFINITY;
    }
    for (unsigned i = 0; i < count; i++) {
        float *v = stream + i * VFLOATS;
        for (unsigned j = 0; j < 3; j++) {
            if (!isfinite(p[i][j]))
                return 0;
            v[j] = p[i][j];
            g->lo[j] = fminf(g->lo[j], v[j]);
            g->hi[j] = fmaxf(g->hi[j], v[j]);
        }
        v[4] = 1;
        v[18] = alpha;
    }
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(count * VFLOATS * sizeof(float)), stream);
    return 1;
}
int r3d_mesh_update_colours(r3d_mesh *mesh, const gt4_model *model)
{
    if (!mesh || !model || mesh->skinned || mesh->morph || !mesh->ngroups ||
        (size_t)model->count * 3 != (size_t)mesh->vertices)
        return 0;
    size_t bytes = (size_t)mesh->vertices * VFLOATS * sizeof(float);
    static float *stream;
    static size_t capacity;
    if (bytes > capacity) {
        void *p = realloc(stream, bytes);
        if (!p)
            return 0;
        stream = p;
        capacity = bytes;
    }
    for (int i = 0; i < model->count; i++) {
        const gt4_triangle *t = &model->triangles[i];
        float e1[3], e2[3], face[3];
        for (int k = 0; k < 3; k++) {
            e1[k] = t->v[1].p[k] - t->v[0].p[k];
            e2[k] = t->v[2].p[k] - t->v[0].p[k];
        }
        face[0] = e1[1] * e2[2] - e1[2] * e2[1];
        face[1] = e1[2] * e2[0] - e1[0] * e2[2];
        face[2] = e1[0] * e2[1] - e1[1] * e2[0];
        for (int j = 0; j < 3; j++) {
            const gt4_vertex *x = &t->v[j];
            float *o = stream + ((size_t)i * 3 + j) * VFLOATS;
            const float *n = x->normal;
            float nl = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (nl < 1e-6f)
                n = face;
            memcpy(o, x->p, 12);
            memcpy(o + 3, n, 12);
            memcpy(o + 6, x->uv, 8);
            memcpy(o + 8, x->rgb, 12);
            memcpy(o + 11, x->ambient, 12);
            memcpy(o + 14, x->specular, 12);
            o[17] = x->shine;
            o[18] = x->alpha;
        }
    }
    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, stream);
    return 1;
}

void r3d_draw_ui(r3d_mesh *mesh, const r3d_view *view, const r3d_ui_light *light)
{
    r3d_lights one = {1};
    memcpy(one.direction[0], light->direction, sizeof one.direction[0]);
    memcpy(one.ambient[0], light->ambient, sizeof one.ambient[0]);
    memcpy(one.diffuse[0], light->diffuse, sizeof one.diffuse[0]);
    memcpy(one.specular[0], light->specular, sizeof one.specular[0]);
    draw_mesh(mesh, view, &one);
}
void r3d_draw_lit(r3d_mesh *mesh, const r3d_view *view, const r3d_lights *lights)
{
    draw_mesh(mesh, view, lights);
}

/* Persistent half-resolution silhouette and two separable blur targets.
   No triangle/pixel loops or readback on the CPU. The five-tap Gaussian is
   a native approximation to the original six jittered light projections. */
int r3d_floor_shadow(r3d_mesh *mesh, const r3d_view *v, const float light[3], float darkness,
                     float blur)
{
    static GLuint cast, filter, fbo[2], tex[2], quad;
    static int failed_shadow;
    static struct {
        GLint rot, pos, cam, r, u, f, L, focal, image, step, darkness;
    } S;
    if (failed_shadow || !mesh || darkness <= 0 || light[1] <= .1f)
        return 0;
    if (!cast) {
        cast = gl_program(
            "#version 330 core\nlayout(location=0) in vec3 p; uniform mat3 rot; uniform vec3 pos,cam,r,u,f,L; uniform vec2 focal;\n"
            "void main(){vec3 w=rot*p+pos; float den=L.y-w.y; w=L+(w-L)*(L.y/max(.01,den)); w.y=0.; w-=cam; float z=dot(w,f);\n"
            "gl_Position=vec4(dot(w,r)*focal.x,dot(w,u)*focal.y,z-0.4,z); if(den<=.01)gl_Position=vec4(2.,2.,2.,1.);}\n",
            "#version 330 core\nout vec4 o; void main(){o=vec4(0.,0.,0.,1.);}\n");
        filter = gl_program(
            "#version 330 core\nout vec2 uv; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.-1.,0.,1.);}\n",
            "#version 330 core\nin vec2 uv; out vec4 o; uniform sampler2D image; uniform vec2 step; uniform float darkness;\n"
            "void main(){float a=texture(image,uv).a*.227027; a+=(texture(image,uv+step*1.384615).a+texture(image,uv-step*1.384615).a)*.316216;\n"
            "a+=(texture(image,uv+step*3.230769).a+texture(image,uv-step*3.230769).a)*.070270; o=vec4(0.,0.,0.,a*darkness);}\n");
        if (!cast || !filter) {
            if (cast)
                glDeleteProgram(cast);
            if (filter)
                glDeleteProgram(filter);
            cast = filter = 0;
            failed_shadow = 1;
            return 0;
        }
#define SU(field, name) S.field = glGetUniformLocation(cast, name)
        SU(rot, "rot");
        SU(pos, "pos");
        SU(cam, "cam");
        SU(r, "r");
        SU(u, "u");
        SU(f, "f");
        SU(L, "L");
        SU(focal, "focal");
#undef SU
        S.image = glGetUniformLocation(filter, "image");
        S.step = glGetUniformLocation(filter, "step");
        S.darkness = glGetUniformLocation(filter, "darkness");
        glGenFramebuffers(2, fbo);
        glGenTextures(2, tex);
        glGenVertexArrays(1, &quad);
        GLint saved;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved);
        glActiveTexture(GL_TEXTURE0);
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, tex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 320, 224, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                failed_shadow = 1;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, saved);
        if (failed_shadow)
            return 0;
    }
    GLint target;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, 320, 224);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(cast);
    glUniformMatrix3fv(S.rot, 1, GL_TRUE, v->rot);
#define SHV(field, values) glUniform3fv(S.field, 1, values)
    SHV(pos, v->pos);
    SHV(cam, v->cam);
    SHV(r, v->r);
    SHV(u, v->u);
    SHV(f, v->f);
    SHV(L, light);
#undef SHV
    float fy = v->focal_y > 0 ? v->focal_y : v->focal;
    glUniform2f(S.focal, v->focal / 320 * projection_x, fy / 224);
    glBindVertexArray(mesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertices);
    glUseProgram(filter);
    glBindVertexArray(quad);
    glUniform1i(S.image, 0);
    glUniform1f(S.darkness, 1);
    float radius = fmaxf(0, blur * .25f); /* same native half-resolution blur scale */
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glUniform2f(S.step, radius / 320, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp[0], vp[1], vp[2], vp[3]);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glUniform2f(S.step, 0, radius / 224);
    glUniform1f(S.darkness, fminf(1, darkness));
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    return 1;
}

void r3d_clear_alpha(float alpha)
{
    if (!r3d_available())
        return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp[0], vp[1], vp[2], vp[3]);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0, 0, 0, alpha);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

int r3d_begin_target_view(int size)
{
    if (!r3d_begin_target(size))
        return 0;
    projection_x = target_saved_projection;
    return 1;
}

void r3d_end_target_add(const float rgb[3], int blur)
{
    r3d_end_target_add_road(rgb, blur, NULL);
}

void r3d_end_target_add_road(const float rgb[3], int blur, const r3d_view *road)
{
    static GLuint copy, quad, other_fbo, other_tex;
    static int other_size, failed_copy;
    static struct {
        GLint image, shift, range, tint, linear_output;
    } C;
    int size = target_size;
    GLuint picture = target_color;
    if (!size)
        return;
    r3d_end();
    if (!copy && !failed_copy) {
        copy = gl_program(
            "#version 330 core\nout vec2 uv; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.-1.,0.,1.);}\n",
            /* uv: 0..1 across the drawn rectangle, v up; the GS's rows go
               down, so its "first texels" are the picture's top */
            "#version 330 core\nin vec2 uv; out vec4 o; uniform sampler2D image; uniform vec2 shift; uniform float range; uniform vec3 tint;uniform int linear_output;\n"
            "void main(){vec2 t=vec2(uv.x*range,1.-(1.-uv.y)*range)+shift; vec3 c=texture(image,t).rgb*tint;if(linear_output==1){c=clamp(c,0.,1.);c=mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}o=vec4(c,1.);}\n");
        if (!copy)
            failed_copy = 1;
        else {
            C.image = glGetUniformLocation(copy, "image");
            C.shift = glGetUniformLocation(copy, "shift");
            C.range = glGetUniformLocation(copy, "range");
            C.tint = glGetUniformLocation(copy, "tint");
            C.linear_output = glGetUniformLocation(copy,"linear_output");
            glGenVertexArrays(1, &quad);
        }
    }
    if (copy) {
        glUseProgram(copy);
        glBindVertexArray(quad);
        glUniform1i(C.image, 0);
        glUniform1i(C.linear_output,0);
        glActiveTexture(GL_TEXTURE0);
        glDisable(GL_BLEND);
        glViewport(0, 0, size, size);
        if (blur > 0) {
            if (!other_fbo) {
                glGenFramebuffers(1, &other_fbo);
                glGenTextures(1, &other_tex);
            }
            if (other_size != size) {
                glBindTexture(GL_TEXTURE_2D, other_tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             NULL);
                other_size = size;
            }
        }
        for (int i = 0; i < blur; i++) {
            /* a sprite with its texture half a texel on: each pixel takes
               the mean of the four texels at and after it (across, down) */
            GLuint from = i & 1 ? other_tex : target_color, to = i & 1 ? target_color : other_tex;
            glBindFramebuffer(GL_FRAMEBUFFER, i & 1 ? target_fbo : other_fbo);
            if (!(i & 1))
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, to, 0);
            glBindTexture(GL_TEXTURE_2D, from);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glUniform2f(C.shift, .5f / size, -.5f / size);
            glUniform1f(C.range, 1);
            glUniform3f(C.tint, 1, 1, 1);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            picture = to;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, target_saved_fb);
    memcpy(vp, target_saved_vp, sizeof vp);
    projection_x = target_saved_projection;
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(vp[0], vp[1], vp[2], vp[3]);
    if (copy) {
        glBindTexture(GL_TEXTURE_2D, picture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glUniform2f(C.shift, 0, 0);
        glUniform1f(C.range, (size - (blur - .5f)) / size);
        glUniform3f(C.tint, rgb[0], rgb[1], rgb[2]);
#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
        /* Original: func_003411D0 +0x0 (retained encoded GS sheen pass).
           P: its encoded LDR result must enter the float scene as linear light. */
        glUniform1i(C.linear_output,video_hdr_active());
#endif
        /* 003411D0's destination-alpha mask covers multiple materials.
           Preserve its authored strength: world-up is not roughness metadata. */
        (void)road;
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_DST_ALPHA, GL_ONE, GL_ONE, GL_ZERO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_BLEND);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, target_color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindVertexArray(0);
}

struct r3d_sprite_texture {
    GLuint id;
};

r3d_sprite_texture *r3d_sprite_texture_create(const tex1_image *image)
{
    r3d_sprite_texture *t;
    if (!image || !image->rgba || !r3d_available())
        return NULL;
    t = calloc(1, sizeof *t);
    if (!t)
        return NULL;
    /* the GS's alpha (128 = 1) kept as it is: the shader scales it */
    t->id = upload(image->width, image->height, image->rgba, image->native_alpha);
    if (!t->id) {
        free(t);
        return NULL;
    }
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, image->magnify ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, image->minify ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void r3d_sprite_texture_destroy(r3d_sprite_texture *t)
{
    if (!t)
        return;
    glDeleteTextures(1, &t->id);
    free(t);
}

void r3d_draw_sprites(const r3d_sprite_texture *t, const r3d_view *view, const float *corners,
                      int squares, int mode)
{
    static GLuint prog, vao, vbo;
    static GLint u_focal, u_clip, u_image, u_test;
    static int failed_sprites;
    static const int order[6] = {0, 1, 2, 1, 3, 2};
    float *buf;
    if (!t || !view || !corners || squares <= 0 || failed_sprites)
        return;
    if (!prog) {
        prog = gl_program(
            "#version 330 core\nlayout(location=0) in vec3 p; layout(location=1) in vec2 uv; layout(location=2) in vec4 c;\n"
            "uniform vec2 focal, clip; out vec2 v_uv; out vec4 v_c;\n"
            "void main(){float z=-p.z; v_uv=uv; v_c=c;\n"
            "gl_Position=vec4(p.x*focal.x,p.y*focal.y,z*clip.x+clip.y,z);}\n",
            /* the texel's alpha is the GS's (128 = 1), stored as 0..255 */
            "#version 330 core\nin vec2 v_uv; in vec4 v_c; out vec4 o; uniform sampler2D image; uniform int test;\n"
            "void main(){vec4 t=texture(image,v_uv); if(test==1 && t.a<=0.) discard; o=vec4(t.rgb*v_c.rgb,t.a*255./128.*v_c.a);}\n");
        if (!prog) {
            failed_sprites = 1;
            return;
        }
        u_focal = glGetUniformLocation(prog, "focal");
        u_clip = glGetUniformLocation(prog, "clip");
        u_image = glGetUniformLocation(prog, "image");
        u_test = glGetUniformLocation(prog, "test");
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                              (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float),
                              (void *)(5 * sizeof(float)));
    }
    buf = malloc((size_t)squares * 6 * 9 * sizeof(float));
    if (!buf)
        return;
    for (int q = 0; q < squares; q++)
        for (int k = 0; k < 6; k++)
            memcpy(buf + ((size_t)q * 6 + k) * 9, corners + ((size_t)q * 4 + order[k]) * 9,
                   9 * sizeof(float));
    glUseProgram(prog);
    {
        float fy = view->focal_y > 0 ? view->focal_y : view->focal;
        glUniform2f(u_focal, view->focal / 320 * projection_x, fy / 224);
        {
            float k[2];
            depth_coeffs((view->near_clip > 0 ? view->near_clip : .2f) + view->depth_bias,
                         (view->far_clip > view->near_clip ? view->far_clip : 20000) + view->depth_bias, k);
            glUniform2f(u_clip, k[0], k[1]);
        }
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glUniform1i(u_image, 0);
    glUniform1i(u_test, mode == 2);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc(GL_DEPTH_FUNC_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    if (mode == 2)
        glDisable(GL_BLEND);
    else {
        glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, mode == 1 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                            GL_ONE);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)squares * 6 * 9 * sizeof(float)), buf,
                 GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, squares * 6);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    free(buf);
    glUseProgram(program);
}

#ifndef MOON_DEKO3D
int r3d_warmup(void)
{
    static int ready;
    if (ready)
        return 1;
    if (!r3d_available())
        return 0;
    /* Exercise actual draw paths on a tiny temporary target, so Mesa also
       sees their vertex/blend variants. The next loading frame replaces
       the destination; no readback or GPU completion is requested. */
    r3d_begin(0, 0, 640, 448, NULL);
    if (!r3d_begin_target(16))
        return 0;
    int ok = r3d_begin_layer(0);
    if (ok) {
        r3d_end_layer_sample(1, 0, 0, 2);
        ok = r3d_begin_layer(0);
        r3d_end_layer_sample(1, 0, 1, 2);
    }
    uint8_t rgba[16 * 16 * 4] = {0};
    r3d_texture wheel = {0};
    wheel.width = wheel.height = wheel.tex_w = wheel.tex_h = 16;
    wheel.id = upload(16, 16, rgba, NULL);
    ok = wheel_accumulate(&wheel, 2, .1f) && ok;
    glDeleteTextures(1, &wheel.id);
    glDeleteTextures(2, wheel.wheel_tex);
    glDeleteFramebuffers(2, wheel.wheel_fbo);
    gt4_triangle triangle = {0};
    triangle.texture = -1;
    for (int i = 0; i < 3; i++)
        triangle.v[i].p[2] = 4;
    gt4_model model = {0};
    model.triangles = &triangle;
    model.count = 1;
    r3d_mesh *mesh = r3d_mesh_create(&model);
    r3d_view view = {0};
    view.rot[0] = view.rot[4] = view.rot[8] = view.r[0] = view.u[1] = view.f[2] = 1;
    view.focal = 320;
    r3d_draw(mesh, &view);
    const float light[3] = {0, 4, 0}, black[3] = {0, 0, 0};
    ok = r3d_floor_shadow(mesh, &view, light, 1, 1) && ok;
    r3d_mesh_destroy(mesh);
    tex1_image image = {0};
    image.width = image.height = image.tex_w = image.tex_h = 16;
    image.rgba = rgba;
    r3d_sprite_texture *sprite = r3d_sprite_texture_create(&image);
    float corners[4 * 9] = {0};
    for (int i = 0; i < 4; i++)
        corners[i * 9 + 2] = -2;
    r3d_draw_sprites(sprite, &view, corners, 1, 0);
    ok = sprite != NULL && ok;
    r3d_sprite_texture_destroy(sprite);
    r3d_end_target_add(black, 1);
    r3d_end();
    plat_texture *picture = plat_texture_create(16, 16, rgba);
    if (picture) {
        uint8_t lut[256][3], mix[3][3] = {{128, 0, 0}, {0, 128, 0}, {0, 0, 128}};
        for (int i = 0; i < 256; i++)
            lut[i][0] = lut[i][1] = lut[i][2] = (uint8_t)i;
        video_license_image(picture, 0, 0, 1, 1, lut, mix);
        plat_texture_destroy(picture);
    }
    ready = ok && picture != NULL && glGetError() == GL_NO_ERROR;
    return ready;
}
#endif

#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
#include "render3d_car_shadow.inc"
#endif

#if !defined(__SWITCH__) && !defined(MOON_SWITCH_RENDER_TEST) && !defined(__ANDROID__)
#include "render3d_shadow_map.inc"
#include "render3d_lamp_shadow.inc"
#endif
