#ifndef MOON_SAVE_H
#define MOON_SAVE_H
#include "adhoc/adhoc_vm.h"
#include <stddef.h>
#include <stdint.h>
#define GT4_SAVE_SLOTS 8
#define GT4_SAVE_LIMIT (4u * 1024u * 1024u)
typedef struct {
    char name[97];
    uint64_t timestamp;
    int status;
} gt4_save_info;
/* 1 valid, 2 recovered backup, 0 empty, negative error. */
int gt4_save_read(const char *path, uint8_t **data, size_t *size, uint64_t *stamp);
int gt4_save_write(const char *path, const uint8_t *data, size_t size);
int gt4_save_mkdir(const char *path);
/* Bounded, version-neutral typed VM data. No callbacks/host objects are saved. */
int gt4_state_pack(ah_vm *vm, ah_obj *game, uint8_t **data, size_t *size);
int gt4_state_replace(ah_vm *vm, ah_obj *game, const uint8_t *data, size_t size);
int gt4_state_merge(ah_vm *vm, ah_obj *game, const uint8_t *data, size_t size);
/* One session shared across cached menu VMs, independent of UI/callback state. */
void gt4_save_init(const char *directory);
/* Game startup: inspect the active index now, read the profile with its visible load screen. */
void gt4_save_init_pending(const char *directory);
int gt4_save_pending_slot(void);
void gt4_save_shutdown(void);
int gt4_save_active(void);
int gt4_save_dirty(void);
void gt4_save_mark_dirty(void);
const char *gt4_save_error(void);
void gt4_save_report_error(const char *message);
int gt4_save_export_destination(const char *path);
int gt4_save_info_get(int slot, gt4_save_info *info);
int gt4_save_valid_name(const char *name);
typedef int (*gt4_save_writer)(const char *, const uint8_t *, size_t, void *);
int gt4_save_create(int slot, const char *name);
int gt4_save_create_with_writer(int slot, const char *name, gt4_save_writer write, void *user);
int gt4_save_rename_with_writer(const char *name, gt4_save_writer write, void *user);
int gt4_save_import_ps2_with_writer(int slot, const char *path, gt4_save_writer write, void *user);
int gt4_save_load_slot(int slot);
int gt4_save_load_slot_with_reader(int slot,
                                   int (*read)(const char *, uint8_t **, size_t *, void *),
                                   void *user);
int gt4_save_load_slot_with_io(int slot, int (*read)(const char *, uint8_t **, size_t *, void *),
                               gt4_save_writer write, void *user);
/* Read another profile without changing the active player or its dirty state. */
/* Export the persisted profile without changing active player or dirty state. */
int gt4_save_export_profile(int slot, const char *destination);
int gt4_save_read_profile(ah_vm *vm, ah_obj *game, int slot);
int gt4_save_store(void);
/* Serialization remains on the caller; only the immutable file write is presented. */
int gt4_save_store_with_writer(int (*write)(const char *, const uint8_t *, size_t, void *),
                               void *user);
int gt4_save_delete_slot(int slot);
int gt4_save_rename(const char *name);
int gt4_save_capture(ah_vm *vm, ah_obj *game, int dirty);
void gt4_save_restore(ah_vm *vm, ah_obj *game);
/* Refresh only shared preferences, preserving live garage/record changes. */
int gt4_save_restore_options(ah_vm *vm, ah_obj *game);
double gt4_save_option_number(const char *name, double fallback);
/* Host-owned numeric option arrays; no conversion of PS2 save bytes. */
int gt4_save_option_array_get(const char *name, int *data, size_t count);
int gt4_save_option_array_set(const char *name, const int *data, size_t count);
int gt4_save_option_blob_get(const char *name, unsigned char *data, size_t size);
int gt4_save_option_blob_set(const char *name, const unsigned char *data, size_t size);
int gt4_save_auto_enabled(void);
/* the save folder ("" when none) and the active profile's name */
const char *gt4_save_directory(void);
const char *gt4_save_player(void);
#endif
