#ifndef OPENGT4_WINDOWS_FILE_H
#define OPENGT4_WINDOWS_FILE_H

#ifdef _WIN32
#include <windows.h>

/* PC storage policy: tolerate short-lived scanner/indexer locks without deleting
 * either file on failure. Ten attempts, with nine waits totalling 450 ms.
 * This retry schedule is a PC adaptation requested by the user, not GT4 timing. */
static inline int plat_windows_replace_file(const wchar_t *source, const wchar_t *destination,
                                            DWORD flags)
{
    for (unsigned attempt = 0; attempt < 10; attempt++) {
        if (MoveFileExW(source, destination, flags))
            return 1;
        DWORD error = GetLastError();
        if (attempt == 9 || (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION &&
                             error != ERROR_LOCK_VIOLATION)) {
            SetLastError(error);
            return 0;
        }
        Sleep(10 * (attempt + 1));
    }
    return 0;
}
#endif

#endif
