/* P: launcher image reconstruction with the same AMD FSR1 EASU/RCAS passes
   as src/platform/sdl/fsr.c. Each launcher owns its hidden GPU context. */
#include "launcher_fsr.h"
#include "gl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern const char fsr_ffx_a[],fsr_ffx_fsr1[];
extern const size_t fsr_ffx_a_size,fsr_ffx_fsr1_size;
struct launcher_fsr {
    SDL_Window *window;
    SDL_GLContext context;
    GLuint easu,rcas,vao,fbo,mid,out;
    GLint easu_con[4],easu_src,easu_size,rcas_con,rcas_src,rcas_unpremultiply;
    int state,cached_w,cached_h;
};
static const char *vertex(void) { return
    "#version 430 core\n"
    "void main(){ vec2 p=vec2((gl_VertexID<<1)&2, gl_VertexID&2); gl_Position=vec4(p*2.0-1.0,0.0,1.0); }\n"; }

static const char *easu_head(void) { return  "#version 430 core\n#define A_GPU 1\n#define A_GLSL 1\n"; }
static const char *easu_mid(void) { return  "\n#define FSR_EASU_F 1\n"
                              "uniform sampler2D u_src;\n"
                              "AF4 FsrEasuRF(AF2 p){return textureGather(u_src,p,0);}\n"
                              "AF4 FsrEasuGF(AF2 p){return textureGather(u_src,p,1);}\n"
                              "AF4 FsrEasuBF(AF2 p){return textureGather(u_src,p,2);}\n"; }
static const char *easu_main(void) { return
    "\nuniform uvec4 u_con0,u_con1,u_con2,u_con3; uniform vec2 u_size; out vec4 o;\n"
    "void main(){ AF3 c; FsrEasuF(c,AU2(gl_FragCoord.xy),u_con0,u_con1,u_con2,u_con3);\n"
    "  o=vec4(c,texture(u_src,gl_FragCoord.xy/u_size).a); }\n"; }
static const char *rcas_mid(void) { return
    "\n#define FSR_RCAS_F 1\n#define FSR_RCAS_PASSTHROUGH_ALPHA 1\n"
    "uniform sampler2D u_src;\n"
    "AF4 FsrRcasLoadF(ASU2 p){return texelFetch(u_src,clamp(p,ASU2(0),textureSize(u_src,0)-1),0);}\n"
    "void FsrRcasInputF(inout AF1 r,inout AF1 g,inout AF1 b){}\n"; }
static const char *rcas_main(void) { return
    "\nuniform uvec4 u_con; uniform int u_unpremultiply; out vec4 o;\n"
    "void main(){ AF1 r,g,b,a; FsrRcasF(r,g,b,a,AU2(gl_FragCoord.xy),u_con);\n"
    "  vec3 c=vec3(r,g,b); if(u_unpremultiply==1 && a>0.0) c=min(c/a,vec3(1.0)); o=vec4(c,a); }\n"; }

static char *join(const char *a, const char *b, size_t bn, const char *c, const char *d, size_t dn,
                  const char *e)
{
    size_t n = strlen(a) + bn + strlen(c) + dn + strlen(e) + 1;
    char *s = malloc(n);
    if (!s)
        return NULL;
    size_t at = 0;
    memcpy(s + at, a, strlen(a));
    at += strlen(a);
    memcpy(s + at, b, bn);
    at += bn;
    memcpy(s + at, c, strlen(c));
    at += strlen(c);
    memcpy(s + at, d, dn);
    at += dn;
    memcpy(s + at, e, strlen(e));
    at += strlen(e);
    s[at] = 0;
    return s;
}

static int available(launcher_fsr *f)
{
    if (f->state)
        return f->state > 0;
    f->state = -1;
    GLint major = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    if (major < 4)
        return 0; /* the FSR shaders need GLSL 4.x (textureGather, bit operations) */
    char *e = join(easu_head(), fsr_ffx_a, fsr_ffx_a_size, easu_mid(), fsr_ffx_fsr1, fsr_ffx_fsr1_size,
                   easu_main());
    char *r = join(easu_head(), fsr_ffx_a, fsr_ffx_a_size, rcas_mid(), fsr_ffx_fsr1, fsr_ffx_fsr1_size,
                   rcas_main());
    if (e && r) {
        f->easu = gl_program(vertex(), e);
        f->rcas = gl_program(vertex(), r);
    }
    free(e);
    free(r);
    if (!f->easu || !f->rcas) {
        fprintf(stderr, "FSR: shaders unavailable, textures are not upscaled\n");
        return 0;
    }
    const char *names[4] = {"u_con0", "u_con1", "u_con2", "u_con3"};
    for (int i = 0; i < 4; i++)
        f->easu_con[i] = glGetUniformLocation(f->easu, names[i]);
    f->easu_src = glGetUniformLocation(f->easu, "u_src");
    f->easu_size = glGetUniformLocation(f->easu, "u_size");
    f->rcas_con = glGetUniformLocation(f->rcas, "u_con");
    f->rcas_src = glGetUniformLocation(f->rcas, "u_src");
    f->rcas_unpremultiply = glGetUniformLocation(f->rcas, "u_unpremultiply");
    glGenVertexArrays(1, &f->vao);
    glGenFramebuffers(1, &f->fbo);
    f->state = 1;
    return 1;
}

static unsigned bits(float f)
{
    unsigned u;
    memcpy(&u, &f, 4);
    return u;
}

static GLuint texture(int w, int h, const void *px, GLint filter)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static GLuint resolve(launcher_fsr *f,GLuint input, int w, int h, int ow, int oh)
{
    if (!input || w < 1 || h < 1 || ow < 1 || oh < 1 || !available(f))
        return 0;
    GLint old_fbo = 0, old_vp[4], old_prog = 0, old_vao = 0, old_tex = 0, old_active = 0;
    GLboolean blend = glIsEnabled(GL_BLEND), scissor = glIsEnabled(GL_SCISSOR_TEST),
              depth = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
    glGetIntegerv(GL_VIEWPORT, old_vp);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex);

    if (f->cached_w != ow || f->cached_h != oh) {
        if (f->mid)
            glDeleteTextures(1, &f->mid);
        if (f->out)
            glDeleteTextures(1, &f->out);
        f->mid = texture(ow, oh, NULL, GL_NEAREST);
        f->out = texture(ow, oh, NULL, GL_LINEAR);
        f->cached_w = ow;
        f->cached_h = oh;
    }

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindVertexArray(f->vao);
    glBindFramebuffer(GL_FRAMEBUFFER, f->fbo);
    glViewport(0, 0, ow, oh);

    /* EASU: constants as FsrEasuCon() computes them (viewport = whole image) */
    unsigned c[16] = {bits((float)w / ow),
                      bits((float)h / oh),
                      bits(.5f * w / ow - .5f),
                      bits(.5f * h / oh - .5f),
                      bits(1.f / w),
                      bits(1.f / h),
                      bits(1.f / w),
                      bits(-1.f / h),
                      bits(-1.f / w),
                      bits(2.f / h),
                      bits(1.f / w),
                      bits(2.f / h),
                      bits(0.f / w),
                      bits(4.f / h),
                      0,
                      0};
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f->mid, 0);
    glUseProgram(f->easu);
    for (int i = 0; i < 4; i++)
        glUniform4ui(f->easu_con[i], c[i * 4], c[i * 4 + 1], c[i * 4 + 2], c[i * 4 + 3]);
    glUniform1i(f->easu_src, 0);
    glUniform2f(f->easu_size, (float)ow, (float)oh);
    glBindTexture(GL_TEXTURE_2D, input);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* RCAS: FsrRcasCon(sharpness 0.5 stops) */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f->out, 0);
    glUseProgram(f->rcas);
    glUniform4ui(f->rcas_con, bits(0.70710678f), 0, 0, 0); /* exp2(-0.5), restrained sharpening */
    glUniform1i(f->rcas_src, 0);
    glUniform1i(f->rcas_unpremultiply, 0);
    glBindTexture(GL_TEXTURE_2D, f->mid);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)old_fbo);
    glViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
    glUseProgram((GLuint)old_prog);
    glBindVertexArray((GLuint)old_vao);
    glBindTexture(GL_TEXTURE_2D, (GLuint)old_tex);
    glActiveTexture((GLenum)old_active);
    if (blend)
        glEnable(GL_BLEND);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
    if (depth)
        glEnable(GL_DEPTH_TEST);
    return f->out;
}

launcher_fsr *launcher_fsr_open(void)
{
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) return NULL;
    int major=2,minor=1,profile=0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,&major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,&minor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,&profile);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    launcher_fsr *f=calloc(1,sizeof *f);
    if(f) {
        f->window=SDL_CreateWindow("Moon Engine image filter",0,0,16,16,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
        if(f->window) f->context=SDL_GL_CreateContext(f->window);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,minor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,profile);
    if(!f) { SDL_QuitSubSystem(SDL_INIT_VIDEO); return NULL; }
    if(!f->context || !gl_load() || !available(f)) { launcher_fsr_close(f); return NULL; }
    return f;
}
void launcher_fsr_close(launcher_fsr *f)
{
    if(!f) return;
    if(f->context) {
        SDL_GL_MakeCurrent(f->window,f->context);
        if(f->state==1) {
            glDeleteProgram(f->easu); glDeleteProgram(f->rcas);
            glDeleteTextures(1,&f->mid); glDeleteTextures(1,&f->out);
            glDeleteFramebuffers(1,&f->fbo); glDeleteVertexArrays(1,&f->vao);
        }
        SDL_GL_DeleteContext(f->context);
    }
    if(f->window) SDL_DestroyWindow(f->window);
    free(f); SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
uint8_t *launcher_fsr_image(launcher_fsr *f,const uint8_t *rgba,int w,int h,int ow,int oh)
{
    if(!f || !rgba || w<1 || h<1 || ow<1 || oh<1 || ow>2048 || oh>2048) return NULL;
    if(SDL_GL_MakeCurrent(f->window,f->context)<0) return NULL;
    uint8_t *pixels=malloc((size_t)ow*oh*4);
    if(!pixels) return NULL;
    GLuint input=texture(w,h,rgba,GL_LINEAR),output=resolve(f,input,w,h,ow,oh);
    if(output) {
        glBindTexture(GL_TEXTURE_2D,output);
        glPixelStorei(GL_PACK_ALIGNMENT,1);
        glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }
    glDeleteTextures(1,&input);
    if(!output) { free(pixels); return NULL; }
    return pixels;
}
