/*
 * OpenGL 3.3 core entry points for the SDL backend. GL 1.1 functions come
 * from the system library (opengl32 on Windows); later ones are loaded at
 * run time through SDL_GL_GetProcAddress by gl_load().
 */
#ifndef MOON_GL_H
#define MOON_GL_H

#include <SDL.h>
#if defined(__SWITCH__) || defined(MOON_SWITCH_RENDER_TEST)
unsigned gl_program_count(void);
#endif
#ifdef __SWITCH__
#include <glad/glad.h>
#else
#include <SDL_opengl.h>
#include "gl_base.h"
#endif

#define MOON_GL_FUNCS(X)                                                                        \
    X(PFNGLACTIVETEXTUREPROC, moon_glActiveTexture)                                             \
    X(PFNGLATTACHSHADERPROC, glAttachShader)                                                       \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                                           \
    X(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer)                                                 \
    X(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer)                                               \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                                                 \
    X(PFNGLBLENDCOLORPROC, moon_glBlendColor)                                                   \
    X(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer)                                                 \
    X(PFNGLBLENDEQUATIONSEPARATEPROC, glBlendEquationSeparate)                                     \
    X(PFNGLBLENDFUNCSEPARATEPROC, glBlendFuncSeparate)                                             \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                                           \
    X(PFNGLBUFFERSUBDATAPROC, glBufferSubData)                                                     \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus)                                   \
    X(PFNGLCOMPILESHADERPROC, glCompileShader)                                                     \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                                                     \
    X(PFNGLCREATESHADERPROC, glCreateShader)                                                       \
    X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)                                                     \
    X(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers)                                           \
    X(PFNGLDELETEPROGRAMPROC, glDeleteProgram)                                                     \
    X(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers)                                         \
    X(PFNGLDELETESHADERPROC, glDeleteShader)                                                       \
    X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays)                                           \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)                                 \
    X(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer)                                 \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D)                                       \
    X(PFNGLGENBUFFERSPROC, glGenBuffers)                                                           \
    X(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers)                                                 \
    X(PFNGLGENERATEMIPMAPPROC, glGenerateMipmap)                                                   \
    X(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers)                                               \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays)                                                 \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                                             \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                                       \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)                                               \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv)                                                         \
    X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)                                           \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram)                                                         \
    X(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage)                                         \
    X(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample)                   \
    X(PFNGLSHADERSOURCEPROC, glShaderSource)                                                       \
    X(PFNGLUNIFORM1FPROC, glUniform1f)                                                             \
    X(PFNGLUNIFORM1IPROC, glUniform1i)                                                             \
    X(PFNGLUNIFORM2FPROC, glUniform2f)                                                             \
    X(PFNGLUNIFORM2IPROC, glUniform2i)                                                             \
    X(PFNGLUNIFORM3FPROC, glUniform3f)                                                             \
    X(PFNGLUNIFORM2FVPROC, glUniform2fv)                                                           \
    X(PFNGLUNIFORM3FVPROC, glUniform3fv)                                                           \
    X(PFNGLUNIFORM4FVPROC, glUniform4fv)                                                         \
    X(PFNGLUNIFORM4FPROC, glUniform4f)                                                             \
    X(PFNGLUNIFORM4IPROC, glUniform4i)                                                             \
    X(PFNGLUNIFORM4UIPROC, glUniform4ui)                                                           \
    X(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv)                                               \
    X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)                                               \
    X(PFNGLUSEPROGRAMPROC, glUseProgram)                                                           \
    X(PFNGLVERTEXATTRIBIPOINTERPROC, glVertexAttribIPointer)                                       \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

#ifndef __SWITCH__
#define MOON_GL_DECLARE(type, name) extern type name;
MOON_GL_FUNCS(MOON_GL_DECLARE)
#undef MOON_GL_DECLARE
/* SDL_opengl.h declares these two as functions: use our loaded pointers. */
#define glActiveTexture moon_glActiveTexture
#define glBlendColor moon_glBlendColor
#endif

#ifdef __ANDROID__
/* GLES pays these calls on the CPU for every draw. The cache only skips a
   call when the value is already current; the renderer's output is unchanged. */
void moon_gl_state_reset(void);
void moon_gl_state_stats(unsigned *state_skips, unsigned *texture_skips);
void moon_gl_state_bind_texture(GLenum target, GLuint texture);
void moon_gl_state_use_program(GLuint program);
void moon_gl_state_blend_func_separate(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha,
                                       GLenum dst_alpha);
void moon_gl_state_enable(GLenum cap);
void moon_gl_state_disable(GLenum cap);
void moon_gl_state_bind_buffer(GLenum target, GLuint buffer);
void moon_gl_state_bind_vertex_array(GLuint array);
void moon_gl_state_vertex_attrib_pointer(GLuint index, GLint size, GLenum type,
                                         GLboolean normalized, GLsizei stride,
                                         const void *pointer);
void moon_gl_state_delete_textures(GLsizei n, const GLuint *textures);
void moon_gl_state_delete_buffers(GLsizei n, const GLuint *buffers);
void moon_gl_state_delete_vertex_arrays(GLsizei n, const GLuint *arrays);
void moon_gl_state_active_texture(GLenum unit);
#ifndef MOON_GL_IMPLEMENTATION
#undef glBindTexture
#undef glUseProgram
#undef glBlendFuncSeparate
#undef glEnable
#undef glDisable
#undef glBindBuffer
#undef glBindVertexArray
#undef glVertexAttribPointer
#undef glDeleteTextures
#undef glDeleteBuffers
#undef glDeleteVertexArrays
#undef glActiveTexture
#define glBindTexture moon_gl_state_bind_texture
#define glUseProgram moon_gl_state_use_program
#define glBlendFuncSeparate moon_gl_state_blend_func_separate
#define glEnable moon_gl_state_enable
#define glDisable moon_gl_state_disable
#define glBindBuffer moon_gl_state_bind_buffer
#define glBindVertexArray moon_gl_state_bind_vertex_array
#define glVertexAttribPointer moon_gl_state_vertex_attrib_pointer
#define glDeleteTextures moon_gl_state_delete_textures
#define glDeleteBuffers moon_gl_state_delete_buffers
#define glDeleteVertexArrays moon_gl_state_delete_vertex_arrays
#define glActiveTexture moon_gl_state_active_texture
#else
#undef glActiveTexture
#define glActiveTexture moon_glActiveTexture
#endif
#endif

/* GL 4.2 / ARB_shader_image_load_store. Optional: the desktop path may be a
   3.3 core context, so a NULL pointer is a no-op at call sites. */
typedef void (APIENTRY *moon_glMemoryBarrierProc)(GLbitfield);
extern moon_glMemoryBarrierProc moon_glMemoryBarrier;
/* Loads the entry points for the current context. 0 if any is missing. */
int gl_load(void);
/* Reversed depth (OpenGL 4.5 / ARB_clip_control): far 0, near 1 in a float
   buffer, nearly even precision with distance. 0 keeps the classic
   [-1, 1] depth in 24 bits. Set once by video_gl after the context. */
extern int gl_reversed_z;
int gl_enable_reversed_z(void);
#define GL_DEPTH_FUNC_LESS (gl_reversed_z ? GL_GREATER : GL_LESS)
#define GL_DEPTH_FUNC_LEQUAL (gl_reversed_z ? GL_GEQUAL : GL_LEQUAL)
#define GL_DEPTH_FAR (gl_reversed_z ? 0.0 : 1.0)
/* Compiles and links a vertex/fragment program; 0 on error (logged). */
GLuint gl_program(const char *vertex, const char *fragment);

#endif
