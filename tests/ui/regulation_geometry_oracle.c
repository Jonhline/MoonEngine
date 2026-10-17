/* Original ColorFace submission. Rasterization and colour lifetime are boundaries. */
#define REGULATION_ENTRY regulation_fixture_main
#include "../gtmode/regulation_dialog.c"
#undef REGULATION_ENTRY
#include "ee.h"
#include "ui_motion.h"
ee_fn recomp_lookup(uint32_t);
#define OBJECT 0x1800000u
#define CONTEXT 0x1801000u
#define TABLE 0x1802000u
static float width,height,vertices[8];
static unsigned submissions;
static void putf(ee_ctx *,uint32_t,float);
static unsigned nodes;
static int original_scissor[4];
static void framebuffer_width(ee_ctx *c){c->r[2].uw[0]=640;}
static void framebuffer_height(ee_ctx *c){c->r[2].uw[0]=448;}
static void capture_scissor(ee_ctx *c){for(unsigned i=0;i<4;i++)memcpy(&original_scissor[i],c->spr+0x2298+4*i,4);}

#define PREF (OBJECT+0x8000)
#define NODE(i) (OBJECT+0x10000+(i)*0x100)
static void first(ee_ctx *c){c->r[2].uw[0]=nodes?NODE(0):0;}
static void next(ee_ctx *c){unsigned i=(c->r[4].uw[0]-NODE(0))/0x100+1;c->r[2].uw[0]=i<nodes?NODE(i):0;}
static void eligible(ee_ctx *c){c->r[2].uw[0]=1;}
static void preferred(ee_ctx *c){c->r[2].uw[0]=PREF+(c->r[4].uw[0]-OBJECT)/0x100*16;}
static void geometry(ee_ctx *c){memcpy(c->ram+c->r[4].uw[0]+0x34,c->fpr+12,4);for(unsigned k=1;k<4;k++)putf(c,c->r[4].uw[0]+0x34+4*k,c->fpr[12+k].f);}
static void get_geometry(ee_ctx *c){for(unsigned k=0;k<4;k++) {uint32_t at=c->r[5+k].uw[0];memcpy(c->ram+at,c->ram+c->r[4].uw[0]+0x34+4*k,4);}}
static void size(ee_ctx *c){memcpy(c->ram+c->r[5].uw[0],c->ram+c->r[4].uw[0]+0x3c,4);memcpy(c->ram+c->r[6].uw[0],c->ram+c->r[4].uw[0]+0x40,4);}

static void put(ee_ctx *c,uint32_t a,uint32_t v){memcpy(c->ram+a,&v,4);}
static void putf(ee_ctx *c,uint32_t a,float v){memcpy(c->ram+a,&v,4);}
static void noop(ee_ctx *c){(void)c;}
static void polygon(ee_ctx *c){if(c->r[5].uw[0]!=4) exit(1);memcpy(vertices,c->ram+c->r[6].uw[0],sizeof vertices);submissions++;}
static ee_fn boundary(uint32_t pc){switch(pc){case 0x106298:return framebuffer_width;case 0x1062b8:return framebuffer_height;case 0x4a2e98:return capture_scissor;case 0x206868:return first;case 0x25c300:return next;case 0x2073d8:return eligible;case 0x25bf60:return preferred;case 0x25b538:return geometry;case 0x25b500:return get_geometry;case 0x263c40:case 0x207378:return noop;case 0x25b498:return size;case 0x2030a0:case 0x2030e8:case 0x203118:case 0x1f00000:return noop;case 0x1f00010:return polygon;default:return NULL;}}
static ee_fn lookup(uint32_t pc){ee_fn f=boundary(pc);return f?f:recomp_lookup(pc);}
int main(int argc,char **argv){
 CHECK(argc==3);ee_ctx c;CHECK(!ee_init(&c));CHECK(ee_load_image(&c,argv[1],0x100000)>0x500000);c.lookup=lookup;c.hook=boundary;
 put(&c,CONTEXT+4,TABLE);put(&c,TABLE+0x198+4,0x1f00000);put(&c,TABLE+0x1b0+4,0x1f00010);
 put(&c,OBJECT+0xa4,OBJECT+0x200);put(&c,OBJECT+0xa8,OBJECT+0x210);
 const float heights[]={48,55,68,128,220};
 for(unsigned i=0;i<sizeof heights/sizeof *heights;i++){
  width=100;height=heights[i];putf(&c,OBJECT+0x3c,width);putf(&c,OBJECT+0x40,height);c.r[29].uw[0]=0x1ef0000;c.r[4].uw[0]=OBJECT;c.r[5].uw[0]=CONTEXT;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));lookup(0x2854f8)(&c);
  CHECK(vertices[0]==0 && vertices[1]==0 && vertices[2]==width && vertices[3]==0);
  CHECK(vertices[4]==0 && vertices[5]==height && vertices[6]==width && vertices[7]==height);
 }
 CHECK(submissions==5);
 /* Execute the original MBox allocation too: with zero spacing it must
    allocate contiguous rows, irrespective of the four-pixel FBox padding. */
 for(unsigned rows=4;rows<=5;rows++){
  float pitch=rows==4?55.f:48.f;nodes=rows*2;
  put(&c,OBJECT+0xc0,2);put(&c,OBJECT+0xc4,0);put(&c,OBJECT+0xc8,0);
  putf(&c,OBJECT+0x3c,400);putf(&c,OBJECT+0x40,rows*pitch);
  putf(&c,PREF+8,400);putf(&c,PREF+12,rows*48.f);
  for(unsigned j=0;j<nodes;j++){
   uint32_t a=NODE(j),pref=PREF+(a-OBJECT)/0x100*16;
   put(&c,a+0x9c,(1u<<31)|(1u<<3)|(1u<<4)|(1u<<5)|(1u<<6));
   putf(&c,a+0x3c,j%2?300.f:100.f);putf(&c,a+0x40,48);
   putf(&c,pref,0);putf(&c,pref+4,0);putf(&c,pref+8,j%2?300.f:100.f);putf(&c,pref+12,48);
  }
  c.r[29].uw[0]=0x1ef0000;c.r[4].uw[0]=OBJECT;c.r[5].uw[0]=CONTEXT;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));lookup(0x2b8778)(&c);
  for(unsigned j=0;j<nodes;j++){
   float rect[4];memcpy(rect,c.ram+NODE(j)+0x34,sizeof rect);
   CHECK(rect[0]==(j%2?100.f:0.f) && rect[1]==(j/2)*pitch && rect[3]==pitch);
  }
 }

 /* Snapshot Sunday Cup: cell origins 79.000004 + row*55. Captured
    GS SCISSOR1 inclusive Y bounds are 36..60,62..86,88..112,113..137. */
 c.spr[0x2007]=1;
 const int starts[]={36,62,88,113};
 for(unsigned sample=0;sample<804;sample++){
  float rect[4]={120.f,79.00000381469727f+(sample%4)*55.f,100.f,55.f};
  if(sample>=4){rect[1]=(float)(sample-4)*.37f-40.f;rect[3]=(float)(sample%130)+.375f;}
  memcpy(c.ram+0x1f00100,rect,sizeof rect);
  c.r[29].uw[0]=0x1ef0000;c.r[4].uw[0]=CONTEXT;c.r[5].uw[0]=0x1f00200;c.r[6].uw[0]=0x1f00100;
  jmp_buf jump;c.exit_jmp=&jump;CHECK(!setjmp(jump));lookup(0x215fe0)(&c);
  for(unsigned i=0;i<4;i++)memcpy(&c.fpr[12+i].f,c.ram+0x1f00200+4*i,4);
  lookup(0x4a2df8)(&c);
  int native[4];gt4_ui_scissor(rect,640,448,1,native);
  if(getenv("GT4_REGULATION_SCISSOR_NEGATIVE")) native[3]++;
  if(memcmp(native,original_scissor,sizeof native)) fprintf(stderr,"scissor %u native %d,%d,%d,%d original %d,%d,%d,%d\n",sample,native[0],native[1],native[2],native[3],original_scissor[0],original_scissor[1],original_scissor[2],original_scissor[3]);
  CHECK(!memcmp(native,original_scissor,sizeof native));
  if(sample<4) CHECK(native[1]==starts[sample] && native[3]==25);
 }
 ee_free(&c);
 /* Check the reference-derived native presentation separately from original
    submitted geometry. This is not a PS2 raster comparison. */
 char *args[]={argv[0],argv[2],NULL};CHECK(regulation_fixture_main(2,args)==0);
 puts("PASS original ColorFace/MBox geometry and snapshot-derived GS scissors (804 exact cases).");return 0;
}
