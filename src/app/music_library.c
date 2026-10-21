/* P: opt-in local music, with bounded streaming and no file work in audio callbacks. */
#include "music_library.h"
#include "music_override.h"
#include "pcm_music.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#ifdef MOON_LINKED_MOVIES
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#endif
enum { RING = 32768 };
typedef struct {
    int count, assigned[ML_CONTEXTS];
    ml_playlist lists[ML_LISTS];
} ml_model;
struct music_library {
    moon_options *store;
    ml_model model;
    SDL_mutex *mutex, *transport, *status_lock;
    SDL_Thread *thread;
    SDL_atomic_t stop, enabled, revision, wanted, ready, ready_revision, read, write, preview,
        preview_track, preview_eof;
    SDL_atomic_t valid[ML_CONTEXTS], played, peak;
    int16_t pcm[RING * 2];
    char path[ML_PATH], status[256], disc[ML_PATH];
};
static FILE *mfopen(const char *path, const char *mode)
{
#ifdef _WIN32
    wchar_t p[4096], m[16];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, p, 4096) ||
        !MultiByteToWideChar(CP_UTF8, 0, mode, -1, m, 16))
        return NULL;
    return _wfopen(p, m);
#else
    return fopen(path, mode);
#endif
}
static int remove_file(const char *path)
{
#ifdef _WIN32
    wchar_t p[4096];
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, p, 4096) && DeleteFileW(p);
#else
    return remove(path) == 0;
#endif
}
static int replace_file(const char *src, const char *dest)
{
#ifdef _WIN32
    wchar_t a[4096], b[4096];
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, a, 4096) &&
           MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, dest, -1, b, 4096) &&
           MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    return rename(src, dest) == 0;
#endif
}
static int clean(const char *s, size_t capacity)
{
    if (!s || !*s || strlen(s) >= capacity)
        return 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned c = *p++;
        if (c < 128) {
            if (c < 32 || c == 127)
                return 0;
            continue;
        }
        unsigned need = c >= 0xc2 && c <= 0xdf   ? 1
                        : c >= 0xe0 && c <= 0xef ? 2
                        : c >= 0xf0 && c <= 0xf4 ? 3
                                                 : 99;
        if (need == 99)
            return 0;
        unsigned cp = c & ((1u << (6 - need)) - 1);
        for (unsigned n = 0; n < need; n++) {
            if ((*p & 0xc0) != 0x80)
                return 0;
            cp = (cp << 6) | (*p++ & 63);
        }
        if (cp < (need == 1   ? 128u
                  : need == 2 ? 2048u
                              : 65536u) ||
            cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return 0;
    }
    return 1;
}
static void display_name(char *out, size_t capacity, const char *source)
{
    size_t n = strlen(source);
    if (n < capacity) {
        memcpy(out, source, n + 1);
        return;
    }
    n = capacity - 4;
    while (n && ((unsigned char)source[n] & 0xc0) == 0x80)
        n--;
    memcpy(out, source, n);
    memcpy(out + n, "...", 4);
}
static const char *base(const char *path)
{
    const char *s = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\')
            s = p + 1;
    return s;
}
static int absolute_path(const char *path, const char *parent, char out[ML_PATH])
{
    if (!clean(path, ML_PATH) || strstr(path, "://"))
        return 0;
    if (!strncmp(path, "gt4:", 4)) {
        unsigned id;
        char tail;
        if (sscanf(path + 4, "%u%c", &id, &tail) != 1 || id >= 256)
            return 0;
        strcpy(out, path);
        return 1;
    }
    char joined[ML_PATH];
    int absolute =
        path[0] == '/' || path[0] == '\\' || (isalpha((unsigned char)path[0]) && path[1] == ':');
    if (!absolute && parent && *parent) {
        const char *tail = base(parent);
        size_t n = (size_t)(tail - parent);
        if (n + strlen(path) >= sizeof joined)
            return 0;
        memcpy(joined, parent, n);
        strcpy(joined + n, path);
        path = joined;
    }
#ifdef _WIN32
    wchar_t w[4096], full[4096];
    if (strchr(path, '"') || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w, 4096))
        return 0;
    DWORD n = GetFullPathNameW(w, 4096, full, NULL);
    return n && n < 4096 && WideCharToMultiByte(CP_UTF8, 0, full, -1, out, ML_PATH, NULL, NULL) > 0;
#else
    if (path[0] == '/') {
        strcpy(out, path);
        return 1;
    }
    char cwd[ML_PATH];
    if (!getcwd(cwd, sizeof cwd))
        return 0;
    return snprintf(out, ML_PATH, "%s/%s", cwd, path) < ML_PATH;
#endif
}
int music_library_available(const char *path)
{
    if (path && !strncmp(path, "gt4:", 4))
        return 1;
    FILE *f = mfopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}
static void status(music_library *m, const char *text)
{
    SDL_LockMutex(m->status_lock);
    snprintf(m->status, sizeof m->status, "%s", text);
    SDL_UnlockMutex(m->status_lock);
}
static void publish(music_library *m)
{
    for (int c = 0; c < ML_CONTEXTS; c++) {
        int id = m->model.assigned[c];
        SDL_AtomicSet(&m->valid[c], id >= 0 && id < m->model.count && m->model.lists[id].count > 0);
    }
    SDL_AtomicAdd(&m->revision, 1);
}
static int write_model(music_library *m)
{
    char tmp[ML_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", m->path);
    FILE *f = mfopen(tmp, "wbx");
    if (!f)
        return 0;
    int ok = fprintf(f, "MoonMusic=1\n") > 0;
    for (int c = 0; c < ML_CONTEXTS; c++)
        ok &= fprintf(f, "context=%d,%d\n", c, m->model.assigned[c]) > 0;
    for (int n = 0; n < m->model.count; n++) {
        ml_playlist *p = &m->model.lists[n];
        ok &= fprintf(f, "playlist=%s\nshuffle=%d\n", p->name, p->shuffle) > 0;
        if (p->catalog)
            ok &= fprintf(f, "catalog=1\n") > 0;
        for (int i = 0; i < p->count; i++)
            ok &= fprintf(f, "track=%d\t%s\t%s\n", p->tracks[i].enabled, p->tracks[i].title,
                          p->tracks[i].path) > 0;
        ok &= fprintf(f, "end=\n") > 0;
    }
    if (fflush(f) || ferror(f))
        ok = 0;
    if (fclose(f))
        ok = 0;
    if (ok)
        ok = replace_file(tmp, m->path);
    if (!ok)
        remove_file(tmp);
    return ok;
}
static int read_line(FILE *f, char *line, size_t capacity)
{
    size_t n = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == 0 || n + 1 >= capacity)
            return -1;
        if (c == '\n')
            break;
        line[n++] = (char)c;
    }
    if (ferror(f))
        return -1;
    if (c == EOF && !n)
        return 0;
    if (n && line[n - 1] == '\r')
        n--;
    line[n] = 0;
    return 1;
}
static int load_model(music_library *m)
{
    FILE *f = mfopen(m->path, "rb");
    if (!f)
        return 1;
    ml_model *v = calloc(1, sizeof *v);
    if (!v) {
        fclose(f);
        return 0;
    }
    for (int c = 0; c < ML_CONTEXTS; c++)
        v->assigned[c] = -1;
    char line[ML_PATH + 160];
    int ok = 1, open = -1, header = 0, read = 0;
    while ((read = read_line(f, line, sizeof line)) > 0) {
        line[strcspn(line, "\r\n")] = 0;
        if (!header) {
            header = !strcmp(line, "MoonMusic=1");
            if (!header) {
                ok = 0;
                break;
            }
            continue;
        }
        if (!strncmp(line, "context=", 8)) {
            int c, id;
            char extra;
            if (sscanf(line + 8, "%d,%d%c", &c, &id, &extra) != 2 || c < 0 || c >= ML_CONTEXTS ||
                id < -1 || id >= ML_LISTS) {
                ok = 0;
                break;
            }
            v->assigned[c] = id;
        } else if (!strncmp(line, "playlist=", 9)) {
            if (open >= 0 || v->count == ML_LISTS || !clean(line + 9, 64)) {
                ok = 0;
                break;
            }
            open = v->count++;
            strcpy(v->lists[open].name, line + 9);
        } else if (!strcmp(line, "catalog=1") && open >= 0)
            v->lists[open].catalog = 1;
        else if (!strncmp(line, "shuffle=", 8) && open >= 0 &&
                 (!strcmp(line + 8, "0") || !strcmp(line + 8, "1")))
            v->lists[open].shuffle = line[8] == '1';
        else if (!strncmp(line, "track=", 6) && open >= 0) {
            ml_playlist *p = &v->lists[open];
            char *title = strchr(line + 6, '\t'), *path = title ? strchr(title + 1, '\t') : NULL;
            if (p->count == ML_TRACKS || !title || !path || title != line + 7 ||
                (line[6] != '0' && line[6] != '1')) {
                ok = 0;
                break;
            }
            *title++ = 0;
            *path++ = 0;
            if (!clean(title, 128) || !clean(path, ML_PATH)) {
                ok = 0;
                break;
            }
            ml_track *t = &p->tracks[p->count++];
            t->enabled = line[6] == '1';
            strcpy(t->title, title);
            strcpy(t->path, path);
        } else if (!strcmp(line, "end=") && open >= 0)
            open = -1;
        else {
            ok = 0;
            break;
        }
    }
    if (read < 0 || ferror(f) || !header || open >= 0)
        ok = 0;
    for (int c = 0; c < ML_CONTEXTS; c++)
        if (v->assigned[c] >= v->count)
            ok = 0;
    fclose(f);
    if (ok)
        m->model = *v;
    free(v);
    return ok;
}
/* A mutation is committed atomically, or the in-memory model is restored too. */
static ml_model *begin(music_library *m)
{
    if (!m)
        return NULL;
    ml_model *old = malloc(sizeof *old);
    if (!old)
        return NULL;
    SDL_LockMutex(m->mutex);
    *old = m->model;
    return old;
}
static int commit(music_library *m, ml_model *old, int valid)
{
    int ok = valid && write_model(m);
    if (ok)
        publish(m);
    else
        m->model = *old;
    SDL_UnlockMutex(m->mutex);
    free(old);
    return ok;
}
int music_library_count(music_library *m)
{
    return m ? m->model.count : 0;
}
const ml_playlist *music_library_list(music_library *m, int n)
{
    return m && n >= 0 && n < m->model.count ? &m->model.lists[n] : NULL;
}
int music_library_create(music_library *m, const char *name)
{
    if (!clean(name, 64))
        return -1;
    ml_model *old = begin(m);
    if (!old)
        return -1;
    int id = m->model.count;
    int ok = id < ML_LISTS;
    if (ok) {
        memset(&m->model.lists[id], 0, sizeof(ml_playlist));
        strcpy(m->model.lists[id].name, name);
        m->model.count++;
    }
    return commit(m, old, ok) ? id : -1;
}
void music_library_disc(music_library *m, const char *disc)
{
    if (!m || !disc || strlen(disc) >= ML_PATH)
        return;
    SDL_LockMutex(m->mutex);
    if (strcmp(m->disc, disc)) {
        strcpy(m->disc, disc);
        publish(m);
    }
    SDL_UnlockMutex(m->mutex);
}
int music_library_catalog(music_library *m)
{
    if (!m)
        return -1;
    for (int i = 0; i < m->model.count; i++)
        if (m->model.lists[i].catalog)
            return i;
    int id = music_library_create(m, "Library");
    if (id < 0)
        return -1;
    ml_model *old = begin(m);
    if (!old)
        return -1;
    m->model.lists[id].catalog = 1;
    return commit(m, old, 1) ? id : -1;
}
int music_library_replace(music_library *m, int id, const ml_track *tracks, int count, int shuffle)
{
    const ml_playlist *p = music_library_list(m, id);
    if (!p || count < 0 || count > ML_TRACKS || (count && !tracks))
        return 0;
    for (int i = 0; i < count; i++)
        if (!clean(tracks[i].path, ML_PATH) || !clean(tracks[i].title, 128))
            return 0;
    if (p->count == count && p->shuffle == !!shuffle &&
        (!count || !memcmp(p->tracks, tracks, (size_t)count * sizeof *tracks)))
        return 1;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    p = &m->model.lists[id];
    if (count)
        memmove(m->model.lists[id].tracks, tracks, (size_t)count * sizeof *tracks);
    m->model.lists[id].count = count;
    m->model.lists[id].shuffle = !!shuffle;
    return commit(m, old, 1);
}
int music_library_rename(music_library *m, int id, const char *name)
{
    if (!music_library_list(m, id) || !clean(name, 64))
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    strcpy(m->model.lists[id].name, name);
    return commit(m, old, 1);
}
int music_library_duplicate(music_library *m, int id)
{
    if (!music_library_list(m, id))
        return -1;
    ml_model *old = begin(m);
    if (!old)
        return -1;
    int n = m->model.count, ok = n < ML_LISTS;
    if (ok) {
        m->model.lists[n] = m->model.lists[id];
        display_name(m->model.lists[n].name, 58, m->model.lists[id].name);
        strcat(m->model.lists[n].name, " copy");
        m->model.count++;
    }
    return commit(m, old, ok) ? n : -1;
}
int music_library_delete(music_library *m, int id)
{
    if (music_library_list(m, id) && music_library_list(m, id)->catalog)
        return 0;
    if (!music_library_list(m, id))
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    memmove(&m->model.lists[id], &m->model.lists[id + 1],
            (size_t)(m->model.count - id - 1) * sizeof(ml_playlist));
    m->model.count--;
    for (int c = 0; c < ML_CONTEXTS; c++) {
        if (m->model.assigned[c] == id)
            m->model.assigned[c] = -1;
        else if (m->model.assigned[c] > id)
            m->model.assigned[c]--;
    }
    SDL_AtomicSet(&m->preview, -1);
    return commit(m, old, 1);
}
static int path_equal(const char *a, const char *b);
int music_library_add(music_library *m, int id, const char *path, const char *title)
{
    if (!music_library_list(m, id))
        return 0;
    char full[ML_PATH];
    if (!absolute_path(path, NULL, full))
        return 0;
    const ml_playlist *existing = music_library_list(m, id);
    if (existing->catalog)
        for (int i = 0; i < existing->count; i++)
            if (path_equal(existing->tracks[i].path, full))
                return 1;
    char label[128];
    if (title && *title) {
        if (!clean(title, 128))
            return 0;
        strcpy(label, title);
    } else {
        if (!clean(base(full), ML_PATH))
            return 0;
        display_name(label, sizeof label, base(full));
    }
    const char *name = label;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    ml_playlist *p = &m->model.lists[id];
    int ok = p->count < ML_TRACKS;
    if (ok) {
        ml_track *t = &p->tracks[p->count++];
        strcpy(t->path, full);
        strcpy(t->title, name);
        t->enabled = 1;
    }
    return commit(m, old, ok);
}
int music_library_remove(music_library *m, int id, int row)
{
    const ml_playlist *p = music_library_list(m, id);
    if (!p || row < 0 || row >= p->count)
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    ml_playlist *w = &m->model.lists[id];
    memmove(&w->tracks[row], &w->tracks[row + 1], (size_t)(w->count - row - 1) * sizeof(ml_track));
    w->count--;
    return commit(m, old, 1);
}
int music_library_move(music_library *m, int id, int row, int delta)
{
    const ml_playlist *p = music_library_list(m, id);
    int to = row + delta;
    if (!p || row < 0 || row >= p->count || to < 0 || to >= p->count)
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    ml_track t = m->model.lists[id].tracks[row];
    m->model.lists[id].tracks[row] = m->model.lists[id].tracks[to];
    m->model.lists[id].tracks[to] = t;
    return commit(m, old, 1);
}
int music_library_track_rename(music_library *m, int id, int row, const char *title)
{
    const ml_playlist *p = music_library_list(m, id);
    if (!p || row < 0 || row >= p->count || !clean(title, 128))
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    strcpy(m->model.lists[id].tracks[row].title, title);
    return commit(m, old, 1);
}
int music_library_track_enabled(music_library *m, int id, int row)
{
    const ml_playlist *p = music_library_list(m, id);
    if (!p || row < 0 || row >= p->count)
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    m->model.lists[id].tracks[row].enabled ^= 1;
    return commit(m, old, 1);
}
int music_library_shuffle(music_library *m, int id)
{
    if (!music_library_list(m, id))
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    m->model.lists[id].shuffle ^= 1;
    return commit(m, old, 1);
}
int music_library_jukebox_commit(music_library *m, int id, const int *order, const int *enabled,
                                 int count, int shuffle)
{
    const ml_playlist *p = music_library_list(m, id);
    if (!p || count != p->count || !order || !enabled)
        return 0;
    unsigned char seen[ML_TRACKS] = {0};
    for (int i = 0; i < count; i++)
        if (order[i] < 0 || order[i] >= count || seen[order[i]]++)
            return 0;
    int changed = p->shuffle != !!shuffle;
    for (int i = 0; i < count; i++)
        changed |= order[i] != i || p->tracks[i].enabled != !!enabled[i];
    if (!changed)
        return 1;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    ml_playlist *w = &m->model.lists[id];
    for (int i = 0; i < count; i++) {
        w->tracks[i] = old->lists[id].tracks[order[i]];
        w->tracks[i].enabled = !!enabled[order[i]];
    }
    w->shuffle = !!shuffle;
    return commit(m, old, 1);
}
int music_library_assign(music_library *m, int context, int id)
{
    if (!m || context < 0 || context >= ML_CONTEXTS || id < -1 || id >= m->model.count)
        return 0;
    ml_model *old = begin(m);
    if (!old)
        return 0;
    m->model.assigned[context] = id;
    return commit(m, old, 1);
}
int music_library_assignment(music_library *m, int c)
{
    return m && c >= 0 && c < ML_CONTEXTS ? m->model.assigned[c] : -1;
}
static int path_equal(const char *a, const char *b)
{
#ifdef _WIN32
    while (*a && *b) {
        int x = *a == '\\' ? '/' : (unsigned char)*a, y = *b == '\\' ? '/' : (unsigned char)*b;
        if (tolower(x) != tolower(y))
            return 0;
        a++;
        b++;
    }
    return *a == *b;
#else
    return !strcmp(a, b);
#endif
}
static const char *relative_reference(const char *source, const char *destination,
                                      char out[ML_PATH])
{
    if (!strncmp(source, "gt4:", 4))
        return source;
    char src[ML_PATH], dst[ML_PATH];
    snprintf(src, sizeof src, "%s", source);
    snprintf(dst, sizeof dst, "%s", destination);
    for (char *p = src; *p; p++)
        if (*p == '\\')
            *p = '/';
    for (char *p = dst; *p; p++)
        if (*p == '\\')
            *p = '/';
    char *tail = strrchr(dst, '/');
    if (!tail)
        return source;
    tail[1] = 0;
#ifdef _WIN32
    if (tolower((unsigned char)src[0]) != tolower((unsigned char)dst[0]) || src[1] != ':' ||
        dst[1] != ':')
        return source;
#endif
    size_t last = 0, i = 0;
    while (src[i] && dst[i]) {
        int a = (unsigned char)src[i], b = (unsigned char)dst[i];
#ifdef _WIN32
        a = tolower(a);
        b = tolower(b);
#endif
        if (a != b)
            break;
        if (src[i] == '/')
            last = i + 1;
        i++;
    }
    if (!last)
        return source;
    size_t n = 0;
    for (i = last; dst[i]; i++)
        if (dst[i] == '/') {
            if (n + 3 >= ML_PATH)
                return source;
            memcpy(out + n, "../", 3);
            n += 3;
        }
    if (n + strlen(src + last) >= ML_PATH)
        return source;
    strcpy(out + n, src + last);
    return out;
}
int music_library_export(music_library *m, int id, const char *path)
{
    const ml_playlist *p = music_library_list(m, id);
    if (!p || !clean(path, ML_PATH))
        return 0;
    char full[ML_PATH], protected_path[ML_PATH];
    if (!absolute_path(path, NULL, full))
        return 0;
    if (absolute_path(m->path, NULL, protected_path) && path_equal(full, protected_path))
        return 0;
    if (absolute_path(m->store->path, NULL, protected_path) && path_equal(full, protected_path))
        return 0;
    for (int n = 0; n < m->model.count; n++)
        for (int t = 0; t < m->model.lists[n].count; t++)
            if (path_equal(full, m->model.lists[n].tracks[t].path))
                return 0;
    char tmp[ML_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = mfopen(tmp, "wbx");
    if (!f)
        return 0;
    int ok = fprintf(f, "#EXTM3U\n#PLAYLIST:%s\n#MOON_SHUFFLE:%d\n", p->name, p->shuffle) > 0;
    /* Relative references on the same volume; audio itself is never copied. */
    for (int i = 0; i < p->count; i++) {
        const ml_track *t = &p->tracks[i];
        char relative[ML_PATH];
        const char *ref = relative_reference(t->path, full, relative);
        ok &= fprintf(f, "#EXTINF:-1,%s\n#MOON_ENABLED:%d\n%s\n", t->title, t->enabled, ref) > 0;
    }
    if (fflush(f) || ferror(f))
        ok = 0;
    if (fclose(f))
        ok = 0;
    if (ok)
        ok = replace_file(tmp, path);
    if (!ok)
        remove_file(tmp);
    return ok;
}
int music_library_import(music_library *m, const char *path)
{
    if (!m || !clean(path, ML_PATH))
        return -1;
    FILE *f = mfopen(path, "rb");
    if (!f)
        return -1;
    ml_playlist *list = calloc(1, sizeof *list);
    if (!list) {
        fclose(f);
        return -1;
    }
    display_name(list->name, 64, base(path));
    char line[ML_PATH + 160], title[128] = "";
    int enabled = 1, ok = 1, header = 0, read = 0;
    while ((read = read_line(f, line, sizeof line)) > 0) {
        line[strcspn(line, "\r\n")] = 0;
        char *s = line;
        if (strlen(s) >= 3 && !memcmp(s, "\xef\xbb\xbf", 3))
            s += 3;
        if (!*s)
            continue;
        if (!strcmp(s, "#EXTM3U"))
            header = 1;
        else if (!strncmp(s, "#PLAYLIST:", 10)) {
            if (!clean(s + 10, 64)) {
                ok = 0;
                break;
            }
            strcpy(list->name, s + 10);
        } else if (!strncmp(s, "#EXTINF:", 8)) {
            char *comma = strchr(s, ',');
            if (comma && clean(comma + 1, 128))
                strcpy(title, comma + 1);
        } else if (!strcmp(s, "#MOON_ENABLED:0"))
            enabled = 0;
        else if (!strcmp(s, "#MOON_ENABLED:1"))
            enabled = 1;
        else if (!strcmp(s, "#MOON_SHUFFLE:1"))
            list->shuffle = 1;
        else if (*s != '#') {
            if (list->count == ML_TRACKS) {
                ok = 0;
                break;
            }
            ml_track *t = &list->tracks[list->count];
            if (!absolute_path(s, path, t->path)) {
                ok = 0;
                break;
            }
            const char *name = *title ? title : base(t->path);
            if (!clean(name, ML_PATH)) {
                ok = 0;
                break;
            }
            display_name(t->title, sizeof t->title, name);
            t->enabled = enabled;
            list->count++;
            title[0] = 0;
            enabled = 1;
        }
    }
    if (read < 0 || ferror(f) || (!list->count && !header))
        ok = 0;
    fclose(f);
    if (!ok) {
        free(list);
        return -1;
    }
    ml_model *old = begin(m);
    if (!old) {
        free(list);
        return -1;
    }
    int id = m->model.count;
    ok = id < ML_LISTS;
    if (ok) {
        m->model.lists[id] = *list;
        m->model.count++;
    }
    free(list);
    return commit(m, old, ok) ? id : -1;
}

/* Decoder: normalized local WAV directly; everything else through the libav
   linked into the binary (tools/build_libav.sh), to 48 kHz stereo s16. Without
   linked libav, Windows still streams from an existing ffmpeg.exe. */
typedef struct {
    FILE *wav;
    gt4_pcm_music *game;
    uint32_t remaining;
#ifdef MOON_LINKED_MOVIES
    AVFormatContext *format;
    AVCodecContext *codec;
    SwrContext *swr;
    AVPacket *packet;
    AVFrame *frame;
    int16_t *pending;
    int stream, ended, pending_frames, pending_at, pending_cap;
#elif defined(_WIN32)
    HANDLE pipe, process;
#endif
} decoder;
#ifdef MOON_LINKED_MOVIES
static int libav_open(decoder *d, const char *path)
{
    /* libav's file protocol takes UTF-8 paths on Windows too */
    if (avformat_open_input(&d->format, path, NULL, NULL) < 0)
        return 0;
    if (avformat_find_stream_info(d->format, NULL) < 0)
        return 0;
    const AVCodec *codec = NULL;
    int s = av_find_best_stream(d->format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (s < 0 || !codec)
        return 0;
    for (unsigned i = 0; i < d->format->nb_streams; i++)
        if ((int)i != s)
            d->format->streams[i]->discard = AVDISCARD_ALL; /* cover art, video */
    d->stream = s;
    d->codec = avcodec_alloc_context3(codec);
    if (!d->codec ||
        avcodec_parameters_to_context(d->codec, d->format->streams[s]->codecpar) < 0 ||
        avcodec_open2(d->codec, codec, NULL) < 0)
        return 0;
    if (d->codec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&d->codec->ch_layout, d->codec->ch_layout.nb_channels);
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&d->swr, &stereo, AV_SAMPLE_FMT_S16, 48000, &d->codec->ch_layout,
                            d->codec->sample_fmt, d->codec->sample_rate, 0, NULL) < 0 ||
        swr_init(d->swr) < 0)
        return 0;
    d->packet = av_packet_alloc();
    d->frame = av_frame_alloc();
    return d->packet && d->frame;
}
static int libav_convert(decoder *d, const AVFrame *frame)
{
    int in = frame ? frame->nb_samples : 0, need = swr_get_out_samples(d->swr, in);
    if (need <= 0)
        return 0;
    if (need > d->pending_cap) {
        int16_t *grown = realloc(d->pending, (size_t)need * 4);
        if (!grown)
            return -1;
        d->pending = grown;
        d->pending_cap = need;
    }
    uint8_t *out = (uint8_t *)d->pending;
    int n = swr_convert(d->swr, &out, need, frame ? (const uint8_t **)frame->extended_data : NULL,
                        in);
    d->pending_frames = n > 0 ? n : 0;
    d->pending_at = 0;
    return n;
}
static int libav_read(decoder *d, int16_t *out, int frames)
{
    int done = 0;
    while (done < frames) {
        if (d->pending_at < d->pending_frames) {
            int take = d->pending_frames - d->pending_at;
            if (take > frames - done)
                take = frames - done;
            memcpy(out + done * 2, d->pending + d->pending_at * 2, (size_t)take * 4);
            d->pending_at += take;
            done += take;
            continue;
        }
        if (d->ended)
            break;
        int r = avcodec_receive_frame(d->codec, d->frame);
        if (!r) {
            r = libav_convert(d, d->frame);
            av_frame_unref(d->frame);
            if (r < 0)
                d->ended = 1;
        } else if (r == AVERROR(EAGAIN)) {
            if (av_read_frame(d->format, d->packet) < 0) {
                avcodec_send_packet(d->codec, NULL); /* drain the decoder */
                continue;
            }
            if (d->packet->stream_index == d->stream)
                avcodec_send_packet(d->codec, d->packet); /* a bad packet is skipped */
            av_packet_unref(d->packet);
        } else {
            libav_convert(d, NULL); /* the resampler's tail */
            d->ended = 1;
        }
    }
    return done ? done : -1;
}
#endif
static uint32_t u32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void decoder_close(decoder *d)
{
    gt4_pcm_music_close(d->game);
    if (d->wav)
        fclose(d->wav);
#ifdef MOON_LINKED_MOVIES
    avcodec_free_context(&d->codec);
    avformat_close_input(&d->format);
    swr_free(&d->swr);
    av_packet_free(&d->packet);
    av_frame_free(&d->frame);
    free(d->pending);
#elif defined(_WIN32)
    if (d->pipe)
        CloseHandle(d->pipe);
    if (d->process) {
        if (WaitForSingleObject(d->process, 0) == WAIT_TIMEOUT) {
            TerminateProcess(d->process, 0);
            WaitForSingleObject(d->process, 2000);
        }
        CloseHandle(d->process);
    }
#endif
    memset(d, 0, sizeof *d);
}
static int decoder_open(decoder *d, const char *path, const char *disc)
{
    memset(d, 0, sizeof *d);
    if (!strncmp(path, "gt4:", 4)) {
        unsigned index;
        char tail;
        if (sscanf(path + 4, "%u%c", &index, &tail) != 1 || !disc || !*disc)
            return 0;
        const char *error = NULL;
        gt4_vol *v = gt4_vol_open(disc, &error);
        if (v) {
            d->game = gt4_pcm_music_open(v, "racing", index);
            gt4_vol_close(v);
        }
        if (d->game)
            gt4_pcm_music_once(d->game, 1);
        return d->game != NULL;
    }
    FILE *f = mfopen(path, "rb");
    if (!f)
        return 0;
    unsigned char h[16];
    int format = 0;
    if (fread(h, 1, 12, f) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4)) {
        for (int n = 0; n < 128 && fread(h, 1, 8, f) == 8; n++) {
            uint32_t size = u32(h + 4);
            if (size > 0x7fffffffu)
                break;
            if (!memcmp(h, "fmt ", 4) && size >= 16) {
                if (fread(h, 1, 16, f) != 16)
                    break;
                format = h[0] == 1 && h[1] == 0 && h[2] == 2 && h[3] == 0 && u32(h + 4) == 48000 &&
                         h[14] == 16 && h[15] == 0;
                if (fseek(f, (long)(size - 16 + (size & 1)), SEEK_CUR))
                    break;
            } else if (!memcmp(h, "data", 4) && format && size >= 4 && !(size & 3)) {
                d->wav = f;
                d->remaining = size;
                return 1;
            } else if (fseek(f, (long)(size + (size & 1)), SEEK_CUR))
                break;
        }
    }
    fclose(f);
#ifdef MOON_LINKED_MOVIES
    if (libav_open(d, path))
        return 1;
    decoder_close(d);
    return 0;
#elif defined(_WIN32)
    wchar_t exe[4096] = L"", input[4096];
    char *pref = SDL_GetPrefPath("MoonEngine", "MoonEngine");
    if (pref) {
        char name[ML_PATH];
        snprintf(name, sizeof name, "%sffmpeg.exe", pref);
        MultiByteToWideChar(CP_UTF8, 0, name, -1, exe, 4096);
        SDL_free(pref);
    }
    if (!*exe || GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
        GetModuleFileNameW(NULL, exe, 4096);
        wchar_t *p = wcsrchr(exe, L'\\');
        if (p)
            wcscpy(p + 1, L"ffmpeg.exe");
    }
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
#ifdef MOON_FFMPEG
        MultiByteToWideChar(CP_UTF8, 0, MOON_FFMPEG, -1, exe, 4096);
        if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES)
#endif
        {
            if (!SearchPathW(NULL, L"ffmpeg.exe", NULL, 4096, exe, NULL))
                return 0;
        }
    }
    if (strchr(path, '"') ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, input, 4096))
        return 0;
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0))
        return 0;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nil = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = {0};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nil;
    si.hStdError = nil;
    si.hStdOutput = wr;
    wchar_t command[10000];
    _snwprintf(
        command, 10000,
        L"\"%ls\" -nostdin -v error -protocol_whitelist file,pipe -i \"%ls\" -vn -sn -dn -ac 2 -ar 48000 -f s16le pipe:1",
        exe, input);
    PROCESS_INFORMATION pi = {0};
    int ok =
        CreateProcessW(exe, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi) != 0;
    CloseHandle(wr);
    if (nil != INVALID_HANDLE_VALUE)
        CloseHandle(nil);
    if (!ok) {
        CloseHandle(rd);
        return 0;
    }
    CloseHandle(pi.hThread);
    d->pipe = rd;
    d->process = pi.hProcess;
    return 1;
#else
    return 0;
#endif
}
/* -1 EOF, 0 temporarily unavailable, positive complete stereo frames. */
static int decoder_read(decoder *d, int16_t *out, int frames)
{
    if (d->game) {
        if (gt4_pcm_music_ended(d->game))
            return -1;
        gt4_pcm_music_render(out, frames, d->game);
        return frames;
    }
    if (d->wav) {
        unsigned take = (unsigned)frames;
        if (take > d->remaining / 4)
            take = d->remaining / 4;
        if (!take)
            return -1;
        size_t n = fread(out, 4, take, d->wav);
        d->remaining -= (uint32_t)n * 4;
        return n ? (int)n : -1;
    }
#ifdef MOON_LINKED_MOVIES
    return d->format ? libav_read(d, out, frames) : -1;
#elif defined(_WIN32)
    DWORD available = 0, n = 0;
    if (!PeekNamedPipe(d->pipe, NULL, 0, NULL, &available, NULL))
        return -1;
    if (available < 4)
        return WaitForSingleObject(d->process, 0) == WAIT_TIMEOUT ? 0 : -1;
    DWORD take = (DWORD)frames * 4;
    if (take > available)
        take = available & ~3u;
    if (!ReadFile(d->pipe, out, take, &n, NULL))
        return -1;
    return n ? (int)(n / 4) : -1;
#else
    return -1;
#endif
}
static int player_thread(void *user)
{
    music_library *m = user;
    decoder d = {0};
    ml_playlist *p = malloc(sizeof *p);
    if (!p)
        return 0;
    int revision = -1, context = -1, index = -1, failures = 0, file_data = 0;
    int order[ML_TRACKS] = {0}, order_at = ML_TRACKS;
    /* Each playback context keeps its own pass through the playlist. Preview
       selection and returning from another context must not rewind it. */
    ml_playlist *history = calloc(ML_CONTEXTS, sizeof *history);
    int saved_order[ML_CONTEXTS][ML_TRACKS], saved_at[ML_CONTEXTS];
    if (!history) { free(p); return 0; }
    for (int c = 0; c < ML_CONTEXTS; c++) saved_at[c] = ML_TRACKS;
    uint32_t random = 0x341f234u;
    int16_t chunk[2048 * 2];
    char disc[ML_PATH] = "";
    while (!SDL_AtomicGet(&m->stop)) {
        int preview = SDL_AtomicGet(&m->preview),
            want = preview >= 0 ? ML_CONTEXTS : SDL_AtomicGet(&m->wanted),
            rev = SDL_AtomicGet(&m->revision);
        if (!SDL_AtomicGet(&m->enabled) && preview < 0)
            want = -1;
        if (want != context || rev != revision) {
            SDL_LockMutex(m->transport);
            SDL_AtomicSet(&m->ready, -1);
            SDL_AtomicSet(&m->write, 0);
            SDL_AtomicSet(&m->read, 0);
            SDL_UnlockMutex(m->transport);
            if (context >= 0 && context < ML_CONTEXTS) {
                memcpy(saved_order[context], order, sizeof order);
                saved_at[context] = order_at;
            }
            decoder_close(&d);
            context = want;
            revision = rev;
            index = -1;
            failures = 0;
            order_at = ML_TRACKS;
            p->count = 0;
            SDL_LockMutex(m->mutex);
            int id = preview >= 0                      ? preview
                     : want >= 0 && want < ML_CONTEXTS ? m->model.assigned[want]
                                                       : -1;
            if (id >= 0 && id < m->model.count)
                *p = m->model.lists[id];
            snprintf(disc, sizeof disc, "%s", m->disc);
            SDL_UnlockMutex(m->mutex);
            if (want >= 0 && want < ML_CONTEXTS) {
                if (!memcmp(&history[want], p, sizeof *p)) {
                    memcpy(order, saved_order[want], sizeof order);
                    order_at = saved_at[want];
                } else {
                    history[want] = *p;
                    saved_at[want] = ML_TRACKS;
                }
            }
            /* Consumer discards old-generation samples before ready is published. */
            if (preview >= 0) {
                int start = SDL_AtomicGet(&m->preview_track);
                if (start >= 0 && start < p->count) {
                    p->tracks[0] = p->tracks[start];
                    p->tracks[0].enabled = 1;
                    p->count = 1;
                    p->shuffle = 0;
                } else
                    p->count = 0;
                index = -1;
            }
        }
        if (preview >= 0 && SDL_AtomicGet(&m->preview_eof) == rev + 1) {
            SDL_Delay(4);
            continue;
        }
        if (context < 0 || !p->count || failures >= p->count) {
            if (preview >= 0)
                SDL_AtomicSet(&m->preview_eof, rev + 1);
            if (failures >= p->count && SDL_AtomicGet(&m->read) == SDL_AtomicGet(&m->write)) {
                SDL_LockMutex(m->transport);
                SDL_AtomicSet(&m->ready, -1);
                SDL_UnlockMutex(m->transport);
                if (p->count && failures == p->count) {
                    status(m, "No playable files in this playlist; original music is used.");
                    failures++;
                }
            }
            SDL_Delay(4);
            continue;
        }
        if (!d.wav && !d.game
#ifdef MOON_LINKED_MOVIES
            && !d.format
#elif defined(_WIN32)
            && !d.pipe
#endif
        ) {
            if (order_at >= p->count) {
                for (int i = 0; i < p->count; i++)
                    order[i] = i;
                if (p->shuffle)
                    for (int i = 0; i < p->count; i++) {
                        /* musiccontrol.h:27..38: original integer-range draw and
                       Array.move insertion, a complete pass without repeats. */
                        int at = (int)(gt4_pcm_music_random_unit(&random) * (float)(i + 1));
                        if (at > i)
                            at = i;
                        int value = order[i];
                        memmove(order + at + 1, order + at, (size_t)(i - at) * sizeof *order);
                        order[at] = value;
                    }
                order_at = 0;
            }
            index = order[order_at++];
            if (!p->tracks[index].enabled || !decoder_open(&d, p->tracks[index].path, disc)) {
                failures++;
                continue;
            }
            file_data = 0;
            status(m, p->tracks[index].title);
        }
        int r = SDL_AtomicGet(&m->read), w = SDL_AtomicGet(&m->write),
            space = (r - w - 1) & (RING - 1);
        if (((w - r) & (RING - 1)) >= 8192) {
            SDL_Delay(1);
            continue;
        }
        if (space < 2048) {
            SDL_Delay(1);
            continue;
        }
        int n = decoder_read(&d, chunk, space > 2048 ? 2048 : space);
        if (n < 0) {
            decoder_close(&d);
            if (preview >= 0)
                SDL_AtomicSet(&m->preview_eof, rev + 1);
            if (!file_data)
                failures++;
            else
                failures = 0;
            continue;
        }
        if (!n) {
            SDL_Delay(1);
            continue;
        }
        failures = 0;
        file_data = 1;
        SDL_LockMutex(m->transport);
        if (rev != SDL_AtomicGet(&m->revision)) {
            SDL_UnlockMutex(m->transport);
            continue;
        }
        for (int i = 0; i < n; i++) {
            int at = (w + i) & (RING - 1);
            m->pcm[at * 2] = chunk[i * 2];
            m->pcm[at * 2 + 1] = chunk[i * 2 + 1];
        }
        SDL_AtomicSet(&m->write, (w + n) & (RING - 1));
        SDL_AtomicSet(&m->ready, context);
        SDL_AtomicSet(&m->ready_revision, rev);
        SDL_UnlockMutex(m->transport);
    }
    decoder_close(&d);
    free(history);
    free(p);
    return 0;
}
int music_library_render(music_library *m, int context, int16_t *out, int frames, int gain,
                         int paused)
{
    if (!m || !out || frames < 0 || context < 0 || context >= ML_CONTEXTS)
        return 0;
    int preview = SDL_AtomicGet(&m->preview);
    if (preview < 0 && (!SDL_AtomicGet(&m->enabled) || !SDL_AtomicGet(&m->valid[context])))
        return 0;
    int wanted = preview >= 0 ? ML_CONTEXTS : context;
    SDL_AtomicSet(&m->wanted, context);
    SDL_LockMutex(m->transport);
    if (SDL_AtomicGet(&m->ready) != wanted ||
        SDL_AtomicGet(&m->ready_revision) != SDL_AtomicGet(&m->revision)) {
        SDL_UnlockMutex(m->transport);
        return 0;
    }
    memset(out, 0, (size_t)frames * 4);
    if (paused) {
        SDL_UnlockMutex(m->transport);
        return 1;
    }
    int r = SDL_AtomicGet(&m->read), w = SDL_AtomicGet(&m->write), n = (w - r) & (RING - 1);
    if (n > frames)
        n = frames;
    if (gain < 0)
        gain = 0;
    if (gain > 256)
        gain = 256;
    for (int i = 0; i < n; i++) {
        int at = (r + i) & (RING - 1);
        for (int c = 0; c < 2; c++) {
            int value = (int)m->pcm[at * 2 + c] * gain / 128;
            out[i * 2 + c] = (int16_t)(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
        }
    }
    int peak = 0;
    for (int i = 0; i < n * 2; i++) {
        int value = out[i] < 0 ? -(int)out[i] : out[i];
        if (value > peak)
            peak = value;
    }
    if (peak > SDL_AtomicGet(&m->peak))
        SDL_AtomicSet(&m->peak, peak);
    SDL_AtomicAdd(&m->played, n);
    SDL_AtomicSet(&m->read, (r + n) & (RING - 1));
    SDL_UnlockMutex(m->transport);
    return 1;
}
static int hook(void *m, int c, int16_t *out, int n, int gain, int paused)
{
    return music_library_render(m, c, out, n, gain, paused);
}
music_library *music_library_open(moon_options *store)
{
    if (!store || strlen(store->path) + 6 >= ML_PATH)
        return NULL;
    music_library *m = calloc(1, sizeof *m);
    if (!m)
        return NULL;
    m->store = store;
    m->mutex = SDL_CreateMutex();
    m->transport = SDL_CreateMutex();
    m->status_lock = SDL_CreateMutex();
    if (!m->mutex || !m->transport || !m->status_lock) {
        SDL_DestroyMutex(m->status_lock);
        SDL_DestroyMutex(m->transport);
        SDL_DestroyMutex(m->mutex);
        free(m);
        return NULL;
    }
    snprintf(m->path, sizeof m->path, "%s.music", store->path);
    for (int c = 0; c < ML_CONTEXTS; c++)
        m->model.assigned[c] = -1;
    if (!load_model(m))
        strcpy(m->status, "Library file is invalid; it has not been replaced.");
    SDL_AtomicSet(&m->wanted, -1);
    SDL_AtomicSet(&m->ready, -1);
    SDL_AtomicSet(&m->preview, -1);
    SDL_AtomicSet(&m->preview_track, -1);
    publish(m);
    music_library_sync(m);
    m->thread = SDL_CreateThread(player_thread, "custom music streaming", m);
    if (!m->thread) {
        SDL_DestroyMutex(m->status_lock);
        SDL_DestroyMutex(m->transport);
        SDL_DestroyMutex(m->mutex);
        free(m);
        return NULL;
    }
    return m;
}
void music_library_sync(music_library *m)
{
    if (m)
        SDL_AtomicSet(&m->enabled, moon_options_int(m->store, "audio.custom_music", 0, 0, 1));
}
int music_library_enabled(music_library *m)
{
    return m ? SDL_AtomicGet(&m->enabled) : 0;
}
int music_library_enable(music_library *m, int on)
{
    if (!m)
        return 0;
    moon_options old = *m->store;
    if (!moon_options_set(m->store, "audio.custom_music", on ? "1" : "0") ||
        !moon_options_save(m->store)) {
        *m->store = old;
        return 0;
    }
    music_library_sync(m);
    if (!on)
        music_library_preview(m, -1, -1);
    return 1;
}
void music_library_preview(music_library *m, int list, int track)
{
    if (!m)
        return;
    SDL_AtomicSet(&m->preview_track, track);
    SDL_AtomicSet(&m->preview, list);
    SDL_AtomicAdd(&m->revision, 1);
}
int music_library_preview_playing(music_library *m)
{
    if (!m || SDL_AtomicGet(&m->preview) < 0)
        return 0;
    return SDL_AtomicGet(&m->preview_eof) != SDL_AtomicGet(&m->revision) + 1 ||
           SDL_AtomicGet(&m->read) != SDL_AtomicGet(&m->write);
}
void music_library_status(music_library *m, char *out, int bytes)
{
    if (!m || bytes < 1)
        return;
    SDL_LockMutex(m->status_lock);
    snprintf(out, (size_t)bytes, "%s", m->status);
    SDL_UnlockMutex(m->status_lock);
}
void music_library_close(music_library *m)
{
    if (!m)
        return;
    moon_music_override_clear(m);
    SDL_AtomicSet(&m->stop, 1);
    if (m->thread)
        SDL_WaitThread(m->thread, NULL);
    SDL_DestroyMutex(m->status_lock);
    SDL_DestroyMutex(m->transport);
    SDL_DestroyMutex(m->mutex);
    free(m);
}
/* The app registers only after its complete audio lifecycle is available. */
static int title_hook(void *user, int context, char *out, int capacity)
{
    music_library *m = user;
    if (!m || capacity < 1 || (!SDL_AtomicGet(&m->enabled) && SDL_AtomicGet(&m->preview) < 0))
        return 0;
    int wanted = SDL_AtomicGet(&m->preview) >= 0 ? ML_CONTEXTS : context;
    if (SDL_AtomicGet(&m->ready) != wanted ||
        SDL_AtomicGet(&m->ready_revision) != SDL_AtomicGet(&m->revision))
        return 0;
    music_library_status(m, out, capacity);
    return 1;
}
void music_library_install(music_library *m)
{
    moon_music_override_set(m ? hook : NULL, m);
    moon_music_override_titles(m ? title_hook : NULL);
}

unsigned music_library_played(music_library *m)
{
    return m ? (unsigned)SDL_AtomicGet(&m->played) : 0;
}
int music_library_peak(music_library *m)
{
    return m ? SDL_AtomicGet(&m->peak) : 0;
}
