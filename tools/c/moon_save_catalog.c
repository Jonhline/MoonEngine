/* Read-only editor catalogue. Names/paint choices/record ordinals come from
   the user's disc, never a redistributed data list. No graphics or simulation. */
#include "vol.h"
#include "specdb.h"
#include "career_race_lookup.h"
#include "career_license.h"
#include "career_original.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { uint8_t *p; size_t n; } blob;
static unsigned u32(const uint8_t *p){return p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static blob table(gt4_vol *v,const char *name){char path[128];blob b;snprintf(path,sizeof path,"/specdb/GT4_US2560/%s",name);b.p=gt4_vol_read(v,path,&b.n);return b;}
static void quote(const char *s){putchar('"');for(const unsigned char *p=(const unsigned char *)(s?s:"");*p;p++){if(*p=='"'||*p=='\\')printf("\\%c",*p);else if(*p<32)printf("\\u%04x",*p);else putchar(*p);}putchar('"');}
typedef struct {unsigned order;const char *name;uint8_t rgb[3];} paint;
static void paints(blob links,blob vars,blob strings,unsigned id)
{
    uint8_t row[64];const char *model=NULL;paint list[128];int n=0;
    if(gt4_specdb_row(links.p,links.n,id,row,sizeof row)==8 &&
       gt4_specdb_row(vars.p,vars.n,u32(row),row,sizeof row)==52)
        model=gt4_specdb_string(strings.p,strings.n,u32(row));
    unsigned rows=vars.n>=16?u32(vars.p+8):0;
    for(unsigned i=0;model && i<rows && n<128;i++){
        if(gt4_specdb_row_at(vars.p,vars.n,i,row,sizeof row)!=52)continue;
        const char *m=gt4_specdb_string(strings.p,strings.n,u32(row));
        if(!m||strcmp(m,model))continue;
        list[n].order=u32(row+4);list[n].name=gt4_specdb_string(strings.p,strings.n,u32(row+12));
        memcpy(list[n++].rgb,row+36,3);
    }
    for(int i=0;i<n;i++)for(int j=i+1;j<n;j++)if(list[j].order<list[i].order){paint p=list[i];list[i]=list[j];list[j]=p;}
    printf("\"paints\":[");for(int i=0;i<n;i++){if(i)putchar(',');printf("{\"index\":%d,\"name\":",i);quote(list[i].name);printf(",\"rgb\":[%u,%u,%u]}",list[i].rgb[0],list[i].rgb[1],list[i].rgb[2]);}putchar(']');
}
int main(int argc,char **argv)
{
    if(argc!=2){fprintf(stderr,"Choose a GT4 disc image.\n");return 2;}
    const char *error=NULL;gt4_vol *v=gt4_vol_open(argv[1],&error);
    if(!v){fprintf(stderr,"Cannot read disc: %s\n",error?error:"invalid file");return 1;}
    blob labels=table(v,"GENERIC_CAR.idi"),names=table(v,"CAR_NAME_american.dbt"),strings=table(v,"american_StrDB.sdb"),links=table(v,"CAR_VARIATION_american.dbt"),vars=table(v,"VARIATIONamerican.dbt"),rlabels=table(v,"RACE.idi"),races=table(v,"RACE.dbt");
    gt4_career_race_lookup_data lookup;
    if(!labels.p||!names.p||!strings.p||!rlabels.p||!races.p||!gt4_career_race_lookup_data_load(argv[1],&lookup)){fprintf(stderr,"This catalogue currently needs the supported GT4 US disc.\n");return 1;}
    printf("{\"version\":1,\"cars\":[");int first=1;
    for(size_t i=0;i<gt4_specdb_idi_count(labels.p,labels.n);i++){
        const char *label=NULL,*name=NULL;uint32_t id;uint8_t row[64];
        if(!gt4_specdb_idi_at(labels.p,labels.n,i,&label,&id))continue;
        if(gt4_specdb_row(names.p,names.n,id,row,sizeof row)==16)name=gt4_specdb_string(strings.p,strings.n,u32(row));
        if(!first)putchar(',');first=0;printf("{\"id\":%u,\"label\":",id);quote(label);printf(",\"name\":");quote(name?name:label);putchar(',');paints(links,vars,strings,id);putchar('}');
    }
    printf("],\"licenses\":[");first=1;
    const char *groups[]={"l0b","l0a","lib","lia","l0s"},*display[]={"B","A","IB","IA","S"};
    for(int g=0;g<5;g++)for(int n=1;n<=16;n++){
        char code[16];uint8_t row[151];snprintf(code,sizeof code,"%s%04d",groups[g],n);
        int record=gt4_career_race_record_index(&lookup,rlabels.p,rlabels.n,races.p,races.n,code);
        if(record<0||!gt4_career_race_row(&lookup,rlabels.p,rlabels.n,races.p,races.n,code,row))continue;
        if(!first)putchar(',');first=0;
        printf("{\"code\":");quote(code);printf(",\"group\":");quote(display[g]);printf(",\"number\":%d,\"record\":%d,\"ranking\":%d,\"targets\":[%u,%u,%u]}",n,record,gt4_career_license_index(code),gt4_career_license_time(row,0),gt4_career_license_time(row,1),gt4_career_license_time(row,2));
    }
    printf("]}\n");
    free(labels.p);free(names.p);free(strings.p);free(links.p);free(vars.p);free(rlabels.p);free(races.p);gt4_vol_close(v);return 0;
}
