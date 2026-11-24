#ifndef MOON_HTTP_H
#define MOON_HTTP_H
#include <stddef.h>
/* HTTPS GET for GitHub release checks. Returns a NUL-terminated buffer or NULL. */
unsigned char *moon_http_get(const char *url, size_t limit, size_t *got);
#endif
