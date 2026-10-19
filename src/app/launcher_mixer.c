#include "launcher_mixer.h"
#include <stdio.h>
#include <string.h>
void launcher_mixer_load(launcher_mixer *m, const moon_options *store)
{
    memset(m, 0, sizeof *m);
    m->music = moon_options_int(store, "audio.launcher_music_volume", 78, 0, 128);
    m->effects = moon_options_int(store, "audio.launcher_effects_volume", 128, 0, 128);
    m->muted = moon_options_int(store, "audio.launcher_music_muted", 0, 0, 1);
    m->dragging = -1;
}
int launcher_mixer_music(const launcher_mixer *m)
{
    return m->muted ? 0 : m->music;
}
int launcher_mixer_save(launcher_mixer *m, moon_options *store)
{
    const char *keys[] = {"audio.launcher_music_volume", "audio.launcher_effects_volume",
                          "audio.launcher_music_muted"};
    int values[] = {m->music, m->effects, m->muted};
    for (int i = 0; i < 3; i++) {
        char value[16];
        snprintf(value, sizeof value, "%d", values[i]);
        if (!moon_options_set(store, keys[i], value))
            return 0;
    }
    if (!moon_options_save(store))
        return 0;
    m->dirty = 0;
    return 1;
}
static int inside(float x, float y, float l, float t, float w, float h)
{
    return x >= l && x <= l + w && y >= t && y <= t + h;
}
static void volume(launcher_mixer *m, int row, int value)
{
    int *v = row ? &m->effects : &m->music;
    if (value < 0)
        value = 0;
    if (value > 128)
        value = 128;
    if (*v != value) {
        *v = value;
        m->dirty = 1;
    }
}
int launcher_mixer_event(launcher_mixer *m, const SDL_Event *e, float x, float y)
{
    if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        m->dragging = -1;
        return 0;
    }
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode key = e->key.keysym.sym;
        if (key == SDLK_F8) {
            if (!e->key.repeat)
                m->open = !m->open;
            m->dragging = -1;
            return 1;
        }
        if (key == SDLK_m) {
            if (!e->key.repeat) {
                m->muted = !m->muted;
                m->dirty = 1;
            }
            return 1;
        }
        if (m->open) {
            if (key == SDLK_ESCAPE) {
                m->open = 0;
                m->dragging = -1;
                return 1;
            }
            if (key == SDLK_UP || key == SDLK_DOWN) {
                m->row ^= 1;
                return 1;
            }
            if (key == SDLK_LEFT || key == SDLK_RIGHT) {
                int step = e->key.keysym.mod & KMOD_SHIFT ? 1 : 8;
                volume(m, m->row,
                       (m->row ? m->effects : m->music) + (key == SDLK_LEFT ? -step : step));
                return 1;
            }
        }
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT && m->dragging >= 0) {
        m->dragging = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && m->dragging >= 0) {
        volume(m, m->dragging, (int)((x - 86) * 128 / 112 + .5f));
        return 1;
    }
    if (e->type == SDL_MOUSEWHEEL && e->wheel.y && m->open && inside(x, y, 16, 96, 228, 56)) {
        int row = y >= 124;
        m->row = row;
        volume(m, row, (row ? m->effects : m->music) + (e->wheel.y > 0 ? 8 : -8));
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (inside(x, y, 16, 64, 64, 22)) {
            m->open = !m->open;
            return 1;
        }
        if (inside(x, y, 86, 64, 112, 22)) {
            m->muted = !m->muted;
            m->dirty = 1;
            return 1;
        }
        if (m->open) {
            for (int i = 0; i < 2; i++)
                if (inside(x, y, 80, 96 + i * 28, 124, 22)) {
                    m->dragging = m->row = i;
                    volume(m, i, (int)((x - 86) * 128 / 112 + .5f));
                    return 1;
                }
            if (inside(x, y, 16, 90, 228, 94))
                return 1;
            m->open = 0;
        }
    }
    return 0;
}
