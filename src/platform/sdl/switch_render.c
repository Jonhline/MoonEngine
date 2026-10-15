/* P: retain the native port's sphere-map/composition formulas on the GPU.
   Mirrors course_reflection (0044E528/0044EEA8 context; the six-face remap is
   the port's adaptation), compose_studio (001D5C00) and compose_env (001D5B08).
   No framebuffer readback, synchronization or per-frame texture allocation. */
#include "switch_render.h"
#include "render3d.h"
#include "video_gl.h"
#include "gl.h"
#include "platform/switch_profile.h"
#include <stdlib.h>

struct switch_gpu_image {
    plat_texture *image;
    GLuint fbo, source[4], atlas_fbo;
    int width, height, face;
    GLuint restore;
};
static GLuint mix_program, env_program, vao;
static int failed;
static GLint mix_weights, env_right, env_up, env_forward, env_face, env_size;
static const char *vertex = "#version 330 core\n"
                            "void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);"
                            "gl_Position=vec4(p*2.-1.,0.,1.);}\n";
static const char *mix_fragment = "#version 330 core\n"
                                  "uniform sampler2D a,b,c,d; uniform vec4 weights; out vec4 color;"
                                  "void main(){ivec2 p=ivec2(gl_FragCoord.xy); vec3 v=vec3(0.);"
                                  "if(weights.x>0.)v+=texelFetch(a,p,0).rgb*255.*weights.x;"
                                  "if(weights.y>0.)v+=texelFetch(b,p,0).rgb*255.*weights.y;"
                                  "if(weights.z>0.)v+=texelFetch(c,p,0).rgb*255.*weights.z;"
                                  "if(weights.w>0.)v+=texelFetch(d,p,0).rgb*255.*weights.w;"
                                  "color=vec4(floor(clamp(v,0.,255.))/255.,1.);}\n";
static const char *env_fragment =
    "#version 330 core\n"
    "uniform sampler2D atlas; uniform vec3 right,up,forward; uniform int face_size,map_size;"
    "out vec4 color;"
    "const vec3 F[6]=vec3[6](vec3(1,0,0),vec3(-1,0,0),vec3(0,0,1),vec3(0,0,-1),vec3(0,1,0),vec3(0,-1,0));"
    "const vec3 U[6]=vec3[6](vec3(0,1,0),vec3(0,1,0),vec3(0,1,0),vec3(0,1,0),vec3(0,0,-1),vec3(0,0,1));"
    "vec3 get(int face,int x,int y){return texelFetch(atlas,ivec2(face*face_size+x,face_size-1-y),0).rgb*255.;}"
    "void main(){vec2 p=gl_FragCoord.xy; float mx=(p.x/float(map_size)-.5)*2.;"
    "float my=-(p.y/float(map_size)-.5)*2.; float m2=mx*mx+my*my;"
    "if(m2>1.){float k=1./sqrt(m2);mx*=k;my*=k;m2=1.;}"
    "float nz=sqrt(1.-m2);vec3 R=vec3(2.*nz*mx,2.*nz*my,2.*nz*nz-1.);"
    "vec3 D=R.x*right+R.y*up-R.z*forward;int best=0;float big=-1.;"
    "for(int i=0;i<6;i++){float q=dot(D,F[i]);if(q>big){big=q;best=i;}}"
    "vec3 rr=cross(F[best],U[best]);float qx=dot(D,rr)/big,qy=dot(D,U[best])/big;"
    "float halfsize=float(face_size)/2.;"
    "float fx=clamp(halfsize+qx*halfsize-.5,0.,float(face_size-1));"
    "float fy=clamp(halfsize-qy*halfsize-.5,0.,float(face_size-1));"
    "int x0=int(fx),y0=int(fy),x1=min(x0+1,face_size-1),y1=min(y0+1,face_size-1);"
    "float wx=fx-float(x0),wy=fy-float(y0);"
    "vec3 a=get(best,x0,y0)*(1.-wx)+get(best,x1,y0)*wx;"
    "vec3 b=get(best,x0,y1)*(1.-wx)+get(best,x1,y1)*wx;"
    "color=vec4(floor(clamp(a*(1.-wy)+b*wy+.5,0.,255.))/255.,1.);}\n";

int switch_gpu_warmup(void)
{
    if (mix_program && env_program)
        return 1;
    if (failed)
        return 0;
    mix_program = gl_program(vertex, mix_fragment);
    env_program = gl_program(vertex, env_fragment);
    if (!mix_program || !env_program) {
        failed = 1;
        return 0;
    }
    glGenVertexArrays(1, &vao);
    glUseProgram(mix_program);
    const char *names[] = {"a", "b", "c", "d"};
    for (int i = 0; i < 4; i++)
        glUniform1i(glGetUniformLocation(mix_program, names[i]), i);
    mix_weights = glGetUniformLocation(mix_program, "weights");
    glUseProgram(env_program);
    glUniform1i(glGetUniformLocation(env_program, "atlas"), 0);
    env_right = glGetUniformLocation(env_program, "right");
    env_up = glGetUniformLocation(env_program, "up");
    env_forward = glGetUniformLocation(env_program, "forward");
    env_face = glGetUniformLocation(env_program, "face_size");
    env_size = glGetUniformLocation(env_program, "map_size");
    return 1;
}

static GLuint texture(int w, int h, const unsigned char *bytes)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return id;
}
static switch_gpu_image *create(int w, int h)
{
    if (!switch_gpu_warmup())
        return NULL;
    switch_gpu_image *s = calloc(1, sizeof *s);
    if (!s)
        return NULL;
    s->image = plat_texture_create(w, h, NULL);
    if (!s->image) {
        free(s);
        return NULL;
    }
    s->width = w;
    s->height = h;
    GLuint restore = video_draw_framebuffer();
    glGenFramebuffers(1, &s->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           video_texture_id(s->image), 0);
    int complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, restore);
    if (!complete) {
        switch_gpu_image_destroy(s);
        return NULL;
    }
    return s;
}
switch_gpu_image *switch_gpu_mix_create(int w, int h, const unsigned char *const layers[4])
{
    switch_gpu_image *s = create(w, h);
    if (s) {
        glActiveTexture(GL_TEXTURE0);
        for (int i = 0; i < 4; i++)
            s->source[i] = texture(w, h, layers[i]);
    }
    return s;
}
switch_gpu_image *switch_gpu_env_create(int face_size, int map_size)
{
    switch_gpu_image *s = create(map_size, map_size);
    if (!s)
        return NULL;
    s->face = face_size;
    glActiveTexture(GL_TEXTURE0);
    s->source[0] = texture(6 * face_size, face_size, NULL);
    GLuint restore = video_draw_framebuffer();
    glGenFramebuffers(1, &s->atlas_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, s->atlas_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s->source[0], 0);
    int complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, restore);
    if (!complete) {
        switch_gpu_image_destroy(s);
        return NULL;
    }
    return s;
}
void switch_gpu_image_destroy(switch_gpu_image *s)
{
    if (!s)
        return;
    plat_texture_destroy(s->image);
    glDeleteFramebuffers(1, &s->fbo);
    glDeleteFramebuffers(1, &s->atlas_fbo);
    glDeleteTextures(4, s->source);
    free(s);
}
plat_texture *switch_gpu_image_texture(const switch_gpu_image *s)
{
    return s ? s->image : NULL;
}
static void begin(switch_gpu_image *s, GLuint program)
{
    video_flush2d();
    s->restore = video_draw_framebuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    glViewport(0, 0, s->width, s->height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glColorMask(1, 1, 1, 1);
    glUseProgram(program);
    glBindVertexArray(vao);
}
static void end(switch_gpu_image *s)
{
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, s->restore);
}
void switch_gpu_mix(switch_gpu_image *s, const float weights[4])
{
    SWP_PUSH(SWP_GL);
    begin(s, mix_program);
    glUniform4f(mix_weights, weights[0], weights[1], weights[2], weights[3]);
    for (int i = 0; i < 4; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, s->source[i]);
    }
    end(s);
    SWP_POP();
}
void switch_gpu_env_capture(switch_gpu_image *s, int face)
{
    r3d_copy_target(s->atlas_fbo, face * s->face);
    r3d_end_target(NULL);
}
void switch_gpu_env_remap(switch_gpu_image *s, const float r[3], const float u[3], const float f[3])
{
    SWP_PUSH(SWP_GL);
    begin(s, env_program);
    glUniform3fv(env_right, 1, r);
    glUniform3fv(env_up, 1, u);
    glUniform3fv(env_forward, 1, f);
    glUniform1i(env_face, s->face);
    glUniform1i(env_size, s->width);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s->source[0]);
    end(s);
    SWP_POP();
}
