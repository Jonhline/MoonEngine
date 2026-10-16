# Add-a-car: the load path, and the seam where a foreign model gets in

Scope: how the **OpenGT4 engine (this fork)** loads one car from the user's disc —
container → file bytes → decoded CAR4 → textures → physics params → drawn car — and the
single place a loader can prefer a user-supplied file over the disc's own.

Every claim below is `path:line` + the quoted line. Anything unmeasured is under
**UNKNOWNS**, not inferred. Read-only survey (no `src/` or `include/` file was touched).

Region note, measured across the tree: the SpecDB directory is **hardcoded to
`/specdb/GT4_US2560`** (`src/core/spectables.c:37`:
`snprintf(t->dir, sizeof t->dir, "%s", dir ? dir : "/specdb/GT4_US2560");`; also
`src/platform/sdl/car_model.c:13`, `src/app/race_game.c:635`). So every path below is the
US v2.00 layout.

---

## 1. THE LOAD PATH, STEP BY STEP

### 1.1 disc image → GT4.VOL container
The engine opens the ISO and *locates* the volume; it never sees the inner files as files.

- `src/core/vol.c:88` — `return find_in_iso(v->f, movies, movies ? "GT4L1.VOL" : "GT4.VOL", &v->base, &size);`
  (`locate_in_iso`, via `find_in_iso` reading the ISO 9660 root at `vol.c:53`).
- `src/core/vol.c:232` — `uint64_t toc = (uint64_t)GT4_TOC_PAGE * PAGE;` — the RoFS index page.
  Discriminator is the declared page size, keyed on measured pages:
  `vol.c:234` — `if (read_at(v, toc, h, sizeof h) && le32(h) == RoFS_MAGIC && le16(h + 0x10) == PAGE) {`
  (retail v2.00 page `0x2159`, Spec II `0x22b7` — comment at `vol.c:224-231`).
- Index pages are raw-deflated then XOR'd (`vol.c:280` `core_inflate_raw(...)`, `vol.c:287`
  `page[k] ^= TOC_XOR;`) and parsed into a **directory tree of entries**:
  `vol.c:162` `static int parse_page(gt4_vol *v, const uint8_t *p)`; the tree is
  `parent/name/type/page/size` (`vol_entry`, `vol.c:21-29`).

**The container is a tree of paths, and a lookup is a walk of that tree** — this is the fact
the rest of the answer turns on:
- `src/core/vol.c:321-338` — `lookup()`: `uint32_t node = 1;` … `e = child(v, node, path, len);`
  `/car/menu/xyz` is resolved by walking root → `car` → `menu` → `xyz`.
- `src/core/vol.c:312-318` — `child()`: linear scan of entries by `parent` node + name.

### 1.2 path → bytes (the one choke point)
- `src/core/vol.c:340` — `uint8_t *gt4_vol_read(gt4_vol *v, const char *path, size_t *size)`
  - `vol.c:342` — `const vol_entry *e = lookup(v, path, NULL);`
  - `vol.c:347-348` — uncompressed: `if (!read_at(v, at, out, e->size)) …`
  - `vol.c:350-355` — compressed: magic `0xFFF7EEC5`, `-size` check, `core_inflate_raw(...)`.

**Every asset in the game comes out of this one function.** Verified callers: car model
(`car_model.c:198`), wheels (`car_model.c:251`), wing (`car_model.c:261`), `.pat`
(`car_model.c:204`), SpecDB tables (`car_model.c:13`), course (`race_view.c:295`), and the
race entry tables (`race_game.c:635`). The returned buffer is caller-owned `malloc` memory
(freed with `free()` — e.g. `car_model.c:168`, `:269`).

### 1.3 label → id → variation → model filename
A human-readable car ("GENERIC_CAR label") is turned into a filename through three SpecDB
tables, in `load_car`:
- `src/platform/sdl/car_model.c:175` — `if(!gt4_specdb_label(labels.data,labels.size,code,&id) || row(links,id,r)!=8)return NULL;`
- `src/platform/sdl/car_model.c:176` — `if(row(variants,u32(r),r)!=52)return NULL;const char *model=str(strings,u32(r));`
  (string index → model code, from `VARIATIONamerican.dbt`).
- Tables opened in `car_model_tables_open`, `car_model.c:152-153`:
  `GENERIC_CAR.idi`, `CAR_VARIATION_american.dbt`, `VARIATIONamerican.dbt`, `american_StrDB.sdb`.
  `.idi` is a `GTID` label→id map (`src/core/specdb.c:22` `gt4_specdb_label`), `.dbt` rows are
  `GTDB` binary-searched by id (`specdb.c:39` `gt4_specdb_row_index`).
- Sanitised model code (no `/`, no `\`, ≤24 chars) at `car_model.c:177`.

### 1.4 model path construction (menu vs race vs open roof)
- menu / showroom — `src/platform/sdl/car_model.c:198`:
  `char path[96];size_t size=0;snprintf(path,sizeof path,"/car/%s/%s",directory,model);uint8_t *data=gt4_vol_read(v,path,&size);`
  with `directory="menu"` by default (`car_model.c:190`).
- race LOD / open roof — `src/platform/sdl/car_model.c:195-196`:
  `a->race_open=generic[0x14]>=2 || (generic[0x14]==1 && cars<3);` then
  `directory=a->race_open?"open":"lod";`
- driver/LicenceConcourse geometry path, `car_model.c:167`:
  `snprintf(path,sizeof path,"/car/%s/%s",generic[20]>=2 || (generic[20]==1 && cars<3)?"open":"lod",model);`
- replacement wheel sub-model, `car_model.c:250`: `snprintf(wheel_path,sizeof wheel_path,"/wheel/%s/%s",cars>0?"lod":"menu",wheel);`
- wing sub-model, `car_model.c:260`: `snprintf(wing_path,sizeof wing_path,"/wing/%s/%s",cars>0?"lod":"menu",wing);   /* 00454378 */`
- race model's paint table is a *sidecar file*: `car_model.c:203`
  `snprintf(pat_path,sizeof pat_path,"%s.pat",path);` — read at `:204`, appended to the CAR4
  buffer at `:206-208` with the `+0x1C` (Pat0) pointer written.

**So the engine's car identity is exactly a set of paths:** `/car/{menu,lod,open}/<model>`,
`/car/.../<model>.pat`, `/wheel/{menu,lod}/<w>`, `/wing/{menu,lod}/<wing>`, each resolved
through the volume's path tree.

### 1.5 bytes → decoded model (no EE/VU execution)
The CAR4 buffer is decoded to plain triangles + `Tex1` textures:
- `src/platform/sdl/car_model.c:240` — `a->car=installed?gt4_model_decode_car_wing(data,size,(int)a->colors[a->choice].order-1,1):gt4_model_decode(data,size,(int)a->colors[a->choice].order-1);`
- shadow/aux model indices: `car_model.c:269-270` — `a->shadow=gt4_model_decode_part(data,size,…,2);` / `a->cast=…,1);`
- wheels are decoded and attached to the body at measured native offsets: `car_model.c:94`
  `gt4_model *part=piece?gt4_model_decode_part(...):gt4_model_decode_wheel_part(...)`, then
  `car_model.c:125` `gt4_model_attach(a->car,part,tr,copies);`.
- Decode entry points declared in `src/core/model.h:51-58`; header note "The frontend converts
  CAR4/MDLS data from the user's disc to ordinary triangles and RGBA textures. It does not
  execute EE/VU code." (`docs/MODELS.md:31-32`).

### 1.6 textures
Textures are **carried inside the model's own MDLS/Tex1 streams** — the decoder fills
`gt4_model.textures` (`src/core/model.h:31` `tex1_image *textures; int ntextures;`).
No separate texture-path read exists for a car. The only extra texture asset is the shared
reflection map: `src/platform/sdl/race_view.c:356-357`
`gt4_vol_read(v,"/projects/arcade/US/CarRoot.gpb",…)` → `gpb_load(…,"image/arcade/car/env0.png",…)`.
Replacement parts share the parent's texture set explicitly:
`race_view.c:403` `gt4_model part=*a->steering[i];part.textures=a->car->textures;part.ntextures=a->car->ntextures;`.

### 1.7 physics parameters
Physics is **not** read by the model loader — it is a separate SpecDB path keyed off the same
car id:
- `src/platform/sdl/car_model.c:30` — `if(disc_row(v,"GENERIC_CAR.dbt",id,r)!=24 || disc_row(v,"DEFAULT_PARTS.dbt",u32(r),parts)!=208)return;`
- geometry from those rows — `car_model.c:31` `CHASSIS.dbt` (wheelbase), `:33` `SUSPENSION.dbt`
  (front/rear ride heights), `:49` `RACINGMODIFY.dbt` (tread), `:52-53` `FRONTTIRE/REARTIRE` →
  `TIRESIZE.dbt`.
- the physics-facing map `TUNED_CAR -> DEFAULT_PARTS` (engine, drive, chassis, suspension,
  brake, tyres, gearbox, steering, LSD) is `src/core/physics/garage.c:226-245`:
  `/* func_00444690 +0x00..0x1ac: TUNED_CAR -> DEFAULT_PARTS. */` … `set_key(gar,map[i][0],part);`.

### 1.8 decoded model → drawn car
- meshes built once, on the loader worker: `src/platform/sdl/race_view.c:397`
  `static void make_car(car_meshes *m,const car_model *a)` → `race_view.c:400`
  `m->car=r3d_mesh_create(a->car);`.
- the whole race asset (course + up to 6 cars) is loaded on a worker thread:
  `race_view.c:290` `static race_asset *load(const char *disc,const char *course,const char *car,…)`,
  opening its **own** volume — `race_view.c:292` `gt4_vol *v=gt4_vol_open(disc,&err);`.
- per-car load from that volume: `race_view.c:345` `car_model_tables *t=car_model_tables_open(v);`
  and `race_view.c:348-349` `car_model_load_race_parts(v,t,car,color,nothers+1,0,wheel,wing)` / opponents.
- draw: `race_view.c:946` `static void draw_car(const race_asset *env,const car_meshes *m,const car_model *a,…)`.
- the race-entry side resolves the label to a car id too — `src/app/race_game.c:635`
  `labels = vol ? gt4_vol_read(vol, "/specdb/GT4_US2560/GENERIC_CAR.idi", &n) : NULL;`,
  `race_game.c:680` `gt4_specdb_label(labels, n, label, &d.car[d.cars])`.

---

## 2. WHAT A CAR OBJECT MUST CONTAIN

The decoded object is `car_model` (`src/platform/sdl/car_model.h:9-35`); the thing the engine
animates is its node set, not a single mesh:

- **body** — `car_model.h:12` `gt4_model *car,*shadow,*hull;`
- **4 wheels again as their own models, about their hubs** — `car_model.h:16-19`:
  `/* The wheels again as their own models, about their hubs (front then rear, the model's
  +x side first), for the race view to turn them; car's first body_count triangles are the
  body without them. */` → `gt4_model *wheel[4];float hub[4][3],wheel_limit[2],tire_size[4][3];int body_count;`
- **brakes (steer/travel, don't spin with the tire)** — `car_model.h:20-22`
  `gt4_model *brake[4];`
- **steering frames** — `car_model.h:23` `gt4_model *steering[GT4_MODEL_STEERING_MAX];`
- **cockpit/camera points** — `car_model.h:24-28`
  `/* The race cameras' points on the car (CAR4 header +0x80 count, +0x84 offset; 32 bytes each…) */`
  `float cam_point[32][8];int cam_points;`, filled at `car_model.c:213-216`.
- **driver seat / pilot animation** — `car_model.h:29-31` `gt4_driver_car driver; uint8_t *driver_data;… const char *driver_path;`.
- **exhaust outlets, ride-height pose** — `car_model.h:32-34`
  `float body_sine,body_cosine,body_lift; int race_open;unsigned exhaust_count; float exhaust[8][6];`.

The node contract is carried per-triangle by an attachment tag, defined in
`src/core/model.h:17-18`:
`uint8_t attachment; /* 0 body, 1..4 road wheels, 5+ steering frames */`.
Wheel/brake extraction keys on it: `car_model.c:61`
`for(int i=0;i<a->body_count;i++)if(a->car->triangles[i].attachment==native+1)count++;`.

Variants: paint colours come from `VARIATIONamerican.dbt` rows keyed to the model string —
`car_model.c:183-187` (each row's VarOrder is one-based, `docs/MODELS.md:48-49`), and the
per-colour geometry is selected by patch table `Pat0` (`.pat` sidecar).

---

## 3. THE OVERRIDE SEAM

**`gt4_vol_read()` — `src/core/vol.c:340` — is the single seam.**
It is the one function through which every car byte (model, wheels, wing, `.pat`, SpecDB rows,
course) passes, and its only input is a flat path string. A folder override inserted at the top
of it —

```
lookup(v, path) already returns the disc entry;
   before/around it:  if (<user dir>/<path> exists) return its bytes (malloc'd, size set)
```

— prefers a user-supplied file over the disc's own **for any path**, with **no per-call-site
change**. That is what makes it the right seam: `/car/lod/<model>`, `/wheel/lod/<w>`,
`/specdb/GT4_US2560/GENERIC_CAR.dbt` etc. are all just strings here.

Constraints a correct implementation must respect (all measured in this file):
- the return is **caller-owned `malloc` memory**, freed with `free()` — the override must
  return `malloc`'d bytes, not a read-only/mmap pointer (`vol.c:344` `malloc`; callers
  `car_model.c:168,204,251,261,269`).
- `*size` must be set exactly (`vol.c:358` `if (size) *size = e->size;`).
- returning a foreign CAR4 is **not sufficient** — it still has to survive `gt4_model_decode`
  (`car_model.c:240`) and the attachment-tag contract of §2. The loader has no fallback mesh.

Two **narrower** alternatives, if a full VOL override is too broad:
- **Model-path seam** — `car_model.c:198` (`snprintf(path,…, "/car/%s/%s", directory, model);`).
  A modder supplies a new model code in a SpecDB `VARIATION` row and drops
  `/car/lod/<code>` + `/car/menu/<code>` (+ `.pat`) into the override dir. This is the
  **swap** shape (G6.0a): it needs no volume-layer change but reuses/replaces an existing
  label's model.
- **Label seam** — `car_model.c:175-176` resolves label→id→model through `.idi`/`CAR_VARIATION`/`VARIATION`.
  A **new** car id requires these tables to grow (the G6.0a blocker): `.idi` is a flat
  count+index table (`specdb.c:11-12` `size_t count=r32(b+4);… size_t base=16+count*8;`), and
  `race_game.c:635,680` also read `GENERIC_CAR.idi` for entry resolution.

Recommended shape for G6.0: **override at `gt4_vol_read` for the model/wheel/wing/pat paths,
new SpecDB rows for identity/data** — one choke point, additive rows, nothing in the renderer.

---

## 4. FENCES + RISKS

Must **not** change (each is load-bearing for fidelity or for the honest claim):

1. **Decoder + attachment contract** — `gt4_model_decode` and the `attachment` tag
   (`model.h:17`) are what the wheels/brakes/steering extraction assumes
   (`car_model.c:61,68`). A foreign model that ignores them renders without turning wheels.
2. **Measured native offsets** — wheel/brake/tire placement, ride height, tread and rim scale
   are reproduced from specific original call sites (`car_model.c:59,69-70,84-87,101-103,122`).
   "A pretty mesh with wrong wheelbase/tread will be positioned wrong" (`WISH LIST G6.0`, quoted
   `docs/vulcan4/WISHLIST.md:62-64`). Do not re-tune these to fit an imported car.
3. **Byte-identical reference captures** — regression set "M Coupe, Focus, Ka, BMW 120i, Model T
   captures are **byte-identical**" (`docs/MODELS.md:271`). Anything that alters the disc path
   (e.g. an override that fires when it should not) breaks this.
4. **`gt4_vol_read` free()/size contract** (§3) and the `.pat` splice at `car_model.c:202-208`.
5. **Region dir hardcoding** (`spectables.c:37`) — paths are US-only today; a non-US disc is a
   separate change, not a side effect of the loader.
6. **LAW 12** — everything additive ships OFF by default; `OFF` = untouched 2005 path
   (`AGENTS.md` LAW 12, `docs/vulcan4/WISHLIST.md:21-24`).
7. **LAW 1** — no disc data / extracted model committed; the override reads the *user's* files.

Risks: the override is a **trust boundary** — a malformed override file reaches a decoder whose
bound checks are host-added (`specdb.c:7` `span()`, `car_model.c:182` count check); it must be
rejected, not approximated (LAW 2/3). A full-VOL override also intercepts `/specdb/*` and
`/crs/*`, i.e. it can silently change non-car content.

---

## 5. UNKNOWNS

- **No CAR4 writer exists** in this tree, and none is proven elsewhere; `docs/MODS.md:74-78`
  estimates 4–6 sessions for an OpenGT4-only writer. So "load a foreign car" today means a file
  that already satisfies `gt4_model_decode` — an unbuilt capability, not a measured one.
- **Textures are read from inside the model** (§1.6) — no independent car texture path was
  found; a foreign model must carry its own `Tex1`. Exact acceptance rules of the Tex1 reader
  for an out-of-spec texture are **not** measured here.
- **Which `VARIATION` row the model string comes from** when a label has several
  (`docs/MODELS.md:43-44`, "Variation rows can share an ID") — the loader takes the first
  matching (`car_model.c:183-187`); the *original engine's* selection per screen is not re-read
  here.
- **AI/opponent and shop/prize integration** (`WISHLIST G6.0` items 4–5) — not traced; the
  tables exist (`career_admission.c:34` `ALLOW_ENTRY.dbt`, `career_event.c:48-50` `RACE.*`) but
  their growth behaviour is the G6.0a survey, not this one.
- **Whether a car id can be appended** without breaking `.idi`/`.dbt` binary search
  (`specdb.c:39` assumes original order) — unmeasured; that is G6.0a's dish.
