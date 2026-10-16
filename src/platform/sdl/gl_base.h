/* Core entry points shared by the OpenGL and Vulkan command backends. */
#ifndef MOON_GL_BASE_H
#define MOON_GL_BASE_H
#ifndef __SWITCH__
typedef void(APIENTRYP MOON_PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void(APIENTRYP MOON_PFN_glClear)(GLbitfield mask);
typedef void(APIENTRYP MOON_PFN_glClearColor)(GLclampf red, GLclampf green, GLclampf blue,
                                              GLclampf alpha);
typedef void(APIENTRYP MOON_PFN_glClearDepth)(GLclampd depth);
typedef void(APIENTRYP MOON_PFN_glColorMask)(GLboolean red, GLboolean green, GLboolean blue,
                                             GLboolean alpha);
typedef void(APIENTRYP MOON_PFN_glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
                                                     GLint yoffset, GLint x, GLint y, GLsizei width,
                                                     GLsizei height);
typedef void(APIENTRYP MOON_PFN_glCullFace)(GLenum mode);
typedef void(APIENTRYP MOON_PFN_glDeleteTextures)(GLsizei n, const GLuint *textures);
typedef void(APIENTRYP MOON_PFN_glDepthFunc)(GLenum func);
typedef void(APIENTRYP MOON_PFN_glDepthMask)(GLboolean flag);
typedef void(APIENTRYP MOON_PFN_glDisable)(GLenum cap);
typedef void(APIENTRYP MOON_PFN_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void(APIENTRYP MOON_PFN_glFinish)(void);
typedef void(APIENTRYP MOON_PFN_glFrontFace)(GLenum mode);
typedef void(APIENTRYP MOON_PFN_glGenTextures)(GLsizei n, GLuint *textures);
typedef GLenum(APIENTRYP MOON_PFN_glGetError)(void);
typedef void(APIENTRYP MOON_PFN_glGetFloatv)(GLenum pname, GLfloat *params);
typedef void(APIENTRYP MOON_PFN_glGetIntegerv)(GLenum pname, GLint *params);
typedef const GLubyte *(APIENTRYP MOON_PFN_glGetString)(GLenum name);
typedef void(APIENTRYP MOON_PFN_glGetTexImage)(GLenum target, GLint level, GLenum format,
                                               GLenum type, GLvoid *pixels);
typedef void(APIENTRYP MOON_PFN_glGetTexLevelParameteriv)(GLenum target, GLint level, GLenum pname,
                                                          GLint *params);
typedef void(APIENTRYP MOON_PFN_glGetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
typedef GLboolean(APIENTRYP MOON_PFN_glIsEnabled)(GLenum cap);
typedef void(APIENTRYP MOON_PFN_glPixelStorei)(GLenum pname, GLint param);
typedef void(APIENTRYP MOON_PFN_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height,
                                              GLenum format, GLenum type, GLvoid *pixels);
typedef void(APIENTRYP MOON_PFN_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void(APIENTRYP MOON_PFN_glTexImage2D)(GLenum target, GLint level, GLint internalFormat,
                                              GLsizei width, GLsizei height, GLint border,
                                              GLenum format, GLenum type, const GLvoid *pixels);
typedef void(APIENTRYP MOON_PFN_glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
typedef void(APIENTRYP MOON_PFN_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void(APIENTRYP MOON_PFN_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset,
                                                 GLint yoffset, GLsizei width, GLsizei height,
                                                 GLenum format, GLenum type, const GLvoid *pixels);
typedef void(APIENTRYP MOON_PFN_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);

typedef void(APIENTRYP MOON_PFN_glEnable)(GLenum cap);

#define MOON_GL_BASE_FUNCS(X)                                                                      \
    X(MOON_PFN_glEnable, opengt4_glEnable)                                                         \
    X(MOON_PFN_glBindTexture, opengt4_glBindTexture)                                               \
    X(MOON_PFN_glClear, opengt4_glClear)                                                           \
    X(MOON_PFN_glClearColor, opengt4_glClearColor)                                                 \
    X(MOON_PFN_glClearDepth, opengt4_glClearDepth)                                                 \
    X(MOON_PFN_glColorMask, opengt4_glColorMask)                                                   \
    X(MOON_PFN_glCopyTexSubImage2D, opengt4_glCopyTexSubImage2D)                                   \
    X(MOON_PFN_glCullFace, opengt4_glCullFace)                                                     \
    X(MOON_PFN_glDeleteTextures, opengt4_glDeleteTextures)                                         \
    X(MOON_PFN_glDepthFunc, opengt4_glDepthFunc)                                                   \
    X(MOON_PFN_glDepthMask, opengt4_glDepthMask)                                                   \
    X(MOON_PFN_glDisable, opengt4_glDisable)                                                       \
    X(MOON_PFN_glDrawArrays, opengt4_glDrawArrays)                                                 \
    X(MOON_PFN_glFinish, opengt4_glFinish)                                                         \
    X(MOON_PFN_glFrontFace, opengt4_glFrontFace)                                                   \
    X(MOON_PFN_glGenTextures, opengt4_glGenTextures)                                               \
    X(MOON_PFN_glGetError, opengt4_glGetError)                                                     \
    X(MOON_PFN_glGetFloatv, opengt4_glGetFloatv)                                                   \
    X(MOON_PFN_glGetIntegerv, opengt4_glGetIntegerv)                                               \
    X(MOON_PFN_glGetString, opengt4_glGetString)                                                   \
    X(MOON_PFN_glGetTexImage, opengt4_glGetTexImage)                                               \
    X(MOON_PFN_glGetTexLevelParameteriv, opengt4_glGetTexLevelParameteriv)                         \
    X(MOON_PFN_glGetTexParameteriv, opengt4_glGetTexParameteriv)                                   \
    X(MOON_PFN_glIsEnabled, opengt4_glIsEnabled)                                                   \
    X(MOON_PFN_glPixelStorei, opengt4_glPixelStorei)                                               \
    X(MOON_PFN_glReadPixels, opengt4_glReadPixels)                                                 \
    X(MOON_PFN_glScissor, opengt4_glScissor)                                                       \
    X(MOON_PFN_glTexImage2D, opengt4_glTexImage2D)                                                 \
    X(MOON_PFN_glTexParameterf, opengt4_glTexParameterf)                                           \
    X(MOON_PFN_glTexParameteri, opengt4_glTexParameteri)                                           \
    X(MOON_PFN_glTexSubImage2D, opengt4_glTexSubImage2D)                                           \
    X(MOON_PFN_glViewport, opengt4_glViewport)

#define MOON_GL_BASE_DECLARE(type, name) extern type name;
MOON_GL_BASE_FUNCS(MOON_GL_BASE_DECLARE)
#undef MOON_GL_BASE_DECLARE
#define glEnable opengt4_glEnable
#define glBindTexture opengt4_glBindTexture
#define glClear opengt4_glClear
#define glClearColor opengt4_glClearColor
#define glClearDepth opengt4_glClearDepth
#define glColorMask opengt4_glColorMask
#define glCopyTexSubImage2D opengt4_glCopyTexSubImage2D
#define glCullFace opengt4_glCullFace
#define glDeleteTextures opengt4_glDeleteTextures
#define glDepthFunc opengt4_glDepthFunc
#define glDepthMask opengt4_glDepthMask
#define glDisable opengt4_glDisable
#define glDrawArrays opengt4_glDrawArrays
#define glFinish opengt4_glFinish
#define glFrontFace opengt4_glFrontFace
#define glGenTextures opengt4_glGenTextures
#define glGetError opengt4_glGetError
#define glGetFloatv opengt4_glGetFloatv
#define glGetIntegerv opengt4_glGetIntegerv
#define glGetString opengt4_glGetString
#define glGetTexImage opengt4_glGetTexImage
#define glGetTexLevelParameteriv opengt4_glGetTexLevelParameteriv
#define glGetTexParameteriv opengt4_glGetTexParameteriv
#define glIsEnabled opengt4_glIsEnabled
#define glPixelStorei opengt4_glPixelStorei
#define glReadPixels opengt4_glReadPixels
#define glScissor opengt4_glScissor
#define glTexImage2D opengt4_glTexImage2D
#define glTexParameterf opengt4_glTexParameterf
#define glTexParameteri opengt4_glTexParameteri
#define glTexSubImage2D opengt4_glTexSubImage2D
#define glViewport opengt4_glViewport
#endif
#endif
