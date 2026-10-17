#ifndef MOON_RENDERER_STARTUP_H
#define MOON_RENDERER_STARTUP_H
/* -1: ordinary game invocation; otherwise the diagnostic's exit status. */
int moon_renderer_diagnostic(int argc, char **argv);
/* 1 when a Vulkan device opens; its name (or the reason it does not) in `text`. */
int moon_renderer_probe_vulkan(char *text, unsigned long capacity);
#endif
