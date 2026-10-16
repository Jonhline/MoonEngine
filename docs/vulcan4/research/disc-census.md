# Disc census — real container totals for GT4.VOL

**Status: COMPLETE (measured).** Every number below was printed by a command in this session on
this box. Read-only: nothing was modified on the disc, in `src/`, or in `include/`; nothing was
extracted from the disc (the only files written are this document and scratch listings in `/tmp`).

- Retail disc: `/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso`
  (5,314,478,080 B, SCUS-97328, volume `GRANTURISMO4`).
- Reader: `python3 /home/or/vulcan4/tools/vol_file.py <iso> ls|cat|extract …`, run from `/home/or/vulcan4`.
  On this disc it reports: `index at page 0x2159, 18562 entries, data at 0x1106000`.
- Comparison disc: `/mnt/ssd/gt4/spec2/Gran Turismo 4 (Spec II v1.10)/Gran Turismo 4 (Spec II v1.10).iso`
  (3,683,418,112 B) — **already extracted** from the `.7z` before this task, so it cost nothing to
  measure. Its index: `page 0x22b7, 27242 entries, data at 0x2000000`. No archive was unpacked here.

---

## 1. CONTAINER FACTS

Top level of `GT4.VOL` on the retail disc: **33 entries, all of them directories, zero files.**

```
advertise  bgm        cameras   car      character  config   crs       description
dnas       eyetoy     fep       font     icon       menu     music     narration
photo      piece      pit       printer  projects   race     race_display  replay
rtext      script     sound     specdb   text       tire     usedcar   wheel   wing
```

(`ls | wc -l` → 33; `awk '{print $1}' | sort | uniq -c` → `33 dir`.)

Sub-containers that matter for the two totals:

| Container | Entries | Note |
|---|---|---|
| `specdb/GT4_US2560` | 171 | the whole car/course database, 2 of the 171 are subdirs (`CARS`, `RACES`) |
| `specdb/GT4_US2560/CARS` | **804** | one file per car record, named by 16-hex-digit id |
| `specdb/GT4_US2560/RACES` | 828 | one file per race/event record |
| `car` | 3 dirs | `lod`, `menu`, `open` — no files at this level |
| `car/lod` | 1614 | **807** models + 807 matching `.pat` |
| `car/menu` | 807 | menu-preview models |
| `car/open` | 154 | **77** models + 77 `.pat` (opening/tuning view only, a subset) |
| `crs` | 221 | **110** course containers + 110 `.ad` + 1 subdir (`bestline`) |
| `piece/crs_name` | 23 | per-course name plates |
| `piece/crs_logo` | 113 | course logos (incl. `r_` reverse variants) |
| `piece/car_logo` | 401 | car logos |
| `usedcar` | 400 | used-car-showroom slot files (`EU_used####.adc`) |

Spec II disc, same reader, for comparison:

- Top level: **34 dirs, zero files** — `bgm`, `music`, `sound` are gone; `carsound`, `sound_gt`,
  `mpeg`, `textdata` are new.
- `specdb/GT4_PREMIUM_US2560` (118 entries, vs 171 retail) with `CARS` = **744** files.
- `car/lod` = 1628 (**814** models), `car/menu` = 816, `crs` = 222 (**111** containers + 111 `.ad`),
  `piece/crs_name` = 23, `piece/crs_logo` = 123, `piece/car_logo` = 401.

---

## 2. CAR SIDE

Where car data lives:

- **Models:** `car/lod/<id>` (+ `<id>.pat`), `car/menu/<id>`, `car/open/<id>` (+ `.pat`). Ids are
  short manufacturer codes, e.g. `acra0001`, `alfa0001`, `benz0001`.
- **Per-car database rows:** `specdb/GT4_US2560/CARS/<16 hex digits>` — e.g.
  `0000000000000015`, `0000000000000016` … plus the `*.dbt` / `*.idi` tables next to it.
- **Per-car text:** `CAR_NAME_<language>.dbt` in nine languages (american, big5, british, french,
  german, italian, japanese, korean, spanish); `piece/car_logo/*.img` for the logo art.

### COUNTABLE TOTAL — CARS = **804**

Two independent counts, both printed, agree:

1. `ls specdb/GT4_US2560/CARS | grep -c '^comp'` → **804** files.
2. `CAR_NAME_american.dbt` (and british/french/german/italian/korean/spanish) declare
   **804** in the `u32` at header offset `+0x08` — dumped as `47544442 0300 0800 24030000 10000000`,
   i.e. `0x324 = 804`.

The same header field reproduces two other independently-counted sizes exactly (see §3 and below),
which is why it is being read as a row count:

| Table | `u32 @ +0x08` | Independent count | Match |
|---|---|---|---|
| `CAR_NAME_american.dbt` | 804 | `specdb/…/CARS` dir = 804 | ✅ |
| `COURSE.dbt` | 110 | `crs/` non-`.ad` comps = 110 | ✅ |
| `COURSE_NAME_american.dbt` | 23 | `piece/crs_name` dir = 23 | ✅ |

Language-variant table row counts (all printed, same field): american/british/french/german/
italian/korean/spanish = **804**; **japanese = 790**; **big5 = 789**. So three of the nine name
tables are short by 14 and 15 rows respectively — a real, measured asymmetry, not a rounding.

**Model count vs database count:** `car/lod` holds **807** models and `car/menu` holds **807**, while
the database has **804** rows. The 3-car difference is unexplained and left open (below) — it was not
chased further, because 804 is the number the two independent counts support.

Other car-side totals (printed): `RACES` = **828** records; `ENEMY_CARS.dbt` header = 2558;
`CHASSIS.dbt` / `CAR_VARIATION_*.dbt` / `GENERIC_CAR.dbt` / `RACINGMODIFY.dbt` headers = 722 each;
`usedcar` = **400** slot files; `piece/car_logo` = **401**.

> Sampling note: no sampling was needed on the retail disc — the counts above are exhaustive over the
> named directories, produced by `grep -c` / `wc -l`, not by iteration into thousands of file
> *contents*. File *contents* were read only for the small `*.dbt` headers.

---

## 3. COURSE SIDE

Where course data lives:

- **Course geometry:** `crs/<name>` (compressed track container) with a sibling `crs/<name>.ad`;
  110 of each on the retail disc, plus one subdir `crs/bestline`.
- **Per-course database rows:** `specdb/GT4_US2560/COURSE.dbt` (+ `COURSE.idi`), with
  `COURSE_NAME_<language>.dbt` in the same nine languages.
- **Per-course art:** `piece/crs_name` (23), `piece/crs_logo` (113), plus `piece/crs_map`,
  `piece/crs_mini`, `piece/crs_pict` subdirs under `piece/`.

### COUNTABLE TOTAL — COURSES = **110**

Two printed counts agree:

1. `ls crs | grep '^comp' | grep -vc '\.ad$'` → **110** course containers (`ls crs | grep -c '\.ad$'`
   → 110 sidecars, so every container has one).
2. `COURSE.dbt` header `u32 @ +0x08` = `0x6e` = **110**; `COURSE.idi` carries the same **110**
   (`47544944 6e000000 …`, `GTID` + `110`).

Course **name** plates are a separate, smaller set: `piece/crs_name` = **23**, and every
`COURSE_NAME_*.dbt` declares 23 rows. 23 is therefore the *location* count, not the *layout* count —
110 containers include layout variants (`fisco`, `fisco_old`, `fisco_slalom`; `gymkhana`,
`gymkhana_free`, `gymkhana_maze`, `gymkhana_spiral_r`, `gymkhana_spral`, `gymkhana_super`).
`piece/crs_logo` = 113 (110 forward + a handful of extras such as `r_20r60r`).

Spec II comparison (printed): `crs` = **111** containers + 111 `.ad`; `COURSE.dbt` header = `0x72` =
**114**; `COURSE_NAME_american.dbt` header = **23**. Spec II's own container count (111) and its
database header (114) disagree by 3 — reported, not resolved.

---

## 4. WHAT THE NUMBERS ARE USED FOR

- **Cars total (804 DB rows / 807 `car/lod` models)** — the fixed ceiling that `WISHLIST.md` **G6.0**
  ("NEW CARS", the captain's *"can we add new cars?"*) and **G6.0a** ("SWAP vs ADD: find every place
  GT4 assumes a fixed car count") must beat. The 804 name-table rows are the tables G6.0a has to audit
  before a car can be *added* rather than *swapped*.
- **Courses total (110 containers)** — the map half of the same release gate, `WISHLIST.md` **G6.33**;
  its 23-row `crs_name` set is the smaller, location-level list that event/eligibility logic is likely
  keyed to, so 110 vs 23 is itself a fixed-count trap worth naming in the audit.

---

## 5. FAILURES + UNKNOWNS

Nothing failed on the retail disc; every command above exited `0`. Open items:

- **GTDB header semantics are inferred, not decoded.** The `u32 @ +0x08` field is *read* as a row
  count because it matched three independently-counted directory sizes (§2 table). The rest of the
  GTDB header is NOT decoded: `u16 @ +0x04` is `0x0003` on retail and `0x0001` on Spec II, and the
  meaning of `u32 @ +0x0C` (804-name tables: 16; `COURSE.dbt`: 22) is unknown. Per LAW 3 this is
  reported, not guessed.
- **`*.dbt` payloads are not plain text.** Dumping `CAR_NAME_american.dbt` and
  `COURSE_NAME_american.dbt` yields no ASCII names (`strings` → `GTDB` only); the blobs are
  encoded/byte-swapped name codes (numerous `u16` pairs in the `0x0f05`/`0x1304` range). So the 804
  rows could not be *listed* by name from the `.dbt` alone — the readable names must be reached
  through the `text`/`rtext` containers, which were not opened here.
- **807 vs 804.** `car/lod` and `car/menu` each hold 807 model entries; the database holds 804 rows.
  Which end is authoritative, and what the 3 extra model ids are, is unmeasured.
- **Spec II has no `GT4_US2560`.** Its database lives at `specdb/GT4_PREMIUM_US2560` with 744 `CARS`
  files, and its top level drops `bgm`/`music`/`sound` for `carsound`/`sound_gt`/`mpeg`/`textdata`.
  Its 111-container `crs` vs 114-row `COURSE.dbt` mismatch is unresolved.
- **Untouched:** `RACES` (828 records), `race`, `race_display`, `menu`, `projects`, `script`,
  `text`, `rtext` were only counted or listed at the top level — none of their formats were decoded.
