/* P: linear RGBA16F scene -> SDR filmic resolve. UI is composed afterward.
   Screen-space single-scattering shafts use the resolved depth as an occlusion
   mask, not scene brightness. No CPU readback or hidden frame timer. */
static struct {
    int enabled, rays, allocated_hdr, width, height, ambient_occlusion;
    video_light_volume world;
    GLuint program, vao, fbo, color, depth, depth_fbo;
    GLint image, z, extent, sun, sun_color, reverse;
    GLint world_on, ao_on, volume_on, light_on, camera, right_axis, up_axis, forward_axis,
        light_direction, focal, clip, light_projection, light_depth;
    float solar[3], color_value[3];
    struct {
        GLuint program, vao, fbo, color;
        GLint image, extent;
        int width, height;
    } aa;
} daylight_post;
void video_hdr_options(int enabled, int rays)
{
    daylight_post.enabled = enabled != 0;
    if (!enabled)
        daylight_post.solar[2] = 0;
    daylight_post.rays = rays >= 0 && rays <= 2 ? rays : 0;
}
void video_light_volume_view(const video_light_volume *view)
{
    daylight_post.world = *view;
}
void video_ambient_occlusion(int enabled)
{
    daylight_post.ambient_occlusion = enabled != 0;
}
int video_hdr_active(void)
{
    if (!scene_active || !daylight_post.allocated_hdr)
        return 0;
    GLint target = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
    /* Reflection maps and road-sheen targets retain their encoded LDR format.
       Only the actual float scene accepts linear radiance. */
    return (GLuint)target == scene_fbo;
}
void video_solar_projection(float x, float y, float visible, const float color[3])
{
    daylight_post.solar[0] = x;
    daylight_post.solar[1] = y;
    daylight_post.solar[2] = visible;
    memcpy(daylight_post.color_value, color, 12);
}
static GLuint daylight_post_resolve(GLuint source, GLuint scene, int width, int height)
{
    if (!daylight_post.program) {
        daylight_post.program = gl_program(
            "#version 330 core\nconst vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));out vec2 uv;void main(){gl_Position=vec4(p[gl_VertexID],0,1);uv=p[gl_VertexID]*.5+.5;}",
            "#version 330 core\nin vec2 uv;out vec4 o;uniform sampler2D image,z;uniform vec2 extent;uniform vec3 sun,sun_color;uniform int reverse;"
            "float openSky(vec2 at){if(any(lessThan(at,vec2(0)))||any(greaterThan(at,vec2(1))))return 0.;float d=texture(z,at).r;return reverse==1?step(d,1e-7):step(.999999,d);}"
#include "video_volume_shader.inc"
            "void main(){vec3 c=texture(image,uv).rgb,bloom=vec3(0);for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){vec3 b=texture(image,uv+vec2(x,y)*3./extent).rgb;bloom+=max(b-1.,vec3(0.))/25.;}c+=bloom*.08;if(world_on==1 && ao_on==1)c*=ambientVisibility();if(world_on==1 && volume_on==1)c=c*exp(-min(sceneDistance(uv)*length(cameraRay(uv)),80.)*.0015)+worldScatter();"
            "if(sun.z>0. && volume_on==0){vec2 delta=sun.xy-uv;float shaft=0.;for(int i=0;i<48;i++){float t=(float(i)+.5)/48.;vec2 at=uv+delta*t;float angular=exp(-length((at-sun.xy)*vec2(extent.x/extent.y,1.) )*5.);shaft+=openSky(at)*angular*(1.-t*.4)/48.;}c+=sun_color*shaft*.3*sun.z;}"
            "c=max(c,vec3(0));c=clamp((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14),0.,1.);c=mix(c*12.92,1.055*pow(c,vec3(1./2.4))-.055,step(vec3(.0031308),c));o=vec4(c,1);}");
        if (!daylight_post.program)
            return source;
        glGenVertexArrays(1, &daylight_post.vao);
        glGenFramebuffers(1, &daylight_post.fbo);
        glGenFramebuffers(1, &daylight_post.depth_fbo);
        glGenTextures(1, &daylight_post.color);
        glGenTextures(1, &daylight_post.depth);
#define LOC(field, name) daylight_post.field = glGetUniformLocation(daylight_post.program, name)
        LOC(image, "image");
        LOC(z, "z");
        LOC(extent, "extent");
        LOC(sun, "sun");
        LOC(sun_color, "sun_color");
        LOC(reverse, "reverse");
        LOC(world_on, "world_on");
        LOC(ao_on, "ao_on");
        LOC(volume_on, "volume_on");
        LOC(light_on, "light_on");
        LOC(camera, "camera");
        LOC(right_axis, "right_axis");
        LOC(up_axis, "up_axis");
        LOC(forward_axis, "forward_axis");
        LOC(light_direction, "light_direction");
        LOC(focal, "focal");
        LOC(clip, "clip");
        LOC(light_projection, "light_projection");
        LOC(light_depth, "light_depth");
#undef LOC
    }
    glActiveTexture(GL_TEXTURE0);
    if (width != daylight_post.width || height != daylight_post.height) {
        glBindTexture(GL_TEXTURE_2D, daylight_post.color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, daylight_post.depth);
        /* The depth blit requires identical source/destination formats.
           Scene depth includes stencil in both depth conventions. OpenGL 4.6
           section 18.3.1 rejects copying it to a depth-only texture. */
        glTexImage2D(GL_TEXTURE_2D, 0, gl_reversed_z ? 0x8CAD : GL_DEPTH24_STENCIL8,
                     width, height, 0, GL_DEPTH_STENCIL,
                     gl_reversed_z ? 0x8DAD : 0x84FA, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        daylight_post.width = width;
        daylight_post.height = height;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, daylight_post.depth_fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                           daylight_post.depth, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           daylight_post.color, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, scene);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, daylight_post.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, daylight_post.color,
                           0);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(1, 1, 1, 1);
    glUseProgram(daylight_post.program);
    glBindVertexArray(daylight_post.vao);
    glUniform1i(daylight_post.image, 0);
    glUniform1i(daylight_post.z, 1);
    glUniform2f(daylight_post.extent, width, height);
    glUniform1i(daylight_post.reverse, gl_reversed_z != 0);
    glUniform3f(daylight_post.sun, daylight_post.solar[0], daylight_post.solar[1],
                daylight_post.rays ? daylight_post.solar[2] : 0);
    glUniform3fv(daylight_post.sun_color, 1, daylight_post.color_value);
    const video_light_volume *view = &daylight_post.world;
    glUniform1i(daylight_post.world_on, view->valid);
    glUniform1i(daylight_post.ao_on, daylight_post.ambient_occlusion);
    glUniform1i(daylight_post.volume_on, daylight_post.rays == 2 && view->valid);
    glUniform1i(daylight_post.light_on, view->shadow != 0);
    glUniform3fv(daylight_post.camera, 1, view->camera);
    glUniform3fv(daylight_post.right_axis, 1, view->right);
    glUniform3fv(daylight_post.up_axis, 1, view->up);
    glUniform3fv(daylight_post.forward_axis, 1, view->forward);
    glUniform3fv(daylight_post.light_direction, 1, view->sun);
    glUniform2fv(daylight_post.focal, 1, view->focal);
    glUniform2fv(daylight_post.clip, 1, view->clip);
    glUniformMatrix4fv(daylight_post.light_projection, 1, GL_FALSE, view->shadow_projection);
    glUniform1i(daylight_post.light_depth, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, view->shadow);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, daylight_post.depth);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
    return daylight_post.color;
}

static void daylight_post_destroy(void)
{
    if (daylight_post.program)
        glDeleteProgram(daylight_post.program);
    glDeleteTextures(1, &daylight_post.color);
    glDeleteTextures(1, &daylight_post.depth);
    glDeleteFramebuffers(1, &daylight_post.fbo);
    glDeleteFramebuffers(1, &daylight_post.depth_fbo);
    glDeleteVertexArrays(1, &daylight_post.vao);
    memset(&daylight_post, 0, sizeof daylight_post);
}
