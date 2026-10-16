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
    X(MOON_PFN_glEnable, moon_glEnable)                                                         \
    X(MOON_PFN_glBindTexture, moon_glBindTexture)                                               \
    X(MOON_PFN_glClear, moon_glClear)                                                           \
    X(MOON_PFN_glClearColor, moon_glClearColor)                                                 \
    X(MOON_PFN_glClearDepth, moon_glClearDepth)                                                 \
    X(MOON_PFN_glColorMask, moon_glColorMask)                                                   \
    X(MOON_PFN_glCopyTexSubImage2D, moon_glCopyTexSubImage2D)                                   \
    X(MOON_PFN_glCullFace, moon_glCullFace)                                                     \
    X(MOON_PFN_glDeleteTextures, moon_glDeleteTextures)                                         \
    X(MOON_PFN_glDepthFunc, moon_glDepthFunc)                                                   \
    X(MOON_PFN_glDepthMask, moon_glDepthMask)                                                   \
    X(MOON_PFN_glDisable, moon_glDisable)                                                       \
    X(MOON_PFN_glDrawArrays, moon_glDrawArrays)                                                 \
    X(MOON_PFN_glFinish, moon_glFinish)                                                         \
    X(MOON_PFN_glFrontFace, moon_glFrontFace)                                                   \
    X(MOON_PFN_glGenTextures, moon_glGenTextures)                                               \
    X(MOON_PFN_glGetError, moon_glGetError)                                                     \
    X(MOON_PFN_glGetFloatv, moon_glGetFloatv)                                                   \
    X(MOON_PFN_glGetIntegerv, moon_glGetIntegerv)                                               \
    X(MOON_PFN_glGetString, moon_glGetString)                                                   \
    X(MOON_PFN_glGetTexImage, moon_glGetTexImage)                                               \
    X(MOON_PFN_glGetTexLevelParameteriv, moon_glGetTexLevelParameteriv)                         \
    X(MOON_PFN_glGetTexParameteriv, moon_glGetTexParameteriv)                                   \
    X(MOON_PFN_glIsEnabled, moon_glIsEnabled)                                                   \
    X(MOON_PFN_glPixelStorei, moon_glPixelStorei)                                               \
    X(MOON_PFN_glReadPixels, moon_glReadPixels)                                                 \
    X(MOON_PFN_glScissor, moon_glScissor)                                                       \
    X(MOON_PFN_glTexImage2D, moon_glTexImage2D)                                                 \
    X(MOON_PFN_glTexParameterf, moon_glTexParameterf)                                           \
    X(MOON_PFN_glTexParameteri, moon_glTexParameteri)                                           \
    X(MOON_PFN_glTexSubImage2D, moon_glTexSubImage2D)                                           \
    X(MOON_PFN_glViewport, moon_glViewport)

#define MOON_GL_BASE_DECLARE(type, name) extern type name;
MOON_GL_BASE_FUNCS(MOON_GL_BASE_DECLARE)
#undef MOON_GL_BASE_DECLARE
#define glEnable moon_glEnable
#define glBindTexture moon_glBindTexture
#define glClear moon_glClear
#define glClearColor moon_glClearColor
#define glClearDepth moon_glClearDepth
#define glColorMask moon_glColorMask
#define glCopyTexSubImage2D moon_glCopyTexSubImage2D
#define glCullFace moon_glCullFace
#define glDeleteTextures moon_glDeleteTextures
#define glDepthFunc moon_glDepthFunc
#define glDepthMask moon_glDepthMask
#define glDisable moon_glDisable
#define glDrawArrays moon_glDrawArrays
#define glFinish moon_glFinish
#define glFrontFace moon_glFrontFace
#define glGenTextures moon_glGenTextures
#define glGetError moon_glGetError
#define glGetFloatv moon_glGetFloatv
#define glGetIntegerv moon_glGetIntegerv
#define glGetString moon_glGetString
#define glGetTexImage moon_glGetTexImage
#define glGetTexLevelParameteriv moon_glGetTexLevelParameteriv
#define glGetTexParameteriv moon_glGetTexParameteriv
#define glIsEnabled moon_glIsEnabled
#define glPixelStorei moon_glPixelStorei
#define glReadPixels moon_glReadPixels
#define glScissor moon_glScissor
#define glTexImage2D moon_glTexImage2D
#define glTexParameterf moon_glTexParameterf
#define glTexParameteri moon_glTexParameteri
#define glTexSubImage2D moon_glTexSubImage2D
#define glViewport moon_glViewport
#endif
#endif
