#include "music_override.h"
#include <SDL.h>
static SDL_SpinLock lock;
static moon_music_override hook;
static void *owner;
static moon_music_title title_hook;
void moon_music_override_set(moon_music_override callback, void *user)
{
    SDL_AtomicLock(&lock);
    hook = callback;
    owner = user;
    if (!callback)
        title_hook = NULL;
    SDL_AtomicUnlock(&lock);
}
int moon_music_override_render(int context, int16_t *out, int frames, int gain, int paused)
{
    SDL_AtomicLock(&lock);
    int result = hook ? hook(owner, context, out, frames, gain, paused) : 0;
    SDL_AtomicUnlock(&lock);
    return result;
}

void moon_music_override_titles(moon_music_title callback)
{
    SDL_AtomicLock(&lock);
    title_hook = callback;
    SDL_AtomicUnlock(&lock);
}
void moon_music_override_clear(void *user)
{
    SDL_AtomicLock(&lock);
    if (owner == user) {
        hook = NULL;
        title_hook = NULL;
        owner = NULL;
    }
    SDL_AtomicUnlock(&lock);
}
int moon_music_override_title(int context, char *out, int capacity)
{
    SDL_AtomicLock(&lock);
    int result = title_hook ? title_hook(owner, context, out, capacity) : 0;
    SDL_AtomicUnlock(&lock);
    return result;
}
