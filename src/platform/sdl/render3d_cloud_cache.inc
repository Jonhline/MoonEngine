/* P: camera-independent hemispheric cloud radiance/transmittance and a nearby
   ground shadow field. Same volume integrator as the reference path, evaluated
   once for a scene clock/light instead of at every covered scene pixel. */
static struct {
    GLuint program, vao, fbo, volume, shadow, sky_image;
    GLint sun, direct, sky, time, night, kind, origin;
    float key[12];
    int valid;
} cloud_cache;
static int cloud_cache_prepare(const r3d_view *view)
{
    if (!night_sky_enabled || !view->daylight ||
        (getenv("MOON_CLOUD_REFERENCE") && *getenv("MOON_CLOUD_REFERENCE") == '1'))
        return 0;
    float key[12];
    memcpy(key, view->sun_direction, 12);
    memcpy(key + 3, view->sun_color, 12);
    memcpy(key + 6, view->sky_color, 12);
    /* 7 m/s cloud drift moves less than half a metre per 15 Hz update.
       Quantize the deterministic frame clock; pause/replay never uses wall time. */
    key[9] = floorf(view->sky_seconds * 15.f) / 15.f;
    key[10] = floorf(view->cam[0] / 2048.f) * 2048.f;
    key[11] = floorf(view->cam[2] / 2048.f) * 2048.f;
    if (cloud_cache.valid && !memcmp(key, cloud_cache.key, sizeof key))
        return 1;
    if (!cloud_cache.program) {
        cloud_cache.program = gl_program(
            "#version 330 core\nconst vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));out vec2 uv;void main(){gl_Position=vec4(p[gl_VertexID],0,1);uv=p[gl_VertexID]*.5+.5;}",
            "#version 330 core\nin vec2 uv;out vec4 o;uniform vec3 u_sun_direction,u_sun_color,u_sky_color;uniform int kind;uniform vec2 origin;uniform sampler2D baked_cloud;"
#define MOON_CLOUD_BAKE
#include "render3d_atmosphere_shader.inc"
#undef MOON_CLOUD_BAKE
            "void main(){if(kind==1){o=vec4(cloudSunReference(vec3(origin.x+(uv.x-.5)*8192.,0.,origin.y+(uv.y-.5)*8192.)));return;}float y=uv.y*uv.y,a=(uv.x-.5)*6.28318530718,r=sqrt(max(0.,1.-y*y));vec3 ray=vec3(cos(a)*r,y,sin(a)*r);if(kind==2){vec4 c=texture(baked_cloud,uv);o=vec4(clearAtmosphereSky(ray)*c.a+c.rgb,1.);}else o=cloudVolume(ray);}");
        if (!cloud_cache.program)
            return 0;
        glGenVertexArrays(1, &cloud_cache.vao);
        glGenFramebuffers(1, &cloud_cache.fbo);
        glGenTextures(1, &cloud_cache.volume);
        glGenTextures(1, &cloud_cache.shadow);
        glGenTextures(1, &cloud_cache.sky_image);
#define CL(field, name) cloud_cache.field = glGetUniformLocation(cloud_cache.program, name)
        CL(sun, "u_sun_direction");
        CL(direct, "u_sun_color");
        CL(sky, "u_sky_color");
        CL(time, "u_sky_seconds");
        CL(night, "u_night_sky");
        CL(kind, "kind");
        CL(origin, "origin");
#undef CL
        glActiveTexture(GL_TEXTURE0 + 5);
        for (int i = 0; i < 3; i++) {
            glBindTexture(GL_TEXTURE_2D, i == 1 ? cloud_cache.shadow : i == 2 ? cloud_cache.sky_image : cloud_cache.volume);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, i == 1 ? 128 : 256, 128, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, i == 1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        /* Units 5/6 hold sky-or-volume radiance and ground shadow. */
        glActiveTexture(GL_TEXTURE0 + 5);
        glBindTexture(GL_TEXTURE_2D, cloud_cache.volume);
        glActiveTexture(GL_TEXTURE0 + 6);
        glBindTexture(GL_TEXTURE_2D, cloud_cache.shadow);
        glActiveTexture(GL_TEXTURE0);
    }
    GLint draw, read, viewport[4], active_program, vao;
    GLboolean mask[4];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &active_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    int depth = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND),
        cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, cloud_cache.fbo);
    glUseProgram(cloud_cache.program);
    glBindVertexArray(cloud_cache.vao);
    glUniform3fv(cloud_cache.sun, 1, key);
    glUniform3fv(cloud_cache.direct, 1, key + 3);
    glUniform3fv(cloud_cache.sky, 1, key + 6);
    glUniform1f(cloud_cache.time, key[9]);
    glUniform1i(cloud_cache.night, 1);
    glUniform2fv(cloud_cache.origin, 1, key + 10);
    int first = cloud_cache.valid && !memcmp(key, cloud_cache.key, 10 * sizeof(float)) ? 1 : 0;
    glUniform1i(glGetUniformLocation(cloud_cache.program,"baked_cloud"),5);
    for (int i = first; i < 3; i++) {
        if (first && i == 2) continue;
        glActiveTexture(GL_TEXTURE0+5);
        glBindTexture(GL_TEXTURE_2D,i==0?cloud_cache.shadow:cloud_cache.volume);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               i == 1 ? cloud_cache.shadow : i == 2 ? cloud_cache.sky_image : cloud_cache.volume, 0);
        glViewport(0, 0, i == 1 ? 128 : 256, 128);
        glUniform1i(cloud_cache.kind, i);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glActiveTexture(GL_TEXTURE0+5);
    glBindTexture(GL_TEXTURE_2D,cloud_cache.volume);
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depth)
        glEnable(GL_DEPTH_TEST);
    if (blend)
        glEnable(GL_BLEND);
    if (cull)
        glEnable(GL_CULL_FACE);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
    glColorMask(mask[0], mask[1], mask[2], mask[3]);
    glUseProgram(active_program);
    glBindVertexArray(vao);
    memcpy(cloud_cache.key, key, sizeof key);
    cloud_cache.valid = 1;
    return 1;
}
static void cloud_cache_bind(int enabled, const r3d_view *view)
{
    glUniform1i(U.cloud_cache, enabled);
    if (!enabled)
        return;
    glUniform1i(U.cloud_volume, 5);
    glUniform1i(U.cloud_shadow, 6);
    /* Reuse the volume sampler for the sky draw; surface draws still read
       volume/transmittance. Retain the eight-sampler Vulkan contract. */
    glActiveTexture(GL_TEXTURE0+5);
    glBindTexture(GL_TEXTURE_2D,view->daylight>=2 && view->sun_direction[1]>-.12f
                                  ? cloud_cache.sky_image : cloud_cache.volume);
    glActiveTexture(GL_TEXTURE0);
    glUniform2fv(U.cloud_origin, 1, cloud_cache.key + 10);
}
