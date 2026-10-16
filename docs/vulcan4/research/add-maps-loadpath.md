# ADD A MAP — the course load path, traced, and the override seam

Research note for `WISHLIST.md` **G6.33**. READ-ONLY trace of the current tree
(branch `main`, HEAD `7ad1f1f`). Every claim below is a `path:line` with the
quoted line. Anything not measured is in **UNKNOWNS**, not guessed.

Format credit: the course/runway layouts are cross-checked, not re-derived —
`src/core/course.h:5`:
> `* road surface triangles. Layouts cross-checked with Nenkai's MIT PDTools`
and `src/core/course.h:6`:
> `* (CourseDataFileGT4, RunwayData) and GT4Runway.bt; see docs/COURSES.md.`
Upstream's own credit is `UPSTREAM-README.md:130` (Nenkai's PDTools, MIT).

---

## (a) WHERE a course's data comes from — container, file, offsets

**The container is the disc's `GT4.VOL`**, read directly (no IOP/CDVD).
`src/core/vol.h:14`:
> `/* path: a disc image (.iso) or a GT4.VOL file. NULL on error; *err says why. */`

The volume is located inside the ISO 9660 root by name —
`src/core/vol.c:88`:
> `    return find_in_iso(v->f, movies, movies ? "GT4L1.VOL" : "GT4.VOL", &v->base, &size);`

The format is `RoFS`, keyed — `src/core/vol.c:15`:
> `#define RoFS_MAGIC    0xACB990ADu     /* bitwise NOT of "RoFS" */`

Files are fetched by path through one API — `src/core/vol.h:26`:
> `uint8_t *gt4_vol_read(gt4_vol *v, const char *path, size_t *size);`

**Two files per course, both under `/crs/`.** The display/preview path builds —
`src/platform/sdl/race_view.c:293`:
> `    char path[96];size_t size=0;snprintf(path,sizeof path,"/crs/%s",course);`

The race path builds two — `src/core/physics/race_run.c:219`:
> `    snprintf(path, sizeof path, "/crs/%s", name);`
`src/core/physics/race_run.c:221`:
> `    snprintf(path, sizeof path, "/crs/%s.ad", name);`

So the course is `<label>` and `<label>.ad`. `race_run.h:16`:
> `    char course[32];      /* a /crs name, e.g. "tsukuba" */`

**The course file's own header** (`src/core/course.c:13`):
> `/* Course header: 0x04 world model set, 0xdc runway (GT4Runway.bt). Runway`

Read at `src/core/course.c:23`:
> `    size_t world = u32(d + 0x04), runway = u32(d + 0xdc);`

Runway (`RNW4`) fields — `src/core/course.c:31-37`:
> `    c->length = f32(r + 0x14);`
> `    c->grid_count = u16(r + 0x3a);`
> `    c->road_vertex_count = u16(r + 0x46);`
> `    c->road_tri_count = u16(r + 0x48);`
> `    c->grids = runway + u32(r + 0x5c);`
> `    c->road_vertices = runway + u32(r + 0x74);`
> `    c->road_tris = runway + u32(r + 0x78);`

Other blocks addressed off the same file header, each validated by its own
magic: `GDGT` at `+0xbc` (`course_gadget.c:72`), `REP4` at `+0xc0`
(`course_pregrid.c:39`), `Mot1` at `+0xD0`/`+0xD4` (`race_game.c:2073`),
`Env0` at `+0x80` (`race_view.c:329`), `Tex1` at `+0x68` and `+0x9c`
(`race_view.c:332`,`:335`), model sets at `+0x54,+0x58,+0x14,+0x5C`
(`race_view.c:319-339`), vision `Vls0` at `+8+pass*4` (`race_view.c:313`).

**The `.ad` file's blocks are relocated into guest memory** at `COURSE+0x9c`
— `src/core/physics/race_run.c:242-244`:
> `    for (uint32_t k = 0; k < 5; k++) {`
> `        uint32_t off = le32(adf + 4 + k * 4);`
> `        w32(COURSE + 0x9c + k * 4, off ? ad + off : 0);`

Slot 0 of those five is the AI line (see (d)).

---

## (b) WHAT the engine consumes at runtime

The validated view of the course file — `src/core/course.h:14-23`:
> `typedef struct {`
> `    const uint8_t *data;       /* whole course file, owned by the caller */`
> `    size_t         size;`
> `    size_t         world;      /* world model set (MDLS) offset */`
> `    size_t         runway;     /* runway (RNW4) offset */`
> `    float          length;     /* course length, metres */`
> `    int            grid_count;`
> `    int            road_vertex_count, road_tri_count;`
> `    size_t         grids, road_vertices, road_tris;   /* absolute offsets */`
> `} gt4_course;`

Consumed at load, in `race_view.c`'s `load()`:

| Piece | Site | Line |
|---|---|---|
| World model set (`MDLS`) | `race_view.c:308` | `    a->world=gt4_model_decode_world(a->data+a->course.world,size-a->course.world,NULL);` |
| Vision / LOD selection (`Vls0`) | `race_view.c:312-316`, `course.c:159` | `if(at && (at>=size \|\| !gt4_course_vision_open(a->vision+pass,a->data+at,size-at,a->world->source_models)))` |
| Course gadgets (`GDGT` + its `MDLS`) | `race_view.c:318`, `course_gadget.c:69-124` | `    if(photo!=1)a->gadgets=gt4_course_gadgets_open(&a->course);` |
| Road surface index | `race_view.c:321` | `    a->surface=gt4_course_surface_create(&a->course);` |
| Sky / far / scenery / env-sky sets | `race_view.c:319,320,338,339` | `    a->sky=course_set(a->data,size,0x54);` |
| Lighting, fog, sheen, env colour, shadow | `race_view.c:322-326` | `    course_lights(a->data,size,&a->lights,a->light_fade);` |
| Wheel exposure (`Env0 +0xEA0`) | `race_view.c:329-330` | `if(env && env<size && size-env>=0xEA4 && !memcmp(a->data+env,"Env0",4))memcpy(&a->wheel_exposure,a->data+env+0xEA0,4);` |
| Smoke + particle textures (`Tex1`) | `race_view.c:332-336` | `if(t && t<size && size-t>=16 && !memcmp(a->data+t,"Tex1",4)){int n=tex1_decode(a->data+t,size-t,&a->smoke);` |

Start positions and heading come from the runway's grid table —
`src/core/course.c:57-64` reads 16-byte XYZ+heading records at `c->grids`.

Race-start grid heading is re-read from guest memory —
`src/core/physics/init.c:1006`:
> `    uint32_t rw = GT4_U32(GT4_U32(course + 4) + 0xdc), list;   /* func_00396800 */`

**Pregrid (start/photo camera bank)**: `REP4` at course `+0xC0` —
`src/core/course_pregrid.c:39`:
> `    uint32_t at=u32(course+0xc0);if(!at || at>size || size-at<0x28 || memcmp(course+at,"REP4",4))return NULL;`
opened once from the loaded course — `src/app/race_game.c:3144-3145`:
> `    if(!g->pregrid_tried){const gt4_course *c=race_view_course(g->view);g->pregrid_tried=1;`
> `        if(c)g->pregrid=gt4_pregrid_open(c->data,c->size);`

**Cameras**: the race/replay camera motions are `Mot1` banks off the course
header — `src/app/race_game.c:2071`:
> `        /* the course header's Mot1 banks: +0xD0 the start's, +0xD4 the`
`src/app/race_game.c:2077`:
> `        if (at && at < course->size) g->start_cam = gt4_motion_bank_open(course->data + at, course->size - at);`
The motion bank format is in `src/core/course_motion.c:1-15`.

So: **the entire course is one file plus a sidecar, every block
magic-tagged and bounds-checked before use.** No table, no manifest.

---

## (c) Collision / surface / off-track — data or code?

**Both, split cleanly: the surface and the walls are DATA, the
interpretation is CODE.**

The runway block is copied into guest memory and its internal offsets are
turned into absolute addresses — `src/core/physics/race_run.c:238-240`:
> `    memcpy(gt4_ptr(rw), crs + co.runway, size);`
> `    gt4_runway_relocate(rw);`
> `    w32(COURSE_DATA + 0xdc, rw);`
(`gt4_runway_relocate` = `src/core/physics/runway.c:14-34`, whose comment
`:1-3` says it: *"road and wall collision, the kd tree, the course length"*.)

The collision mesh is that same block reached from the car —
`src/core/physics/collision.c:8`:
> `/* Collision mesh of a course (the object at [[car+8]+4]+0xDC). */`
and the triangle record carries the surface in the data —
`src/core/physics/collision.c:12`:
> `#define MESH_TRIS    0x78   /* 12 bytes each: u16 v[3], u8 surface, u8 kind */`

**Data → surface byte.** **Code → what the byte means.** Two code layers:

1. A **kind-dependent remap** of some faces — `src/core/physics/lines.c:160-166`:
> `static uint32_t surface_of(const gt4_ray_hit *h)`
> `{`
> `    static const uint8_t map[13] = { 0x14, 0x15, 2, 3, 4, 5, 6, 0x14, 8, 0x16, 0x15, 0x15, 0x15 };`
> `    uint32_t s = h->surface & 0xff;`
> `    if (!(h->kind & 2)) return s;`
> `    return s < 13 ? map[s] : s;`

2. A **grip lookup**, mode-gated — `src/core/physics/body.c:38-46`:
> `float gt4_surface_factor(uint32_t car, uint8_t surface)`
> `{`
> `    int8_t mode = (int8_t)GT4_U8(car + CAR_56A);`
> `    uint32_t base, table;`
> `    if (mode != 1 && mode != 2 && !GT4_U8(car + CAR_56B)) return K(0x3f800000u);`
> `    base = car + 0x14;`
> `    table = GT4_U32(GT4_U32(base + 0xe4) + GT4_U8(GT4_U32(base + 0xe8) + 0x544) * 4 + 0x10024);`
> `    return GT4_F32(table + 0xa8 + GT4_U8(GT4_U32(car + CAR_PARAMS) + PARAMS_TYRE_INDEX + surface) * 4);`

Consumed where grip is applied — `src/core/physics/ai.c:276`:
> `            g = g * gt4_surface_factor(car, (uint8_t)surface);`
and `src/core/physics/body.c:123`:
> `        mu = mu * gt4_surface_factor(car, surface);`

**The ground query itself** (height, pitch, roll, surface) is
`src/core/physics/lines.c:170-203` (`ground_at`), a vertical ray against the
kd tree (`collision.c:1-3`).

**Walls are DATA, walked by CODE** — `src/core/physics/course_objects.c:104`:
> `int gt4_course_object_wall(gt4_course_gadget *g,uint32_t wall,`
with the mesh's wall list swept at `course_objects.c:142`:
> `static void walls(gt4_course_gadget *g,uint32_t mesh,const float previous[3],const float points[8][3])`

**Off-track: no separate flag was found.** The only per-surface state the
engine reads is the `MESH_TRIS` byte above (plus `kind`), so "off track" is
whatever the authored surface byte says, interpreted by the two code layers
here. Nothing in this trace invents a track boundary. Anything beyond that
is **UNKNOWN** (see below).

Course-declared type (`dry/wet/dirt/snow`) comes from the SpecDB, not the
course file — `src/core/arcade.c:141-146`:
> `const char *gt4_arcade_course_type(gt4_arcade *a, const char *course)`
> `{`
> `    static const char *const types[] = { "dry", "wet", "dirt", "snow" };`
> `    uint8_t r[24];`
> `    if (!course_row(a, course, r) || r[0x10] > 3) return "unknown";`
> `    return types[r[0x10]];`

---

## (d) The AI line — where it comes from and who reads it

**It comes from the `.ad` file.** `src/core/physics/lines.c:1`:
> `/* The course's driving lines at the race start: the ADLN block of the`
`src/core/physics/lines.c:2`:
> `   course's .ad file (at course+0x9C) made ready for the driver model`

Landed there by `race_run.c:240-245` (quoted in (a)): `/crs/<name>.ad`,
block 0 → `COURSE + 0x9c`. Its magic — `src/core/physics/ai.c:21`:
> `#define LINE_MAGIC   0x4e4c4441u   /* "ADLN" */`

Checked at read — `src/core/physics/ai.c:37`:
> `    if (GT4_U32(GT4_U32(GT4_U32(lines) + 0x9c)) == LINE_MAGIC) {`

Line length/widening — `src/core/physics/ai.c:26-30`:
> `/* A line: count points of `stride` bytes (func_00469F20). Point: +0 kind`
> `   (0 straight, else arc), +1 s8 speed trim %, +2 s8 (-128 skip), +4 x,`
> `   +8 y, +0xc z, +0x10 s16, +0x12 s8 surface, +0x14 distance along the`
> `   course, +0x18 arc radius, +0x1c turn angle, +0x24 arc start angle, +0x28`
> `   byte. */`

**Consumers.** The driver model owns the line — `src/core/physics/ai.c:7-10`:
> `   The model's block sits in the car at +0x14 ("obj" here): +0x1a..0x1f`
> `   state bytes (0x1c line index, 0x1d section, 0x1e state), +0x28 target`
> `   speed, +0x2c blend toward the second line (0x21), +0x30 steering, +0x54`
> `   the aim's lateral offset, +0xe4 session, +0xe8 car, +0xec the lines. */`

Line switching — `src/core/physics/ai.c:1887`:
> `int gt4_ai_line_switch(uint32_t obj, int line, int now)`
called from `src/core/physics/race.c:162`, `src/core/physics/driver.c:789`,
`src/core/physics/laps.c:209`. Line index 4 is the pit line —
`src/core/physics/ai.c:88`:
> `static int pit_line_at(uint32_t lines, float *x, float *y, float *z, float d)`

Each **line point also carries its own surface byte** (`+0x12`, `ai.c:28`),
so the AI's grip and the car's grip read the same authored field.

---

## OVERRIDE SEAM — where a user-supplied course can be loaded

**The seam is the `/crs/` read, and it is a single chokepoint: `gt4_vol_read`.**
Every course path in the engine is built from a bare label string, and the
only place that string becomes bytes is `gt4_vol_read(v, "/crs/<label>", …)`.

There are exactly two call sites:
- `src/platform/sdl/race_view.c:295` — `    if(a){a->data=gt4_vol_read(v,path,&size);}` (preview, Time Trial, Photo, Film)
- `src/core/physics/race_run.c:220` and `:222` — `    crs = gt4_vol_read(vol, path, &n);` / `    adf = gt4_vol_read(vol, path, &an);` (the race)

Both entry points already take a plain label, and validate it:
`src/platform/sdl/race_view.h:16`:
> `int race_view_request(race_view *v,const char *course,const char *car,int color);`
`src/platform/sdl/race_view.c:539`:
> `    if(!v || v->worker || !course || !*course || strlen(course)>80 || strchr(course,'/') || strchr(course,'\\'))return 0;`

So an importer that writes a user's owned disc's course to a file the
engine can serve **needs no code change to geometry, collision, AI line,
pregrid or cameras** — those are all magic-tagged blocks inside the two
files and are already validated on open.

**The seam, stated plainly:** intercept `gt4_vol_read` for paths under
`/crs/` (prefix `"/crs/"`, suffix `".ad"`) and serve `<label>` and
`<label>.ad` from a user directory instead of the volume. That is the whole
load path. `LAW 1` holds: the loader ships, the data does not.

**But the seam is not enough on its own.** Three couplings sit outside it:

1. **A label must resolve to a course id** before the race can be selected.
   `src/core/arcade.c:137`:
   > `    return a && label && gt4_specdb_label(a->course_idi, a->course_idi_size, label, &id) &&`
   fed from `src/core/arcade.c:43`:
   > `    a->course_idi = read_table(vol, "COURSE.idi", &a->course_idi_size);`
   A new course needs an **additive id** in the `COURSE.idi` / `COURSE.dbt`
   pair (this is the `G6.0a` fixed-count job with `N = courses`).
2. **The menu/event tables index that id** — the course is chosen as a
   label and stored into `race_run.h:16`'s `char course[32]`; that same
   string is the save key (`session_best_update(game,mn,race_course,…)`,
   `src/app/main.c:2456`). So the id must never be reused.
3. **The `.ad` sidecar must be supplied too**, or the race path fails at
   `race_run.c:223`:
   > `    if (!crs || !adf || an < 24 || !gt4_course_open(&co, crs, n)) {`
   No `.ad`, no race — only the preview would draw.

**Cheap route, with precedence already in the repo:** re-import an existing
course's own `/crs/<label>` + `/crs/<label>.ad` pair from a disc the user
owns, under a new label with a new id. That exercises the seam end to end
without authoring a byte. Authored tracks are the second step, and the
repo's importer target is the shape named in `G6.33` / `G6.24`.

---

## UNKNOWNS — not measured, not guessed

- **Off-track / track-limits behaviour beyond the surface byte.** No code
  path in this trace reads a track-edge or "off course" flag separate from
  `MESH_TRIS`'s `surface`/`kind`. Whether the game derives off-track from
  the surface value, from the wall list, or from a table not yet located —
  **UNKNOWN**.
- **Whether `/crs/<label>` and `/crs/<label>.ad` are the only two files a
  course touches.** `/crs/` was not listed (`gt4_vol_list` exists —
  `src/core/vol.h:37` — but no listing was run here, per READ-ONLY).
  The `E5`/`G7.0` survey with `tools/vol_file.py` is the measurement.
- **The additive-id mechanism.** Whether `COURSE.idi` is a flat lookup or an
  id-keyed table with a fixed stride was not read past `arcade.c:43`. Not
  measured here — the `G6.0a`-style survey owns it.
- **Course name/minimap/thumbnail metadata source.** `race_hud.c:274` reads
  `/crs/%s` for the HUD, but the name string, minimap and thumbnail sources
  were not traced. **UNKNOWN**.
- **`GT4L1.VOL` (movies volume).** `vol.c:88` shows a second volume for
  movies; whether any course resource lives there was not checked.

## Confidence

High on (a), (b), (d) and the seam: the load path is short, every site is
quoted, and the two `/crs/` call sites are the complete set found by grep.
Medium on (c): the surface byte and grip layers are solid, but the
off-track / track-limits decision is not located and is marked UNKNOWN
rather than filled in. The hardest missing piece is not the loader — it is
the **additive course id** in `COURSE.idi`/`COURSE.dbt` and everything that
indexes it.
