# GT4 texture format (Tex1) — as VULCAN 4 decodes it, and the minimum a writer must emit

Scope: the texture container the engine ingests (`Tex1`), its pixel formats,
palettes, swizzle and mips; where car and course textures live; and the smallest
correct thing a future writer must produce. Extends — does not duplicate —
`docs/MODELS.md:229-280` (lamp texture addressing), `docs/TEX1_FORMAT.md` (the
upstream container note), and `docs/vulcan4/WISHLIST.md` G6.23 (enhanced textures,
`WISHLIST.md:783-814`).

Every claim is `path:line` + a quoted line. Nothing here is inferred from the
PS2 spec unless the code shows it.

---

## 1 CONTAINER

`Tex1` is not a packed image format — it is a **snapshot of 4 MB of PS2 video
memory plus the descriptors to read it back**, converted to plain RGBA once at
load (`tex1.h:2-6`: *"a snapshot of PS2 video memory: blocks uploaded in one
pixel format and read back in another"*). Decoder: `tex1_decode`/`tex1_free`.

### 1.1 File header

| Offset | Field | Evidence |
|---|---|---|
| `+0x00` | `"Tex1"` magic, required | `tex1.c:131` `if (size < 0x30 \|\| memcmp(d, "Tex1", 4)) return -1;` |
| `+0x0c` | file size (`u32`) — documented, **not read by the decoder** | `docs/TEX1_FORMAT.md:16` |
| `+0x14` | texture count `ntex` (`u16`) | `tex1.c:132` `unsigned ntex = le16(d + 0x14), ntr = le16(d + 0x16);` |
| `+0x16` | transfer count `ntr` (`u16`) | `tex1.c:132` (same line) |
| `+0x18` | textures offset `ptex` (`u32`) | `tex1.c:133` `uint32_t ptex = le32(d + 0x18), ptr = le32(d + 0x1c);` |
| `+0x1c` | transfers offset `ptr` (`u32`) | `tex1.c:133` (same line) |

Validation is all-or-nothing: `ntex` must be non-zero and both arrays must fit in
the buffer, else `-1` (`tex1.c:134`
`if (!ntex || (uint64_t)ptex + 0x28ull * ntex > size || (uint64_t)ptr + 12ull * ntr > size) return -1;`).

### 1.2 Transfer records — 12 bytes each, at `ptr`

`tex1.c:141-142`:
```
const uint8_t *t = d + ptr + 12 * i;
uint32_t p = le32(t), tbp = le16(t + 4), bw = t[6], psm = t[7], w = le16(t + 8), h = le16(t + 10);
```
- `+0` `p` (`u32`): byte offset of this block's data inside the file.
- `+4` `tbp` (`u16`): destination base pointer, in 256-byte VRAM blocks.
- `+6` `bw` (`u8`): buffer width, **in 64-pixel units** (`tex1.c:60` `tbp in 256-byte blocks, bw (buffer width) in 64-pixel units`).
- `+7` `psm` (`u8`): pixel format of the block **as uploaded**.
- `+8` `w`, `+10` `h` (`u16`): stored dimensions.

Each transfer is copied into a 4 MB scratch VRAM that wraps at 4 MB
(`tex1.c:17` `#define VRAM_SIZE (4u * 1024 * 1024)`; `tex1.c:65`
`... & (VRAM_SIZE - 1);   /* GS memory wraps */`). Transfers are applied in
order (`tex1.c:140-147`).

### 1.3 Texture descriptors — 0x28 bytes each, at `ptex`

`tex1.c:150-151`: `const uint8_t *t = d + ptex + 0x28 * i;` /
`uint64_t tex0 = le64(t), clamp = le64(t + 0x20);`. The block is GS registers
`TEX0`(0x00) `TEX1`(0x08) `MIPTBP1`(0x10) `MIPTBP2`(0x18) `CLAMP`(0x20)
(`docs/TEX1_FORMAT.md:19`).

`TEX0` decoded (`tex1.c:152-154`, `:169-170`, `:176`):
```
uint32_t tbp = tex0 & 0x3fff, tbw = (tex0 >> 14) & 0x3f, psm = (tex0 >> 20) & 0x3f;
uint32_t tw = 1u << ((tex0 >> 26) & 0xf), th = 1u << ((tex0 >> 30) & 0xf);
uint32_t cbp = (tex0 >> 37) & 0x3fff, csa = (tex0 >> 56) & 0x1f;
```
- bits 0-13 `TBP` read base (256-byte blocks); bits 14-19 `TBW` (64-px units);
  bits 20-25 `PSM` **read** format; bits 26-29 `TW`, 30-33 `TH` (log2, so the
  stored texture is always a power of two); bit 34 `TCC` → `color_component`
  (`tex1.c:169`); bits 35-36 `TFX` → `texture_function` (`tex1.c:170`);
  bits 37-50 `CBP` CLUT base; bits 51-54 `CPSM` CLUT format (`tex1.c:176`
  `unsigned cpsm = (unsigned)((tex0 >> 51) & 15);`); bit 55 `CSM` (`tex1.c:177`);
  bits 56-60 `CSA` CLUT offset (`tex1.c:154`, `tex1.c:198`).

`TEX1` (`tex1.c:171`, `:227`): bit 5 `MMAG`, bits 6-8 `MMIN`
(`tex1.h:21` `magnify,minify; /* ... 0 nearest, 1 bilinear */`), bits 2-4 `MXL`
= authored mip level count (`tex1.c:227` `unsigned levels=(unsigned)((le64(t+8)>>2)&7);`).

`CLAMP` (`tex1.c:157-168`): bits 0-1 `WMS`, bits 2-3 `WMT`; `MINU` bits 4-13,
`MAXU` bits 14-23, `MINV` 24-33, `MAXV` 34-43. Mode `2` = `REGION_CLAMP`, and in
that mode `MAXU+1 × MAXV+1` **is the image's own size inside the power-of-two
texture** — the decoder crops to it (`tex1.c:157-161`; `docs/TEX1_FORMAT.md:31-32`).

Output per texture (`tex1.h:14-25`): `width/height` = cropped size; `tex_w/tex_h`
= the stored power-of-two size; `rgba` = `width*height*4` alpha 0..255;
`native_alpha` = the original GS coefficient (128 = unity); plus
wrap/min/max, magnify/minify, `mip_count` and `mip_rgba[6]`.

---

## 2 FORMATS + CLUT + SWIZZLE

### 2.1 PSM values the decoder accepts

`tex1.c:19`:
```
enum { PSMCT32 = 0x00, PSMCT24 = 0x01, PSMT8 = 0x13, PSMT4 = 0x14 };
```

| PSM | Name | Where handled |
|---|---|---|
| `0x00` | `PSMCT32` | upload `tex1.c:92-96`, read `tex1.c:207-211` |
| `0x01` | `PSMCT24` | read only, `tex1.c:212-219` |
| `0x13` | `PSMT8` (256-entry palette index) | upload `tex1.c:97-101`, read `tex1.c:202-203` |
| `0x14` | `PSMT4` (16-entry palette index, 2 px/byte) | upload `tex1.c:102-110`, read `tex1.c:204-206` |
| `2` | `PSMCT16`-style **transfer only**, special case | `tex1.c:143-145` |

Any other transfer PSM or read PSM is a hard failure (`tex1.c:111-113`,
`tex1.c:220-221` → `goto fail`). Frame sizes above 1024 are rejected
(`tex1.c:162-163` `if (tw > 1024 || th > 1024) goto fail;`).

`PSMCT24` is read straight through the 32-bit layout; its alpha byte is ignored
and forced to the GS `TEXA.TA0` value `0x80` (`tex1.c:212-219`, comment:
*"the 32-bit layout, its alpha byte not read: the GS gives TEXA.TA0, taken as
0x80 (the course's spark image, header +0x9C, drawn with TCC 0 anyway)"*).

The `psm == 2 && w == 8 && h == 2` transfer is the standalone 16-entry RGB5A1
frame palette used by 4-bit frame-fill images such as `4R_A`; the decoder skips
it as a VRAM upload (`tex1.c:145`) and reads it directly as the palette
(`tex1.c:177-193`). Arbitrary 16-bit textures remain unsupported
(`docs/TEX1_FORMAT.md:41-49`).

### 2.2 Palette / CLUT handling

Two paths (`tex1.c:176-198`):

- **Standalone 16-entry RGB5A1** for the small-frame-palette case
  (`cpsm == 2 && psm == PSMT4 && csa == 0 && !((tex0>>55)&1)`): the 16 entries
  are expanded directly from 16-bit `RGB5A1` — `pal[k][c] = (v<<3)|(v>>2)`,
  alpha `0/255`, `palalpha 0/128` (`tex1.c:186-193`, `:192`
  `pal[k][3] = rgb & 0x8000 ? 255 : 0;palalpha[k]=rgb&0x8000?128:0;`).
- **32-bit CLUT read out of VRAM** otherwise, requiring `cpsm == 0`
  (`tex1.c:194` `} else if (cpsm != 0) goto fail;`):
  - `PSMT8` reads all 256 entries: `tex1.c:195-196`
    `for (unsigned k = 0; k < 256; k++) clut(vram, cbp, k, pal[k], &palalpha[k]);`
  - `PSMT4` reads the 16 entries at `CSA*16`: `tex1.c:197-198`
    `for (unsigned k = 0; k < 16; k++) clut(vram, cbp, csa * 16 + k, pal[k], &palalpha[k]);`

The CLUT reader (`tex1.c:120-127`) is where the storage-mode detail lives:
```
unsigned j = (i & 0xe7) | ((i & 0x08) << 1) | ((i & 0x10) >> 1);
const uint8_t *p = vram + addr32(cbp, 1, j % 16, j / 16);
...
out[3] = (uint8_t)(p[3] >= 0x80 ? 255 : p[3] * 2);
```
i.e. **CSM1 default: index bits 3 and 4 are swapped** (comment `tex1.c:117-119`
*"bits 3 and 4 of the index are swapped"*), the CLUT is read through the
`PSMCT32` layout at `cbp` with `bw=1`, and **PS2 alpha is 0x80 = opaque, doubled
and clamped to 0..255**.

### 2.3 Swizzle rules

The on-disc/VRAM layout is the GS page → block → column order, per format. The
decoder carries the real tables verbatim, `tex1.c:21-58`:
`blk32`/`col32` (`PSMCT32`), `col8` (with `blk32`) for `PSMT8`,
`blk4`/`col4` (with `col4`'s second-half `+256` rule) for `PSMT4`
(`tex1.c:49` `/* PSMT4 in-block nibble order for rows 0-7; rows 8-15 are the same plus 256. */`).

Address functions (`tex1.c:61-83`):
- `PSMCT32` `tex1.c:63-65`: `page = (y / 32) * bw + x / 64;` then
  `blk = tbp + page * 32 + blk32[(y % 32) / 8][(x % 64) / 8];` →
  `blk * 256 + col32[y % 8][x % 8] * 4` (4 bytes/pixel).
- `PSMT8` `tex1.c:70-72`: `page = (y / 64) * (bw / 2) + x / 128;` 1 byte/pixel.
- `PSMT4` `tex1.c:78-82`: `page = (y / 128) * (bw / 2) + x / 128;`, nibble =
  `col4[y % 8][x % 32] + ((y % 16) >= 8 ? 256u : 0u)`, address `blk*256 + nib/2`,
  shift `(nib & 1) * 4`.

**Key consequence for a writer.** `upload()` writes source pixel `(x,y)` at the
address its *upload format* computes, taking the source linearly:
`tex1.c:96` `memcpy(vram + addr32(tbp, bw, x, y), src + ((size_t)y * w + x) * 4, 4);`
If the transfer's `psm` and the texture's `PSM` are **both `PSMCT32` with the
same `tbp`/`bw`**, upload and readback use the *same* address function, so the
map is the identity: the transfer payload is effectively **row-major 32-bit
pixels, unswizzled**. The swizzle only bites when the two formats differ — the
normal disc case of data "uploaded as `PSMCT32` (or in their own format) and read
as `PSMT8`/`PSMT4`" (`tex1.c:8-9`).

### 2.4 Mip levels

Authored levels only, never generated (`tex1.c:224-248`). `MXL` selects 0..6
(`tex1.c:227-229`); `MIPTBP1` at `t+16` holds levels 1-3, `MIPTBP2` at `t+24`
holds 4-6, each as three 20-bit `TBP14/TBW6` pairs (`tex1.c:231-233`). Each level
is `tw>>level × th>>level`, floor 1 (`tex1.c:234-236`), read with the same format
and palette as level 0 and stored in `mip_rgba[level-1]` (`tex1.c:236-247`).
6 is the hard cap (`tex1.c:228` `if(levels>6)goto fail;`). Everything is
power-of-two storage; region addressing is applied per level
(`docs/TEX1_FORMAT.md:57-65`).

### 2.5 Renderer upload

Everything collapses to host RGBA8 at upload. `render3d.c:233-250`:
```
static GLuint upload(int w, int h, const uint8_t *rgba, const uint8_t *native)
...
memcpy(px + i * 4, rgba + i * 4, 3);
px[i * 4 + 3] = native ? native[i] : rgba[i * 4 + 3];
...
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
```
i.e. **RGB from `rgba`, alpha from `native_alpha` (the GS 0..255/128 coefficient)
when present**. Per-model textures are uploaded one-to-one from `m->textures[i]`
(`render3d.c:350-357`), sprites via `r3d_sprite_texture_create` →
`upload(image->rgba, image->native_alpha)` (`render3d.c:1185`). Filtering comes
from `magnify/minify` (`render3d.c:1188-1189`). No PS2 format reaches the GPU —
`docs/vulcan4/WISHLIST.md:805-807` names this as the substitution point for
G6.23: *"every decoded texture passes through `tex1_decode` into `tex1_image`
… and the backend uploads plain RGBA (`render3d.c:233-250`) — so the
substitution point needs no renderer change at all."*

---

## 3 WHERE CAR TEXTURES LIVE

**Embedded inside the model file.** A car is `CAR4` + one or more `MDLS` bodies;
each `MDLS` carries its own Tex1 texture banks, addressed by a texture list.

Model file entry (`model.c:650-652`):
```
int car=!memcmp(bytes,"CAR4",4);
if(car){base=u32(bytes+24);patch=u32(bytes+28);}
if(!span(size,base,128) || memcmp(bytes+base,"MDLS",4))return NULL;
```
- `+0x18` main-MDLS offset, `+0x1c` `Pat0` patch-table offset (matches
  `docs/MODELS.md:45` *"CAR4's main model is at header offset 0x18; Pat0 at 0x1c"*).
- `MDLS` header fields (`model.c:664`): `ns` shapes `u16(d+24)`, `nm` `u16(d+26)`,
  **`nt` textures `u16(d+28)`**.

The texture bank loop (`model.c:668-676`):
```
size_t lists=u32(d+68);...size_t list=u32(d+lists);
...
for(unsigned i=0;i<r.nt;i++) {
    r.texbase[i]=-1;size_t at=u32(d+list+i*4);...
    tex1_image *images=NULL;int count=tex1_decode(d+at,n-at,&images);
    if(count>0){...r.texbase[i]=m->ntextures;memcpy(m->textures+m->ntextures,images,...);m->ntextures+=count;...}
    else m->unsupported++;
}
```
So: **`MDLS+0x44` → a `u32` → an array of `nt` `u32` offsets, each pointing at a
`Tex1` blob inside the same `MDLS`**. Every bank is a full Tex1 file (its own
header, transfers and descriptors). Shapes then index textures as
`texbase[table] + tex - 1` (`model.c:554-556`). A model's textures are freed with
it (`model.c:692` `tex1_free(m->textures,m->ntextures)`).

Where the car file is read from:
- `/car/<open|lod>/<model>` — `car_model.c:198`
  `snprintf(path,sizeof path,"/car/%s/%s",directory,model);`, with the `Pat0`
  sibling `"<path>.pat"` read separately (`car_model.c:200-204`); showroom labels
  map through SpecDB to `/car/menu/<ModelCode>` (`docs/MODELS.md:41-46`).
- Race car body via `gt4_model_decode` / wing part (`car_model.c:240`, `:262`).

Car textures that are **not** in the car file: the arcade environment map. It
lives in a `gpb2` package as a Tex1 named `image/arcade/car/env0.png`, read from
`/projects/arcade/US/CarRoot.gpb` (`showroom.c:102-104`, `race_view.c:356-358`;
package format `gpb.c:12-40`, `gpb.c:11` `if (size < 0x10 || memcmp(pkg, "gpb2", 4)) return 0;`).
`docs/MODELS.md:119-121` ties it to the CarFace reflection pass.

---

## 4 WHERE COURSE TEXTURES LIVE

Also **embedded**, in two places: inside each course `MDLS` model set, and as two
standalone `Tex1` blobs pointed to by the course file header. The course file
itself is read from `/crs/<name>` (`race_view.c:294-295`
`snprintf(path,sizeof path,"/crs/%s",course); ... a->data=gt4_vol_read(v,path,&size);`).

**A) Inside model sets.** Every course model set is a `MDLS` reached by a header
offset and decoded with `gt4_model_decode_world`, which runs the same
Tex1-bank loop as a car (`race_view.c:144-148`):
```
static gt4_model *course_set(const uint8_t *data,size_t size,int field)
{
    size_t at=...data[field...];
    if(!at || at>size || size-at<128 || memcmp(data+at,"MDLS",4))return NULL;
    return gt4_model_decode_world(data+at,size-at,NULL);
}
```
So sky/far/scenery/env-sky/world/sheen all get their textures from the Tex1 banks
inside their own `MDLS` (`model.c:668-676`). The sets and their header fields
(giving the MDLS offset) are: world `a->course.world`, environment `+0x14`,
reflection set `+0x24`, reflection mask `+0x34`, sky `+0x54`, far `+0x58`,
env-sky `+0x5C`, sheen sky `+0xC8` (`race_view.c:319-320`, `:338-339`, `:234-236`,
`:308`). Offsets are catalogue-matched in
`docs/vulcan4/research/add-maps-loadpath.md:63-64`, `:94-101`.

**B) Two standalone Tex1 blobs, addressed directly off the course header**
(`race_view.c:331-336`):
```
/* the smoke's Tex1 (course header +0x68, the world's +0x31D0 +0x34): four frames side by side */
uint32_t t;memcpy(&t,a->data+0x68,4);
if(t && t<size && size-t>=16 && !memcmp(a->data+t,"Tex1",4)){int n=tex1_decode(a->data+t,size-t,&a->smoke);...}
/* the particles' Tex1 (header +0x9C, the world's D_00621B44) */
memcpy(&t,a->data+0x9c,4);
if(t && t<size && size-t>=16 && !memcmp(a->data+t,"Tex1",4)){int n=tex1_decode(a->data+t,size-t,&a->bits);...}
```
`+0x68` = smoke (four frames side by side), `+0x9C` = particles. Same offsets as
`docs/MODS.md:89-90` (*"os Tex1 da fumaça (+0x68) e das partículas (+0x9C)"*)
and `add-maps-loadpath.md:62-63`.

`PSMCT24` appears in course data (`tex1.c:212-219` comment names "the course's
spark image, header +0x9C, drawn with TCC 0"). Course particle/smoke images are
plain `Tex1` blobs; the course's env map, if any, is a `MDLS` bank like the rest.

---

## 5 WHAT A WRITER MUST EMIT

**Ingest format: the engine does not take raw RGBA or PS2-swizzled data directly;
it takes a `Tex1` container.** The decoder turns it into host RGBA + native alpha
(§2.5). So a writer produces a Tex1 and nothing else.

### 5.1 The smallest correct Tex1 (recommended)

One transfer, one texture, `PSMCT32`, no CLUT, no mips. Concretely:

1. **Header** (`tex1.c:131-133`): bytes `0..3` = `"Tex1"`; total size ≥ `0x30`;
   `u16` at `+0x14` = `1` (`ntex`); `u16` at `+0x16` = `1` (`ntr`); `u32` at
   `+0x18` = offset of the single texture record; `u32` at `+0x1c` = offset of
   the single transfer record.
2. **Transfer record (12 bytes)** (`tex1.c:141-142`): `u32 p` = offset of the
   pixel block; `u16 tbp` = a chosen VRAM base in 256-byte blocks; `u8 bw` =
   buffer width in 64-px units, `bw = max(1, W/64)`; `u8 psm = 0x00` (`PSMCT32`);
   `u16 w = W`, `u16 h = H`.
3. **Pixel block**: `W*H*4` bytes, **row-major, byte order R,G,B,A**, PS2 alpha
   (`tex1.c:96` `memcpy(vram + addr32(tbp, bw, x, y), src + ((size_t)y * w + x) * 4, 4);`
   — because upload and readback use the same `addr32`, the layout is the
   identity, §2.3). Emit **`A = 0x80` for opaque**; anything ≥ `0x80` decodes to
   255, below is doubled (`tex1.c:211` `o[3] = (uint8_t)(p[3] >= 0x80 ? 255 : p[3] * 2);`).
   Emit `0x00` for fully transparent.
4. **Texture record (0x28 bytes)** at `ptex`, first 8 bytes = `TEX0`
   (`tex1.c:151-154`): `TBP=tbp`, `TBW=bw`, `PSM=0x00`, `TW=log2(W)`, `TH=log2(H)`;
   `CPSM=0`, `CSM=0`, `CBP=0`, `CSA=0` (no palette, so the CLUT branches are
   never taken — `tex1.c:194` requires `cpsm==0`). Next 8 bytes = `TEX1`:
   `MXL=0` (no mips, `tex1.c:227`), set `MMAG/MMIN` for filtering
   (`tex1.c:171`). Bytes `+0x20` = `CLAMP`: set `WMS`/`WMT` wrap modes
   (`tex1.c:166`). If `W`/`H` are not powers of two, set `REGION_CLAMP` (mode `2`)
   and `MAXU=W-1`, `MAXV=H-1` so the decoder crops to the true size
   (`tex1.c:157-161`); the stored `TW`/`TH` stay the next power of two ≥ `W`/`H`.

The engine then ingests exactly the image you wrote, **unswizzled and
unpalettised** — no CLUT, no PS2 swizzle, no mip chain required. `PSMCT32` costs
4 bytes/pixel on disc; that is the price of the two-format-free path.

### 5.2 If size matters (still correct): `PSMT8` + `CLUT`

Swap the texture `PSM` to `0x13` and add a second transfer that deposits the
palette, then a `CLUT` (`CBP`, `CPSM=0`, `CSM=0`, `CSA=0`). You must also emit
the index bitmap **in `PSMT8` layout** (`addr8`, `tex1.c:68-73`) if the transfer
`psm` differs from the read `PSM` — that is the swizzle case. Palette must be
32-bit, storage mode CSM1 (**index bits 3 and 4 swapped**) with alpha `≤ 0x80`
(`tex1.c:120-127`). `PSMT4` is the same with 16 entries at `CSA*16` and the
`col4` nibble order (`tex1.c:197-198`, `:76-83`). More moving parts, 1 or 0.5
bytes/pixel instead of 4.

### 5.3 Where a replacement texture is substituted (writer target for G6.23)

The natural hook is per decoded `tex1_image`, because that is the only thing the
renderer ever sees (`render3d.c:350-357`). `WISHLIST.md:801-804` records the
community pack as `.ktx` named `<texture hash>-<clut hash>-r<register word>-<gs offset>`
with the `r…` segment being the packed `TEX0` word — so a future writer/loader
must reproduce the GS texture and CLUT hashes over the same source bytes; it does
not need to re-emit a Tex1 container for a substitution. A *Tex1 writer* (an
importer authoring new cars/courses) instead re-emits §5.1/§5.2 bytes into the
`MDLS` bank the disc expects.

### 5.4 Already-noted cost/limits

`docs/MODS.md:74-78` scopes the `CAR4` writer at "4 a 6 sessões" including
*"`Tex1` ao contrário do leitor"* — writing Tex1 is explicitly the mirror of the
reader, and a real-PS2-accepted CAR4 is out of scope. That is the honest bound.

---

## 6 UNKNOWNS

- **`+0x04..+0x0b` and `+0x1c..+0x1f` of the file header** beyond the size field
  at `+0x0c` are never read by `tex1_decode`; a writer may leave them zero but we
  have not confirmed what the original tool writes there. (`docs/TEX1_FORMAT.md:16`
  documents only `+0x0c`.)
- **`bw`'s exact unit for `PSMT8`/`PSMT4` transfers** is taken as 64-px units
  (`tex1.c:60`); the code uses `bw/2` for those page formulas
  (`tex1.c:70`, `:78`) but no fixture in-tree exercises a non-trivial `bw` for a
  4/8-bit *transfer* — car/course data reaches those PSMs by upload-as-`PSMCT32`
  + read-as-`PSMTx`, which sidesteps it.
- **`PSM` bitmap beyond the four accepted values** — `0x13`/`0x14` are the only
  16/8-bit read formats implemented; `PSMCT16` (`0x02`) exists only as the 8×2
  frame palette special case (`tex1.c:145`). Whether any car/course texture uses
  `PSMCT16`/`PSMT8H`-style formats is not measured here.
- **`TFX`/`TCC` renderer semantics** are stored (`tex1.h:19`) but whether every
  combination is honoured downstream is out of scope for this note.
- **The `MDLS+0x44` texture-list field's relation to the `ns` shape table** — we
  read `lists=u32(d+68)` then `list=u32(d+lists)` (`model.c:668`); the indirection
  is confirmed by the code path but its authored meaning (a shape→bank table?) is
  not documented here.
- No GT4 asset data was read to write this; every constant comes from the source
  and the in-tree docs. A writer has not been built or round-trip tested — this
  file specifies the target, not a verified writer.
