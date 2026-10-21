/* Offline save-editor operations through the engine's existing native factory.
   Inputs/outputs are isolated temporary profiles, never the player's source. */
#include "save.h"
#include "gtmode_game.h"
#include "career_vehicle.h"
#include "career_blob.h"
#include "career_setting.h"
#include "specdb.h"
#include "vol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
static ah_val get(ah_vm *vm,ah_obj *o,const char *k){ah_val v=ah_nil();if(o)ah_get_member(vm,o,k,&v);return v;}
static int whole(const char *s,int lo,int hi,int *out){char *end;errno=0;long n=strtol(s,&end,10);if(errno||!*s||*end||n<lo||n>hi)return 0;*out=(int)n;return 1;}
static int decimal(const char *s,double *out){char *end;errno=0;double d=strtod(s,&end);if(errno||!*s||*end||!isfinite(d)||d<=0)return 0;*out=d;return 1;}
static int fail(const char *s){fprintf(stderr,"%s\n",s);return 1;}
static void metadata(ah_vm *vm,ah_obj *car,const char *disc)
{
    gt4_vol *v=gt4_vol_open(disc,NULL);if(!v)return;size_t ng=0,nn=0,ns=0;
    uint8_t *generic=gt4_vol_read(v,"/specdb/GT4_US2560/GENERIC_CAR.dbt",&ng),*names=gt4_vol_read(v,"/specdb/GT4_US2560/CAR_NAME_american.dbt",&nn),*strings=gt4_vol_read(v,"/specdb/GT4_US2560/american_StrDB.sdb",&ns),row[24];
    uint32_t id=(uint32_t)get(vm,car,"code").u.i;ah_set_member(vm,car,"spec_id",ah_int(id));
    if(gt4_specdb_row(generic,ng,id,row,sizeof row)==24){int32_t price;memcpy(&price,row+8,4);ah_set_member(vm,car,"original_base_price",ah_int(price));}
    if(gt4_specdb_row(names,nn,id,row,sizeof row)==16){uint32_t a,b;memcpy(&a,row,4);memcpy(&b,row+8,4);const char *name=gt4_specdb_string(strings,ns,a),*shortname=gt4_specdb_string(strings,ns,b);if(name)ah_set_member(vm,car,"name",ah_str(vm,name));if(shortname)ah_set_member(vm,car,"short_name",ah_str(vm,shortname));}
    free(generic);free(names);free(strings);gt4_vol_close(v);
}
static int nearest(gt4_career_vehicle *v,uint8_t body[0x4b8],int which,double target,int32_t result[3])
{
    unsigned slot;memcpy(&slot,body+0x488,4);if(slot>=3)return 0;
    size_t at=slot*0x178+(which==0?0x163:0x162);int32_t low[3],high[3];
    body[at]=1;if(!gt4_career_vehicle_stats(v,body,low))return 0;
    body[at]=255;if(!gt4_career_vehicle_stats(v,body,high))return 0;
    double scale=which==0?100.0:1.0,min=low[which==0?0:2]/scale,max=high[which==0?0:2]/scale;
    if(target<min||target>max){fprintf(stderr,"This setup supports %.2f–%.2f %s using its saved 1–255%% adjustment.\n",min,max,which==0?"cv":"kg");return 0;}
    int lo=1,hi=255,best=100;double distance=HUGE_VAL;
    while(lo<=hi){int mid=(lo+hi)/2;body[at]=(uint8_t)mid;int32_t stats[3];if(!gt4_career_vehicle_stats(v,body,stats))return 0;
        double actual=stats[which==0?0:2]/scale,d=fabs(actual-target);if(d<distance){distance=d;best=mid;}if(actual<target)lo=mid+1;else hi=mid-1;}
    body[at]=(uint8_t)best;return gt4_career_vehicle_stats(v,body,result);
}
static void sync(ah_vm *vm,ah_obj *game,ah_obj *car,int index,const uint8_t body[0x4b8],const int32_t stats[3])
{
    gt4_career_blob_set(vm,car,"full_car_ps2",body,0x4b8);
    ah_obj *wear=ah_array_new(vm);for(int i=0;i<32;i++)ah_array_push(vm,wear,ah_int(body[0x498+i]));ah_set_member(vm,car,"oil_state",ah_objv(wear));
    if(stats){uint64_t flags=(uint64_t)get(vm,car,"flags").u.i;flags=(flags&~(UINT64_C(0x3ffff)<<9))|((uint64_t)((uint32_t)stats[0]&0x3ffff)<<9);flags=(flags&~(UINT64_C(8191)<<32))|((uint64_t)((uint32_t)stats[2]&8191)<<32);
        int64_t signed_flags;memcpy(&signed_flags,&flags,8);ah_set_member(vm,car,"flags",ah_int(signed_flags));ah_set_member(vm,car,"power",ah_int(stats[0]));ah_set_member(vm,car,"weight",ah_int(stats[2]));}
    uint8_t unit[32];gt4_game_car_unit(vm,car,unit);gt4_career_blob_set(vm,car,"compact_unit_ps2",unit,32);
    ah_obj *garage=get(vm,game,"garage").u.o;if(get(vm,garage,"riding_index").u.i==index)gt4_career_blob_set(vm,garage,"current_car_ps2",body,0x4b8);
}
int main(int argc,char **argv)
{
    if(argc<6)return fail("Usage: moon_save_vehicle DISC INPUT OUTPUT ACTION [arguments]");
    const char *disc=argv[1],*action=argv[4];uint8_t *data=NULL;size_t size=0;uint64_t stamp;
    if(gt4_save_read(argv[2],&data,&size,&stamp)!=1)return fail("Cannot read temporary native profile.");
    if(!gt4_game_load_disc(disc))return fail("Cannot load the original car data from this disc.");
    ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"editor",NULL,NULL,NULL);
    if(!gt4_state_replace(vm,game,data,size))return fail("Invalid temporary profile.");free(data);gt4_game_bind(vm,game);
    ah_obj *garage=get(vm,game,"garage").u.o,*cars=get(vm,garage,"cars").u.o;ah_val ret;
    gt4_career_vehicle *factory=NULL;int index=-1;
    if(!strcmp(action,"add")||!strcmp(action,"stats")||!strcmp(action,"tune")||!strcmp(action,"refresh")){
        factory=gt4_career_vehicle_open(disc);if(!factory)return fail("The native vehicle factory could not read this disc.");
        gt4_game_set_car_factory(gt4_career_vehicle_create_seeded,factory);
    }
    if(!strcmp(action,"add")){
        int color;if(argc!=7||!whole(argv[6],0,127,&color))return fail("Choose a valid factory paint.");
        ah_val args[]={ah_str(vm,argv[5]),ah_int(color)};
        if(ah_call_member(vm,garage,"addNewCar",2,args,&ret)<0)return fail("Cannot add this model; check the model, paint and garage capacity.");
        index=(int)ret.u.i;
        ah_obj *created=ah_array_get(get(vm,garage,"cars").u.o,index).u.o;metadata(vm,created,disc);printf("{\"index\":%d}\n",index);
    }else{
        if(!whole(argv[5],0,999,&index))return fail("Invalid garage slot.");
        ah_obj *car=ah_as_obj(ah_array_get(cars,index),AO_MODULE);if(!car)return fail("That garage slot is empty.");
        if(!strcmp(action,"delete")||!strcmp(action,"select")){
            ah_val arg=ah_int(index);if(ah_call_member(vm,garage,!strcmp(action,"delete")?"delCar":"changeRidingCar",1,&arg,&ret)<0)return fail("Garage operation failed.");
        }else if(!strcmp(action,"stats")||!strcmp(action,"tune")||!strcmp(action,"refresh")){
            uint8_t body[0x4b8];int32_t stats[3];if(!gt4_game_car_body(vm,car,body)||!gt4_career_vehicle_stats(factory,body,stats))return fail("This car has no valid complete native setup.");
            if(!strcmp(action,"tune")){
                double cv,kg;if(argc!=8||!decimal(argv[6],&cv)||!decimal(argv[7],&kg)||kg>8191)return fail("Enter positive power (cv) and mass (kg, up to 8191).");
                if(!nearest(factory,body,0,cv,stats)||!nearest(factory,body,1,kg,stats))return 1;
                sync(vm,game,car,index,body,stats);
            }
            if(!strcmp(action,"refresh"))sync(vm,game,car,index,body,stats);
            printf("{\"power_cv\":%.2f,\"weight_kg\":%d,\"torque\":%.2f,\"power_percent\":%u,\"weight_percent\":%u}\n",stats[0]/100.0,stats[2],stats[1]/100.0,body[(unsigned)(body[0x488]|body[0x489]<<8)*0x178+0x163],body[(unsigned)(body[0x488]|body[0x489]<<8)*0x178+0x162]);
            if(!strcmp(action,"stats")){gt4_career_vehicle_close(factory);ah_vm_free(vm);return 0;}
        }else return fail("Unknown vehicle action.");
    }
    /* Keep imported compact blobs consistent with the authoritative occupied
       and vacant objects. Native methods handle count, selection and history. */
    uint8_t units[32000];if(gt4_career_blob_get(vm,garage,"original_units_ps2",units,sizeof units)){
        cars=get(vm,garage,"cars").u.o;ah_obj *vacant=ah_as_obj(get(vm,garage,"vacant_slots"),AO_MODULE);
        for(int i=0;i<1000;i++){ah_obj *car=ah_as_obj(ah_array_get(cars,i),AO_MODULE);char key[16];if(!car){snprintf(key,sizeof key,"%d",i);car=ah_as_obj(get(vm,vacant,key),AO_MODULE);}if(car)gt4_game_car_unit(vm,car,units+i*32);}
        gt4_career_blob_set(vm,garage,"original_units_ps2",units,sizeof units);
    }
    data=NULL;if(!gt4_state_pack(vm,game,&data,&size)||!gt4_save_write(argv[3],data,size))return fail("Cannot write temporary vehicle result.");
    free(data);if(factory){gt4_game_set_car_factory(NULL,NULL);gt4_career_vehicle_close(factory);}ah_vm_free(vm);return 0;
}
