# The mesh contract of `gt4_model_decode_part`

Scope: everything a **generated** (importer-produced) model must satisfy so the showroom
(`src/app/main.c:114`), the race (`src/app/race_game.c:3270`) and course props
(`src/core/course_gadget.c:88`) render and drive it. Evidence is `path:line` + quoted line.
Read-only analysis; no offsets invented — where the code **computes** a value instead of
hardcoding it, that is stated.

Call chain under test:

```
model.c:593  gt4_model *gt4_model_decode_part(const uint8_t *bytes,size_t size,int color,int model_index)
model.c:594  {return gt4_model_decode_stats(bytes,size,color,model_index,NULL);}
model.c:596  gt4_model *gt4_model_decode_stats(...)
model.c:597  {return decode(bytes,size,color,model_index,stats,1/4096.f,0,0,0,-1);}
```

So `gt4_model_decode_part` == `decode(..., fixed = 1/4096.f, human=0, wheel=0, wing=0, branch=-1)`.
`fixed` is the car/course fixed-point scale; `1/4096` means "cars" (contrast `WORLD_FIXED (1/500.f)`
at `model.c:12` used only by `gt4_model_decode_world`).

---

## 1 DECODER INPUT

### 1.1 Container

| Condition | Evidence |
|---|---|
| Buffer must exist and be ≥ 128 bytes | `model.c:649` `if(!bytes \|\| size<128)return NULL;` |
| Magic is detected, not required, at offset 0 | `model.c:650` `int car=!memcmp(bytes,"CAR4",4);` |
| CAR4 ⇒ two header pointers | `model.c:651` `if(car){base=u32(bytes+24);patch=u32(bytes+28);}` |
| Otherwise `base = 0` | `model.c:649` `size_t base=0,patch=0;` |
| `"MDLS"` must be at `base`, and 128 bytes must follow | `model.c:652` `if(!span(size,base,128) \|\| memcmp(bytes+base,"MDLS",4))return NULL;` |
| MDLS self-size at `base+16`; must be ≥128 and inside the buffer | `model.c:653` `size_t n=u32(bytes+base+16);if(n<128 \|\| !span(size,base,n))return NULL;` |

The decoder then **copies** the MDLS out (`model.c:654` `uint8_t *d=malloc(n);...memcpy(d,bytes+base,n);`)
and every subsequent field is read from that copy, so all `d+N` offsets below are MDLS-relative.

### 1.2 Optional Pat0 recolour (CAR4 + colour >= 0 only)

```
model.c:655  if(car && color>=0 && span(size,patch,32) && !memcmp(bytes+patch,"Pat0",4)) {
model.c:656      unsigned nc=u16(bytes+patch+16),np=u16(bytes+patch+18);
model.c:657      if((unsigned)color<nc && span(size,patch+32,(size_t)nc*np*4))for(unsigned i=0;i<np;i++) {
model.c:658          size_t p=patch+(size_t)u32(bytes+patch+32+((size_t)color*np+i)*4);
model.c:659          if(!span(size,p,8)){...goto bad;}
model.c:660          if(!span(n,off,len)||!span(size,p+8,len)){...goto bad;}memcpy(d+off,bytes+p+8,len);
```

Pat0: `+16` colour count, `+18` patch count, `+32` a colour-major `u32` offset table, each entry
being `{u32 dest_offset, u32 byte_len}` followed by `len` replacement bytes applied **inside the
MDLS copy**. If `patch` does not point at `"Pat0"` the patch is silently skipped (no recolor, no
failure). External `.pat` files are stitched on by the loader, not the decoder (`car_model.c:202-211`).

### 1.3 MDLS header fields the decoder reads

All read as little-endian `u16`/`u32` (`model.c:8-9`).

| Field | Offset | Code | Meaning |
|---|---|---|---|
| shape count | `d+24` | `model.c:664` `r.ns=u16(d+24);` | number of shapes |
| material count | `d+26` | `model.c:664` `r.nm=u16(d+26);` | number of 80-byte materials |
| texture count | `d+28` | `model.c:664` `r.nt=u16(d+28);` | number of Tex1 banks |
| shape offset table | `d+60` | `model.c:665` `!span(n,u32(d+60),(size_t)r.ns*4)` | `ns` × `u32` shape offsets |
| texture list pointer | `d+68` | `model.c:668` `size_t lists=u32(d+68);` | points at a `u32` that points at the `nt` × `u32` bank table |
| model count | `d+22` | `model.c:677` `unsigned models=u16(d+22);` | number of model records |
| model list | `d+56` | `model.c:677` `size_t mo=u32(d+56);` | `models` × **40-byte** records |
| material table | `d+64` | `model.c:418` `size_t material=u32(r->d+64)+(size_t)mat*80;` | `nm` × 80 bytes |
| branch count | `d+32` | `model.c:639` `if(r->hand_branch>=u16(r->d+32) ...)` | jump-table arity (human paths) |
| host count | `d+36` | `model.c:624` `count=u16(r->d+36);` | human grasp hosts (human paths) |
| host table | `d+76` | `model.c:624` `size_t hosts=u32(r->d+76);` | 8 bytes per host (human paths) |
| skin bone slots | `d+46` | `model.c:58` `unsigned slots=u16(r->d+46);` | skinned path only |
| PS2 VM base | `d+92` | `model.c:617` `size_t base=u32(r->d+92),...` | hand/VM program (human path) |

Strides **are hardcoded** (shape list 4, model record 40, material 80) — the file does not carry them.

### 1.4 Model record

`model.c:677` bounds the 40-byte records; `model.c:682` reads the command stream from `+24`:

```
model.c:682  int first=m->count,table=0;if(!commands(&r,u32(d+mo+i*40+24),&table)){...goto fail;}
```

`+24` is the **only** model-record field `decode_part` uses. (Human paths also use `+36`,
`model.c:617`.)

### 1.5 Shape record

```
model.c:395  if(!span(r->n,a,16))FAIL;
model.c:396  unsigned np=u16(r->d+a+10),kind=r->d[a+8]&31;
model.c:400  unsigned tween_mode=r->d[a+8]>>5;
model.c:406  float authored_scale=fl(r->d+a+4);
model.c:407  if(!isfinite(authored_scale) || authored_scale<0 || authored_scale>1)FAIL;
model.c:408  if(!span(r->n,a+16,(size_t)np*8))FAIL;
model.c:412  size_t desc=a+16+j*8,p=a+(size_t)u32(r->d+desc),len=u16(r->d+desc+4)*16,end=p+len;
model.c:414  unsigned flags=u16(r->d+desc+6),mat=(flags>>9)-1,tex=flags&511;
```

* `+4` float `authored_scale` must be in `[0,1]`; when `> 0` it **overrides** the fixed-point scale
  for compressed positions (`model.c:477` `*out=(float)x*(authored_scale>0?authored_scale:r->fixed);`).
  `0` means "use `r.fixed`" (`1/4096`).
* `+8`: low 5 bits = `kind`, high 3 bits = tween mode. `kind 2` without a table means the
  untextured/"special" texture id −2 (`model.c:557` `if(kind==2 && table==-1)texture=-2;`).
* `+10`: `np` = packet count, `np*8` descriptor bytes at `a+16`.
* Descriptor: `+0` u32 offset **relative to the shape base**; `+4` u16 length **in 16-byte units**
  (`*16`); `+6` u16 flags → `mat=(flags>>9)-1`, `tex=flags&511`.
* `tex` is 1-based: `model.c:555` `if(table>=0 && table<(int)r->nt && tex>0 && r->texbase[table]>=0)texture=r->texbase[table]+(int)tex-1;`

### 1.6 String / key table

There is **none** on the `decode_part` path. Strings appear only in the human-grasp validator,
which `gt4_model_decode_part` never enters (`model.c:679` `if(human>=2 && ...)goto fail;` — here
`human==0`):

```
model.c:628  found=(span(r->n,name,10) && !memcmp(r->d+name,"leftGrasp",10)) ||
model.c:629        (span(r->n,name,11) && !memcmp(r->d+name,"rightGrasp",11));
model.c:635  ... if(span(r->n,name,5) && !memcmp(r->d+name,"time",5))found=1;
```

An importer may therefore ship a mesh with an empty host/model-name table as far as
`gt4_model_decode_part` is concerned.

### 1.7 Command stream (the driving program)

`commands()` (`model.c:158`) walks the model record's stream one byte per opcode, with a hard
budget: `model.c:188` `for(int budget=0;budget<8192 && span(r->n,p,1);budget++)`. Termination is
opcode `0`:

```
model.c:208  case 0:return 1;
```

Opcodes that produce geometry:

```
model.c:209  case 4:case 5:            /* draw shape arg (1- or 2-byte arg) */
model.c:215      if(!shape(r,u32(r->d+u32(r->d+60)+arg*4),*table))FAIL;
model.c:231  case 2:case 3: {          /* nested model draw */ model.c:240 commands(...,+24...)
model.c:272  case 43:case 44:          /* set texture table */
model.c:273  case 80:case 81:          /* external slot bind; arg==2 = rim bank (r->wheel only) */
model.c:284  case 6:case 77:           /* callback: jump table, this is where attachment tags are set */
model.c:246  case 12:                  /* push matrix + attachment[depth+1]=attachment[depth] */
model.c:248  case 15:case 16:          /* load / mult matrix (op 15 resets attachment to 0) */
model.c:255  case 17:case 18:          /* translate / scale */
model.c:262  case 19: case 20/21/22:   /* rotate */
```

Everything else is render state (`model.c:351-380`) or a skip of a known byte count. **Any opcode
not listed hits `default`:**

```
model.c:381  default:
model.c:382      r->out->unsupported++;
model.c:383      if(r->stats){r->stats->fail_op=(int)op;r->stats->fail_offset=p-1;}
model.c:384      FAIL;
```

`FAIL` (`model.c:39`) returns 0 → `decode()` `goto fail` → **whole model is NULL**. There is no
partial decode.

---

## 2 REJECTION POINTS

Every one of these makes `gt4_model_decode_part` return `NULL`. Grouped by where it fires.
`FAIL` is `#define FAIL do{...fail_line=__LINE__;return 0;}while(0)` (`model.c:39`).

**Container / header (`decode`, `model.c:646-689`)**

| # | Evidence | Rejects |
|---|---|---|
| R1 | `model.c:649` `if(!bytes \|\| size<128)return NULL;` | buffer < 128 B |
| R2 | `model.c:652` `memcmp(bytes+base,"MDLS",4)` | missing MDLS magic at `base` |
| R3 | `model.c:653` `if(n<128 \|\| !span(size,base,n))return NULL;` | MDLS size field <128 or beyond buffer |
| R4 | `model.c:659-660` `if(!span(size,p,8)){...goto bad;}` / `if(!span(n,off,len)...)goto bad;` | malformed Pat0 entry |
| R5 | `model.c:663` `gt4_model *m=calloc(...);if(!m){...goto bad;}` | allocation |
| R6 | `model.c:665` `if(r.ns>4096 \|\| r.nt>128 \|\| !span(n,u32(d+60),(size_t)r.ns*4)){...goto fail;}` | `ns>4096`, `nt>128`, shape table OOB |
| R7 | `model.c:668-669` `if(!span(n,lists,4)){...}` + `if(!span(n,list,r.nt*4)){...}` | texture list pointer OOB |
| R8 | `model.c:671` `if(!span(n,at,48)){...goto fail;}` | each Tex1 bank header < 48 B |
| R9 | `model.c:677` `if(!span(n,mo,(size_t)models*40)){...goto fail;}` | model list OOB |
| R10 | `model.c:678` `if(model_index>=0 && (unsigned)model_index>=models){...goto fail;}` | requested `model_index` ≥ model count |
| R11 | `model.c:679` `if(human>=2 && ...)goto fail;` | not reachable from `decode_part` (`human==0`) |
| R12 | `model.c:682` `if(!commands(...)){...goto fail;}` | any command-stream failure below |
| R13 | `model.c:685` `if(human>=2 && ...)goto fail;` | not reachable from `decode_part` |

Note `model.c:666-676`: a `tex1_decode` that returns ≤0 is **not** fatal — it just counts
(`model.c:675` `else m->unsupported++;`).

**Command stream (`commands`, `model.c:158-392`)**

| # | Evidence | Rejects |
|---|---|---|
| C1 | `model.c:381-384` `default:` → `FAIL` | **any unknown opcode** |
| C2 | `model.c:188,389` budget loop then `FAIL` | >8192 opcodes without opcode `0` |
| C3 | `model.c:210` `if(!span(r->n,p,skip))FAIL;` (op 4/5) | truncated shape index |
| C4 | `model.c:212` `if(arg>=r->ns \|\| *table<-1 \|\| *table>=(int)r->nt)FAIL;` | shape index ≥ `ns`, or texture table id out of range |
| C5 | `model.c:234` `if(!span(r->n,p,skip))FAIL;` (op 2/3) | truncated model index |
| C6 | `model.c:237` `if(arg>=models \|\| !span(r->n,mo,(size_t)(arg+1)*40) \|\| r->nesting>=8)FAIL;` | nested model id OOB / nesting > 8 |
| C7 | `model.c:246` `if(depth+1>=32)FAIL;` | matrix stack > 32 deep |
| C8 | `model.c:249-250` `if(!span(r->n,p,64))FAIL;` + `if(!isfinite(a.m[k]))FAIL;` | truncated / non-finite matrix |
| C9 | `model.c:256-257,263,268` | truncated / non-finite translate/scale/rotate args |
| C10 | `model.c:272,274,282,285,349,350,354-363,366-379` | truncated args for state / VM / misc ops |
| C11 | `model.c:286-287,312,325,330,344` | malformed jump-table branch offsets in callbacks |
| C12 | `model.c:289,291` | human callbacks (unreachable here) |
| C13 | `model.c:386` `if(!span(r->n,p,skip))FAIL;` | any opcode whose payload runs past the MDLS |

**Shape (`shape`, `model.c:393-581`)**

| # | Evidence | Rejects |
|---|---|---|
| S1 | `model.c:395` `if(!span(r->n,a,16))FAIL;` | shape record truncated |
| S2 | `model.c:402` `if(r->human>=2 && !tween)FAIL;` | unreachable here |
| S3 | `model.c:407` `if(!isfinite(authored_scale) \|\| authored_scale<0 \|\| authored_scale>1)FAIL;` | bad authored scale |
| S4 | `model.c:408` `if(!span(r->n,a+16,(size_t)np*8))FAIL;` | packet descriptor table OOB |
| S5 | `model.c:413` `if(!span(r->n,p,len))FAIL;` | packet payload beyond MDLS |
| S6 | `model.c:420,428,436` `if(!isfinite(...))FAIL;` | non-finite material colours |
| S7 | `model.c:442,443` `if(!span(end,p,bytes))FAIL;` | VIF word runs past the packet |
| S8 | `model.c:449` `if(f==3)FAIL;` | float component size 4 (`f==3`) — see §3 |
| S9 | `model.c:456` `if(start+num>256)FAIL;` | compressed position run past 256 slots |
| S10 | `model.c:479` `if(!isfinite(*out) \|\| fabsf(*out)>100000)FAIL;` | position out of range / non-finite |
| S11 | `model.c:491` `if(start+num>256)FAIL;` | UV run past 256 slots |
| S12 | `model.c:498,505` `if(fabsf(...)>10000)FAIL;` | UV magnitude > 10000 (or non-finite at `:505`) |
| S13 | `model.c:514` `unsigned start=addr&0x3f;if(start+num>256)FAIL;` | normal run past 256 slots |
| S14 | `model.c:519` `if(!isfinite(normal) \|\| fabsf(normal)>10000)FAIL;` | bad normal |
| S15 | `model.c:504,524` `if(dst>=256)FAIL;` | tween/colour slot past 256 |
| S16 | `model.c:530` `if(!span(end,p,bytes))FAIL;` | generic payload past packet |
| S17 | `model.c:552` `if(skinned && !skin_packet(...))FAIL;` | unreachable here (no skin on static cars) |

**Soft (not) rejections — geometry lost silently**

```
model.c:553  if(nv<3)continue;                       /* < 3 vertices: no triangles from this packet */
model.c:559  if(nr<2){r->out->unsupported++;continue;}  /* no strip-restart list: no triangles */
model.c:564  if(first+count>(unsigned)nv)count=(unsigned)nv-first;   /* clamped, not failed */
model.c:565  if(count<3)break;                       /* short strip: strip skipped */
```

There is **no** triangle-count rejection; `append` only refuses past 4,000,000
(`model.c:17` `if(m->count>=4000000)return 0;`).

---

## 3 FVF / VERTEX LAYOUT VARIANTS

The packet is a VIF-style word stream, 4 bytes per word: `addr` u16, `num` byte, `op` byte & 0x7f.

```
model.c:441  unsigned addr=u16(r->d+p),num=r->d[p+2],op=r->d[p+3]&127;p+=4;size_t bytes=0;
```

**Vertex unpack family** — entered when `(op&0x60)==0x60`:

```
model.c:447  else if((op&0x60)==0x60) {
model.c:448      unsigned f=op&3,c=((op>>2)&3)+1,sz=f==0?4:f==1?2:1;num=num?num:256;
model.c:449      if(f==3)FAIL;bytes=(size_t)num*c*sz;if(!span(end,p,bytes))FAIL;
```

So the accepted combinations are **c ∈ {1,2,3,4}, size ∈ {32,16,8}** — `f=0→4B`, `f=1→2B`, `f=2→1B`.
`f==3` (a "4-byte × N" form) is a hard `FAIL`. `num==0` means 256.

What each address pattern is bound to:

| Stream | Address test | Evidence |
|---|---|---|
| Positions | `(addr&0xbfc0)==0x8000 && c>=3` | `model.c:450` |
| UV, signed fixed | `(addr&0xffc0)==0x8040 && c==2 && f<=2 && !tween` | `model.c:481` |
| UV, alternates | `(addr==0xc040 \|\| addr==0x8040 \|\| (tween&&(0xc060\|\|0x8060))) && c==2 && f<=1` | `model.c:500` |
| Normals | `((addr&0xbfc0)==0x8040 \|\| (addr&0xbfc0)==0x8080) && c==3 && (f==0 \|\| !(addr&0x4000))` | `model.c:507` |
| Strip resets | `addr==0xc040 && f==2` | `model.c:522` |
| Vertex colour | `(addr==0xc080 \|\| (tween_mode==1 && tween && addr==0xc0a0)) && c>=3 && f==2` | `model.c:523` |

**Position encodings** (three, chosen by address, not by a format field):

```
model.c:461  if(addr==0xc000 && f==0 && authored_scale>0) *out=(int32_t)u32(q)*authored_scale;
model.c:464  else if(addr==0xc000 && f==0 && r->fixed==WORLD_FIXED && (r->d[a+8]&128)) *out=(int32_t)u32(q)/65536.f;
model.c:465  else if((addr&0x4000) && f==0) *out=fl(q);        /* 0x4000 bit ⇒ IEEE float */
model.c:474      int32_t x=f==0?(int32_t)u32(q):f==1?(int16_t)u16(q):(int8_t)*q;
model.c:477      *out=(float)x*(authored_scale>0?authored_scale:r->fixed);
```

* `0x4000` bit set + `f==0` ⇒ **float32 positions** (uncompressed).
* `0xC000` ⇒ **int32 scaled by `authored_scale`** (if nonzero), or world 1/65536.
* Otherwise ⇒ **signed fixed point** with the delta accumulator: `mode==2` adds into `row[k]`,
  `mode==1` offsets by `row[k]` (`model.c:475-476`), where `row` is set by STROW (`op 0x30`,
  `model.c:443`) and `mode` by STMOD (`op 0x05`, `model.c:445`). Car scale = `r.fixed` = 1/4096.

Normals: `model.c:518` `float normal=f==0?fl(q):f==1?(int16_t)u16(q)/32767.f:(int8_t)*q/127.f;`
— signed V3-8 at `0x8080` (lit) / `0x8040` (reflection), per the comment at `model.c:509-513`.

**Unknown layout → what happens:** the three data branches are an `else if` chain. If neither the
position test (`model.c:450`), the signed-UV test (`model.c:481`), the alternate-UV test
(`model.c:500`), the normal test (`model.c:507`), the reset test (`model.c:522`) nor the colour test
(`model.c:523`) matches — e.g. `c==1`, or `addr` outside the listed patterns — the body is skipped
and control falls to:

```
model.c:530  if(!span(end,p,bytes))FAIL;p=(p+bytes+3)&~(size_t)3;
```

i.e. the payload **is consumed and discarded**, and the vertices it was meant to fill keep their
zero-initialised values (`model.c:415` `gt4_vertex v[256]={0};`). Consequence: a triangle of three
zero positions at the model origin, or a collapsed silhouette — **not** an error. The only layout
that hard-fails is `f==3` (`model.c:449`). `c==1` is accepted by the size arithmetic but has no
consumer branch, so it is silently useless.

Other VIF words: `op==0x30` STROW 16 B (`model.c:443`), `op==0x31` STCOL 16 B (`model.c:444`),
`op==0x20` 4 B (`model.c:446`), `op==0x4a` `(num?num:256)*8` (`model.c:528`), `op==0x50||0x51`
`addr*16` (`model.c:529`). Plus `addr==0xc0c0` = 12 B carrying the blend flag at bit 21 of `+4`
(`model.c:442`).

**Strip assembly** (how vertices become triangles):

```
model.c:560  for(unsigned strip=1;strip<nr;strip++) {
model.c:563      unsigned count=(reset[strip]+6)/3;
model.c:564      if(first+count>(unsigned)nv)count=(unsigned)nv-first;
model.c:565      if(count<3)break;
model.c:566      for(unsigned i=0;i+2<count;i++) {
model.c:567          unsigned indices[3]={first+i+(i&1?2:0),first+i+1,first+i+(i&1?0:2)};
```

Alternating-winding triangle strip; `reset[]` is a per-strip **vertex** count that yields
`(n+6)/3` triangles.

---

## 4 ATTACHMENT TAGS

Definition:

```
model.h:17  uint8_t attachment; /* 0 body, 1..4 road wheels, 5+ steering frames */
```

Set in three places, all inside the callback opcode `6`/`77` (`model.c:284-347`):

| Value | Set by | Evidence |
|---|---|---|
| `0` (body) | default on `append` and on stack init | `model.c:28` `t->attachment=0;` · `model.c:164` `uint8_t attachment[32]={0};` |
| `1..4` (wheels) | car callback arg `11..14` | `model.c:321-322` `if(r->car && op==6 && u16(r->d+p)>=11 && u16(r->d+p)<=14) attachment[depth]=(uint8_t)(u16(r->d+p)-10);` |
| `5+frame` (steering) | car callback arg `8` (the steering-frame rotate) | `model.c:299` `if(r->car && op==6 && u16(r->d+p)==8) { ... attachment[depth]=(uint8_t)(5+frame); }` |
| reset to `0` | an absolute matrix load | `model.c:252` `if(op==15)attachment[depth]=0; /* load replaces callback transforms */` |
| inherited | a matrix push | `model.c:246` `stack[depth+1]=stack[depth];attachment[depth+1]=attachment[depth];` |
| onto triangles | after each shape draw | `model.c:216-217` `gt4_triangle *t=...;t->state=state;t->attachment=attachment[depth];` |

The steering frame index is `frame = 5 + n` where `n` counts distinct authored matrices
accumulated in `steering_frame[]` (`model.c:301-306`, bounded by `GT4_MODEL_STEERING_MAX 8`,
`model.h:19`); `attachment` is only 8 bits and the tag is `5+frame`, so at most 3 distinct
steering frames fit before the tag aliases wheel values.

**Who reads a tag, and the visual consequence of getting it wrong**

* Showroom body subset drops wheels: `showroom.c:723` `if(tr->attachment<1 || tr->attachment>4)body.triangles[body.count++]=*tr;`
* Showroom static pose skips wheels: `car_model.c:42` `if(body->triangles[i].attachment>=1 && body->triangles[i].attachment<=4)continue;`
* Wheel/brake split reads the native tag: `car_model.c:61` `if(a->car->triangles[i].attachment==native+1)count++;`, `car_model.c:68` `if(tr->attachment!=native+1)continue;` — picking the geometry authored for `slot^1`, `slot=axle*2+side` (`car_model.c:57`).
* Race splits steering: `race_view.c:414` `if(tag<5 || !m->steering[tag-5])rest.triangles[rest.count++]=...` and `race_view.c:437` `if(!tag || (tag>=5 && !m->steering[tag-5]))body.triangles[body.count++]=...`
* Steering geometry is extracted by tag: `model.c:708` `if(m->triangles[i].attachment==5+frame)`.

Failure modes:

* **Body triangles tagged 1..4** → they vanish from the body subset (`showroom.c:723`,
  `car_model.c:42`) and get relocated to a wheel hub (`car_model.c:74`
  `tr->v[j].p[k]=local->v[j].p[k]+hub[k];`) — body panels fly out to the wheels.
* **Wheels tagged 0** → drawn as bodywork at the authored origin; the car sits with wheels fused
  into the chassis (if the authored vertices are at the origin, four rims pile at (0,0,0)).
* **Nothing tagged 1..4** → `car_model.c:62` `if(!count)continue;` — no wheel/brake mesh is built at
  all (`a->wheel[]`/`a->brake[]` stay NULL). The car still loads (see §5) but is wheel-less.
* **Wheels/steering tagged ≥5 with no matching `steering[]` frame** → the race's static fallback
  keeps them (`race_view.c:414`) but the **posed body** also keeps them (`race_view.c:437`: the
  condition needs `!m->steering[tag-5]` to be false to drop them) — so they render at their authored
  pose, unsteered.
* **Steering mesh tagged 1..4** → treated as a wheel; the rim ends up on one hub and never turns.

For a generated car this is the load-bearing point: **tags are only ever produced by callback
opcodes 6/11..14 and 6/8 in the command stream.** An importer that only emits shapes cannot tag
wheels; the tag has to be expressed through the authored command program (or a hand-built stream
that mimics it).

---

## 5 MISSING-PART BEHAVIOUR

The decoder has no "empty model" concept — a missing or unparsable model is **NULL**:

```
model.c:678  if(model_index>=0 && (unsigned)model_index>=models){...goto fail;}
```

So asking for `model_index` ≥ `models` fails the whole call. What the callers do with NULL / zero
triangles:

**Showroom / car (`car_model.c:272`)**

```
car_model.c:272  if(!a->car || !a->car->count){fprintf(stderr,"showroom: empty car %s wing=%s\n",code,wing?wing:"");goto fail;}
car_model.c:273  fprintf(stderr,"showroom: %s model=%s color=%u triangles=%d textures=%d unsupported=%d\n",...);
car_model.c:274  fail:car_model_free(a);return NULL;
```

The "showroom: empty car" path covers **both** `a->car == NULL` (decode refused) and
`a->car->count == 0` (decoded but every packet produced no triangles — the soft paths in §2). Either
way the entire car load fails and the caller gets NULL; there is no placeholder car.

Which parts the loader asks for:

```
car_model.c:240  a->car=installed?gt4_model_decode_car_wing(data,size,(int)a->colors[a->choice].order-1,1):gt4_model_decode(data,size,(int)a->colors[a->choice].order-1);
car_model.c:269  ...a->shadow=gt4_model_decode_part(data,size,(int)a->colors[a->choice].order-1,2);
car_model.c:270  a->cast=gt4_model_decode_part(data,size,(int)a->colors[a->choice].order-1,1);
```

So the CAR4 is expected to publish at least **model 0 (body), model 1 (auxiliary volume/cast),
model 2 (contact shadow)**. `shadow`/`cast` are used without a NULL gate at `car_model.c:269-270`
within the `if(a->car)` block — a NULL there is tolerated downstream (`race_view.c:400`
`m->shadow=r3d_mesh_create(a->shadow);` handles NULL), but a *missing model 2* is exactly the
`model_index>=models` case from `model.c:678`.

Note the deliberate non-use of model 1 in the showroom: `car_model.c:235-238` documents that model 1
is "an auxiliary volume, not additional opaque bodywork" and needs its own pass context.

Other missing-part paths, all explicit:

```
car_model.c:252  if(!wheel_data || !wheel_size){fprintf(stderr,"showroom: missing wheel %s\n",wheel_path);...goto fail_data;}
car_model.c:263  if(!part){fprintf(stderr,"showroom: cannot decode wing %s\n",wing_path);...goto fail_data;}
car_model.c:267  if(!attached){...goto fail_data;}
course_gadget.c:88  if(model<0 || !(g->model[i]=gt4_model_decode_part(d+mdls,n-mdls,-1,model*2)))goto fail;
```

`course_gadget.c:88` shows the index convention used by course props: the descriptor's signed model
index is **doubled** (`model*2`, comment at `course_gadget.c:84-85`: "the second model is its
shadow"), and **any one** prop model that fails to decode fails the whole gadget table
(`course_gadget.c:120` `fail:gt4_course_gadgets_close(g);return NULL;`).

Race trophy: `race_game.c:3270` decodes three CLUT variants of one MDLS and simply skips NULLs
(`race_game.c:3271` `if(m){...}`).

---

## 6 MINIMUM VIABLE MESH

In plain terms — the smallest thing that both **decodes** and **drives**:

1. **A buffer ≥ 128 bytes.** Either the MDLS at offset 0, or a CAR4 header with `u32(+24)` = the MDLS
   offset. If CAR4, `u32(+28)` is either 0 or a valid `"Pat0"` block; a nonzero pointer that is not
   `"Pat0"` is silently ignored (`model.c:655`), but a *malformed* Pat0 aborts (`model.c:659-660`).
2. **MDLS whose `u32(+16)` is its own size**, ≥ 128 and ≤ the enclosing buffer.
3. **`u16(+22) models ≥ 1`** (and ≥ `model_index+1` for every index the caller uses — the showroom
   trio needs 0, 1 and 2). Each 40-byte model record needs a command-stream offset at `+24`.
4. **`u16(+24) ns ≥ 1`**, a shape offset table at `u32(+60)`, and `ns ≤ 4096`.
5. **`u16(+28) nt` texture banks**, a `u32` pointer at `u32(+68)` to an `nt`-entry `u32` table, and
   each entry pointing at ≥ 48 bytes of Tex1. `nt == 0` is legal and yields untextured geometry
   (`model.c:665-676`: the span checks degenerate to zero-length and the loop body never runs) —
   the triangles then carry `texture == -1`.
6. **`u16(+26) nm` materials**: `nm == 0` is legal — the material lookup is guarded
   (`model.c:419` `if(mat<r->nm && span(r->n,material,32))`, `model.c:425`), so vertices keep the
   defaults `diffuse={1,1,1}`, `alpha=1`, `shine=16` (`model.c:422-424`).
7. **One shape**, `kind` 1 (opaque), `np ≥ 1`, `authored_scale` in `[0,1]` (use 0 to mean "the
   decoder's `1/4096`"), one 8-byte packet descriptor whose `+6` flags give `tex = 0` (untextured)
   and a `mat` index that is either negative or `< nm`.
8. **Inside that packet:**
   * a strip-restart list — `addr 0xc040`, `f==2` — with **`num ≥ 2`** so `nr ≥ 2`
     (`model.c:522`), otherwise `model.c:559` discards everything;
   * positions — `addr 0x8000` (`(addr&0xbfc0)==0x8000`, `c==3`), `f==1` (V3-16) or `f==0` (V3-32),
     either at `0x4000`-bit addresses for float or signed fixed point for compressed;
   * at least `reset[1] ≥ 3` vertices, so `(reset[1]+6)/3 ≥ 3` triangles are emitted (`model.c:563`);
   * optional normals (`0x8080`, `c==3`) and UVs (`0x8040`, `c==2`) — without normals the renderer
     substitutes a per-face normal (`model.c:509-513`).
9. **A command stream** starting at the model record's `+24` that reaches the shape via opcode `4/5`
   after setting a texture table with `43/44` (or `80/81`), and **ends with opcode `0`** within 8192
   opcodes. Every byte must be one of the opcodes in §1.7 — one unknown byte costs the whole model.
10. **To actually drive (wheels + steering),** the command stream must additionally contain callback
    opcode `6` with ids `11..14` around each wheel's shape draws (that is the **only** place tags
    1..4 come from, `model.c:321-322`) and id `8` for the steering frame (`model.c:299-306`). A mesh
    with no such callbacks loads and renders as a static body but produces no wheel split
    (`car_model.c:62 if(!count)continue;`) and no steering.

The practical minimum that renders *and* drives is therefore: **CAR4 (or bare MDLS) · models 0/1/2 ·
one opaque shape per part · V3-16 positions + 0xc040 strip resets with `num ≥ 2` · a command stream
that draws the body with no tag and each wheel inside callback 6/11..14.** Steering is optional; the
car drives without it, but the front wheels then never turn.

---

## 7 UNKNOWNS

* **Header fields not read by this path.** `decode_part` never touches `d+0..+21`, `d+30`, `d+34`,
  or anything past `d+92`; their role is unstated here. `WORLD_FIXED` (`model.c:12`), the world
  `+8` bit-7 test (`model.c:463`) and the tween machinery (`model.c:399-401`) are only reachable via
  other entry points.
* **Strides are assumed, not declared.** Shape list = 4 B, model record = 40 B, material = 80 B,
  host record = 8 B (`model.c:418`, `:624`, `:677`, `:682`). A generated file cannot state them.
* **`f==3` and `c==1`.** `f==3` is a hard failure (`model.c:449`) and `c==1` is accepted by the size
  arithmetic but has no consumer branch — the intent of the `c==1` case (why it is legal to skip) is
  not established by this file.
* **`kind` semantics.** Only `kind==2` is special-cased (`model.c:557`); the meaning of kinds 0, 1,
  3..31 is not decoded here beyond `t->kind=1` on append (`model.c:28`).
* **`tex1_decode`** (`model.c:672`) is not audited by this document; a bank it rejects degrades to
  `unsupported++`, not failure, so a malformed texture list can silently produce untextured draws.
* **Trailing MDLS bytes.** Nothing checks that the command streams and shapes exhaust the MDLS; a
  file may carry unreferenced bytes, and the loader in `car_model.c:202-211` appends a `.pat` block
  past the MDLS end and rewrites `+28`, which assumes MDLS-relative spans never reach it.
* **`attachment` for `lamp`.** Callback 6 id `0` sets `lamp` (`model.c:323-326`, `model.c:574`)
  rather than `attachment`; the interaction of the lamp branch with a generated stream is not
  covered here.
* **Budget effects.** Hitting the 8192-opcode ceiling fails the model (`model.c:389`) but leaves no
  indication in `gt4_model_stats` other than `fail_op == -2` (`model.c:388`).
