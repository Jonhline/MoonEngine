#include "gl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(__SWITCH__) || defined(OPENGT4_SWITCH_RENDER_TEST)
static unsigned program_count;
unsigned gl_program_count(void)
{
    return program_count;
}
#endif

#ifndef __SWITCH__
#define OPENGT4_GL_DEFINE(type, name) type name;
OPENGT4_GL_FUNCS(OPENGT4_GL_DEFINE)
#undef OPENGT4_GL_DEFINE
#endif

int gl_load(void)
{
#ifdef __SWITCH__
    return gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
#else
    int ok = 1;
#define OPENGT4_GL_LOAD(type, name)                                                                \
    name = (type)SDL_GL_GetProcAddress(strncmp(#name, "opengt4_", 8) ? #name : #name + 8);         \
    if (!name) {                                                                                   \
        fprintf(stderr, "OpenGL: missing %s\n", #name);                                            \
        ok = 0;                                                                                    \
    }
    OPENGT4_GL_FUNCS(OPENGT4_GL_LOAD)
#undef OPENGT4_GL_LOAD
    return ok;
#endif
}

int gl_reversed_z;
int gl_enable_reversed_z(void)
{
    typedef void(APIENTRY * clip_control_fn)(GLenum, GLenum);
    clip_control_fn clip_control = (clip_control_fn)SDL_GL_GetProcAddress("glClipControl");
    if (!clip_control || getenv("OPENGT4_CLASSIC_DEPTH"))
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

static GLuint shader(GLenum kind, const char *source)
{
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &source, NULL);
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
#if defined(__SWITCH__) || defined(OPENGT4_SWITCH_RENDER_TEST)
    program_count++;
#endif
#ifdef __SWITCH__
    fprintf(stderr, "Switch program: index=%u compile_link_ms=%.3f\n", program_count,
            1000.0 * (SDL_GetPerformanceCounter() - started) / SDL_GetPerformanceFrequency());
#endif
    return p;
}
