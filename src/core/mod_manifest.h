/*
 * A mod's manifest: what a mod ADDS, so the engine can put it in the game's own
 * lists (docs/vulcan4/MODS-DESIGN.md, "lists merge").
 *
 * The merge in mod_merge.c works on table bytes and needs no manifest. A list
 * that lives inside the game's own authored script -- the Arcade car list is
 * `InitData.car_carlist` in /projects/boot/US_carlist.adc, the dealership is
 * per-maker arrays in CarData -- cannot be merged that way, so the mod says what
 * it added and the engine appends it:
 *
 *     # MOON ENGINE mod manifest  (written by the SDK, read by the engine)
 *     name  Moon's first car
 *     car   moon_first_added_car maker=ford
 *     course moon_first_added_track
 *
 * One `mod.txt` per mod folder, read from the same MOON_OVERLAY list the
 * lookups use, in tick order. With no variable set nothing is read at all
 * (LAW 12): the game's own lists are exactly the disc's.
 */
#ifndef MOON_MOD_MANIFEST_H
#define MOON_MOD_MANIFEST_H

#include <stddef.h>

#define MOON_MOD_MAX 64          /* entries of one kind across all mods */

/* Labels a mod declares, in mod order. `makers` may be NULL; when given, the
   parallel entry is "" if the manifest named none. Returns how many were filled. */
size_t moon_mod_added_cars(const char **labels, const char **makers, size_t cap);
size_t moon_mod_added_courses(const char **labels, size_t cap);

/* The mod folders in force, ';'-separated, in the order they should be consulted.
   MOON_OVERLAY wins when it is set (that is what the launcher hands over);
   otherwise the ticked mods listed in <prefs>/mods.ini inside the user's Moon
   Engine folder. Resolved once -- mods do not change while the game runs -- and
   "" when there are none, which means the disc answers everything (LAW 12). */
const char *moon_mod_roots(void);
/* Forget the resolved list: the next moon_mod_roots() reads mods.ini again (the
   launcher's MODS page, before the game starts). */
void moon_mod_roots_reload(void);

/* The folder mods are installed into: <prefs>/mods (WITHOUT a trailing
   separator, ready to be joined). Returns 0 when the backend has no pref dir. */
int moon_mods_dir(char *out, size_t cap);

/* One value out of a mod's own manifest -- "name", "description", "author",
   "version", "image", "price" -- for whatever draws a card in front of the user
   (the launcher's mod manager). Returns 1 and fills `out` when the key is there,
   0 otherwise. The value ends at the end of the line: a description is prose. */
int moon_manifest_field(const char *mod_dir, const char *key, char *out, size_t cap);

#endif /* MOON_MOD_MANIFEST_H */
