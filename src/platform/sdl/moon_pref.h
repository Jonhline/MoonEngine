#ifndef MOON_PREF_H
#define MOON_PREF_H
/* The player data folder, as SDL_GetPrefPath returns it (free with
   SDL_free; ends with a separator). The first call copies the folder of
   the project's former name (OpenGT4) when the new one is still empty. */
char *moon_pref_path(void);
#endif
