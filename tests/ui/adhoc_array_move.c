/* 002ED8C0/002F0138 valid-index relocation, including musiccontrol's shuffle. */
#include "adhoc/adhoc_vm.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Array.move line %d\n", __LINE__);                                     \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
/* musiccontrol.h sortfunc_playlist_dispno: a[0] < b[0] over playlist rows */
static int by_dispno(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)data;
    ah_obj *a = argc == 2 ? ah_as_obj(argv[0], AO_ARRAY) : NULL,
           *b = argc == 2 ? ah_as_obj(argv[1], AO_ARRAY) : NULL;
    if (!a || !b)
        return -1;
    *ret = ah_int(ah_num(ah_array_get(a, 0)) < ah_num(ah_array_get(b, 0)));
    return 0;
}
static int descending(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)data;
    *ret = ah_int(argc == 2 && ah_num(argv[0]) > ah_num(argv[1]));
    return 0;
}
int main(int argc, char **argv)
{
    (void)argv;
    ah_vm *vm = ah_vm_new();
    CHECK(vm);
    for (int n = 1; n <= 20; n++)
        for (int from = 0; from < n; from++)
            for (int to = 0; to < n; to++) {
                ah_obj *array = ah_array_new(vm);
                for (int i = 0; i < n; i++)
                    ah_array_push(vm, array, ah_int(i));
                ah_val args[] = {ah_int(from), ah_int(to)}, result;
                CHECK(ah_call_member(vm,array,"move",2,args,&result)==1);
                CHECK(ah_array_size(array)==n);
                for (int i = 0; i < n; i++) {
                    int expected = i;
                    /* Reference: original 002F0138 directional assignment loops. */
                    if (i == to)
                        expected = from;
                    else if (from < to && i >= from && i < to)
                        expected = i + 1;
                    else if (from > to && i > to && i <= from)
                        expected = i - 1;
                    if (argc > 1 && n == 3 && from == 0 && to == 2 && i == 0)
                        expected = 2; /* swap is not move */
                    CHECK((int)ah_num(ah_array_get(array,i))==expected);
                }
            }
    for (int n = 0; n <= 20; n++)
        for (int erase = -1; erase <= n; erase++) {
            ah_obj *array = ah_array_new(vm);
            for (int i = 0; i < n; i++)
                ah_array_push(vm, array, ah_int(i));
            ah_val argument = ah_int(erase), result;
            CHECK(ah_call_member(vm,array,"erase",1,&argument,&result)==1);
            int valid = erase >= 0 && erase < n;
            CHECK(ah_array_size(array)==n-valid);
            for (int i = 0; i < n - valid; i++)
                CHECK((int)ah_num(ah_array_get(array,i))==i+(valid && i>=erase));
        }
    /* Array.sort(less) 002ED7A0: rows [dispno, played, marked, playno, index] by
       element 0, plain values descending; sizes past the 16 threshold and depth. */
    ah_val less_row = ah_native(vm, "by_dispno", by_dispno, NULL),
           less_desc = ah_native(vm, "descending", descending, NULL);
    unsigned seed = 12345;
    for (int n = 0; n <= 300; n += n < 40 ? 1 : 37) {
        int keys[300];
        for (int i = 0; i < n; i++)
            keys[i] = i;
        for (int i = n - 1; i > 0; i--) {
            seed = seed * 1103515245u + 12345u;
            int j = (int)((seed >> 8) % (unsigned)(i + 1)), t = keys[i];
            keys[i] = keys[j];
            keys[j] = t;
        }
        ah_obj *rows = ah_array_new(vm), *plain = ah_array_new(vm);
        for (int i = 0; i < n; i++) {
            ah_obj *row = ah_array_new(vm);
            ah_array_push(vm, row, ah_int(keys[i]));
            ah_array_push(vm, row, ah_int(0));
            ah_array_push(vm, row, ah_int(1));
            ah_array_push(vm, row, ah_int(keys[i]));
            ah_array_push(vm, row, ah_int(i));
            ah_array_push(vm, rows, ah_objv(row));
            ah_array_push(vm, plain, ah_int(keys[i]));
        }
        ah_val result;
        CHECK(ah_call_member(vm, rows, "sort", 1, &less_row, &result) == 1);
        CHECK(ah_call_member(vm, plain, "sort", 1, &less_desc, &result) == 1);
        for (int i = 0; i < n; i++) {
            ah_obj *row = ah_as_obj(ah_array_get(rows, i), AO_ARRAY);
            int expected = argc > 1 && n == 30 && i == 0 ? 1 : i; /* comparator ignored */
            CHECK(row && (int)ah_num(ah_array_get(row, 0)) == expected);
            CHECK((int)ah_num(ah_array_get(row, 0)) == keys[(int)ah_num(ah_array_get(row, 4))]);
            CHECK((int)ah_num(ah_array_get(plain, i)) == n - 1 - i);
        }
    }
    ah_vm_free(vm);
    puts(
        "PASS Array.move: every valid source/destination at sizes 1..20; source-retaining relocation, not swap");
    puts("PASS Array.sort: native less-than honoured, playlist rows by dispno and descending, n 0..300");
    return 0;
}
