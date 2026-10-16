#ifndef MOON_FAVORITES_H
#define MOON_FAVORITES_H
#include "adhoc/adhoc_vm.h"
#include <stdint.h>

enum { GT4_FAVORITE_CAPACITY = 32 };
typedef struct {
    int32_t max, count;
    uint64_t codes[GT4_FAVORITE_CAPACITY];
} gt4_favorites;
typedef struct {
    void *context;
    uint64_t (*resolve)(void *context, const char *label);
    ah_val (*label)(ah_vm *vm, void *context, uint64_t code);
    void (*changed)(void *context);
} gt4_favorite_labels;

void gt4_favorites_init(gt4_favorites *favorites);
void gt4_favorites_clear(gt4_favorites *favorites);
int gt4_favorites_find(const gt4_favorites *favorites, uint64_t code);
int gt4_favorites_append(gt4_favorites *favorites, uint64_t code);
int gt4_favorites_remove(gt4_favorites *favorites, uint64_t code);
uint64_t gt4_favorites_get(const gt4_favorites *favorites, int index);
int gt4_favorites_load(ah_vm *vm, ah_obj *object, gt4_favorites *favorites);
void gt4_favorites_store(ah_vm *vm, ah_obj *object, const gt4_favorites *favorites);
void gt4_favorites_bind(ah_vm *vm, ah_obj *object, gt4_favorite_labels *labels);
#endif
