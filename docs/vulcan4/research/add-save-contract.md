# The add-a-car / add-a-course save contract (VULCAN 4)

*Task: what it takes for a NEW car id and a NEW course id to be ADDITIVE and SAVE-SAFE — the contract
that must hold so an addition never corrupts an existing save. Read-only survey of the tree; every claim
below is a `path:line` with the quoted line. Anything I could not measure is in UNKNOWNS, not guessed.*

Repo state: `/home/or/vulcan4` @ `7ad1f1f` (branch `main`).
Scope of the evidence: `src/core/save_state.c`, `src/core/save_ps2.c`, `src/core/save.h`,
`src/core/physics/garage.c`, `src/core/career_original.{c,h}`, `src/core/career_vehicle.c`,
`src/core/career_admission.c`, `src/core/career_event.c`, `src/core/menu.c`, `src/core/spectables.{c,h}`,
`src/core/specdb.h`, `src/core/career_home.c`, `src/core/physics/race_run.h`.

**Confidence framing up front, because it decides how to read this:** every fact below is *static* —
read from the fork's own C. Nothing here was verified by round-tripping a real save, because the game is
not playable on this box yet (`AGENTS.md`: *E2 playable — ⏳*). The claims are about *the code's own
contract*; they are exactly as strong as the code is a faithful port, no stronger.

---

## (a) How a save identifies a car today

### The id is a 64-bit SpecDB key: `table << 32 | row`

```
src/core/physics/garage.c:20|static uint64_t key_at(uint32_t gar, uint32_t off)
src/core/physics/garage.c:22|    return (uint64_t)GT4_U32(gar + off) | (uint64_t)GT4_U32(gar + off + 4) << 32;
src/core/physics/garage.c:24|static void set_key(uint32_t gar, uint32_t off, uint64_t key)
src/core/physics/garage.c:26|    GT4_U32(gar + off) = (uint32_t)key;
src/core/physics/garage.c:27|    GT4_U32(gar + off + 4) = (uint32_t)(key >> 32);
```

A car is therefore **32-bit row id in the low half, 32-bit table index in the high half**, stored as two
little-endian u32s. The table index is not free-form:

```
src/core/physics/garage.c:17|#define SPEC_TABLES      0x2bu
src/core/physics/garage.c:33|    uint32_t table = (uint32_t)(key >> 32);
src/core/physics/garage.c:34|    return table < SPEC_TABLES && db->row(db->ctx, table, (uint32_t)key, 0, NULL, 0) > 0;
src/core/spectables.h:8|#define GT4_SPEC_TABLES 0x2b
src/core/career_vehicle.c:113|    if(!db || !oil || color<0 || (key>>32)!=0)return 0;
```

`(key>>32)!=0` — a car built through the vehicle factory **must** live in table 0, `GENERIC_CAR`
(`src/core/spectables.c:12|    "GENERIC_CAR", "BRAKE", "BRAKECONTROLLER", "SUSPENSION", ...` — table 0 is the first name). 0x2b is the
table count; a table index ≥ 0x2b is "unknown", not "reserved".

### Where the garage list lives

Two objects, one native and one PS2-shaped, and they agree on the slot geometry.

```
src/core/career_original.h:11|#define GT4_CAREER_GARAGE_CAPACITY 1000u
src/core/career_original.c:148|    for(unsigned i=0;i<GT4_CAREER_GARAGE_CAPACITY;i++)if(!(garage[0x10+i*0x20]&1))return (int)i;
src/core/career_original.h:31|/* Valid index [0,999]; garage contains the original 0x81f8-byte object.
src/core/career_original.h:32|   Full live car is 0x4c0 bytes, including its eight-byte class header. */
src/core/career_original.c:189|    for(unsigned i=0;i<0x4c0;i++)garage[0x7d08+i]=car[i];
src/core/career_original.c:268|/* func_0043AFB0: profile+0xb8e8 is the garage object, getter then setter. */
```

The garage is the original **0x81f8-byte object living inside the profile at `+0xb8e8`**: a 1000-slot
table of 0x20-byte units (flags byte at `0x10 + i*0x20`, bit 0 = occupied), the "riding" (currently
driven) 0x4c0-byte car at `+0x7d08`, the selected index at `+0x81c8`, the XOR'd money word at `+0x81e8`
(`src/core/career_original.c:255-263`), a dirty flag at `+0x81f4`. Car keys inside a unit are written by
the factory as:

```
src/core/career_vehicle.c:134|    flags|=1u|(((uint64_t)(uint32_t)color&127u)<<2);w64(unit,key);w64(unit+8,flags);
src/core/physics/garage.c:204|    set_key(gar, 0, car);
```

So a unit is `{ key: u64 @+0, flags: u64 @+8 (bit0 occupied, bits 2..8 = paint index), ... }` and **the
car id is 8 bytes at unit offset 0**. The user's own PS2 save carries the same geometry, byte-for-byte
and fixed-size:

```
src/core/save_ps2.c:83|...ah_obj *garage=module(vm,game,"garage"),*cars=array(vm,garage,"cars");size_t units=at;blob(vm,garage,"original_units_ps2",p+units,32000);
src/core/save_ps2.c:83|...for(unsigned i=0;i<1000;i++)ah_array_push(vm,unit_history,ah_int((int64_t)r64(p+units+i*32+24)));
```

32000 bytes = 1000 × 32 — the PS2 garage blob is a fixed 1000-unit table read at a fixed offset.
The serialized profile is fixed-size too:

```
src/core/save_ps2.c:43| if(!valid || r32(p+8)!=0xff60bad3u){free(p);return 0;} ... || (n!=0x3a070 && n!=0x3a160) || ...
```

**The decoded GT4 save is only accepted if its size is exactly 0x3a070 (offline) or 0x3a160 (online).**
That single line is the reason the PS2 container is not extensible: add one byte and every existing save
fails the check.

### What the game does with a car id it does not know

It fails *closed*, and it fails *quietly* — measured at every layer the id passes through:

```
src/core/physics/garage.c:72|    if (!exists(db, key)) return 0;   (part_row)
src/core/physics/garage.c:82|    if (!part_row(db, gar, 0x48, r)) return 0;   (defaults(): the chain stops at the first miss)
src/core/physics/garage.c:200|    memcpy(&key, row, 8);
src/core/physics/garage.c:201|    if (key == ~(uint64_t)0 || !exists(db, key)) return 0;   (gt4_garage_new)
src/core/career_vehicle.c:114|    uint8_t generic[24];if(db->row(db->ctx,0,(uint32_t)key,0,generic,sizeof generic)!=24)return 0;
src/core/physics/race_run.h:196|/* keys: GENERIC_CAR ids (or table << 32 | id); a car the SpecDB cannot
src/core/physics/race_run.h:197|   make rates 0. 0 when the disc could not be read. */
src/core/career_admission.c:40|    /* 001A2BB0 +0x54/+0x70/+0x160 walks all 60 qwords. -1 and failed
src/core/career_admission.c:41|       resolutions are skipped, not end-of-list markers. */
src/core/career_admission.c:47|        if(label && strlen(label)<128)strcpy(labels[(*count)++],label);
src/core/menu.c:3608|       gt4_specdb_row(m->car_db,m->car_db_size,id,row,sizeof row)!=24)return ah_error(vm,"unknown car %s",code);
```

Summary of the unknown-id behaviour: garage entry construction returns 0 (the car cannot be built), a
missing part row stops the defaults chain, the Arcade opponent rating silently becomes **0**, event
eligibility skips the car rather than terminating the list, the menu raises `"unknown car %s"`. **Nothing
in that list rewrites the key it could not resolve.** That property is the whole foundation of this
document — and it is a property of *today's* code, which we must not break.

Also: a car key with a non-zero table high half is only reachable for opponents
(`src/core/physics/race_run.h:28|    uint64_t tuned_car[6]; /* optional ENEMY_CARS key (namespace 0x26) */`,
`src/core/career_admission.c:46|        else if((car>>32)==0x26)label=gt4_specdb_label_for_id(enemy,enemy_size,(uint32_t)car);`).

---

## (b) How a save identifies a course / event

A course is the **same 64-bit key shape**, forced into namespace 0x22:

```
src/core/menu.c:2592|static int course_key(menu *m,const char *label,uint64_t *key){
src/core/menu.c:2593|    if(!label)return 0;course_capacity(m,label);uint32_t id;
src/core/menu.c:2594|    if(!gt4_specdb_label(m->course_labels,m->course_labels_size,label,&id))return 0;
src/core/menu.c:2595|    /* 00447C78 +0x14: COURSE namespace 0x22. */
src/core/menu.c:2596|    *key=UINT64_C(0x2200000000)|id;return 1;
```

`0x2200000000` is `0x22 << 32`: **course table = 0x22, course row id = a u32** resolved from the label
through `COURSE.idi` (`src/core/career_event.c:50|    courses=gt4_vol_read(vol,"/specdb/GT4_US2560/COURSE.idi",&courses_size);`).

The save-side course list is a **sorted array of 128 keys**, and it stores both halves explicitly:

```
src/core/menu.c:2601|    for(unsigned i=0;i<128;i++){ah_obj *entry=ah_module_new(vm,"course",NULL,NULL,NULL);ah_set_member(vm,entry,"code",ah_int((int32_t)keys[i]));ah_set_member(vm,entry,"table",ah_int((int32_t)(keys[i]>>32)));ah_array_push(vm,courses,ah_objv(entry));}
src/core/menu.c:2602|    ah_set_member(vm,available,"courses",ah_objv(courses));
src/core/menu.c:2604|static int course_saved(menu *m,ah_obj *available,uint64_t keys[128]){
src/core/menu.c:2609|            gt4_career_course_unlock(keys,(uint32_t)code.u.i|((uint64_t)(uint32_t)table.u.i<<32));
src/core/career_home.c:64|void gt4_career_course_unlock(uint64_t courses[128],uint64_t key){unsigned i=0;while(i<128 && courses[i]<key)i++;if(i==128 || courses[i]==key)return;memmove(courses+i+1,courses+i,(127-i)*8);courses[i]=key;}
```

and the PS2 layout agrees: 128 course keys right after 1024 car keys.

```
src/core/save_ps2.c:86| ah_obj *available=module(vm,game,"available");codes(vm,available,"cars",p+at,1024);at+=8192;codes(vm,available,"courses",p+at,128);/* 0042EB20: serialized availability override after 128 keys. */
src/core/save_ps2.c:75|static void codes(ah_vm *vm,ah_obj *o,const char *key,const uint8_t *p,unsigned count){...integer(vm,entry,"code",(int32_t)r32(p));integer(vm,entry,"table",(int32_t)r32(p+4));...}
```

**Inside an event (the RACE row), the course is a 16-bit id at offset 0:**

```
src/core/career_event.c:55|    /* 00447550: COURSE key is RACE's leading unsigned short.
src/core/career_event.c:57|    const char *course=gt4_specdb_label_for_id(courses,courses_size,
src/core/career_event.c:117|        const char *course=gt4_specdb_label_for_id(courses,nc,event->race_row[0]|(uint32_t)event->race_row[1]<<8);
```

At runtime a course is a `/crs` directory name, not an id:

```
src/core/physics/race_run.h:16|    char course[32];      /* a /crs name, e.g. "tsukuba" */
```

Unknown course behaves like an unknown car — refused, not substituted:

```
src/core/menu.c:2629|    *ret=ah_bool(0);if(!argc || !course_key(m,ah_cstr(argv[0]),&key))return 0;
src/core/menu.c:2630|    if(!course_saved(m,available,keys))return ah_error(vm,"original course availability unavailable");
```

Also relevant, and *good* news: the PS2 save already carries a per-list `max`/`count` for favourites —
i.e. the game's own format has a notion of "N slots, this many used":

```
src/core/save_ps2.c:87| const char *fav[]={"favorite_course","favorite_car"};for(int i=0;i<2;i++){ah_obj *f=module(vm,game,fav[i]);integer(vm,f,"max",r32(p+at));integer(vm,f,"count",r32(p+at+4));codes(vm,f,"codes",p+at+8,32);at=align16(at+264);}
```

---

## (c) THE ADDITIVE-ID CONTRACT

Hard rules. Each one is *forced* by a line above; break it and the failure mode is named.

**R1 — Allocate a fresh id; never reuse, never renumber, never delete.**
A car id (`(0<<32)|id`, GENERIC_CAR) and a course id (`0x22<<32|id`, COURSE) must be ids that no shipped
disc and no earlier build of ours has ever handed out. A save holds only the number
(`src/core/physics/garage.c:204`), so reuse silently re-points an owned car's whole identity — specs,
model, parts, paint — at whatever now answers that id. *Corrupts by identity aliasing, with no error
raised anywhere.* This is the WISHLIST's own wording, and it is right:
`docs/vulcan4/WISHLIST.md:60|  **Honest limits:** nothing here starts before the game runs; new car IDs must be *additive* (never reuse an existing ID — saves refer to them).`
The only safe allocation is "highest used id in that table + 1", stored in our overlay, and never
compact it afterwards.

**R2 — The new id must resolve through the loader the garage consults, and the resolution must be
stable across builds.**
`exists()` (`src/core/physics/garage.c:31-35`) and the vehicle factory (`src/core/career_vehicle.c:114`)
ask the running SpecDB, not the save. So the id is only meaningful *together with* the table that
answers for it. Two consequences:
- the same id must never be answered by two different rows in two builds (no "temporary" cars on an id
  we might repurpose later);
- if the overlay is missing at load time, the id must remain *unresolvable* — which today's code already
  handles non-destructively (early-return 0, rating 0, skip). **Never "repair", renumber, or clear a key
  the SpecDB cannot resolve.** This is the interaction with `AGENTS.md` LAW 12 (everything off by
  default): a save written with custom cars ON must survive being loaded with them OFF, and the current
  code path is exactly that survival — a garage slot holding an unbuildable key. What it must *not* do is
  pass that key to a "fixup" that writes something else into the slot.

**R3 — A new course id must fit in 16 bits.**
`src/core/career_event.c:117` reads the course out of a RACE row as
`event->race_row[0]|(uint32_t)event->race_row[1]<<8`. A course whose id > 0xFFFF can never be referenced
by a career event, licence test or championship round on this format. (It can still exist as a course and
be unlocked in `available.courses`, whose 128-slot array is 64-bit — but it would be selectable only
through paths that carry the full key.) Also note `src/core/menu.c:2596(|*key=UINT64_C(0x2200000000)|id;)`
— the `id` is `uint32_t`, so anything above 32 bits is already lost at `course_key`.

**R4 — Reserve both halves of a new course: the forward id and its `r_` reverse id.**
```
src/core/menu.c:2621|            gt4_career_course_unlock(keys,key);
src/core/menu.c:2622|            if(ah_get_member(vm,info.u.o,"has_reverse",&reverse) && ah_truthy(reverse)){char name[128];snprintf(name,sizeof name,"r_%s",label);if(course_key(m,name,&key))gt4_career_course_unlock(keys,key);}
```
A course with `has_reverse` consumes **two** ids. Allocate them as a pair or the reverse layout resolves
to someone else's id.

**R5 — Do not change any fixed count or offset in the persisted layout.** These are the numbers that are
load-bearing today; none of them may grow:

| Structure | Fixed | Evidence |
|---|---|---|
| PS2 decoded save size | exactly `0x3a070` / `0x3a160` | `src/core/save_ps2.c:43` |
| Garage units | 1000 × 32 B = 32000, inside 0x81f8 | `src/core/save_ps2.c:83`, `src/core/career_original.h:11,31` |
| Car availability keys | 1024 × 8 B (fixed `each` 8192) | `src/core/save_ps2.c:86` |
| Course availability keys | 128 × 8 B | `src/core/save_ps2.c:86`, `src/core/menu.c:2601` |
| Favourites | 32 codes + max/count, 264 B each | `src/core/save_ps2.c:87` |
| Race records | 768 × 4 B; licence 125 × 10 × 44 B; course-record blob 132×568 | `src/core/save_ps2.c:84,85,86` |
| Event eligibility row | 60 qwords (480 B), `-1` skipped | `src/core/career_admission.c:44-47` |

The 128-course and 1024-car availability arrays are the real ceilings on *unlocks*, not on *additions*:
one key per course, sorted, deduplicated (`src/core/career_home.c:64`). Adding a 129th course is a
distinct problem from adding a car, and it is a *save* problem.

**R6 — Any new persisted state must live inside an existing save root.**
The native save is a generic typed VM serializer with a closed allow-list of root names:

```
src/core/save_state.c:7|static const char *const roots[]={"username","money","option","garage","calendar","race_record","license_record","course_record","machine_record","favorite_car","favorite_course","presents","championship","used_car","statistics","available","major_menu_page","major_menu_widget","ps2_import"};
src/core/save_state.c:39|...if(!known)return 0;   (valid_roots: an unknown root rejects the whole save)
```

A new top-level name (say `custom_cars`) makes `valid_roots` return 0 for **every** save — new saves
would be rejected by an older build, and by any reader that has not been taught the name. New
persistent state therefore goes *inside* one of the 19 roots (e.g. under `garage` or `available`), where
the merge is additive by construction:

```
src/core/save_state.c:36|static void merge(ah_vm *vm,ah_obj *dst,ah_obj *src,int depth){...
src/core/save_state.c:38| else if(!exists || data_value(old))ah_set_member(vm,dst,key,v);}}
```

**The one place an old save can bite a new build** is the replace path, which *prunes*:

```
src/core/save_state.c:44|static void clear_missing(...){... else if(!exists)ah_remove_member(vm,dst,key);}
src/core/save_state.c:45|... ah_get_member(vm,src,roots[j],&v);clear_missing(vm,old.u.o,ah_as_obj(v,AO_MODULE),0);
```

Loading an old save removes live members that the save does not contain. Anything we add *natively under a
root* and expect to persist will be deleted on the next load of an older save, unless it is written into
the save too. Additive-by-merge does not mean immortal; the write side has to carry it.

### What breaks if we get it wrong

| Mistake | Failure | Which line catches it (or doesn't) |
|---|---|---|
| Reuse a car id | Owned cars silently become the new car; parts/paint follow the new row | **none** — no check exists; `src/core/physics/garage.c:204` just reads the number |
| Reuse a course id | Unlocked-course flag aliases two circuits; a record's key names the wrong track | **none** |
| Move a fixed offset / grow the PS2 layout | Every existing PS2 save refused as damaged | `src/core/save_ps2.c:43` — correctly refuses, wrongly blames the user's save |
| Add a new save root name | Whole save rejected by an older/newer reader | `src/core/save_state.c:39` |
| Id ≥ 0x10000 for a new course | Course exists but no event/licence can name it | `src/core/career_event.c:117` truncation |
| Add a "fixup" that rewrites unknown keys | The only actual corruption path for a save that holds an id we can't resolve | would have to be written by us — today's code returns 0 instead everywhere |
| Add >128 unlocked courses | Overflow of the sorted array, silent structural damage | `src/core/career_home.c:64` — `if(i==128 || courses[i]==key)return;` **drops the key silently** |

That last one deserves emphasis: `gt4_career_course_unlock` **returns without inserting** when the array
is full. A 129th course would appear unlocked in the current session and be gone after a reload, with no
error. That is the exact failure shape this project's LAW 2/3 exist to prevent.

---

## (d) The cheapest additive representation the current structures allow

**Cars: a row-source wrapper. Zero bytes in any save format.**

The garage never reads the disc — it reads through a function pointer:

```
src/core/physics/garage.c:34|    return table < SPEC_TABLES && db->row(db->ctx, table, (uint32_t)key, 0, NULL, 0) > 0;
src/core/physics/garage.c:39|    return db->row(db->ctx, (uint32_t)(key >> 32), (uint32_t)key, 0, buf, cap);
src/core/career_vehicle.c:188|    v->db=(gt4_specdb_src){gt4_spec_tables_row,v->tables,gt4_spec_tables_str,gt4_spec_tables_car_parts};return v;
src/core/spectables.c:67|int gt4_spec_tables_row(void *ctx, uint32_t table, uint32_t id, uint32_t nth, uint8_t *out, size_t cap)
src/core/spectables.c:71|    return d ? gt4_specdb_row_nth(d, size, id, nth, out, cap) : 0;
```

`gt4_specdb_src.row` is a virtual row source. The cheapest additive car is therefore: **our own row
source that answers for the new id and defers to `gt4_spec_tables_row` for everything else** — one new
24-byte GENERIC_CAR row plus the `DEFAULT_PARTS` / `CAR_VARIATION` rows it points at, in our overlay,
behind the same vtable. No save change at all: the 1000-slot table already exists, the key is already 64
bits, and an id nobody can resolve already fails closed. The row widths are load-bearing and measured:
GENERIC_CAR = 24 bytes (`src/core/career_vehicle.c:114`), the ALLOW_ENTRY row = 480
(`src/core/career_admission.c:36|    int ok=db && gt4_specdb_row(db,db_size,(uint32_t)key,allowed,sizeof allowed)==sizeof allowed;`),
the parts row = 0x100 (`src/core/physics/garage.c:73|    fetch(db, key, r, 0x100);`).

**Courses: the same trick on `COURSE.idi`, plus the front half of an event row.**

A new course needs a fresh u32 in the COURSE namespace answering to a label, a `/crs` directory name
(`src/core/physics/race_run.h:16`), and — to be selectable from career events — an id ≤ 0xFFFF
(`src/core/career_event.c:117`). Its unlock state costs one of the 128 slots, which is a *budget*, not a
format change: the array is already 128 × 64-bit, and the sorted insert already deduplicates.

**What I would explicitly not do** (ponytail read): no new save root, no version field, no migration
code, no widening of the key, no change to `save_ps2.c`. The structures already carry exactly the thing
needed — an opaque 64-bit id resolved through a swappable row source — and the whole safety story is
"fall back to *unresolvable*", which is already implemented. The only new code is the row source and the
overlay data.

---

## UNKNOWNS

Not measured here; marked so nobody reads them as fact.

1. **The disc's actual id ranges.** I did not open `GENERIC_CAR.dbt` / `COURSE.idi` (LAW 1, and the disc
   lives on the SSD). So "highest used car id + 1" is a *rule*, not a number. Nothing in this document
   depends on the value, but the allocation tool will.
2. **Whether any shipped save already holds an id above the current max in either table.** Only a real
   save + real tables answers this; I read code, not data.
3. **Runtime behaviour of the *original* PS2 game given a key it cannot resolve.** The fork's ported
   paths return 0 / rate 0 / skip; I did not verify those match the original's behaviour on hardware or
   in PCSX2. LAW 13 wants an oracle here before we lean on it.
4. **Whether `valid_roots`-passing saves exist in the wild with names not in the list.** Not measured.
5. **The garage capacity question** — whether GT4 refuses car 1001, and how, is not visible in the code I
   read (`gt4_career_garage_first_free` returns the first free index or 1000, and no caller was traced).
   `src/core/career_original.c:148` is the whole function.
6. **The `clear_missing` prune path was read, not executed.** The claim "loading an old save deletes
   natively-added members" is read off `src/core/save_state.c:44-45`; it has not been run.
7. **PS2 save *writing*** was not surveyed at all — `src/core/save_ps2.c` here is the read path
   (plus media). Whether we can write a GT4-shaped save, and what that implies for new ids, is open.
8. **Whether the `available.cars` 1024-key list or the 60-key event eligibility row binds before the
   garage's 1000 slots do.** Three different ceilings, none exercised.

*Written 2026-10-15. Read-only survey; no file under `src/` or `include/` was modified.*
