#define MOON_GL_IMPLEMENTATION
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

#ifdef __ANDROID__
/* Small GLES state cache. Every entry starts "unknown" after a reset, so the
   first call in a frame always reaches the driver; later equal calls do not. */
#define MOON_GL_STATE_UNKNOWN 0xFFFFFFFFu
#define MOON_GL_STATE_UNITS 8
#define MOON_GL_STATE_ATTRIBS 16

static int state_valid;
static GLuint state_program;
static GLuint state_vao;
static GLuint state_active_unit;
static GLuint state_tex[MOON_GL_STATE_UNITS];
static signed char state_caps[65536];
static GLuint state_array_buffer, state_element_buffer;
static unsigned state_generation;
typedef struct {
    unsigned generation;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const void *pointer;
} moon_attrib_state;
static moon_attrib_state state_attrib[MOON_GL_STATE_ATTRIBS];
static int state_blend_valid;
static GLenum state_src_rgb, state_dst_rgb, state_src_alpha, state_dst_alpha;
static unsigned state_skips, texture_skips;

void moon_gl_state_reset(void)
{
    state_valid = 1;
    state_program = MOON_GL_STATE_UNKNOWN;
    state_vao = MOON_GL_STATE_UNKNOWN;
    state_active_unit = MOON_GL_STATE_UNKNOWN;
    for (int i = 0; i < MOON_GL_STATE_UNITS; i++)
        state_tex[i] = MOON_GL_STATE_UNKNOWN;
    memset(state_caps, 0, sizeof state_caps);
    state_array_buffer = state_element_buffer = MOON_GL_STATE_UNKNOWN;
    state_generation++;
    memset(state_attrib, 0, sizeof state_attrib);
    state_blend_valid = 0;
    state_skips = texture_skips = 0;
}

void moon_gl_state_stats(unsigned *skips, unsigned *textures)
{
    if (skips)
        *skips = state_skips;
    if (textures)
        *textures = texture_skips;
}

static void state_ready(void)
{
    if (!state_valid)
        moon_gl_state_reset();
}

void moon_gl_state_active_texture(GLenum unit)
{
    state_ready();
    if (state_active_unit == unit)
        return;
    glActiveTexture(unit);
    state_active_unit = unit;
}

void moon_gl_state_bind_texture(GLenum target, GLuint texture)
{
    if (target != GL_TEXTURE_2D) {
        glBindTexture(target, texture);
        return;
    }
    state_ready();
    if (state_active_unit >= GL_TEXTURE0 &&
        state_active_unit < GL_TEXTURE0 + MOON_GL_STATE_UNITS) {
        GLuint *current = &state_tex[state_active_unit - GL_TEXTURE0];
        if (*current == texture) {
            texture_skips++;
            return;
        }
        glBindTexture(target, texture);
        *current = texture;
        return;
    }
    glBindTexture(target, texture);
}

void moon_gl_state_use_program(GLuint program)
{
    state_ready();
    if (state_program == program) {
        state_skips++;
        return;
    }
    glUseProgram(program);
    state_program = program;
}

void moon_gl_state_blend_func_separate(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha,
                                       GLenum dst_alpha)
{
    state_ready();
    if (state_blend_valid && state_src_rgb == src_rgb && state_dst_rgb == dst_rgb &&
        state_src_alpha == src_alpha && state_dst_alpha == dst_alpha) {
        state_skips++;
        return;
    }
    glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
    state_src_rgb = src_rgb;
    state_dst_rgb = dst_rgb;
    state_src_alpha = src_alpha;
    state_dst_alpha = dst_alpha;
    state_blend_valid = 1;
}

void moon_gl_state_enable(GLenum cap)
{
    state_ready();
    if (cap < 65536) {
        if (state_caps[cap] == 2) {
            state_skips++;
            return;
        }
        state_caps[cap] = 2;
    }
    glEnable(cap);
}

void moon_gl_state_disable(GLenum cap)
{
    state_ready();
    if (cap < 65536) {
        if (state_caps[cap] == 1) {
            state_skips++;
            return;
        }
        state_caps[cap] = 1;
    }
    glDisable(cap);
}

void moon_gl_state_bind_buffer(GLenum target, GLuint buffer)
{
    state_ready();
    if (target == GL_ARRAY_BUFFER) {
        if (state_array_buffer == buffer) {
            state_skips++;
            return;
        }
        glBindBuffer(target, buffer);
        state_array_buffer = buffer;
        state_generation++;
        return;
    }
    if (target == GL_ELEMENT_ARRAY_BUFFER) {
        if (state_element_buffer == buffer) {
            state_skips++;
            return;
        }
        glBindBuffer(target, buffer);
        state_element_buffer = buffer;
        return;
    }
    glBindBuffer(target, buffer);
}

void moon_gl_state_bind_vertex_array(GLuint array)
{
    state_ready();
    if (state_vao == array) {
        state_skips++;
        return;
    }
    glBindVertexArray(array);
    state_vao = array;
    state_element_buffer = MOON_GL_STATE_UNKNOWN;
    state_generation++;
}

void moon_gl_state_vertex_attrib_pointer(GLuint index, GLint size, GLenum type,
                                         GLboolean normalized, GLsizei stride,
                                         const void *pointer)
{
    state_ready();
    if (index < MOON_GL_STATE_ATTRIBS) {
        moon_attrib_state *a = &state_attrib[index];
        if (a->generation == state_generation && a->size == size && a->type == type &&
            a->normalized == normalized && a->stride == stride && a->pointer == pointer) {
            state_skips++;
            return;
        }
        glVertexAttribPointer(index, size, type, normalized, stride, pointer);
        a->generation = state_generation;
        a->size = size;
        a->type = type;
        a->normalized = normalized;
        a->stride = stride;
        a->pointer = pointer;
        return;
    }
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void moon_gl_state_delete_textures(GLsizei n, const GLuint *textures)
{
    glDeleteTextures(n, textures);
    if (!state_valid)
        return;
    for (GLsizei i = 0; i < n; i++)
        for (int u = 0; u < MOON_GL_STATE_UNITS; u++)
            if (state_tex[u] == textures[i])
                state_tex[u] = MOON_GL_STATE_UNKNOWN;
}

void moon_gl_state_delete_buffers(GLsizei n, const GLuint *buffers)
{
    glDeleteBuffers(n, buffers);
    if (!state_valid)
        return;
    for (GLsizei i = 0; i < n; i++) {
        if (state_array_buffer == buffers[i]) {
            state_array_buffer = MOON_GL_STATE_UNKNOWN;
            state_generation++;
        }
        if (state_element_buffer == buffers[i])
            state_element_buffer = MOON_GL_STATE_UNKNOWN;
    }
}

void moon_gl_state_delete_vertex_arrays(GLsizei n, const GLuint *arrays)
{
    glDeleteVertexArrays(n, arrays);
    if (!state_valid)
        return;
    for (GLsizei i = 0; i < n; i++)
        if (state_vao == arrays[i]) {
            state_vao = MOON_GL_STATE_UNKNOWN;
            state_element_buffer = MOON_GL_STATE_UNKNOWN;
            state_generation++;
        }
}
#endif
