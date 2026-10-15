#include "platform/windows_file.h"
#include <stdio.h>
#include <string.h>

static int write_file(const wchar_t *path, const char *text)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    int ok = file != INVALID_HANDLE_VALUE &&
             WriteFile(file, text, (DWORD)strlen(text), &written, NULL) && written == strlen(text);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    return ok;
}

static int contains(const wchar_t *path, const char *text)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    char bytes[32] = {0};
    DWORD read = 0;
    int ok = file != INVALID_HANDLE_VALUE && ReadFile(file, bytes, sizeof bytes - 1, &read, NULL) &&
             !strcmp(bytes, text);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    return ok;
}

static DWORD WINAPI release_lock(void *handle)
{
    Sleep(50);
    CloseHandle((HANDLE)handle);
    return 0;
}

static int check(int condition, const char *text, int line)
{
    if (!condition)
        fprintf(stderr, "line %d: %s (Windows error %lu)\n", line, text, GetLastError());
    return condition;
}
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!check((condition) != 0, #condition, __LINE__))                                        \
            return 1;                                                                              \
    } while (0)

int main(int argc, char **argv)
{
    wchar_t directory[MAX_PATH], destination[MAX_PATH], source[MAX_PATH];
    CHECK(GetTempPathW(MAX_PATH, directory));
    CHECK(swprintf(destination, MAX_PATH, L"%sopengt4-replace-%lu.dat", directory,
                   GetCurrentProcessId()) > 0);
    CHECK(swprintf(source, MAX_PATH, L"%sopengt4-replace-%lu.tmp", directory,
                   GetCurrentProcessId()) > 0);
    DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    CHECK(write_file(destination, "original"));
    CHECK(write_file(source, "replacement"));
    HANDLE lock = CreateFileW(destination, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(lock != INVALID_HANDLE_VALUE);
    /* Deliberately remove retries: the same positive check must detect the defect. */
    if (argc > 1 && !strcmp(argv[1], "--no-retry")) {
        int ok = MoveFileExW(source, destination, flags) != 0;
        CloseHandle(lock);
        DeleteFileW(source);
        DeleteFileW(destination);
        CHECK(ok);
        return 0;
    }
    HANDLE thread = CreateThread(NULL, 0, release_lock, lock, 0, NULL);
    CHECK(thread);
    CHECK(plat_windows_replace_file(source, destination, flags));
    CHECK(WaitForSingleObject(thread, 2000) == WAIT_OBJECT_0);
    CloseHandle(thread);
    CHECK(contains(destination, "replacement"));
    CHECK(GetFileAttributesW(source) == INVALID_FILE_ATTRIBUTES);

    CHECK(write_file(destination, "original"));
    CHECK(write_file(source, "replacement"));
    lock = CreateFileW(destination, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(lock != INVALID_HANDLE_VALUE);
    CHECK(!plat_windows_replace_file(source, destination, flags));
    DWORD error = GetLastError();
    CHECK(error == ERROR_SHARING_VIOLATION || error == ERROR_ACCESS_DENIED ||
          error == ERROR_LOCK_VIOLATION);
    CloseHandle(lock);
    CHECK(contains(destination, "original"));
    CHECK(contains(source, "replacement"));
    DeleteFileW(source);
    ULONGLONG start = GetTickCount64();
    CHECK(!plat_windows_replace_file(source, destination, flags));
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(GetTickCount64() - start < 200);
    CHECK(contains(destination, "original"));
    CHECK(DeleteFileW(destination));
    return 0;
}
