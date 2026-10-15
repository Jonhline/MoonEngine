#ifndef OPENGT4_TEST_PROCESS_H
#define OPENGT4_TEST_PROCESS_H

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

/* Keep concurrent filesystem fixtures separate even when time/clock match. */
static inline unsigned long test_process_id(void)
{
#ifdef _WIN32
    return (unsigned long)_getpid();
#else
    return (unsigned long)getpid();
#endif
}

#endif
