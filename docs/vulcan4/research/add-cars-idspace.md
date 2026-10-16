# CAR ID SPACE — every fixed-car-count assumption in the VULCAN 4 tree

**Status: PARTIAL.** The core spec/vehicle/model/eligibility path was read in full and the
real counts were measured from the disc on this box. The menu/shop/prize *script* path
(`/projects/*.adc`) and the engine-sound bank table were **not** read — see §4.
Part of WISHLIST `G6.0a` (the survey that turns "can we add cars?" into a work plan).

All measurements below are from the user's disc
`/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso`, read with the project's own built tools
(`/mnt/ssd/vulcan4-build/gt4vol`) — not inferred.

---

## 0. THE ID SPACE, AS IT ACTUALLY IS (measured)

A "car key" is a `uint64_t`: `(namespace << 32) | row_id`.

| fact | value | evidence |
|---|---|---|
| GENERIC_CAR rows (the buyable/ownable catalog) | **722** | `GENERIC_CAR.dbt` GTDB count=722 width=24; `GENERIC_CAR.idi` GTID count=722 |
| GENERIC_CAR row-id range | **21 .. 1077 (0x435)**, sparse (162 gaps) | ids decoded from `GENERIC_CAR.dbt` header pairs |
| Highest generic id (next free additive id ≥) | **1078** | max id 1077 |
| ENEMY_CARS rows (AI/opponent cars) | **2558**, ids 637..5986 | `ENEMY_CARS.dbt` count=2558 width=24 |
| ALLOW_ENTRY rows (event eligibility lists) | **58 rows × 480 bytes** | `ALLOW_ENTRY.dbt` count=58 width=480 |
| CAR_VARIATION_american rows | 722 × 8 bytes | one link row per car |
| VARIATIONamerican rows | 3849 × 52 bytes | colours/variants + model-name index |
| DEFAULT_PARTS rows | 2990 × 208 bytes | physics/parts keys per car |
| menu / race / open models on disc | 807 / 807 / 77 | `docs/MODELS.md:14-16`, `docs/MODELS.md:249-259` |
| catalog labels quoted by upstream docs | 722 | `docs/MODELS.md:21` |

**Namespaces in use** — exactly two are understood:
- `namespace 0` → GENERIC_CAR. `src/core/career_vehicle.c:113`:
  `if(!db || !oil || color<0 || (key>>32)!=0)return 0;`
- `namespace 0x26` → ENEMY_CARS. `src/core/career_admission.c:45-46`:
  `if((car>>32)==0)label=...generic...;` / `else if((car>>32)==0x26)label=...enemy...;`
- `0xFFFFFFFFFFFFFFFF` is the **no-value sentinel**, never a car:
  `src/core/career_admission.c:32` `if(key==UINT64_MAX)return 0;`,
  `src/core/career_admission.c:43` `if(car==UINT64_MAX)continue;`.

---

## 1. WHAT EXISTS — the tables/readers that exist today

**SpecDB readers (all bounded, all count-driven from the file header — these GROW):**
- `src/core/specdb.c:8` `gt4_specdb_label_for_id(...)` — GTID id → label string.
- `src/core/specdb.c:22` `gt4_specdb_label(...)` — label → id.
- `src/core/specdb.c:39` `gt4_specdb_row_index(...)` — GTDB binary search over ids;
  `src/core/specdb.c:45` reads each id as `int32_t signed_id` for the original's signed compare.
- `src/core/specdb.c:120` `gt4_specdb_row(...)` — id → row bytes.
- `src/core/specdb.c:128` `gt4_specdb_row_nth(...)` — repeated ids (VARIATION rows share an id).

No reader hardcodes a car count: counts come from `r32(b+4)` (GTID) / `r32(b+8)` (GTDB).

**Table registry (lazy, from the disc):**
- `src/core/spectables.h:8` `#define GT4_SPEC_TABLES 0x2b` (43 tables).
- `src/core/spectables.c:11` `static const char *const names[GT4_SPEC_TABLES] = { "GENERIC_CAR", ... }`.
- `src/core/spectables.c:49` `gt4_spec_tables_data(...)` loads `%s/%s.dbt` on first use.
- `src/core/spectables.c:79` car parts path:
  `snprintf(path, sizeof path, "%s/CARS/%08X%08X", t->dir, (unsigned)(car >> 32), (unsigned)car);`

**Vehicle / eligibility:**
- `src/core/career_vehicle.c:111` `gt4_career_vehicle_build(...)` — builds a car from its GENERIC_CAR key.
- `src/core/career_vehicle.c:187` `v->labels=gt4_vol_read(v->vol,"/specdb/GT4_US2560/GENERIC_CAR.idi",&v->labels_size);`
- `src/core/career_admission.c:27` `gt4_career_allowed_cars(...)` — resolves an event's allowed-car list.
- `src/core/career_prices.h` used-price path: *"Currently supports GENERIC_CAR keys only"*
  (`src/core/career_prices.h:11-12`) and `src/core/career_prices.c:24`
  `gt4_spec_tables_row(tables,0,(uint32_t)car,0,row,sizeof row)` — table index `0` hardcoded.

**Model reader (paths derived from the model NAME, not the id):**
- `src/platform/sdl/car_model.c:149-153` `car_model_tables_open` reads four disc tables once:
  `GENERIC_CAR.idi`, `CAR_VARIATION_american.dbt`, `VARIATIONamerican.dbt`, `american_StrDB.sdb`.
- `src/platform/sdl/car_model.c:198` menu model: `snprintf(path,sizeof path,"/car/%s/%s",directory,model);`
  (`directory` = `menu`|`open`|`lod`).
- `src/platform/sdl/car_model.c:167` race driver model: `/car/%s/%s` with `open`/`lod`.
- `src/platform/sdl/car_model.c:250` `/wheel/%s/%s`, `src/platform/sdl/car_model.c:260` `/wing/%s/%s`.

**The single read funnel:** every one of the above reads through
`src/core/vol.c:340` `uint8_t *gt4_vol_read(gt4_vol *v, const char *path, size_t *size)`
(declared `src/core/vol.h:26`) — 33 files in `src/` call it.

---

## 2. THE CEILING, RANKED — what breaks first when car N+1 appears

**1. There is no supplier for the new rows/files. The disc is the only source.**
The loaders are count-driven and generic, but every table is read straight from the disc
(`car_model.c:152-153`, `career_vehicle.c:187`, `spectables.c:56`). A NEW car needs a new
`GENERIC_CAR` row, a `CAR_VARIATION`/`VARIATION` row, a `/car/menu/<name>` file and a
`DEFAULT_PARTS` row — none of which exist on the user's disc, and **LAW 1 forbids shipping
them**. There is no overlay layer. This is the first and only true wall: not an array too
small, but nothing to read from. `docs/MODS.md:9-14` names the intended fix (a mod package
that replaces `/car/...` and `/specdb/...` paths + JSON specdb) and states plainly it is
not started: *"Nada disto está em andamento."*

**2. Model resolution is by NAME from the disc's variation tables.**
`car_model.c:176-177` turns id → variation row → string index → `/car/menu/<model>`; the
guard `word==52` and the `american_StrDB.sdb` string must exist. A new car whose model
name is not already in `VARIATIONamerican.dbt`/`american_StrDB.sdb` cannot resolve a mesh.
Pointing a new car at an EXISTING model name works with no engine change.

**3. Event eligibility is hard-capped at 60 cars per list.**
`src/core/career_admission.c:28` `char labels[60][128]`; `:35` `uint8_t allowed[480];`
(480 = 60×8); `:42` `for(unsigned i=0;i<60;i++){`; API mirror `src/core/career_admission.h:12`.
Measured `ALLOW_ENTRY.dbt` width **is exactly 480**. A new car cannot be listed in any
restricted event — it will never satisfy a make/model/PP filter (G6.0's *"shop/menu
integration … event eligibility"*). Only 6..64 row-ids exist (`ALLOW_ENTRY` count=58).

**4. The price / namespace path is GENERIC_CAR-only.**
`career_prices.h:11-12` and `career_prices.c:24` hardcode table `0`. A new car must keep
namespace `0`; `career_vehicle.c:113` rejects any other high word outright.

**5. Garage capacity is 1000.**
`src/core/career_original.h:11` `#define GT4_CAREER_GARAGE_CAPACITY 1000u`;
`career_original.c:152` (*"sum bit zero of the same 1000 flag words"*);
`career_original.c:173` (*"Iterate all 1000 slots"*);
`gtmode_game.c:377` and `:390` `for(unsigned i=0;i<1000;i++){`.
This bounds cars OWNED, not the catalog — a 723rd catalog car is fine; it bites only at
1000 owned cars.

**6. Fixed per-car / name tables (bite only if you exceed them).**
- `src/platform/sdl/car_model.h:15` `car_color colors[128];` — ≤128 paint variants per car.
- `src/core/career_parts.h:5` `country_names[256][24],tuner_names[256][24]`,
  `category_names[100][24]`, `table_names[39][24]` — ≤256 tuners/countries, ≤100 categories.
- `src/platform/sdl/car_model.h:28` `float cam_point[32][8];` — ≤32 camera points / car.

**Ranking:** (1) no row/file supplier → (2) model-name resolution → (3) 60-slot eligibility →
(4) GENERIC_CAR-only price/namespace → (5) 1000-car garage → (6) per-car caps.

---

## 3. WHERE AN ADDITION PLUGS IN — the seam(s)

**Best seam — one function: `gt4_vol_read` (`src/core/vol.c:340`).**
Every table and every asset in the car path reaches the disc through this single call
(33 callers). If it consults an overlay directory *before* the VOL — so that a mod path
shadows a disc path — then **all** of these become growable at once, with no reader edits:
`GENERIC_CAR.idi/.dbt`, `CAR_VARIATION_american.dbt`, `VARIATIONamerican.dbt`,
`american_StrDB.sdb`, `DEFAULT_PARTS.dbt`, `/car/{menu,lod,open}/<name>`,
`/wheel/menu/<name>`, `/wing/menu/<name>`, `/specdb/GT4_US2560/CARS/<key>`.
This is exactly the shape `docs/MODS.md:11-14` already specifies — so the seam is not
invented here, it is the upstream plan; it just has no implementation.

**Additive id rules (from the measurements):** pick a NEW key, never reuse one
(G6.0 honest limit, `WISHLIST.md:60-61`):
- namespace = `0` (GENERIC_CAR) — `career_vehicle.c:113` requires it;
- id ≥ **1078** (max existing 1077). Any existing gap is also never-assigned, but a
  monotonically-growing id above the max is the clean choice.
- `0xFFFFFFFFFFFFFFFF` is reserved as the empty sentinel — never assign it.

**Existing additive hooks already in the tree (proof the pattern works):**
- `src/app/main.c:1479` `if(getenv("OPENGT4_TEST_CAR"))menu_test_car(...);  /* tests: a new car ridden */`
- `tests/physics/setup_run.c:633` *"SETUP_NEWCAR=id: a new garage entry for that GENERIC_CAR row"*
- `tools/c/gt4car_survey.c` and `tools/c/gt4spec.c` enumerate GENERIC_CAR by driving the
  readers with ids straight from the table — the readers already accept any id the table holds.

**Secondary seam (narrower):** `gt4_spec_tables_data` (`spectables.c:49`) if only SpecDB
rows (not models) need to grow — but it does NOT cover `/car/*` assets, so it is strictly
smaller than the `gt4_vol_read` seam.

---

## 4. UNKNOWNS — what could not be determined from this box

- **Shop / used-car / prize car lists are script- or menu-driven and were NOT read.**
  `docs/CAR_CATALOG.md:19-21` says `/projects/arcade/US_carlist.adc` drives ordering from
  the original `CarData` arrays. Whether a shop/prize list indexes cars by *position* in a
  script array (a second fixed-count site) is **UNKNOWN** — the `.adc` scripts were not
  disassembled. `grep` for `PRIZE|prize|shop` in `career_prices.c`/`career_event.c` found
  no prize-car table in C source.
- **Engine-sound bank assignment table — NOT located.** How the game maps a car to its
  motor sample was not found in `src/` (only the system sound bank, `sound.c:152`
  `/sound/gt4sys.ins`). G6.0 item 4 stays UNKNOWN.
- **CAR_NAME text table** is loaded (`menu.c:3110`,
  `/specdb/GT4_US2560/CAR_NAME_american.dbt`) and used (`menu.c:3032` `getCarName`), but its
  row count / key scheme were not measured — untested whether a new car needs a new row here.
- **Runtime behaviour when an id has rows but no `/car/menu/<name>` file** was not executed
  (no synthetic car was injected). The failure path in `load_car` (`car_model.c:272`
  `"showroom: empty car"`) is read, not run.
- **Whether ALLOW_ENTRY's 60-slot width is a hard engine rule or only a disc-format width**
  was not proven; the engine reader hardcodes 480/60 to match the disc, but the original
  menu code that consumes the list was not read.
- **ALLOW_ENTRY row count 58 and ENEMY_CARS id gaps** are measured facts; whether the game
  tolerates an added row id is UNKNOWN.
- Coverage limit: `src/core/menu.c`, `arcade.c`, `career_event.c` were grepped, not read in
  full — a fixed car-count constant could still hide there.

---

*Method: readers cited at `path:line`; all counts read from the disc with the project's
`gt4vol` tool. UNKNOWN is written where nothing was measured — nothing inferred.*
