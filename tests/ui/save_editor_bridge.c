/* Native storage interoperability fixture; no disc, window or user profile. */
#include "save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"save editor bridge line %d\n",__LINE__); return 1; } } while (0)
static ah_obj *fixture(ah_vm *vm)
{
    ah_obj *game=ah_module_new(vm,"game",NULL,NULL,NULL);
    ah_set_member(vm,game,"username",ah_str(vm,"Editor fixture"));
    ah_set_member(vm,game,"money",ah_int(INT64_C(9007199254740993)));
    ah_obj *garage=ah_module_new(vm,"garage",NULL,NULL,NULL);
    ah_obj *array=ah_array_new(vm),*map=ah_map_new(vm);
    ah_array_push(vm,array,ah_nil());ah_array_push(vm,array,ah_bool(1));
    ah_array_push(vm,array,ah_float(-0.0));ah_array_push(vm,array,ah_str(vm,"UTF-8: \xc3\xa9"));
    if (!ah_map_set(vm,map,ah_int(-12),ah_objv(array))) return NULL;
    ah_set_member(vm,garage,"unknown_nested",ah_objv(map));
    ah_set_member(vm,game,"garage",ah_objv(garage));
    return game;
}
int main(int argc,char **argv)
{
    CHECK(argc>=3);ah_vm *vm=ah_vm_new();CHECK(vm);
    ah_obj *game=fixture(vm);CHECK(game);uint8_t *expected=NULL;size_t size=0;
    CHECK(gt4_state_pack(vm,game,&expected,&size));
    if (!strcmp(argv[1],"create")) CHECK(gt4_save_write(argv[2],expected,size));
    else {
        uint8_t *actual=NULL;size_t n=0;uint64_t stamp;
        CHECK(gt4_save_read(argv[2],&actual,&n,&stamp)==1);
        if (argc>3 && !strcmp(argv[3],"--edited")) {
            free(expected);expected=NULL;
            ah_set_member(vm,game,"money",ah_int(INT64_C(12345678901234567)));
            CHECK(gt4_state_pack(vm,game,&expected,&size));
        }
        if (argc>3 && !strcmp(argv[3],"--negative")) expected[size-1]^=1;
        CHECK(n==size && !memcmp(actual,expected,n));
        ah_obj *restored=ah_module_new(vm,"restored",NULL,NULL,NULL);
        CHECK(gt4_state_replace(vm,restored,actual,n));free(actual);
    }
    free(expected);ah_vm_free(vm);return 0;
}
