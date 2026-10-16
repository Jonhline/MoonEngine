# The CAR model container, as VULCAN 4 reads it (CAR4 + inside-MDLS + Pat0)

Scope: the **byte container that IS a car model** for this engine — what `gt4_model_decode_part`
(`src/core/model.c:593`) and the car loader (`src/platform/sdl/car_model.c`) actually touch, byte by
byte, and what a writer would have to emit for a mesh the engine accepts.

This **extends** `docs/MODELS.md` (lines 41-65) and `docs/MODS.md` (lines 31-80); it does not repeat
the mod-design material. It also does not repeat the VOL/disc side — that is
`docs/vulcan4/research/add-cars-loadpath.md` §1.1-1.2 (`gt4_vol_read`, `src/core/vol.c:340`).

Every claim is `path:line` + the quoted line. Read-only survey: no `src/`/`include/` file was
touched, nothing was built. Corrigenda to MODELS.md/MODS.md are called out inline as **FIX**.

**What MODELS.md/MODS.md get wrong or leave implicit (FIX list):**

1. `docs/MODELS.md:45` — "CAR4's main model is at header offset 0x18" is right, but the *whole*
   header contract the engine relies on is 8 fields, not 2 (§1.1).
2. `docs/MODELS.md:45` — "Pat0 at 0x1c contains colour count, patch count" is right about the block,
   but the two u16 counts live at **Pat0+0x10/+0x12** (CAR4+0x1c only *points* at the Pat0 block),
   and the offset table is `nc·np` u32 at **Pat0+0x20**, not a flat list. Evidence §1.4.
3. `docs/MODELS.md:49` — "VarOrder is one-based" is right and is why the loader passes
   `order-1` as the Pat0 colour index (`car_model.c:240`).
4. `docs/MODS.md:36-39` implies the container stores "câmera, piloto, sombra" as named parts: to the
   decoder these are just **model indices** (0 body, 1 cast/hull, 2 shadow) plus three CARINFO
   pointer fields. `docs/MODS.md:54-59`'s glTF names are a *proposed* SDK convention, not the
   engine's; the engine's own tags are the callback attachment bytes (§2.4).

---

## 1. CONTAINER LAYOUT

### 1.1 CAR4 header — only 8 fields are read

`src/core/model.c:650-651`:
```c
int car=!memcmp(bytes,"CAR4",4);
if(car){base=u32(bytes+24);patch=u32(bytes+28);}
```
`src/platform/sdl/car_model.c:212-213`:
```c
if(data && size>=0x88 && !memcmp(data,"CAR4",4)) {
    unsigned n=u32(data+0x80),at=u32(data+0x84);
```
`src/core/driver_model.c:18-19`:
```c
size_t info=u32(d+0x10);if(!info || !span(n,info,0x40))return 0;
unsigned count=u32(d+info+0x38),points=u32(d+info+0x3c);
```
`src/core/driver_model.c:37`:
```c
c.builtin_offset=u32(d+52);c.geometry_offset=u32(d+info+0x50);
```
`src/platform/sdl/car_model.c:91`:
```c
size_t off=u32(data+(piece?44+axle*4:32));
```

| CAR4@ | type | meaning | evidence |
|---|---|---|---|
| 0x00 | `char[4]` | `"CAR4"` — the only magic that switches the decoder into car mode | model.c:650 |
| 0x10 | u32 | offset of **CARINFO** (must be non-zero, ≥0x40 bytes must fit) | driver_model.c:18 |
| 0x18 | u32 | offset of the main **MDLS** (base); `"MDLS"` must be there | model.c:651-652 |
| 0x1C | u32 | offset of the **Pat0** paint block; **0 = no paints** | model.c:651,655 |
| 0x20 | u32 | rim MDLS offset (replacement-rim pass) | car_model.c:91 |
| 0x2C | u32 | front-tire MDLS offset | car_model.c:91 (`44+axle*4`, axle 0) |
| 0x30 | u32 | rear-tire MDLS offset | car_model.c:91 (axle 1) |
| 0x34 | u32 | embedded **built-in driver** MDLS offset (validated by `"MDLS"` + its own +16 size) | driver_model.c:37-40 |
| 0x80 | u32 | camera-point count `n` (clamped to 32) | car_model.c:213,216 |
| 0x84 | u32 | camera-point table offset; **32 bytes/entry, 8 floats** | car_model.c:213-215 |

Notes the code proves:

- `+0x20/+0x2C/+0x30` are the wheel/tire parts that `docs/MODELS.md:60` lists as
  "0x20/0x2c/0x30" — confirmed, and the *(tire,rim)* order is tire first
  (`car_model.c:88-94`, `piece=1` = tire reads 0x2C/0x30, `piece=0` = rim reads 0x20).
- The roof/type byte is **not** in CAR4: `car_model.c:195` `a->race_open=generic[0x14]>=2 || (generic[0x14]==1 && cars<3);`
  where `generic` is the **GENERIC_CAR.dbt row**. **FIX** for anyone assuming a CAR4 header type.
- `data[28]` (CAR4+0x1C) is also *written* by the loader when it glues a `.pat` on (§1.4), so the
  decoder sometimes sees a patch offset the disc never had.
- 0x04-0x0F, 0x14-0x17, 0x24-0x2B, 0x38-0x7F, 0x88+ → UNKNOWN, nothing reads them (§6).

### 1.2 CARINFO (at CAR4+0x10)

| CARINFO@ | type | meaning | evidence |
|---|---|---|---|
| 0x00 | u16 | flags | driver_model.c:36 |
| 0x02 | u8 | driver `type` (→ `/character/*.mdl` table) | driver_model.c:36,65-74 |
| 0x03 | u8 | crew flag | driver_model.c:36,61 |
| 0x38 | u32 | exhaust outlet count | driver_model.c:19 |
| 0x3C | u32 | exhaust outlet table, **24 bytes each: position(3f) + direction(3f)** | driver_model.c:20,24-25,29 |
| 0x50 | u32 | geometry block offset (also the wing-attachment source) | driver_model.c:37, car_model.c:258 |
| 0x58 | f32 | front upper-wheel Y (0 disables the wheel clamp) | car_model.c:219-221 |
| 0x5C | f32 | rear upper-wheel Y | car_model.c:219-221 |

`car_model.c:219-222`:
```c
/* 004514A0 +24..58: CARINFO +58/+5C are front/rear upper wheel Y.
   Zero disables the clamp. They are not the HumanGeom pointer +50. */
if(data && size>=0x14){size_t info=u32(data+0x10);if(info && info<=size && size-info>=0x60){
    memcpy(a->wheel_limit,data+info+0x58,8);
```

### 1.3 CARINFO geometry block (at CARINFO+0x50) — floats, driver + wing

`driver_model.c:42-54` reads it as a flat float record (must be ≥0x7C, wing path wants ≥0xE0):

| geom@ | contents | evidence |
|---|---|---|
| 0x00 | `wheel[3]` | driver_model.c:44 |
| 0x0C | `wheel_normal[3]` | :44 |
| 0x18 | `wheel_radius` | :45 |
| 0x1C, 0x28 | `feet[0]`, `feet[1]` | :45-46 |
| 0x34 | `seat[3]` | :46 |
| 0x40 | `chair_angle` | :47 |
| 0x4C / 0x58 / 0x64 | `shift[3]` / `shift_normal[3]` / `shift_length` | :47-48 |
| 0x6A | int16 `shift_type` | :50 |
| 0x6C / 0x78 | `handbrake[3]` / `handbrake_length` | :48-49 |
| 0xAC / 0xB8 | `fuel_port[3]` / `fuel_normal[3]` (optional, sets `fuel_geometry`) | :53-54 |
| 0xD0 | **wing translation** (12 bytes copied verbatim) | car_model.c:264 |
| 0xDC | **wing uniform scale**, multiplied by 0.5 | car_model.c:264-265 |

`car_model.c:264-266`:
```c
float tr[1][6];memcpy(tr[0],data+geometry+0xd0,12);float scale;memcpy(&scale,data+geometry+0xdc,4);scale*=.5f;
tr[0][3]=tr[0][4]=tr[0][5]=scale;
int attached=gt4_model_attach(a->car,part,tr,1);gt4_model_free(part);
```

### 1.4 Pat0 (the `.pat` sidecar) — colour-major patch table

On-disc form, applied inside the decoder (`model.c:655-661`):
```c
if(car && color>=0 && span(size,patch,32) && !memcmp(bytes+patch,"Pat0",4)) {
    unsigned nc=u16(bytes+patch+16),np=u16(bytes+patch+18);
    if((unsigned)color<nc && span(size,patch+32,(size_t)nc*np*4))for(unsigned i=0;i<np;i++) {
        size_t p=patch+(size_t)u32(bytes+patch+32+((size_t)color*np+i)*4);
        if(!span(size,p,8)){...goto bad;}size_t off=u32(bytes+p),len=u32(bytes+p+4);
        if(!span(n,off,len)||!span(size,p+8,len)){...goto bad;}memcpy(d+off,bytes+p+8,len);
```

| Pat0@ | type | meaning |
|---|---|---|
| 0x00 | `char[4]` | `"Pat0"` |
| 0x10 | u16 | `nc` = colour count |
| 0x12 | u16 | `np` = patches **per colour** |
| 0x20 | u32 × (nc·np) | colour-major offset table: entry `[color*np+i]` → record |
| record+0x00 | u32 | destination offset **relative to the MDLS copy** |
| record+0x04 | u32 | replacement length |
| record+0x08 | bytes[len] | the replacement bytes |

`color` is the loader's paint index, passed as `order-1`
(`car_model.c:240`: `(int)a->colors[a->choice].order-1`); a colour ≥ `nc` is silently skipped, but an
**out-of-bounds record or destination is fatal** (`goto bad` → whole decode returns NULL).

**The race path has no inline paints.** `car_model.c:199-208`:
```c
/* The race's models carry no paints (CAR4 +0x1C zero): func_003B5988
   loads the path plus ".pat" (0x003B59D0), the same Pat0 table as the
   menu model's. Appended here, with +0x1C at it, for the decoder. */
if(cars>0 && data && size>=32 && !memcmp(data,"CAR4",4) && !u32(data+28)) {
    char pat_path[100];size_t pat_size=0;snprintf(pat_path,sizeof pat_path,"%s.pat",path);
    uint8_t *pat=gt4_vol_read(v,pat_path,&pat_size);
    if(pat && pat_size>=32 && !memcmp(pat,"Pat0",4)) {
        size_t at=(size+15)&~(size_t)15;uint8_t *joined=realloc(data,at+pat_size);
        if(joined){data=joined;memset(data+size,0,at-size);memcpy(data+at,pat,pat_size);size=at+pat_size;
            data[28]=(uint8_t)at;data[29]=(uint8_t)(at>>8);data[30]=(uint8_t)(at>>16);data[31]=(uint8_t)(at>>24);}
```
So: sidecar path = model path + `".pat"`; 16-byte-aligned append; CAR4+0x1C rewritten; record
destination offsets stay MDLS-relative, which is why appending works at all.

### 1.5 MDLS header (at `base`)

Verified header fields (offsets are relative to the MDLS start, i.e. `d = bytes+base`):

`src/core/model.c:664-669`:
```c
r.ns=u16(d+24);r.nm=u16(d+26);r.nt=u16(d+28);
if(r.ns>4096 || r.nt>128 || !span(n,u32(d+60),(size_t)r.ns*4)){...goto fail;}
size_t lists=u32(d+68);if(!span(n,lists,4)){...goto fail;}size_t list=u32(d+lists);
```
`src/core/model.c:677`:
```c
unsigned models=u16(d+22);size_t mo=u32(d+56);if(!span(n,mo,(size_t)models*40)){...goto fail;}
```
`src/core/model.c:653`:
```c
size_t n=u32(bytes+base+16);if(n<128 || !span(size,base,n))return NULL;
```

| MDLS@ | type | meaning | evidence |
|---|---|---|---|
| 0x00 | `char[4]` | `"MDLS"` | model.c:652 |
| 0x10 | u32 | total blob size `n` (≥128, must fit in the file) | model.c:653 |
| 0x16 | u16 | **models** count (model descriptor table, 40 B each) | model.c:677 |
| 0x18 | u16 | **shapes** (`ns`) — the shape table's entry count | model.c:664 |
| 0x1A | u16 | **materials** (`nm`), 80 B each | model.c:664,418-437 |
| 0x1C | u16 | **texture tables** (`nt`, ≤128) | model.c:664-665 |
| 0x20 | u16 | callback branch-table size (hand paths) | model.c:639 |
| 0x24 | u16 | VM host/variable count (hand paths) | model.c:624 |
| 0x2A | u16 | symbol count | driver_model.c:88 |
| 0x2E | u16 | skin bind slots (1..20; `slots==0` fails a skinned decode) | model.c:58-59, driver_model.c:105 |
| 0x38 | u32 | **model descriptor table** (models × 40 B) | model.c:677 |
| 0x3C | u32 | **shape table** (`ns` × u32 absolute offsets) | model.c:665,215 |
| 0x40 | u32 | **material table** (nm × 80 B) | model.c:418 |
| 0x44 | u32 | texture "lists" pointer → `list` = u32 there → `nt` × u32 (each → a `Tex1` block, ≥48 B) | model.c:668-672 |
| 0x48 | u32 | skeleton bind table (binds × 80 B) | driver_model.c:105 |
| 0x4C | u32 | VM host table (count × 8 B) | model.c:624 |
| 0x58 | u32 | symbol table (num × 8 B) | driver_model.c:88-92 |
| 0x5C | u32 | VM program base | model.c:617 |
| 0x68 | u32 | dimension/parameter table (`0x800000` = 16 floats, `0x800001` = 2 floats) | driver_model.c:128-134 |

### 1.6 Model descriptor (40 B) and the command stream

`model.c:682`: `if(!commands(&r,u32(d+mo+i*40+24),&table)){...goto fail;}` — the **only** field read is
`+0x18` = offset of that model's command stream (absolute in the MDLS copy). Nested draws read the
same field: `model.c:240` `int ok=commands(r,u32(r->d+mo+arg*40+24),&sub_table);`.

`gt4_model_decode()` maps a CAR4 to **model 0**; `gt4_model_decode_part(...,model_index)` selects one
(`model.c:690-691`):
```c
gt4_model *gt4_model_decode(const uint8_t *bytes,size_t size,int color)
{return gt4_model_decode_part(bytes,size,color,bytes && size>=4 && !memcmp(bytes,"CAR4",4)?0:-1);}
```
The loader uses exactly this to take three roles out of one file — `car_model.c:269-270`:
```c
a->shadow=gt4_model_decode_part(data,size,(int)a->colors[a->choice].order-1,2);
a->cast=gt4_model_decode_part(data,size,(int)a->colors[a->choice].order-1,1);
```
→ model 0 = body (decoded via `gt4_model_decode`, with wing awareness), model 1 = cast/hull, model 2 =
the contact shadow. **A writer that wants a shadow must put it in model 2, not in a named node.**

### 1.7 Command opcodes the decoder executes

`commands()` (`model.c:158-392`) runs a byte stream, one opcode per step, budget 8192
(`model.c:188`), terminated by **opcode 0** (`model.c:208` `case 0:return 1;`). Anything not in the
switch is fatal (`model.c:381-384`):
```c
default:
    r->out->unsupported++;
    if(r->stats){r->stats->fail_op=(int)op;r->stats->fail_offset=p-1;}
    FAIL;
```

Handled: `0` end; `2/3` draw another model (nested, depth ≤8); `4/5` **draw shape** (1- or 2-byte
arg = shape index); `6/77` callback + branch table; `7` skip 17+u16; `8/9` jump (arg<skip → FAIL);
`12/13` push/pop matrix (stack ≤32); `14,41,42,89` skip 1; `15/16` load/multiply 4×4 float matrix
(column-major, OpenGL order — `model.c:105-108`); `17/18` translate/scale; `19/20/21/22` rotate(axis);
`23/24/25/26/27/28/29/30/31/32/36/37/38/45/46/47/64/78/79` render state + culling; `33/34/35` fog;
`39,49,50,52,54` skip 4; `40,51,55,82,83` skip 16; `43/44` bind texture table; `68,72,73,74,75,90`
skip 2; `69,70` skip 6; `76` VM tween register (human only); `80/81` bind external slot 2 (rim);
`84/85` VM colour scale/offset, 8 bytes (`model.c:370`); `10` skip 1+count*12+2.

**Not handled → FAIL** (so a writer must never emit them): 1, 116 (and every other unlisted value).
`docs/MODELS.md:254-259` records that opcodes 2/116 once broke three cars; 2 is now handled.

---

## 2. WHAT THE DECODER DEMANDS

### 2.1 The shape table and one shape record

`model.c:215` (draw): `if(!shape(r,u32(r->d+u32(r->d+60)+arg*4),*table))FAIL;` — table at MDLS+0x3C,
`ns` u32 **absolute** offsets; the draw opcode's argument indexes it, so `arg < ns` is mandatory
(`model.c:212` `if(arg>=r->ns || *table<-1 || *table>=(int)r->nt)FAIL;`).

`model.c:395-414`:
```c
if(!span(r->n,a,16))FAIL;
unsigned np=u16(r->d+a+10),kind=r->d[a+8]&31;
...
float authored_scale=fl(r->d+a+4);
if(!isfinite(authored_scale) || authored_scale<0 || authored_scale>1)FAIL;
if(!span(r->n,a+16,(size_t)np*8))FAIL;
...
for(unsigned j=0;j<np;j++) {
    size_t desc=a+16+j*8,p=a+(size_t)u32(r->d+desc),len=u16(r->d+desc+4)*16,end=p+len;
    if(!span(r->n,p,len))FAIL;
    unsigned flags=u16(r->d+desc+6),mat=(flags>>9)-1,tex=flags&511;
```

| shape@ | type | meaning |
|---|---|---|
| 0x04 | f32 | `authored_scale` — scales the model matrix columns **and** the compressed position unit; must be finite, **0..1** |
| 0x08 | u8 | `kind = &31`, tween mode `>>5` |
| 0x09 | u8 | flags; `&0x80` = skinned (human streams) |
| 0x0A | u16 | `np` = packet-descriptor count |
| 0x10 | 8 B × np | descriptors |

Descriptor (8 B): `+0` u32 **offset relative to the shape start**; `+4` u16 **length in 16-byte
units** (`len = u16*16` — the packet list is `len` bytes, not a count of vertices); `+6` u16
`flags` → `mat = (flags>>9)-1` (0 = "no material", since `mat=-1`), `tex = flags & 511`
(texture-table index, 1-based: `texture = texbase[table] + tex - 1`, `model.c:555`).

Material, 80 B at `u32(d+0x40)+mat*80` (`model.c:418-437`): `+0` ambient rgb, `+0x10` diffuse rgb,
`+0x1C` alpha f32 (kept, **not** treated as opacity), `+0x20` specular rgb, `+0x40` shine f32
(clamped 1..128).

### 2.2 The packet stream: the FVF/vertex layouts the decoder accepts

Inside a packet list, each instruction is a 4-byte header `addr`(u16) | `num`(u8) | `op`(u8&0x7F)
(`model.c:441`). The decoder accepts exactly these forms (`sz = 4/2/1` for `f = 0/1/2`):

`model.c:442-449` (header decoding):
```c
if(addr==0xc0c0){bytes=12;if(!span(end,p,bytes))FAIL;blend_enabled=(u32(r->d+p+4)>>21)&1;}
else if(op==0x30){bytes=16;...row[k]=(int32_t)u32(r->d+p+k*4);}
else if(op==0x31)bytes=16;
else if(op==0x05)mode=addr&3;          /* STMOD: 0 plain, 1 offset by row, 2 accumulate */
else if(op==0x20)bytes=4;
else if((op&0x60)==0x60) {
    unsigned f=op&3,c=((op>>2)&3)+1,sz=f==0?4:f==1?2:1;num=num?num:256;
    if(f==3)FAIL;bytes=(size_t)num*c*sz;if(!span(end,p,bytes))FAIL;
```

| stream | VIF addr | c | f | unit / stored as | evidence |
|---|---|---|---|---|---|
| positions, float | `0xC000` | ≥3 | 0 | raw f32 when `authored_scale==0` (the `0x4000` bit) | model.c:465 |
| positions, int32 × shape scale | `0xC000` | ≥3 | 0 | `(int32)v * authored_scale` | model.c:461-462 |
| positions, fixed 1/4096 (cars) | `0x8000` + start | ≥3 | 0/1/2 | signed int32/int16/int8 × `authored_scale`or `1/4096` | model.c:467-477 |
| positions, STMOD accumulate (courses) | `0x8000` | ≥3 | 0/1/2 | `row[k]+=x` (mode 2) / `x+=row[k]` (mode 1) | model.c:474-477 |
| UV, fixed 1/4096 | `0x8040` (+n) | 2 | ≤2 | signed delta through the row, `/4096` | model.c:481-498 |
| UV, absolute | `0xC040`/`0x8040` | 2 | ≤1 | int16/4096, u32/4096, or f32 at `0x4000` | model.c:500-506 |
| normals | `0x8040`/`0x8080` | 3 | 0/1/2 | f32, int16/32767, int8/127 | model.c:507-521 |
| vertex colour | `0xC080` | ≥3 | 2 | byte/128 multiply; `c==4` also alpha/128 | model.c:523-527 |
| strip restart list | `0xC040` | — | 2 | `nr=num`, `reset[i]` bytes | model.c:522 |
| other | op `0x4a` | — | — | `(num?:256)*8` bytes | model.c:528 |
| other | op `0x50/0x51` | — | — | `addr*16` bytes | model.c:529 |

Positions must land in slots `start..start+num ≤ 256` (`model.c:456`); every value is checked
`isfinite` and `|x| ≤ 100000` (positions), `10000` (uv/normals). Rows are 4-byte aligned:
`model.c:530` `p=(p+bytes+3)&~(size_t)3;`.

**Vertex record the decoder fills** (`model.h:4`):
```c
typedef struct { float p[3],uv[2],rgb[3],normal[3],ambient[3],specular[3],shine,alpha; } gt4_vertex;
```
`p` from positions, `uv` from UV streams, `normal` from normal streams, `rgb` = material diffuse ×
per-vertex colour scale, `ambient`/`specular`/`shine` from the material row, `alpha`=1 default.

### 2.3 Triangles: strip restarts, winding, and what is dropped

`model.c:559-578`:
```c
if(nr<2){r->out->unsupported++;continue;}
for(unsigned strip=1;strip<nr;strip++) {
    unsigned count=(reset[strip]+6)/3;
    if(first+count>(unsigned)nv)count=(unsigned)nv-first;
    if(count<3)break;
    for(unsigned i=0;i+2<count;i++) {
        unsigned indices[3]={first+i+(i&1?2:0),first+i+1,first+i+(i&1?0:2)};
```
- A shape with **fewer than 2 restart entries produces no geometry** (`unsupported++`, no failure).
- Vertex count per strip = `(reset[strip]+6)/3`, clamped to the packet's vertex count.
- Triangle winding alternates per `i` (standard PS2 strip de-interleave), so a writer emitting an
  index-free strip must produce that alternation.
- `nv<3` → shape skipped; `nr<2` → shape skipped.
- `source_model` is stamped per triangle after each command stream (`model.c:683`), so triangles from
  nested/other models carry their authored model index.

### 2.4 Per-triangle attachment tags (model.h:17 semantics) — where they come from

`src/core/model.h:17`:
```c
uint8_t attachment; /* 0 body, 1..4 road wheels, 5+ steering frames */
```
`src/core/model.h:19`: `#define GT4_MODEL_STEERING_MAX 8` → steering tags 5..12.

| tag | meaning | how the decoder assigns it |
|---|---|---|
| 0 | body | default (`model.c:28` `t->attachment=0;`); a matrix **load** clears it (`model.c:252` `if(op==15)attachment[depth]=0; /* load replaces callback transforms */`) |
| 1..4 | the four wheel frames | callback `11..14` → `attachment[depth]=(uint8_t)(u16(r->d+p)-10);` (`model.c:321-322`) — 11→1 … 14→4 |
| 5..12 | steering frames | callback `8` → `attachment[depth]=(uint8_t)(5+frame);` (`model.c:306`), frames deduplicated by matrix (`model.c:299-305`) |
| copy on push | — | `case 12: ... attachment[depth+1]=attachment[depth];` (`model.c:246`) |

A second, orthogonal tag rides along: `uint8_t lamp` (0 none, 1 released-brake branch, 2 lit branch,
`model.h:18`), assigned by callback `0`'s two-branch handling (`model.c:323-327`) and stamped per
triangle at draw (`model.c:217` `t->lamp=lamp;`). `docs/MODELS.md:211-226` records the semantics.

Wheel-frame tags are the **only** way a body triangle is recognised as wheel-adjacent; the loader
splits on them and mirrors them left/right (`car_model.c:61`):
```c
for(int i=0;i<a->body_count;i++)if(a->car->triangles[i].attachment==native+1)count++;
```
with `native = slot^1` and `slot = axle*2+side` (`car_model.c:57`), i.e. native order **1=RL,
2=FL, 3=RR, 4=FR**. Body pose correction skips them (`car_model.c:42`):
```c
if(body->triangles[i].attachment>=1 && body->triangles[i].attachment<=4)continue;
```
Steering parts are extracted once, in callback-local space (`model.c:708` filters
`attachment==5+frame`; `gt4_model_steering_part` inverts the authored frame matrix).

### 2.5 Everything that makes a decode FAIL vs merely degrade

Fatal (`FAIL` → NULL from `gt4_model_decode_part`): file <128 B; `"MDLS"` not at `base`; MDLS size
<128 or outside the file; `ns>4096`; `nt>128`; shape/model/material/list tables out of bounds; a
`Tex1` block that does not fit 48 B; `model_index` ≥ models; an unknown opcode; a command budget of
8192 exceeded; a malformed branch offset; a VIF `f==3`; non-finite or absurd coordinates; a
Pat0 record/destination out of bounds.

**Not** fatal: a texture block whose `Tex1` fails to decode → `unsupported++` (`model.c:675`); a
shape with `nr<2` or `nv<3` → skipped (`model.c:553,559`); an out-of-range Pat0 colour → skipped.

So "valid" is strictly: **every table reachable, every opcode known, every jump target in bounds,
every float finite and in range.** There is no checksum, version, or length-of-list field to satisfy.

---

## 3. NAME/PATH RESOLUTION

A car is addressed by a **GENERIC_CAR label**, resolved through three SpecDB tables (`car_model.c:174-177`):
```c
if(!gt4_specdb_label(labels.data,labels.size,code,&id) || row(links,id,r)!=8)return NULL;
if(row(variants,u32(r),r)!=52)return NULL;const char *model=str(strings,u32(r));
if(!model || strlen(model)>24 || strchr(model,'/') || strchr(model,'\\'))return NULL;
```
with the tables opened at `car_model.c:152-153`:
```c
t->labels=read_table(v,"GENERIC_CAR.idi");t->links=read_table(v,"CAR_VARIATION_american.dbt");
t->variants=read_table(v,"VARIATIONamerican.dbt");t->strings=read_table(v,"american_StrDB.sdb");
```
Chain: `code` → `GENERIC_CAR.idi` label table → id → `CAR_VARIATION_american.dbt` row (8 B, first u32
= variation id) → `VARIATIONamerican.dbt` row (52 B, first u32 = string index into
`american_StrDB.sdb`) → **model filename**, accepted only if 1..24 chars and containing no `/` or `\`.

Directory selection (`car_model.c:190-198`):
```c
const char *directory="menu";
if(cars>0) {
    uint8_t generic[256];if(disc_row(v,"GENERIC_CAR.dbt",id,generic)!=24)goto fail;
    /* Ordinary A-spec, 003882D4..003882F4. Roof type 1 opens only in
       a field of fewer than three; roof type >=2 always selects open. */
    a->race_open=generic[0x14]>=2 || (generic[0x14]==1 && cars<3);
    directory=a->race_open?"open":"lod";
}
char path[96];size_t size=0;snprintf(path,sizeof path,"/car/%s/%s",directory,model);uint8_t *data=gt4_vol_read(v,path,&size);
```

| path | when | contents (what the engine takes from it) |
|---|---|---|
| `/car/menu/<model>` | showroom / menu (`cars==0`) | body = model 0 (+ models 1, 2), inline **Pat0** paints, per-model wheel/tire offsets, CARINFO, camera points |
| `/car/lod/<model>` | race, roof type 0 (or 1 with ≥3 cars) | same structure but **no inline paints** → `.pat` glued at load (§1.4) |
| `/car/open/<model>` | race, roof type ≥2 (or 1 with <3 cars) | the open-roof variant of the same container |
| `<path>.pat` | race, when `u32(CAR4+0x1C)==0` | free-standing `Pat0` paint table |

The same roof rule is repeated for the driver-only path, `car_model.c:167`:
```c
char path[96];snprintf(path,sizeof path,"/car/%s/%s",generic[20]>=2 || (generic[20]==1 && cars<3)?"open":"lod",model);
```
(`generic[20]` == `generic[0x14]`.)

What is **inside** `/car/lod` vs `/car/open` at the byte level is not enumerated anywhere in the tree:
the loader treats them identically, so the only verified difference is roof/geometry content —
**UNKNOWN** whether their model-1/2 or camera tables differ (§6).

`/car/menu` is the only one verified exhaustively: `docs/MODELS.md:249-250` — "807 on US v2.00";
`docs/MODELS.md:15` — "All 807 menu, 77 open and 807 LOD CAR4 conversions passed". The 77 vs 807
split is consistent with open models existing only for open-roof cars.

Paint/colour list: `VARIATION` rows of the same model name are collected and sorted by `order`
(`car_model.c:183-189`), and the colour index passed to the decoder is `order-1` (`car_model.c:240`)
— matching `docs/MODELS.md:49` ("VarOrder is one-based").

---

## 4. WHEELS WINGS PARTS

### 4.1 Stock wheels/tires come from inside the car's own CAR4

`car_model.c:88-97`:
```c
for(int pass=0;pass<2;pass++) {
    int piece=1-pass;
    const uint8_t *mesh=data;size_t mesh_size=size;
    size_t off=u32(data+(piece?44+axle*4:32));
    if(!piece && custom){mesh=custom;mesh_size=custom_size;off=0;}
    if(off>=mesh_size)continue;
    gt4_model *part=piece?gt4_model_decode_part(data+off,size-off,0,0):gt4_model_decode_wheel_part(mesh+off,mesh_size-off,0);if(!part)continue;
    float bounds[3]={0};
    if(piece && !gt4_model_tire_bounds(data+off,size-off,bounds)){gt4_model_free(part);continue;}
```
- `piece==1` (tire): `gt4_model_decode_part(...,0,0)` **plus** `gt4_model_tire_bounds`, which re-walks
  every shape's position stream (`model.c:582-591`) to get radius min/max and max |x| before fitting.
- `piece==0` (rim): `gt4_model_decode_wheel_part` — the "WheelContext" mode where an MDLS callback-6
  argument binds the authored static rim `Tex1` into external slot 2 (`model.c:273-279`).
- Order matters: tire is drawn before rim, and both are attached twice (left/right) unless the front
  tread is 0 (single central tire) or the rim holds the whole axle (`car_model.c:118-125`).

Sizing is **not** in the container: it is SpecDB, `car_model.c:30-53` — `GENERIC_CAR.dbt` (24 B) →
`DEFAULT_PARTS.dbt` (208 B) → `CHASSIS.dbt` (wheelbase at `+4`, mm) and `RACINGMODIFY.dbt` (tread at
`+2/+4`, mm) and `FRONTTIRE/REARTIRE.dbt` → `TIRESIZE.dbt` (4 B) → `showroom_tire_size`
(`src/platform/sdl/showroom_math.h:36-42`). Same for ride height (`SUSPENSION.dbt +4/+10`, signed mm,
`car_model.c:33-37`).

The wheel/frame **attachment tags** in the body mesh are §2.4's 1..4; the load splits them into
brake meshes (`car_model.c:61-82`) and leaves body tags intact.

### 4.2 Replacement wheel — `/wheel/menu/<label>` or `/wheel/lod/<label>`

`car_model.c:243-252`:
```c
/* 00454270 +0x20..0x48 constructs wheel/menu/<label>, or with its
   third argument 0 wheel/lod/<label>: the race's model (00390200
   passes 0 unless the car model is the menu one, +0x14). The
   replacement rim uses the same 0045314C attachment as stock. */
size_t wheel_size=0;uint8_t *wheel_data=NULL;
if(wheel && *wheel && strcmp(wheel,"w000")){
    char wheel_path[128];
    if(strchr(wheel,'/') || strchr(wheel,'\\'))goto fail_data;
    snprintf(wheel_path,sizeof wheel_path,"/wheel/%s/%s",cars>0?"lod":"menu",wheel);
    wheel_data=gt4_vol_read(v,wheel_path,&wheel_size);
```
- Label `"w000"` (and empty) = **stock**: nothing is loaded.
- A non-stock label is an **MDLS-only** file (no CAR4 wrapper): the rim pass decodes it with
  `gt4_model_decode_wheel_part` at offset 0 (`mesh=custom; off=0`), while the tire pass still uses the
  car's own tire MDLS. A `/` or `\` in the label aborts the whole load; a missing file aborts too
  (`car_model.c:252` prints `showroom: missing wheel %s`).
- The label itself arrives from outside the car file: `race_view_set_parts` / `race_view_set_film_parts`
  (`race_view.c:516-529`), i.e. the fitted part from the CDTL/garage path — the container never names it.

### 4.3 Replacement wing — `/wing/menu/<label>` or `/wing/lod/<label>`

`car_model.c:239-240` and `:254-267`:
```c
if(data){int installed=wing && *wing && strcmp(wing,"p000");
    a->car=installed?gt4_model_decode_car_wing(data,size,(int)a->colors[a->choice].order-1,1):gt4_model_decode(data,size,(int)a->colors[a->choice].order-1);if(a->car){
...
    if(installed){
        size_t info=u32(data+16);if(info>size || size-info<0x54){free(wheel_data);goto fail_data;}
        size_t geometry=u32(data+info+0x50);
        if(!geometry || geometry>size || size-geometry<0xe0 || strchr(wing,'/') || strchr(wing,'\\')){free(wheel_data);goto fail_data;}
        char wing_path[128];snprintf(wing_path,sizeof wing_path,"/wing/%s/%s",cars>0?"lod":"menu",wing);   /* 00454378 */
        size_t wing_size=0;uint8_t *wing_data=gt4_vol_read(v,wing_path,&wing_size);
        gt4_model *part=wing_data?gt4_model_decode_part(wing_data,wing_size,0,0):NULL;free(wing_data);
```
- Label `"p000"` (and empty) = no wing. A wing **must** decode or the load fails
  (`car_model.c:263` `showroom: cannot decode wing %s`).
- Placement = geometry block `+0xD0` translation and `+0xDC` scale × 0.5 (§1.3), attached as one
  instance.
- Installing a wing also changes **the car's own** decode: `gt4_model_decode_car_wing(...,installed?1:0)`
  → `wing=1`, which makes callback 47 pick authoring branch 1 (`model.c:750-751`, `model.c:308-314`):
  ```c
  /* 00453698 callback 47 -> 00453950 reads installed wing
     flag +0x80, set by 00453488. Select authored support branch. */
  if(r->car && op==6 && u16(r->d+p)==47 && r->d[p+2]>(unsigned)r->wing){
  ```
  So the wing file carries the aerofoil, and the car carries the support geometry for both states.

### 4.4 Other parts that live in the same container

- Steering wheel: callback 8 frames, tag 5+ (§2.4); split once for races, kept in place for the menu.
- Brake calipers/discs: callback 11..14 body triangles, split into `car_model->brake[4]`
  (`model.h:20-22`, `car_model.c:61-82`).
- Ground shadow: model 2 (`car_model.c:269`), reduced to a flat cast for races as model 1
  (`car_model.c:270`, `car_model.h:13-14`).
- Driver rig: either a shared `/character/*.mdl` chosen by CARINFO `+2 type` / `+3 crew`, or an
  **embedded MDLS at CAR4+0x34** (`driver_model.c:37-40`, `car_model.c:229-233`).
- Exhaust flame outlets: CARINFO `+0x38/+0x3C`, 24 B each (§1.2).
- Cameras: CAR4 `+0x80/+0x84`, 32 B each (§1.1).

---

## 5. WHAT A WRITER MUST EMIT

Minimum for `gt4_model_decode_part(bytes,size,color,0)` to return a non-empty model — the fixture in
`tests/ui/test_model.c:12-21` is a complete, passing example of exactly this:
```c
uint8_t d[512]={0};memcpy(d,"MDLS",4);w32(d+16,sizeof d);d[22]=d[24]=d[28]=d[30]=1;
w32(d+56,128);w32(d+60,168);w32(d+68,172);
w32(d+128+24,180);w32(d+168,192);w32(d+172,176);w32(d+176,400);
d[180]=43;d[182]=4;memcpy(d+400,"Tex1",4); /* absent texture is allowed */
d[200]=1;d[202]=1;w32(d+208,32);d[212]=5;
d[225]=0xc0;d[226]=3;d[227]=0x68;wf(d+240,1);wf(d+256,1);
d[264]=0x40;d[265]=0xc0;d[266]=3;d[267]=0x64;
d[292]=0x40;d[293]=0xc0;d[294]=2;d[295]=0x72;d[297]=3;
gt4_model *m=gt4_model_decode(d,sizeof d,0);CHECK(m && m->count==1);
```
(one MDLS model, one shape, one packet descriptor of 5×16 B, a 3-vertex float-position packet, a
3-vertex float-UV packet, a 2-entry restart list with `reset[1]=3` → 1 triangle; plus a `Tex1` block
whose decode failure is tolerated.)

Emission order for a full car:

1. **Layout the file.** Write `"CAR4"` at 0. Fill `+0x18` = MDLS offset, `+0x1C` = Pat0 offset (or 0),
   `+0x10` = CARINFO offset, `+0x20/+0x2C/+0x30` = rim/front-tire/rear-tire MDLS offsets,
   `+0x34` = built-in driver MDLS if any, `+0x80/+0x84` = camera count + 32-byte-entry table.
   Every u32 offset must fit the file, because every reader span-checks.
2. **MDLS header.** `"MDLS"`, `+0x10` size (≥128, exact), `+0x16` models, `+0x18` ns, `+0x1A` nm,
   `+0x1C` nt, `+0x38` model table, `+0x3C` shape table, `+0x40` materials, `+0x44` texture lists →
   `nt` × u32 → `Tex1` blocks (each must be a decodable `Tex1`, or expect `unsupported++`).
3. **Models.** Model 0 = body, 1 = cast/hull, 2 = contact shadow; each descriptor's `+0x18` points at a
   command stream ending in opcode `0`. Only emit opcodes from §1.7 — one unknown byte kills the car.
4. **Shapes.** `+0x04` scale (0 → floats, or a f32 in 0..1 to scale int fixed positions),
   `+0x08` kind, `+0x09` flags, `+0x0A` np, then np×8-byte descriptors whose `+4` is the packet-list
   length **in 16-byte units**. Draw each with opcode 4/5, arg < ns.
5. **Packets.** Use only §2.2 forms. For a GT4-like car: positions as signed fixed point at `0x8000`
   (16- or 8-bit) or f32 at `0xC000`, UVs at `0x8040` (fixed 1/4096) or `0xC040`, normals at `0x8080`
   (int8) / `0x8040` (f32), colours at `0xC080`, and a **restart list at `0xC040` f=2 with ≥2
   entries**. Align each instruction to 4 bytes.
6. **Tag the parts with callbacks** (§2.4): callback `11..14` before each wheel-frame draw (1..4),
   callback `8` before the steering draw (5+), callback `0`'s two branches if you want brake lamps,
   callback `47`'s two branches if the car can take a wing. Without these the mesh loads but every
   triangle is body: no wheels, no steering, no lamp switching, and `add_wheels` finds
   `count==0` for all four slots (`car_model.c:61-62`) and silently attaches nothing.
7. **Wheel sizing stays out of the file.** Emission must be consistent with
   `DEFAULT_PARTS/CHASSIS/RACINGMODIFY/TIRESIZE/SUSPENSION` for the label, or the wheels will sit
   wrong; `car_model.c:30-59` is the contract.
8. **Materials.** One 80-byte row per material, referenced by `(flags>>9)-1`; keep ambient/specular/
   shine finite (they are range-checked).
9. **Paints.** Menu: inline `Pat0` at `+0x1C`, colour-major offset table for `nc` colours × `np`
   patches, records `{u32 dest_offset, u32 len, bytes}` with dest inside the MDLS, `order` 1-based.
   Race: put the model in `/car/lod` (or `/car/open`) with `+0x1C == 0` and ship `<path>.pat`
   alongside; the loader glues it.
10. **Bounds discipline as validation, not as an afterthought.** Any out-of-range table, jump, or
    non-finite float turns the whole car into NULL — there is no partial fallback at the container
    level (only per-shape/per-texture degradation).

Sizes worth targeting, from `docs/MODS.md:67-69`: race-car triangle counts on the disc run ~4.500-5.000
(Ka 4.582, Ford GT 5.072), and textures at 256×256 or less in the `Tex1` palette formats.

`docs/MODS.md:74-78` already prices the writer at "4 a 6 sessões" for this engine's reader only — with
the above, that work is: `Tex1` writer, `MDLS` writer (tables, packet streams, strip restarts,
materials, draw-state commands, callback frames), per-part split, `Pat0` writer. Round trip against
the disc's 807 cars is the natural test, and matches `docs/MODS.md:76-77`.

---

## 6. UNKNOWNS

Marked UNKNOWN because nothing in the tree reads them — not because they are believed empty.

**CAR4 header:** bytes `0x04-0x0F`, `0x14-0x17`, `0x24-0x2B`, `0x38-0x7F`, `0x88+` (incl. any trailing
table behind the camera points). Whether a CAR4 header ever carries the roof type — the loader reads it
from `GENERIC_CAR.dbt +0x14` instead (`car_model.c:195`). The bytes the loader writes into `data[28..31]`
are a *native* convention, not a disc field (`car_model.c:202` requires the disc's `+0x1C` to be 0
before it does so).

**MDLS header:** `+0x1E`(u16), `+0x22`, `+0x26`, `+0x28`, `+0x2C`, `+0x30-0x37` beyond the branch count,
`+0x50`, `+0x54`, `+0x60`, `+0x64`, `+0x6C`+, and anything ≥`+0x6C` except the dimension table. The
exact names/units of `+0x20` (branch-table size) and `+0x24` (host count) are inferred from one reader
each (`model.c:639`, `model.c:624`). The model descriptor's other 36 bytes are unread.

**Shape record:** bytes `0x00-0x03` and `0x0B-0x0F` beyond kind/flags/np. `kind` values above 2 are
stored on the triangle per draw (`model.c:574`) but never interpreted here, so what kinds 3+ mean is
UNKNOWN. What `tween_mode` means for non-human models is unread.

**Textures:** the `Tex1` block's own layout is `docs/TEX1_FORMAT.md`'s subject; the container only
requires one per `nt` entry. Which external slot 2 (rim) vs slot 0 (environment) is bound in the
original is documented (`model.c:276-279`) but the full external-slot set is not enumerated.

**`/car/open`: what is inside `u32(CAR4+0x20/+0x2C/+0x30)`, whether the camera table differs, and
whether open-roof models carry the same 3 models. The engine treats `/car/open` and `/car/lod` as the
same container (only the path differs, `car_model.c:190-198`), and the 77-open-model figure in
`docs/MODELS.md:15` is a conversion count, not a layout comparison.

**`.pat` present in `/car/menu`:** the menu path never loads a sidecar (`car_model.c:202` gates on
`cars>0`) — so whether a `.pat` file exists beside a menu model is unread, and it is never used.

**Wheel/wing label values:** the loader only validates `"w000"`/`"p000"` as "stock/none" and rejects
`/`; which real labels exist in the disc's `/wheel` and `/wing` trees, and whether all of them are
MDLS (vs some being CAR4), is not verified here.

**`gt4_model_tire_bounds`'s unit assumption** is fixed at 1/4096 (`model.c:586`
`r.fixed=1/4096.f;`) while course worlds use 1/500 (`model.c:12`) — correct for car tires, but it means
a tire MDLS authored with the world scale would be misfit. Not observed on the disc.

---
*Read-only survey, 2026-10-15. No `src/`/`include/` file modified, nothing built, no commit.*
