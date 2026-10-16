# Format: the COURSE container (`/crs/<label>`) — as VULCAN 4 actually reads it

Scope: read-only trace of `src/`. Every field below is quoted with `path:line`.
Where nothing in this tree reads a byte, it says **UNKNOWN** — no offsets are
invented. `docs/COURSES.md` (renderer/set notes) and `docs/MODELS.md` (MDLS
conversion) cover their halves; this file is the container + physics/AI view.

## 0. Entry points (who opens what)

- Renderer: `src/platform/sdl/race_view.c:293` `load()` → `snprintf(path,sizeof path,"/crs/%s",course);` (`:294`) then `gt4_vol_read`, and requires
  `GT4_U32` `gt4_course_open(&a->course,a->data,size)` (`:297`).
- Physics/AI: `src/core/physics/race_run.c:219-222`
  `snprintf(path, sizeof path, "/crs/%s", name);` / `snprintf(path, sizeof path, "/crs/%s.ad", name);`
  → reads **two** files: the course and its `.ad` companion.
- The header comment that names the top-level fields: `src/core/course.c:13-17`
  `/* Course header: 0x04 world model set, 0xdc runway (GT4Runway.bt). ... */`

---

## 1. CONTAINER LAYOUT

The course file is a header of 32-bit **offsets** (relative to the file start)
to model sets and data blocks. `gt4_course_open` (`course.c:18`) rejects a file
shorter than `0x100` (`course.c:22` `if (!d || !span(n, 0, 0x100)) return 0;`).

Header fields actually consumed by this engine:

| Off | Type | Target | Evidence |
|---|---|---|---|
| +0x04 | u32 | world model set `MDLS` | `course.c:23` `size_t world = u32(d + 0x04)`; `course.h:24` `world; /* world model set (MDLS) offset */` |
| +0x08 | u32 | `Vls0` vision pass 0 | `race_view.c:313` `memcpy(&at,a->data+8+pass*4,4);` |
| +0x0C | u32 | `Vls0` vision pass 1 | same loop, `pass==1` |
| +0x10 | u32 | `Vls0` vision pass 2 | same loop, `pass==2` |
| +0x14 | u32 | MDLS scenery set (reflections) | `race_view.c:338` `a->scenery=course_set(a->data,size,0x14);` |
| +0x24 | u32 | MDLS reflection set | `race_view.c:236` `a->sheen_set=course_set(data,size,0x24);` |
| +0x34 | u32 | MDLS reflection mask | `race_view.c:234` `a->sheen_mask=course_set(data,size,0x34);` |
| +0x54 | u32 | MDLS sky | `race_view.c:319` `a->sky=course_set(a->data,size,0x54);` |
| +0x58 | u32 | MDLS far set | `race_view.c:320` `a->far=course_set(a->data,size,0x58);` |
| +0x5C | u32 | MDLS env sky | `race_view.c:339` `a->env_sky=course_set(a->data,size,0x5C);` |
| +0x60 | u32 | MDLS mirror sky | `race_view.c:141-142` `... +60 the mirror's sky.` |
| +0x68 | u32 | `Tex1` smoke textures | `race_view.c:333` `memcpy(&t,a->data+0x68,4);` |
| +0x80 | u32 | `Env0` environment | `race_view.c:329` `{uint32_t env;memcpy(&env,a->data+0x80,4);` |
| +0x9C | u32 | `Tex1` particle textures | `race_view.c:335` `memcpy(&t,a->data+0x9c,4);` |
| +0xB0 | u32 | `REP4` TV-camera bank | `course_tvcam.c:145` `uint32_t at = u32(course + 0xb0);` |
| +0xBC | u32 | `GDGT` gadget block | `course_gadget.c:72` `size_t at=u32(d+0xbc),rw=c->runway;` |
| +0xC0 | u32 | `REP4` grid/pregrid camera | `course_pregrid.c:39` `uint32_t at=u32(course+0xc0);` |
| +0xC8 | u32 | MDLS sheen sky | `race_view.c:236` `a->sheen_sky=course_set(data,size,0xC8);` |
| +0xD0 | u32 | `Mot1` start-camera bank | `race_game.c:2075` `unsigned o = g->replaying ? 0xd4 : 0xd0;` |
| +0xD4 | u32 | `Mot1` replay-opening bank | same line |
| +0xDC | u32 | `RNW4` runway | `course.c:23` `... runway = u32(d + 0xdc);`; `course.h:25` |

Every header offset is validated as **in-bounds before use** — e.g. `course.c:24`
`if (!span(n, world, 128) || memcmp(d + world, "MDLS", 4)) return 0;` and the
optional sets check their own span+magic inside `course_set` (`race_view.c:146-148`).

**UNKNOWN (header):** +0x00; +0x18..+0x20; +0x28..+0x30; +0x38..+0x50; +0x64,
+0x6C..+0x7C; +0x84..+0x98; +0xA0..+0xAC (physics-only, see §5); +0xB4..+0xB8;
+0xC4; +0xCC; +0xD8; +0xE0 and beyond. Nothing in `src/` reads them.

---

## 2. WORLD MODEL SET (`MDLS`)

`a->world=gt4_model_decode_world(a->data+a->course.world, size-a->course.world, NULL);`
(`race_view.c:311`) — the `MDLS` at header +0x04 is the single world set the
renderer converts; `if(!a->world || !a->world->count)` is fatal (`race_view.c:312`).
Conversion lives in `src/core/model.c` (`model.c:652` `if(!span(size,base,128) || memcmp(bytes+base,"MDLS",4))return NULL;`)
and is documented in `docs/MODELS.md` §Data. `gd`'s other `MDLS` sets (scenery,
sky, far, reflections, env sky, sheen) are the header table in §1. The world set's
`source_models` count bounds the `Vls0` model indices (`race_view.c:314`
`...a->world->source_models`; `course.c:162` `... !models)return 0;`).

---

## 3. RUNWAY / ROAD + COLLISION (`RNW4`)

`RNW4` at header +0xDC. `gt4_course_open` requires `RNW4` magic and `span(n,runway,0xa0)`
(`course.c:25`). Offsets inside `RNW4` are **relative to the runway** (`course.c:16-17`
`Runway offsets are relative to the runway.`). The physics runner copies the block
and re-bases its pointers with `gt4_runway_relocate` (`runway.c:19-30`).

| Off | Type | Meaning | Evidence |
|---|---|---|---|
| +0x00 | char[4] | `RNW4` | `course.c:25` |
| +0x04 | u32 | self base (relocation) | `runway.c:19` `uint32_t delta = rnw - GT4_U32(rnw + 4);` |
| +0x14 | f32 | **course length, metres** | `course.c:31` `c->length = f32(r + 0x14);` |
| +0x20 | f32[6] | collision box min/max xyz | `collision.c:12` `#define MESH_BOX 0x20` |
| +0x38 | u16 | kd-table entry count | `runway.c:27` `for (uint32_t i = 0; i < GT4_U16(rnw + 0x38); i++) fix(GT4_U32(rnw + 0x58) + i * 8 + 4, delta);` |
| +0x3A | u16 | **start-grid count** | `course.c:32` `c->grid_count = u16(r + 0x3a);` |
| +0x44 | u16 | section 0 gadget count | `course_gadget.c:91` `... :u16(d+rw+0x44);` |
| +0x46 | u16 | **road-vertex count** | `course.c:33` `c->road_vertex_count = u16(r + 0x46);` |
| +0x48 | u16 | **road-triangle count** | `course.c:34` `c->road_tri_count = u16(r + 0x48);` |
| +0x4A | u16 | collision-leaf count | `runway.c:28` `for (uint32_t i = 0; i < GT4_U16(rnw + 0x4a); i++) fix(GT4_U32(rnw + 0x7c) + i * 16 + 8, delta);` |
| +0x4C | u16 | kd-tree depth (levels) | `collision.c:13` `#define MESH_DEPTH 0x4c` |
| +0x54 | u16 | section/variant count | `course_gadget.c:74` `unsigned types=u32(d+at+0x1c),sections=u16(d+rw+0x54);` |
| +0x58 | u32 | → kd table (8-byte records, +4 ptr) | `runway.c:17,27` |
| +0x5C | u32 | → default grid list | `course.c:35` `c->grids = runway + u32(r + 0x5c);`; `init.c:98` |
| +0x74 | u32 | → **road vertices** (16-byte: x,y,z f32 + pad) | `course.c:36`; `collision.c:14` `MESH_VERTS 0x74` |
| +0x78 | u32 | → **road triangles** (12-byte: u16 v[3], u8 surface, u8 kind) | `course.c:37`; `collision.c:15` `MESH_TRIS 0x78` |
| +0x7C | u32 | → collision leaves (16-byte: s16 count, +8 u16 tri list) | `collision.c:16` `MESH_LEAVES 0x7c`; `runway.c:28` |
| +0x80 | u32 | → kd split bytes (axis<<6\|pos/64) | `collision.c:17` `MESH_SPLITS 0x80` |
| +0x84 | u32 | → wall vertices (16-byte) | `walls.c:508` `uint32_t w = GT4_U32(mesh + 0x84) + (uint32_t)(idx * 16);` |
| +0x8C | u32 | → wall index list (u16) | `walls.c:507` `int16_t idx = (int16_t)GT4_U16(GT4_U32(mesh + 0x8c) + s * 2);` |
| +0x94 | u32 | course feature flags (one bit each) | `init.c:886` `uint32_t rw = ... , bits = GT4_U32(rw + 0x94), r = 0;` |
| +0x98 | u32 | flag 2, bit 1 = "wide" | `init.c:887` `int wide = (GT4_U32(rw + 0x98) & 2) != 0;` |
| +0x9C | u32 | → **variant/section table** (32-byte records) | `course_gadget.c:75`; `session.c:31` `return GT4_U32(runway_of(course) + 0x9c) + (uint32_t)variant(course) * 32;` |

**Record sizes** (what a writer must lay out):
- Grid record, **16 bytes**: `course.c:61` `c->data + c->grids + (size_t)i * 16`; x,y,z f32 + heading f32
  (`course.c:62-63` `g.pos[k]=f32(p+k*4); ... g.heading=f32(p+12);`). Heading is radians; the
  engine adds π at use (`init.c:1010` `return GT4_F32(list + slot * 16 + 0xc) + K(0x40490fdau);`).
- Road vertex, **16 bytes** (`course.c:39` `c->road_vertex_count * 16`), XYZ floats. Triangulated
  ground height uses barycentric interpolation in (x,z), highest triangle below a ceiling
  (`course.c:79-86`, `course.c:134-157`).
- Road triangle, **12 bytes** (`course.c:40` `c->road_tri_count * 12`): `course.c:16`
  `(12 bytes: three u16 vertex indices, 2 bytes, u32 flags)`. Indices must be
  `< road_vertex_count` (`course.c:44-46`).

The variant/section record (32 bytes) fields this tree uses:
`+0x0C` u16 instance count (`course_gadget.c:91` `...:u16(d+table+(i-1)*32+12)`),
`+0x10` u32 → gadget type placement list (`course_gadget.c:96`),
`+0x18` u32 → **start-grid list for that variant** (`init.c:1009`
`else list = GT4_U32(GT4_U32(rw + 0x9c) + (uint32_t)set * 32 + 0x18);`),
`+0x1C` u32 → gadget placement list (`course_gadget.c:96`), `+0x14` u32 ptr fixed by relocate
(`runway.c:31-34`). The **default** start grid (variant < 0) is the runway's own
`+0x5C` list (`init.c:97-98`).

**Start grid / variant selection:** `course+0xB0` holds the chosen variant
(`session.c:30` `static int variant(uint32_t course) { return (int32_t)GT4_U32(course + 0xb0); }`),
written by `session.c:1782` `GT4_U32(course + 0xb0) = (uint32_t)set;`. Negative ⇒ the
runway's own grid table.

---

## 4. VISION DATA (`Vls0`)

Three `Vls0` blocks, one per header slot +0x08/+0x0C/+0x10 (`race_view.c:312-316`,
`course.c:159`). Header layout read by `gt4_course_vision_open` (`course.c:159-184`):

| Off | Type | Meaning | Evidence |
|---|---|---|---|
| +0x00 | char[4] | `Vls0` | `course.c:162` `memcmp(d,"Vls0",4)` |
| +0x10 | u16 | grid width nx | `course.c:163` `unsigned nx=u16(d+0x10),nz=u16(d+0x12),count=u32(d+0x30);` |
| +0x12 | u16 | grid height nz | same |
| +0x14 | u32 | → per-cell candidate pointer array (nx*nz u32) | `course.c:163` `grid=u32(d+0x14)` |
| +0x18..+0x2C | f32[6] | origin x,z (+0x18,+0x1C) and reciprocal extents (+0x28,+0x2C) | `course.c:191` `x=p[0]-f32(d+24),z=p[2]-f32(d+28); ... x=x*f32(d+40);z=z*f32(d+44);` |
| +0x30 | u32 | node count | `course.c:163` `count=u32(d+0x30)` |
| +0x34 | u32 | → nodes (16-byte) | `course.c:163` `nodes=u32(d+0x34)` |

Node, **16 bytes** (`course.c:39` `count*16`): `+0x00` u32 → model-index list;
`+0x04,+0x08,+0x0C` f32 node XYZ (`course.c:168` `for(unsigned k=1;k<4;k++)... f32(node+k*4)`).
List format: u16 entry count, then u16 **world-model indices**, each `< models`
(`course.c:169-171`). Per-cell candidate array: null-terminated u32 chain of node
offsets (`course.c:176-181`). Selection truncates toward zero and clamps
(`fesetround(FE_TOWARDZERO)`, `course.c:188`), picks the nearest node in XYZ, first
wins ties (`course.c:196-201`). An **invalid** `Vls0` (present but malformed) is
**fatal** to the renderer: `race_view.c:314-315` returns `NULL` from `load`.

---

## 5. THE AI LINE — where the opponents' driving line comes from (HEADLINE)

**It is NOT in `/crs/<label>`. The opponents' (and the player model's) driving line
is the `ADLN` block of the companion `/crs/<label>.ad` file, relocated into the in-RAM
course object at `COURSE + 0x9C`.**

Chain of evidence:

1. The physics loader reads the companion file:
   `race_run.c:221` `snprintf(path, sizeof path, "/crs/%s.ad", name);`
   and **requires** it: `race_run.c:223` `if (!crs || !adf || an < 24 || !gt4_course_open(&co, crs, n))`.
2. It writes the `.ad` header's first **five** block offsets into the course object:
   `race_run.c:242-245`
   `for (uint32_t k = 0; k < 5; k++) { uint32_t off = le32(adf + 4 + k * 4); w32(COURSE + 0x9c + k * 4, off ? ad + off : 0); }`
   → `COURSE+0x9C, +0xA0, +0xA4, +0xA8, +0xAC`. (`COURSE` is the resident course object,
   `race_run.c:42` `#define COURSE 0x00bcd180u`; `COURSE+0x04=COURSE_DATA`, `race_run.c:43`.)
3. The AI reads the line from exactly that slot with the magic:
   `ai.c:21` `#define LINE_MAGIC 0x4e4c4441u /* "ADLN" */`
   `ai.c:37` `if (GT4_U32(GT4_U32(lines) + 0x9c) == LINE_MAGIC) {`
   Here `lines` is the per-car AI object (`car+0x14`), `GT4_U32(lines)` = the course
   object, `+0x9C` = the `.ad` block — confirmed by the course-length path
   `ai.c:53-55` `return GT4_F32(GT4_U32(GT4_U32(GT4_U32(lines) + 4) + 0xdc) + 0x14);`
   (`lines+4`=COURSE_DATA, `+0xdc`=runway, `+0x14`=length from §3).

So block 0 of the `.ad` is the `ADLN` line set; block 1 (`COURSE+0xA0`) is the AI
settings (`init.c:882` `uint32_t v = GT4_U32(GT4_U32(cond + 0xcbd8u) + 0xa0);`). Blocks
2..4 (`+0xA4/+0xA8/+0xAC`) are **UNKNOWN** here.

Line set structure (`ai.c:20-51`): `lines+8` → table of per-line u32 pointers; each entry
`+0x00` = point count, points begin at entry`+0x40` when `ADLN` is present (wide, 0x40-byte
points) else entry`+4` (narrow, 0x28-byte points) (`ai.c:36-49`).

Line point (`ai.c:26-31`): `+0 kind` (0 straight, else arc), `+1 s8 speed trim %`,
`+2 s8 (-128 skip)`, `+4 x`, `+8 y`, `+0xC z`, `+0x10 s16`, `+0x12 s8 surface`,
`+0x14 distance along the course`, `+0x18 arc radius`, `+0x1C turn angle`,
`+0x24 arc start angle`, `+0x28 byte`.

Line identities: `ai.c:1942-1943` (comment `func_0046BC00 / func_0046BC40: the line to the
left / right of line (1 left, 6 middle, 2 right)`) and pit line = index 4
(`ai.c:97` `l = line_get(lines, 4);`, `ai.c:128` `if (index == 4) return pit_line_at(...)`).

The AI consumes the line every step: position from `line_find` (`ai.c:61`), target
speed from `section_speed` (`ai.c:1866` `OF(0x28) = kind ? section_speed(obj, s) : ...`),
lane changes via `gt4_ai_line_switch` (`ai.c:1887`, `race.c:162`, `laps.c:209`).

**If `.ad` is absent, a race never starts** (`race_run.c:223` fails the whole load).
The renderer-only preview path (`race_view.c`) never reads `.ad` at all.

---

## 6. WHAT A WRITER MUST EMIT

Minimum for `gt4_course_open` to accept (renderer **and** physics use it, `course.c:18-55`):

1. File named `/crs/<label>` in the volume, size ≥ `0x100` (`course.c:22`).
2. `+0x04` → a valid `MDLS` world set, **≥128 bytes from that offset** (`course.c:24`).
3. `+0xDC` → `RNW4`, `≥0xA0` bytes (`course.c:25`).
4. Inside `RNW4`: `length` finite and `≥0` (`course.c:47`); grid table in-bounds
   (`grid_count*16`), road verts in-bounds (`vertex_count*16`), road tris in-bounds
   (`tri_count*12`) (`course.c:38-43`); every road-tri index `< road_vertex_count`
   (`course.c:44-46`); every grid coord and road-vertex coord finite (`course.c:48-51`).
   In `RNW4+0x3A/+0x46/+0x48` set grid/vertex/triangle counts; `+0x5C/+0x74/+0x78` the
   16/16/12-byte tables; `+0x14` the length.

For the **engine/physics** to accept (beyond the renderer):

5. A companion `/crs/<label>.ad`, `≥24` bytes, header word `adf+4+k*4` = block offset for
   `k=0..4` (`race_run.c:242-245`). Block 0 must be the `ADLN` line set (else the AI falls
   back to narrow 0x28-point records — still reads, different stride, `ai.c:36-49`);
   block 1 (`COURSE+0xA0`) the AI settings (`init.c:882`).
6. The `RNW4` block must relocate cleanly: pointers at `0x58,0x5C,0x60,0x64,0x68,0x6C,0x70,
   0x74,0x78,0x7C,0x80,0x84,0x8C,0x90,0x9C` plus the kd/leaf/section sub-tables
   (`runway.c:15-35`).
7. A start-grid variant: default list at `RNW4+0x5C`, or a variant record at
   `RNW4+0x9C + set*32 + 0x18` with `course+0xB0 = set` (`init.c:1009`, `session.c:1782`).

Optional but read (presence is fine, malformity is not): `Vls0` ×3 at +0x08/+0x0C/+0x10
(invalid ⇒ renderer refuses the course, `race_view.c:314`); MDLS sets at +0x14/+0x24/
+0x34/+0x54/+0x58/+0x5C/+0x60/+0xC8; `Tex1` at +0x68/+0x9C; `Env0` at +0x80; `GDGT` at
+0xBC; `REP4` at +0xB0 (TV) and +0xC0 (pregrid); `Mot1` at +0xD0/+0xD4.

---

## 7. UNKNOWNS (nothing in `src/` reads these)

- Header bytes +0x00, +0x18..+0x20, +0x28..+0x30, +0x38..+0x50, +0x64, +0x6C..+0x7C,
  +0x84..+0x98, +0xB4..+0xB8, +0xC4, +0xCC, +0xD8, +0xE0+.
- `RNW4` +0x00..+0x10 (past the magic), +0x18..+0x20, +0x24..+0x38, +0x3C..+0x44, +0x50,
  the pointer targets of +0x60/+0x64/+0x68/+0x6C/+0x70/+0x90, and everything ≥ +0xA0.
- `Vls0` +0x04..+0x10, the two unnamed floats +0x20/+0x24; node list entry semantics past
  the model-index list.
- `.ad` blocks 2..4 (`COURSE+0xA4/+0xA8/+0xAC`); `.ad` header words beyond the 5 offsets.
- `REP4` entry fields beyond eye/target/roll/fov curves (tvcam/pregrid read only those);
  `Mot1` fields beyond the packed-motion/curve/parameter set in `course_motion.c:1-14`.
- Whether opponent cars spawn from the `RNW4` grid (`+0x5C`) or the variant start set
  (`RW+0x9C+0x18`) in every mode — both paths exist (`init.c:97-98` vs `:1007-1009`);
  no single call site in this tree was traced to a definitive per-mode choice.
- `COURSE` header `+0x10` (the `race_run.c:530` comment names it but no read site was
  quoted) — see `race_run.c:530` `COURSE +0x10, resolved by settings +0xa0`.
