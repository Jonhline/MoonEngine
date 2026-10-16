# ADD a circuit — the COURSE id space and every fixed-course-count assumption

Scope: `docs/vulcan4/WISHLIST.md` G6.33 (CUSTOM MAPS) + G6.0a (the same survey for cars).
READ-ONLY pass over the tree. Every claim below is a `path:line` read, or a measurement taken
from the user's own disc with the repo's own tools. Nothing here is a build or a run result.

**Disc measurements** (US v2.00, `/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso`, extracted with
`/mnt/ssd/vulcan4-build/gt4vol`, parsed with the layout in `src/core/specdb.c`):

| Quantity | Value |
|---|---|
| `COURSE.idi` entries (`count` at `+4`) | **110** |
| `COURSE.dbt` rows (`count` at `+8`), `width` at `+12` | **110**, **22 bytes** |
| `COURSE.dbt` header word at `+4` | `0x80001` — **bit0 set = rows are Huffman-coded** (`specdb.c:62`) |
| id range | `0x2 .. 0xb0`; the two tables carry the **identical, sorted** id set |
| ids free inside `0x2..0xb0` | **65** (110 used, 66 slots, 110-1 overlap none) |
| `/crs` entries | 110 course files + 110 `.ad` files (+ `bestline/`) |

Tsukuba is id `0xa`, Nürburgring `0x41`, Grand Valley `0x1b`, Sarthe 24h `0xaf`. Reverse
layouts are *separate* ids (`r_akasaka` …), which is why 110 keys cover "55 tracks".

---

## 1. WHAT EXISTS — how a course is identified today

The engine's primary key is the **label string**, and the id is a disk-derived lookup — there is
no id literal in the C tree.

1. **Label → id** (`COURSE.idi`, GTID): `gt4_specdb_label` walks every entry (linear, not sorted):
   `src/core/specdb.c:27` `for(size_t i=0;i<count;i++) {`
2. **id → row** (`COURSE.dbt`, GTDB, 22 bytes/row, Huffman-coded here):
   `src/core/menu.c:2585` `int bytes=gt4_specdb_row(m->course_db,m->course_db_size,id,row,sizeof row);`
   `src/core/menu.c:2586` `return bytes==22?row[17]:0; /* US2560 COURSE.EntryMax */`
   Row lookup is a **binary search over an id-sorted table** (`src/core/specdb.c:41-50`).
3. **Car count for the race setup** = that EntryMax, exposed to the authored menu scripts as
   `MCourseData.GetMaxCarNum`: `src/core/menu.c:5317`
   `ah_set_member(vm,course,"GetMaxCarNum",ah_native(vm,"GetMaxCarNum",nat_course_capacity,m));`
4. **SpecDB key** = namespace `0x22` in the high dword, row id low:
   `src/core/menu.c:2595-2596` `/* 00447C78 +0x14: COURSE namespace 0x22. */` /
   `*key=UINT64_C(0x2200000000)|id;` — same constant in `src/core/arcade.c:15`
   `#define T_COURSE 0x22u` and `src/core/disc_replay.c:218`
   `why="course";if(word(info+0xbc)!=0x22 || !label(d->vol,"COURSE",word(info+0xb8),...`
5. **RACE → COURSE link** is the race row's leading **u16**, resolved back through `COURSE.idi`:
   `src/core/career_event.c:57-58`
   `const char *course=gt4_specdb_label_for_id(courses,courses_size,` /
   `event->race_row[0]|(uint32_t)event->race_row[1]<<8);`
   Same two bytes in `src/core/career_license_disc.c:32`, `src/core/menu.c:4328`,
   `src/core/physics/race_run.c:361` (`RACE + 0x78` = the course key).
6. **Assets** are per-label paths, not per-id:
   `src/core/physics/race_run.c:219` `snprintf(path, sizeof path, "/crs/%s", name);` and
   `:221` `snprintf(path, sizeof path, "/crs/%s.ad", name);`
7. **The course row's other fields** the game reads: type `r[0x10]` (`src/core/arcade.c:145`),
   pit package `r[0x14]/r[0x15]` (`src/core/arcade.c:154`), attribute string (`:162`).
8. **The selectable course list is authored script data, not a C array** —
   `src/core/menu.c:2614` `bytes=gt4_vol_read(m->vol,"/projects/boot/US_carlist.adc",&size);`
   `src/core/menu.c:2617-2618` reads `InitData.course_carlist`, and `:2622` adds `r_<label>`
   only when `CourseData[course].has_reverse` is set. **No `static const char *courses[]`
   exists anywhere in `src/`** (grep for track names returns only `src/app/main.c:1540`, a CLI
   default).

## 2. THE CEILING, RANKED — what breaks first when course N+1 appears

**#1 — the label must exist in `COURSE.idi` *and* have a row in `COURSE.dbt`, and no C path
accepts an added label.** First and hardest. Every consumer resolves label→id→row; a miss
returns 0 (capacity) or a hard failure:
- `src/core/menu.c:2584` `if(!gt4_specdb_label(m->course_labels,m->course_labels_size,label,&id))return 0;` → `GetMaxCarNum` = 0
- `src/core/menu.c:2586` `return bytes==22?row[17]:0;` → any width ≠ 22 reads as 0
- `src/core/career_event.c:61` `if(!course || strlen(course)>=sizeof event->desc.course || count<1 || count>6)goto done;`
- `src/core/career_license_disc.c:33` `why="COURSE label";if(!course || strlen(course)>=sizeof e->desc.course)goto done;`
- `src/core/physics/race_run.c:219-226` `if (!crs || !adf || an < 24 || !gt4_course_open(&co, crs, n))` → `"cannot read course %s"`
- **Sort order matters**: the dbt row table is id-sorted and searched by bisection
  (`src/core/specdb.c:41-48`), so an added row must be *inserted*, not appended.

**#2 — the profile's unlocked-course set is exactly 128 u64 keys, and key 129 is silently
dropped.** Measured headroom today: 110 course ids → **18 keys**.
- `src/core/career_home.c:64` `void gt4_career_course_unlock(uint64_t courses[128],uint64_t key){unsigned i=0;while(i<128 && courses[i]<key)i++;if(i==128 || courses[i]==key)return;...`
- `src/core/career_home.c:62` `int gt4_career_course_available(const uint64_t courses[128],...` (`unsigned lo=0,hi=128;`)
- `src/core/menu.c:2599` `static void course_store(ah_vm *vm,ah_obj *available,const uint64_t keys[128])`
- `src/core/menu.c:2601` `for(unsigned i=0;i<128;i++){...}` and `:2605` `for(unsigned i=0;i<128;i++)keys[i]=UINT64_MAX;`
- `src/core/menu.c:2628`, `:2634` — both natives carry a stack `uint64_t keys[128]`.
- Test side confirms the shape: `tests/gtmode/home_run.c:39` `uint64_t courses[128];...0x2008`.

**#3 — a RACE can only name a course id that fits in 16 bits.** Not a wall today (ids ≤ `0xb0`),
but it is the width of the link and must not be crossed by an added id.
- `src/core/career_event.c:58` `event->race_row[0]|(uint32_t)event->race_row[1]<<8);`
- `src/core/career_license_disc.c:32`, `src/core/menu.c:4328`, `src/core/physics/race_run.c:361` (same read).

**#4 — the save carries a fixed 132-byte × 568-unit course-record blob.** Headroom today: 568 vs
110, i.e. *not* the binding limit — but the size is a constant in the save layout.
- `src/core/save_ps2.c:85` `blob(vm,course,"records_ps2",p+at,132*568);at=align16(at+132*568);`
- read side: `src/core/menu.c:2896` `ah_vm *vm=m->vm;enum{UNITS=132,SIZE=0x238};` with `:2908` writing 10 entries/course.

**#5 — 768 race-record slots (races, not courses) — the ceiling a new *event* hits.**
- `src/core/career_race_lookup.c:26` `return index>=0 && index<768?index:-1;`
- `src/core/save_ps2.c:84` `for(int i=0;i<768;i++){...}`
- `src/core/gtmode_game.c:1288` `for(unsigned i=0;i<768;i++){` and `:673` `index.u.i<768`

**#6 — the licence/exam index space is fixed at 125 records, 80 standard exams, 16 per group.**
A licence test pointed at a new course cannot get an index beyond these.
- `src/core/career_license.c:45-46` `unsigned index=group<5?(uint32_t)group*16+(uint32_t)n-1:group==5?(uint32_t)n+79:(uint32_t)n+84;` / `return index<125?(int)index:-1;`
- `src/core/career_license.c:42` `static const char *groups[]={"l0b","l0a","lib","lia","l0s","l0c","l0m"};`
- `src/core/career_license.c:54` `if(index<0 || index>=80 || strlen(code)>=32)return 0;`
- `src/core/save_ps2.c:85` `...for(int i=0;i<125;i++){...i*504...}`

**#7 — the course surface type is a 4-entry enum, and the pit package a 3-way select.**
- `src/core/arcade.c:143-146` `static const char *const types[] = { "dry", "wet", "dirt", "snow" };` / `...|| r[0x10] > 3) return "unknown";`
- `src/core/arcade.c:154` `return course_row(a,course,r) && r[0x14] ? (r[0x15]==2?2:1) : 0;`

**#8 — per-course field caps that an authored new course must respect.**
- entry count: `src/core/career_event.c:61` `count<1 || count>6`; `src/core/physics/race_run.h:17-18` `int cars; /* 1..6 */` / `uint32_t car[6];`
- name length: `src/core/physics/race_run.h:16` `char course[32];      /* a /crs name, e.g. "tsukuba" */` — enforced at `src/core/career_event.c:61`.

**#9 — the COURSE table's 22-byte width is a hard assumption in the only capacity reader.**
- `src/core/menu.c:2586` (quoted above). A wider row reads as capacity 0.

**#10 — two independent, cached, once-only loads of `COURSE.{idi,dbt}`.** A runtime addition must
land before the first read, or it is invisible.
- `src/core/menu.c:2578-2582` `if(!m->course_load_attempted) { m->course_load_attempted=1; m->course_db=gt4_vol_read(...)`
- `src/core/menu.c:4323-4325` (the `MRaceData` path, same guarded read)
- `src/core/physics/race_run.c:361` (its own private read, not cached)

Also present but not a count: the COURSE table's slot in the lazy table set —
`src/core/spectables.h:8` `#define GT4_SPEC_TABLES 0x2b` with `src/core/spectables.c:16`
`"SUPERCHARGER", "WHEEL", "WING", "TIRESIZE", "TIRECOMPOUND", "TIREFORCEVOL", "COURSE",`.

## 3. WHERE AN ADDITION PLUGS IN

The seam is **label→id→row resolution**, not the file format. Nothing in `src/` enumerates
courses, so there is no array to grow and no count constant to bump: the selectable list is
authored script data (`src/core/menu.c:2614-2624`) and the geometry/AI/collision path is
per-label (`/crs/<label>`, `/crs/<label>.ad`) — `src/core/physics/race_run.c:219,221`.

A course added by an overlay (G6.25's shape) needs exactly four things, and then every reader in
§1 works unchanged:

1. a `COURSE.idi` label entry → id (`src/core/specdb.c:22-35`);
2. a 22-byte `COURSE.dbt` row for that id, **inserted in id order** (`src/core/specdb.c:41-48`);
   a raw (uncompressed) overlay table is legal — `src/core/specdb.c:62-65`
   `if(!(r16(b+4)&1)) { ... memcpy(out,b+base+rowoff,width);return (int)width; }` — so the
   Huffman coder of the shipped table does **not** have to be reproduced to add a row;
3. the label in the script's course list (`course_carlist` via `US_carlist.adc`), plus a
   `CourseData[<label>]` entry if the menu needs name/logo/thumbnail/`has_reverse`
   (`src/core/menu.c:2617-2622`);
4. `/crs/<label>` (RNW4 road + world + the pregrid/camera blocks `course.c` / `course_pregrid.c`
   read) and `/crs/<label>.ad`.

The one thing that must be *decided* rather than wired is #2 in §2: an added unlocked course
consumes one of the remaining 18 keys of the 128-key profile table.

## 4. UNKNOWNS

- **`EntryMax` = byte 17 of the 22-byte row is the code's claim, not an independent decode.**
  `src/core/menu.c:2586` says so; I did not Huffman-decode a row to verify it, so the *values*
  in that column are UNKNOWN.
- **Whether the 110 ids are the US-schema only.** `docs/COURSE_CATALOG.md:23` states
  "The bridge is specific to US2560; other disc schemas are not claimed to work." Other regions
  are UNKNOWN.
- **Whether 568 is the shipped GT4 save's course-record count or a host-chosen blob size** —
  `src/core/save_ps2.c:85` is the only evidence I have.
- **Where `m->project`'s `CourseData` comes from** (`src/core/menu.c:2617` reads it, and I did not
  trace its loader) — so whether a new course's name/thumbnail metadata can be injected without
  touching a project `.ad` is UNKNOWN.
- **No build, no run, no save round-trip was performed.** This is a read-only survey; LAW 4
  ("a commit is not proof") still applies to every step above.
- The `keys[128]` table stores *unlocked* keys, and `tests/gtmode/purchase_menu.c:557` asserts
  `unlocked>0 && unlocked<128` — the real unlocked count on a full save was not measured here.
