/* Read-only Atmosphere report upload for a netloader diagnostic session. */
#include <switch.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc > 1 && !inet_aton(argv[1], &__nxlink_host))
        return 1;
    if (!__nxlink_host.s_addr || R_FAILED(socketInitializeDefault()))
        return 1;
    int output = nxlinkStdioForDebug();
    if (output < 0) {
        socketExit();
        return 1;
    }
    setvbuf(stderr, NULL, _IOLBF, 4096);
    const char *roots[] = {"sdmc:/atmosphere/crash_reports", "sdmc:/atmosphere/fatal_reports"};
    for (unsigned r = 0; r < sizeof roots / sizeof *roots; r++) {
        DIR *directory = opendir(roots[r]);
        if (!directory) {
            fprintf(stderr, "Reports unavailable: %s\n", roots[r]);
            continue;
        }
        char recent[3][256] = {{0}};
        struct dirent *entry;
        while ((entry = readdir(directory))) {
            size_t n = strlen(entry->d_name);
            if (n < 4 || n >= sizeof recent[0] || strcmp(entry->d_name + n - 4, ".log"))
                continue;
            for (unsigned i = 0; i < 3; i++) {
                if (strcmp(entry->d_name, recent[i]) <= 0)
                    continue;
                for (unsigned j = 2; j > i; j--)
                    memcpy(recent[j], recent[j - 1], sizeof recent[j]);
                snprintf(recent[i], sizeof recent[i], "%s", entry->d_name);
                break;
            }
        }
        closedir(directory);
        for (unsigned i = 0; i < 3 && recent[i][0]; i++) {
            char path[512];
            snprintf(path, sizeof path, "%s/%s", roots[r], recent[i]);
            FILE *file = fopen(path, "rb");
            fprintf(stderr, "\nREPORT BEGIN %s\n", path);
            if (file) {
                char bytes[4096];
                size_t n, total = 0;
                while (total < 512 * 1024 && (n = fread(bytes, 1, sizeof bytes, file))) {
                    fwrite(bytes, 1, n, stderr);
                    total += n;
                }
                fclose(file);
            }
            fprintf(stderr, "\nREPORT END\n");
        }
    }
    fprintf(stderr, "Report upload complete; SD unchanged.\n");
    fflush(stderr);
    svcSleepThread(1000000000);
    fclose(stderr);
    close(output);
    socketExit();
    return 0;
}
