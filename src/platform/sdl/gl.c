#include "gl.h"
#ifdef MOON_PC_VULKAN
#include "vk_render.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
static unsigned program_count;
unsigned gl_program_count(void)
{
    return program_count;
}
#endif

#ifndef __SWITCH__
#define MOON_GL_DEFINE(type, name) type name;
MOON_GL_FUNCS(MOON_GL_DEFINE)
MOON_GL_BASE_FUNCS(MOON_GL_DEFINE)
#undef MOON_GL_DEFINE
#endif

#ifndef __SWITCH__
static void *graphics_proc(const char *name)
{
#ifdef MOON_PC_VULKAN
    if (moon_vk_render_active())
        return moon_vk_render_proc(name);
#endif
    return SDL_GL_GetProcAddress(name);
}
#endif

#ifdef __ANDROID__
/* GLES 3.0 has no glClearDepth (it has glClearDepthf) and no glGetTexImage.
   Both are optional here: depth clearing is remapped, the texture readback
   simply reports itself unavailable. */
static void (APIENTRY *moon_clear_depthf)(GLfloat);
static void APIENTRY moon_clear_depth_from_float(GLclampd depth)
{
    if (moon_clear_depthf)
        moon_clear_depthf((GLfloat)depth);
}
#endif

static void moon_gl_load_missing(const char *name, int *ok)
{
#ifdef __ANDROID__
    if (!strcmp(name, "moon_glClearDepth") || !strcmp(name, "moon_glGetTexImage")) {
        fprintf(stderr, "OpenGL: optional %s unavailable on GLES\n", name + 5);
        return;
    }
#endif
    fprintf(stderr, "OpenGL: missing %s\n", name);
    *ok = 0;
}

int gl_load(void)
{
#ifdef __SWITCH__
    return gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
#else
    int ok = 1;
#define MOON_GL_LOAD(type, name)                                                                \
    name = (type)graphics_proc(strncmp(#name, "moon_", 5) ? #name : #name + 5);         \
    if (!name)                                                                                   \
        moon_gl_load_missing(#name, &ok);
    MOON_GL_FUNCS(MOON_GL_LOAD)
    MOON_GL_BASE_FUNCS(MOON_GL_LOAD)
#undef MOON_GL_LOAD
#ifdef __ANDROID__
    if (!moon_glClearDepth) {
        moon_clear_depthf = (void (APIENTRY *)(GLfloat))SDL_GL_GetProcAddress("glClearDepthf");
        if (moon_clear_depthf) {
            moon_glClearDepth = moon_clear_depth_from_float;
            fprintf(stderr, "OpenGL: glClearDepth -> glClearDepthf\n");
        } else {
            fprintf(stderr, "OpenGL: glClearDepthf missing too\n");
            ok = 0;
        }
    }
#endif
    return ok;
#endif
}

int gl_reversed_z;
int gl_enable_reversed_z(void)
{
#ifdef MOON_PC_VULKAN
    if (moon_vk_render_active()) {
        gl_reversed_z = 1;
        glDepthFunc(GL_GREATER);
        glClearDepth(0);
        return 1;
    }
#endif
    typedef void(APIENTRY * clip_control_fn)(GLenum, GLenum);
    clip_control_fn clip_control = (clip_control_fn)SDL_GL_GetProcAddress("glClipControl");
    if (!clip_control || getenv("MOON_CLASSIC_DEPTH"))
        return gl_reversed_z = 0;
    clip_control(0x8CA1 /* GL_LOWER_LEFT */, 0x935F /* GL_ZERO_TO_ONE */);
    if (glGetError() != GL_NO_ERROR) {
        clip_control(0x8CA1, 0x935E /* NEGATIVE_ONE_TO_ONE */);
        return gl_reversed_z = 0;
    }
    gl_reversed_z = 1;
    glDepthFunc(GL_GREATER);
    glClearDepth(0);
    return 1;
}

/* Desktop shaders are GLSL 3.30 core. GLES 3.0 needs the ES version line and
   explicit precision qualifiers; every program reaches the driver here. */
static const char *shader_source(const char *source, char **allocated)
{
    *allocated = NULL;
#ifdef __ANDROID__
    static const char version[] = "#version 330 core";
    static const char es_head[] = "#version 300 es\nprecision highp float;\nprecision highp int;\n";
    const char *found = strstr(source, version);
    if (found) {
        const char *rest = found + sizeof version - 1;
        if (*rest == '\r')
            rest++;
        if (*rest == '\n')
            rest++;
        size_t head = (size_t)(found - source);
        size_t length = head + sizeof es_head - 1 + strlen(rest) + 1;
        char *out = malloc(length);
        if (out) {
            memcpy(out, source, head);
            memcpy(out + head, es_head, sizeof es_head - 1);
            strcpy(out + head + sizeof es_head - 1, rest);
            *allocated = out;
            return out;
        }
    }
#endif
    return source;
}

static GLuint shader(GLenum kind, const char *source)
{
    char *allocated = NULL;
    const char *mobile = shader_source(source, &allocated);
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &mobile, NULL);
    free(allocated);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "OpenGL shader: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint gl_program(const char *vertex, const char *fragment)
{
#ifdef __SWITCH__
    Uint64 started = SDL_GetPerformanceCounter();
#endif
    GLuint v = shader(GL_VERTEX_SHADER, vertex), f = shader(GL_FRAGMENT_SHADER, fragment);
    if (!v || !f) {
        if (v)
            glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "OpenGL program: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
    program_count++;
#endif
#ifdef __SWITCH__
    fprintf(stderr, "Switch program: index=%u compile_link_ms=%.3f\n", program_count,
            1000.0 * (SDL_GetPerformanceCounter() - started) / SDL_GetPerformanceFrequency());
#endif
    return p;
}
