/* MSVC-compatibility shim for the non-Windows build.
 *
 * The tests and probes were written against the Windows CRT, so they call
 * _putenv_s(). Rather than edit every test file, CMakeLists.txt force-includes this
 * header on non-Windows builds and the name is mapped onto the POSIX call.
 *
 * The mapping is NOT cosmetic: MSVC's _putenv_s(name, "") REMOVES the variable, while
 * plain setenv(name, "", 1) would leave it defined-but-empty -- and getenv() would then
 * return "" instead of NULL, which every switch in this tree reads as "on". The shim
 * therefore unsets on an empty value, exactly like the CRT it replaces. Both return 0
 * on success, which is what the tests' CHECK(!_putenv_s(...)) expects.
 */
#ifndef OPENGT4_MSVC_COMPAT_H
#define OPENGT4_MSVC_COMPAT_H

#ifndef _WIN32

#include <stdlib.h>
#include <unistd.h>

/* The Win32 process id is used by the tests for a per-run unique directory name.
 * getpid() is the same thing on POSIX. */
static inline unsigned long opengt4_getpid(void) { return (unsigned long)getpid(); }

static inline int opengt4_putenv_s(const char *name, const char *value) {
    if (!name) return -1;
    if (!value || !*value) return unsetenv(name);
    return setenv(name, value, 1);
}

#define _putenv_s opengt4_putenv_s
#define GetCurrentProcessId() opengt4_getpid()

#endif /* !_WIN32 */

#endif /* OPENGT4_MSVC_COMPAT_H */
