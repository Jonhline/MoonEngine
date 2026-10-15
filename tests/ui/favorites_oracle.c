#include "ee.h"
#include "favorites.h"
#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ee_fn recomp_lookup(uint32_t pc);
enum { OBJECT = 0x1800000, SERIALIZED = 0x1801000 };
static int failed;
static uint64_t original(ee_ctx *context, uint32_t function, uint64_t argument)
{
    memset(context->r, 0, sizeof context->r);
    context->r[4].ud[0] = OBJECT;
    context->r[5].ud[0] = argument;
    context->r[29].uw[0] = 0x1ef0000;
    jmp_buf jump;
    context->exit_jmp = &jump;
    if (setjmp(jump)) {
        fprintf(stderr, "original favorites stopped: %s\n", context->end_detail);
        failed = 1;
        return UINT64_MAX;
    }
    context->lookup(function)(context);
    return context->r[2].ud[0];
}
static int equal(ee_ctx *context, const gt4_favorites *favorites)
{
    if (failed || memcmp(context->ram + OBJECT + 8, favorites, sizeof *favorites)) {
        fprintf(stderr, "favorites state differs from original\n");
        return 0;
    }
    original(context, 0x430570, SERIALIZED);
    if (memcmp(context->ram + SERIALIZED, favorites, sizeof *favorites)) {
        fprintf(stderr, "favorites serialized payload differs from original\n");
        return 0;
    }
    return !failed;
}
static uint64_t resolve(void *context, const char *label)
{
    (void)context;
    return strtoull(label, NULL, 16);
}
static ah_val label(ah_vm *vm, void *context, uint64_t code)
{
    (void)context;
    char text[32];
    snprintf(text, sizeof text, "%llx", (unsigned long long)code);
    return ah_str(vm, text);
}
static ah_val call(ah_vm *vm, ah_obj *object, const char *method, const char *argument)
{
    ah_val result = ah_nil(), value = ah_str(vm, argument ? argument : "");
    if (ah_call_member(vm, object, method, argument != NULL, &value, &result) != 1)
        failed = 1;
    return result;
}
static int save_methods(void)
{
    ah_vm *vm = ah_vm_new();
    ah_obj *game = ah_module_new(vm, "game", NULL, NULL, NULL);
    ah_obj *cars = ah_module_new(vm, "favorite_car", NULL, NULL, NULL);
    ah_obj *courses = ah_module_new(vm, "favorite_course", NULL, NULL, NULL);
    ah_set_member(vm, game, "favorite_car", ah_objv(cars));
    ah_set_member(vm, game, "favorite_course", ah_objv(courses));
    gt4_favorite_labels labels = {NULL, resolve, label};
    gt4_favorites_bind(vm, cars, &labels);
    gt4_favorites_bind(vm, courses, &labels);
    if (ah_num(call(vm, cars, "getSize", NULL)) != 0 ||
        ah_truthy(call(vm, cars, "isFull", NULL)))
        return 0;
    if (!ah_truthy(call(vm, cars, "append", "2")) ||
        !ah_truthy(call(vm, cars, "append", "2600000007")) ||
        ah_truthy(call(vm, cars, "append", "2")) ||
        !ah_truthy(call(vm, courses, "append", "2200000009")) ||
        !ah_truthy(call(vm, cars, "has", "2")) ||
        ah_truthy(call(vm, courses, "has", "2")))
        return 0;
    uint8_t *packed = NULL;
    size_t size = 0;
    if (!gt4_state_pack(vm, game, &packed, &size))
        return 0;
    call(vm, cars, "clear", NULL);
    call(vm, courses, "clear", NULL);
    if (!gt4_state_merge(vm, game, packed, size))
        return 0;
    free(packed);
    ah_val list = call(vm, cars, "buildLabelList", NULL);
    ah_obj *array = ah_as_obj(list, AO_ARRAY);
    if (ah_array_size(array) != 2 || strcmp(ah_cstr(ah_array_get(array, 0)), "2") ||
        strcmp(ah_cstr(ah_array_get(array, 1)), "2600000007"))
        return 0;
    if (call(vm, cars, "remove", "2").t != AV_NIL ||
        ah_truthy(call(vm, cars, "has", "2")) ||
        ah_num(call(vm, courses, "getSize", NULL)) != 1)
        return 0;
    for (int i = 0; i < 31; i++) {
        char key[32];
        snprintf(key, sizeof key, "%x", i);
        if (!ah_truthy(call(vm, cars, "append", key)))
            return 0;
    }
    if (!ah_truthy(call(vm, cars, "isFull", NULL)) ||
        ah_truthy(call(vm, cars, "append", "dead")))
        return 0;
    call(vm, cars, "dump", NULL);
    call(vm, cars, "remove", "dead");
    call(vm, cars, "clear", NULL);
    if (ah_num(call(vm, cars, "getSize", NULL)) != 0)
        return 0;
    ah_vm_free(vm);
    return !failed;
}
int main(int argc, char **argv)
{
    if (argc < 2)
        return 2;
    ee_ctx context;
    if (ee_init(&context))
        return 2;
    context.lookup = recomp_lookup;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 2;
    size_t bytes = fread(context.ram + 0x100000, 1, EE_RAM_SIZE - 0x100000, file);
    fclose(file);
    if (bytes <= 0x587a20)
        return 2;
    gt4_favorites favorites;
    gt4_favorites_init(&favorites);
    original(&context, 0x430320, 0);
    if (argc > 2)
        favorites.max = 33;
    if (!equal(&context, &favorites))
        return 1;
    const uint64_t keys[] = {0, 0x2600000002, 0x2200000009, UINT64_MAX};
    for (int i = 0; i < 40; i++) {
        uint64_t key = i < 4 ? keys[i] : (uint64_t)i;
        int actual = gt4_favorites_append(&favorites, key);
        if ((int64_t)original(&context, 0x4303f8, key) != actual || !equal(&context, &favorites))
            return 1;
        if ((int64_t)original(&context, 0x4303b0, key) != gt4_favorites_find(&favorites, key))
            return 1;
        if ((int64_t)original(&context, 0x4303f8, key) != gt4_favorites_append(&favorites, key))
            return 1;
    }
    for (int i = -1; i <= 33; i++)
        if (original(&context, 0x430528, (uint64_t)(int64_t)i) != gt4_favorites_get(&favorites, i))
            return 1;
    for (int i = 0; i < 40; i++) {
        uint64_t key = i < 4 ? keys[i] : (uint64_t)i;
        if ((int64_t)original(&context, 0x430460, key) != gt4_favorites_remove(&favorites, key) ||
            !equal(&context, &favorites))
            return 1;
    }
    gt4_favorites_clear(&favorites);
    original(&context, 0x430360, 0);
    if (!equal(&context, &favorites) || !save_methods())
        return 1;
    ee_free(&context);
    puts("PASS favorites: original limit/order/duplicates/removal/payload; VM methods/save roundtrip");
    return 0;
}
