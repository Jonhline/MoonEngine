#include "favorites.h"
#include <string.h>

void gt4_favorites_clear(gt4_favorites *favorites)
{
    /* Original: func_00430360 +0x04..0x30 */
    memset(favorites->codes, 0xff, sizeof favorites->codes);
    favorites->count = 0;
}
void gt4_favorites_init(gt4_favorites *favorites)
{
    /* Original: func_00430320 +0x18..0x34 */
    favorites->max = GT4_FAVORITE_CAPACITY;
    gt4_favorites_clear(favorites);
}
int gt4_favorites_find(const gt4_favorites *favorites, uint64_t code)
{
    /* Original: func_004303B0 +0x00..0x40 */
    for (int i = 0; i < favorites->count; i++)
        if (favorites->codes[i] == code)
            return i;
    return -1;
}
int gt4_favorites_append(gt4_favorites *favorites, uint64_t code)
{
    /* Original: func_004303F8 +0x18..0x58 */
    if (gt4_favorites_find(favorites, code) >= 0 || favorites->count >= favorites->max)
        return -1;
    int index = favorites->count++;
    favorites->codes[index] = code;
    return index;
}
int gt4_favorites_remove(gt4_favorites *favorites, uint64_t code)
{
    /* Original: func_00430460 +0x00..0xC0 */
    int index = gt4_favorites_find(favorites, code);
    if (index < 0)
        return -1;
    for (int i = index; i < favorites->count - 1; i++)
        favorites->codes[i] = favorites->codes[i + 1];
    favorites->codes[--favorites->count] = UINT64_MAX;
    return index;
}
uint64_t gt4_favorites_get(const gt4_favorites *favorites, int index)
{
    /* Original: func_00430528 +0x00..0x28 */
    return index < 0 || index >= favorites->count ? UINT64_MAX : favorites->codes[index];
}
int gt4_favorites_load(ah_vm *vm, ah_obj *object, gt4_favorites *favorites)
{
    ah_val max, count, codes;
    if (!ah_get_member(vm, object, "max", &max) ||
        !ah_get_member(vm, object, "count", &count) ||
        !ah_get_member(vm, object, "codes", &codes))
        return 0;
    favorites->max = (int)ah_num(max);
    favorites->count = (int)ah_num(count);
    ah_obj *array = ah_as_obj(codes, AO_ARRAY);
    if (favorites->max < 0 || favorites->max > GT4_FAVORITE_CAPACITY ||
        favorites->count < 0 || favorites->count > favorites->max ||
        ah_array_size(array) != GT4_FAVORITE_CAPACITY)
        return 0;
    for (int i = 0; i < GT4_FAVORITE_CAPACITY; i++) {
        ah_obj *entry = ah_as_obj(ah_array_get(array, i), AO_MODULE);
        ah_val code, table;
        if (!entry || !ah_get_member(vm, entry, "code", &code) ||
            !ah_get_member(vm, entry, "table", &table))
            return 0;
        favorites->codes[i] = (uint32_t)(int64_t)ah_num(code) | ((uint64_t)(uint32_t)(int64_t)ah_num(table) << 32);
    }
    return 1;
}
void gt4_favorites_store(ah_vm *vm, ah_obj *object, const gt4_favorites *favorites)
{
    /* Original: func_00430570 +0x08..0xC4 serializes max/count followed by 32 uint64 keys.
       Keep the imported save's code/table representation in the PC profile. */
    ah_set_member(vm, object, "max", ah_int(favorites->max));
    ah_set_member(vm, object, "count", ah_int(favorites->count));
    ah_obj *codes = ah_array_new(vm);
    for (int i = 0; i < GT4_FAVORITE_CAPACITY; i++) {
        ah_obj *entry = ah_module_new(vm, "code", NULL, NULL, NULL);
        ah_set_member(vm, entry, "code", ah_int((int32_t)favorites->codes[i]));
        ah_set_member(vm, entry, "table", ah_int((int32_t)(favorites->codes[i] >> 32)));
        ah_array_push(vm, codes, ah_objv(entry));
    }
    ah_set_member(vm, object, "codes", ah_objv(codes));
}
static int native_favorite(ah_vm *vm, ah_obj *self, void *data, int argc,
                           const ah_val *argv, ah_val *ret)
{
    const char *method = data;
    gt4_favorites favorites;
    if (!gt4_favorites_load(vm, self, &favorites))
        return ah_error(vm, "invalid favorites data");
    if (!strcmp(method, "getSize")) {
        /* Original: func_001C0648 +0x40 */
        *ret = ah_int(favorites.count);
    } else if (!strcmp(method, "isFull")) {
        /* Original: func_001C06D8 +0x40..0x54 */
        *ret = ah_bool(favorites.count >= favorites.max);
    } else if (!strcmp(method, "clear")) {
        gt4_favorites_clear(&favorites);
        gt4_favorites_store(vm, self, &favorites);
        *ret = ah_nil();
    } else {
        return ah_error(vm, "invalid favorites method");
    }
    (void)argc;
    (void)argv;
    return 0;
}
static int native_favorite_labels(ah_vm *vm, ah_obj *self, void *data, int argc,
                                  const ah_val *argv, ah_val *ret)
{
    gt4_favorite_labels *labels = data;
    gt4_favorites favorites;
    if (!gt4_favorites_load(vm, self, &favorites))
        return ah_error(vm, "invalid favorites data");
    /* Original: func_001C0438 +0x80..0x108 */
    ah_obj *array = ah_array_new(vm);
    for (int i = 0; i < favorites.count; i++)
        ah_array_push(vm, array, labels->label(vm, labels->context, favorites.codes[i]));
    *ret = ah_objv(array);
    (void)argc;
    (void)argv;
    return 0;
}
static int favorite_change(ah_vm *vm, ah_obj *self, gt4_favorite_labels *labels,
                            int argc, const ah_val *argv, ah_val *ret, int operation)
{
    gt4_favorites favorites;
    const char *label = argc ? ah_cstr(argv[0]) : NULL;
    if (!label || !gt4_favorites_load(vm, self, &favorites))
        return ah_error(vm, "favorites require a label and valid data");
    /* Original: func_001C0068 +0xA8; func_001C01D0 +0xA8;
       func_001C0338 +0xA8: generic car, tuned car, then course lookup. */
    uint64_t code = labels->resolve(labels->context, label);
    if (operation == 0) {
        *ret = ah_bool(gt4_favorites_find(&favorites, code) >= 0);
    } else if (operation == 1) {
        /* Original: func_001C01D0 +0xF8..0xFC: bool, not insertion index. */
        int index = gt4_favorites_append(&favorites, code);
        if (index >= 0) {
            gt4_favorites_store(vm, self, &favorites);
            if (labels->changed)
                labels->changed(labels->context);
        }
        *ret = ah_bool(index >= 0);
    } else {
        if (gt4_favorites_remove(&favorites, code) >= 0) {
            gt4_favorites_store(vm, self, &favorites);
            if (labels->changed)
                labels->changed(labels->context);
        }
        *ret = ah_nil();
    }
    return 0;
}
static int native_has(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    return favorite_change(vm, self, data, argc, argv, ret, 0);
}
static int native_append(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    return favorite_change(vm, self, data, argc, argv, ret, 1);
}
static int native_remove(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    return favorite_change(vm, self, data, argc, argv, ret, 2);
}
static int native_dump(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    /* Original: func_001C0770 +0x00: no-op. */
    (void)vm; (void)self; (void)data; (void)argc; (void)argv;
    *ret = ah_nil();
    return 0;
}
static int native_clear(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    gt4_favorite_labels *labels = data;
    gt4_favorites before;
    int changed = gt4_favorites_load(vm, self, &before) && before.count != 0;
    int result = native_favorite(vm, self, "clear", argc, argv, ret);
    if (changed && labels->changed)
        labels->changed(labels->context);
    return result;
}
void gt4_favorites_bind(ah_vm *vm, ah_obj *object, gt4_favorite_labels *labels)
{
    gt4_favorites favorites;
    ah_val value;
    if (!ah_get_member(vm, object, "codes", &value)) {
        gt4_favorites_init(&favorites);
        gt4_favorites_store(vm, object, &favorites);
    }
    static const char *const simple[] = {"clear", "getSize", "isFull"};
    for (int i = 0; i < 3; i++)
        ah_set_member(vm, object, simple[i], ah_bind(vm, object, ah_native(vm, simple[i], native_favorite, (void *)simple[i])));
    ah_set_member(vm, object, "clear", ah_bind(vm, object, ah_native(vm, "clear", native_clear, labels)));
    ah_set_member(vm, object, "has", ah_bind(vm, object, ah_native(vm, "has", native_has, labels)));
    ah_set_member(vm, object, "append", ah_bind(vm, object, ah_native(vm, "append", native_append, labels)));
    ah_set_member(vm, object, "remove", ah_bind(vm, object, ah_native(vm, "remove", native_remove, labels)));
    ah_set_member(vm, object, "buildLabelList", ah_bind(vm, object, ah_native(vm, "buildLabelList", native_favorite_labels, labels)));
    ah_set_member(vm, object, "dump", ah_bind(vm, object, ah_native(vm, "dump", native_dump, labels)));
}
