# MOD FORMATS — the layer cake, and what "easier to load" concretely means

**Assembled 2026-10-15** from five engine-side surveys + three community surveys, with Caine's own
re-measurements on the user's disc. Detail lives in `docs/vulcan4/research/format-*.md` and
`web-*.md`; this file is the map, and it marks what was **verified here** versus **reported**.

---

## 1. THE LAYER CAKE

```
disc.iso ─► GT4.VOL (RoFS) ─┬─ /car/{menu,lod,open}/<model>   CAR4 ─► MDLS ─► VM program ─► triangles + Tex1
                            ├─ /crs/<label>                   course container (MDLS + RNW4 + Vls0 + Mot1…)
                            ├─ /crs/<label>.ad                ADLN: the AI lines  ← REQUIRED, see §3
                            └─ /specdb/GT4_US2560/*.dbt/.idi  the tables (cars, courses, events)
```

Everything is read through **`gt4_vol_read`** (`vol.c:340`) and **nothing is extracted** — the reader seeks
into the image (`vol.c:45-48`). That is the seam the mod plan hangs on (`MODS-DESIGN.md`).

## 2. CARS

- **`CAR4`** is the car bundle; its `+0x18` points at an **`MDLS`** model set. `MDLS` carries the shape table,
  materials, texture lists, and — per model — a **command stream** (`docs/vulcan4/research/format-car.md`).
- **The model file is a program, not a mesh.** `model.c` executes a **256-opcode command VM**; draw opcodes
  4/5 index the shape table, and the **attachment tags** (`model.h:17`: 0 body, 1–4 wheels, 5+ steering) are
  produced by the *callbacks the program invokes* (`model.c:306`).
- **Strict, and all-or-nothing:** an unknown opcode, an out-of-range table, an oversized vertex block or a bad
  scale fails the **whole model to NULL** — there is no partial load (`model.c:381-384`, `:407`, `:449`).
  Worse, an *unrecognised vertex address* silently consumes its payload and leaves **zeroed vertices**, and
  `nr<2`/`nv<3` silently yields **no triangles** — a hole, not an error.
- **Wheels:** missing or mis-tagged parts are NULL, and the showroom then fails outright —
  `car_model.c:272` *"showroom: empty car"*; wrong tags drop body panels into wheel hubs (`car_model.c:74`).
- **Minimum viable mesh** (full list in `format-mesh-contract.md`): CAR4/MDLS, models 0/1/2, one opaque shape,
  a `0xc040` reset list with `num ≥ 2`, V3-16 or float positions, a command stream ending in opcode 0 within
  8192 ops, plus body and callback-6/11..14 wheels to actually drive.
- **Who can author it today?** Nobody. The community's own tooling says so out loud
  (`GTPS2ModelTool`: *"THIS IS NOT USABLE FOR REBUILDING CARS"*), and the mod author agrees
  (*"we can't make new cars yet"*).

## 3. COURSES — and the AI line, which is **the `.ad`**

- **The container**: header of 32-bit offsets — `+0x04` `MDLS` world set, `+0xDC` `RNW4` runway, `+0x08/0C/10`
  `Vls0` vision, `+0xB0/0xC0` `REP4`, `+0xBC` `GDGT`, `+0xD0/D4` `Mot1` (`course.c:13-17`). `RNW4` holds
  length, grid count, road vertices/triangles and a variant table; `Vls0` is the culling grid and a malformed
  one kills the render load (`race_view.c:314`).
- **The AI line is NOT here.** It is the **`.ad` companion file**, and the engine's own loader proves the
  linkage: `race_run.c:219-222` reads both files, `:223` **requires** the `.ad` (`an < 24` → *"cannot read
  course"*), and `:242-245` takes the **five block offsets from `adf+4+k*4`** and writes them to
  `COURSE+0x9c+k*4`. **A course without its `.ad` does not race at all.**
- **Verified layout (Caine, all 110 courses on the retail disc, 0 mismatches):** block 0 at `0x80` (the line
  payload), blocks **1–4 exactly 0x80 each**, back to back, file ends at `block4 + 0x80`. **93** courses carry
  `ADLN` (8 lines, stride `0x40`); **17** carry the legacy fixed-point form (stride `0x28`, 2⁻¹² scale).
  Measured files: `yosemite.ad` 16,128 B, `akasaka.ad` 18,688 B, `apricot.ad` 15,744 B.
- **Which line is which** (community + engine trace): line 4 = **pit lane + start grid**, 6 = rolling/middle
  start, 1/2 = left/right lanes, 5 = per-race cleared line. Height, pitch, roll and surface are **computed
  at load**, 0 on disc. Checkpoints, sectors and the grid are in the **container** (`RNW4`), not the `.ad`.
- **Consequence for custom maps:** a track is **two artifacts**, and the honest wording is now *"author a
  course container AND an `.ad`"* — not *"find the AI line"*. No writer for either exists publicly.

## 4. TEXTURES

- **`Tex1`** = a 4 MB VRAM snapshot plus descriptors (`tex1.c:131-134`), formats `PSMCT32/24`, `PSMT8/PSMT4`
  (`tex1.c:19`), CLUT with the CSM1 index-bit swap, PS2 alpha `0x80` = opaque, up to 6 mips.
- **Car textures are embedded inside the `MDLS`** (`model.c:668-676`); courses embed them per model set and
  add two standalone `Tex1` blobs (smoke at header `+0x68`, particles at `+0x9C`).
- **The best news in this whole document:** the engine ingests **plain, unswizzled RGBA** — the writer's
  minimum is one `PSMCT32` transfer, row-major RGBA8888, `CPSM=0`, `MXL=0`. **We do not have to reproduce
  GS swizzle on the authoring side.**

## 5. SO WHAT DOES "EASIER TO LOAD" MEAN, CONCRETELY?

Three converters and one validator, in this order:

1. **mesh → CAR4/MDLS command program** (the hard one — the program *is* the format),
2. **image → Tex1** (the easy one — plain RGBA, per §4),
3. **path list → `.ad`** (the new one — lines + the four 0x80 blocks, layout now measured),
4. **the validator** — because the engine fails *silently* on unrecognised vertex data and give a
   **wheel-less** car without the right tags, the SDK's job (`G6.35`) is to catch all of it before the game
   does.

## 6. VERIFIED BY CAINE HERE vs REPORTED BY THE SURVEYS

| claim | status |
|---|---|
| 110 `.ad` files, block layout, `ADLN` in 93, 17 legacy, 0 mismatches | **verified** (own script, this disc) |
| `.ad` required or the course never loads (`race_run.c:223`) | **verified** (source read) |
| five block offsets at `adf+4+k*4` → `COURSE+0x9c+k*4` | **verified** (source read) |
| the file is a 256-opcode VM program; tags from callbacks | **verified** (source read) `model.c:158`, `:306` |
| `CAR4`/`MDLS` field offsets, VIF packet forms, minimum mesh | reported with `path:line`; not re-measured line by line |
| `Tex1` header/format details; "engine takes plain RGBA" | reported; consistent with `tex1.c:19,131-134` — not round-trip tested |
| community tool inventory, Spec II as shipped, quoted blockers | reported from primary sources with URLs (`web-*.md`) |

## 7. STILL UNKNOWN

- Which of the **8 lines** each AI class/difficulty actually uses (needs guest-RAM tables from the PS2 image).
- `.ad` **blocks 1–4** — their functions are not established.
- Whether an **`.ad` can be authored** at all (no writer, no published field spec beyond the class names).
- `EntryMax` at byte 17 of the 22-byte `COURSE` row: the code's claim, not independently decoded.
- Whether our **"fixed-size tables"** hypothesis is true — the community writes tables (GTSpecDB) and their
  stated wall is the asset side, so this stays our hypothesis, not their finding.
