#ifndef MOON_RENDERER_BACKEND_H
#define MOON_RENDERER_BACKEND_H

enum { MOON_RENDERER_OPENGL, MOON_RENDERER_VULKAN, MOON_RENDERER_COUNT };
const char *moon_renderer_name(int backend);
/* Reject unknown values rather than silently using another API. */
int moon_renderer_parse(const char *name);
const char *moon_renderer_unavailable(int backend);

#endif
