/*
 * The mod source's merge rule: FILES SHADOW, LISTS MERGE
 * (docs/vulcan4/MODS-DESIGN.md).
 *
 * A file a mod supplies replaces the disc's copy. A *table* does not: the disc's
 * rows stay and the mod's rows are added, because replacing GENERIC_CAR.dbt would
 * hide the user's own 722 cars.
 *
 * The three table formats are the SpecDB ones (see specdb.c for the readers):
 *   GTDB -- rows, ticked by id; flags bit 0 means the rows are compressed.
 *   GTID -- label -> id (the car catalog's own list).
 *   GTST -- a string pool addressed by index (models, colours, names).
 *
 * A merge always emits a **raw GTDB / GTID / GTST** (flags bit 0 clear). That is
 * not a shortcut: a compressed table shares one stream between its rows, so two
 * compressed tables cannot be concatenated, while the readers already accept an
 * uncompressed one (specdb.c:62 -- `if(!(r16(b+4)&1))` copies the row straight).
 * No game data is embedded anywhere here; every row comes from the user's disc.
 */
#ifndef OPENGT4_MOD_MERGE_H
#define OPENGT4_MOD_MERGE_H

#include <stddef.h>
#include <stdint.h>

/* Is this buffer a SpecDB table we know how to merge? */
int moon_mod_is_table(const uint8_t *bytes, size_t size);

/* The disc's table plus the mod's rows. Returns a malloc'd buffer (free it) and
   its size, or NULL when the two cannot be merged -- a differing row width, a
   malformed input, an id used twice -- in which case `why` names the reason and
   the caller keeps the disc's own copy. */
uint8_t *moon_mod_merge_table(const uint8_t *disc, size_t disc_size,
                              const uint8_t *mod, size_t mod_size,
                              size_t *out_size, const char **why);

#endif /* OPENGT4_MOD_MERGE_H */
