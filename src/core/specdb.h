#ifndef MOON_SPECDB_H
#define MOON_SPECDB_H
#include <stddef.h>
#include <stdint.h>
int gt4_specdb_label(const uint8_t *idi, size_t size, const char *label, uint32_t *id);
const char *gt4_specdb_label_for_id(const uint8_t *idi, size_t size, uint32_t id);
/* Enumerate a GTID (.idi): how many entries it holds, and the label/id of the
   index'th one (returns 1 on success). Built on the same bounds checks as the
   lookups above; used to walk every car label on the disc. */
size_t gt4_specdb_idi_count(const uint8_t *idi, size_t size);
int gt4_specdb_idi_at(const uint8_t *idi, size_t size, size_t index, const char **label,
                      uint32_t *id);
int gt4_specdb_row_index(const uint8_t *db, size_t size, uint64_t id);
int gt4_specdb_row_at(const uint8_t *db, size_t size, uint32_t index, uint8_t *out,
                      size_t capacity);
int gt4_specdb_row(const uint8_t *db, size_t size, uint32_t id, uint8_t *out, size_t capacity);
/* the nth row (0 first) of those with this id; out NULL: 1 if it exists */
int gt4_specdb_row_nth(const uint8_t *db, size_t size, uint32_t id, uint32_t nth, uint8_t *out,
                       size_t capacity);
const char *gt4_specdb_string(const uint8_t *data, size_t size, uint32_t index);
#endif
