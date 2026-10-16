# Translating a car into GT4 — the three questions, answered from the tree

*Written 2026-10-16 for the captain's test-car plan: a brand-new car (his model, his icon, our manifest).
Every claim carries the file:line it came from. Nothing here was guessed.*

---

## 1. THE MESH — what GT4 actually reads

| what | where it is |
|---|---|
| the file | `/car/<menu\|lod\|open>/<model>` **inside `GT4.VOL`** (`car_model.c:198`; the directory comes from `GENERIC_CAR` row +0x14) |
| the container | **`CAR4`** (base at +0x18, patch at +0x1C) wrapping an **`MDLS`** model set (`model.c:650-653`) |
| the mesh | not vertex buffers — an **opcode stream** (`pglPushMatrix`, translate/scale/rotate, shape and skin packets) read by `commands()` (`model.c:158-392`) |
| the tags | every triangle carries an **attachment id**: 0 body, 1-4 wheels, 5+ steering (`model.h:17`). A mesh without them renders with dead wheels, and there is no fallback mesh |
| the textures | inside the same file, **`Tex1`** containers (`tex1.c:129`): PS2 VRAM layouts (`PSMCT32/24`, `PSMT8/4`) with page/block/column swizzle and CLUTs, mips from `MIPTBP` (`tex1.c:19-248`) |
| the units | car models are **1/4096** (`model.c:12`) |
| the paints | the `Pat0` table: race models ship `CAR4 +0x1C` = 0 and the `.pat` file is appended and patched in `car_model.c:199-208`. Menu models carry their colours in `VARIATIONamerican` rows |

**The reader exists in full** (`model.c`, `tex1.c`) — the game draws cars from these bytes. **No writer exists**, which
is exactly the translation job: our mesh → a `CAR4`/`MDLS` opcode stream with correct attachment tags and a
1/4096 scale, plus `Tex1` textures in GS-legal layouts. The honest first proof is a **round trip**: decode one
disc car, re-encode it, and check the re-encoded file decodes to the same triangles and the same VRAM image.

## 2. SOUND — how a car makes noise (measured end to end)

The engine sound of a car is **one number in its own spec row**, and everything after that is the SPU2:

1. `race_audio.c:352-356` — `ENGINE` row → **`soundNum` = the u16 at row +0x18** (`row[24] | row[25]<<8`);
   the variant comes from the `MUFFLER` row (+6 → `normal0..3`) and the `TURBINE` row (+9 → `+4` = `turbo0..3`).
2. `race_sound.c:56-64` — the bank file is **`/sound/engine/%05u`**, or `/sound/normal0..3/%05u` /
   `/sound/turbo0..3/%05u` when a variant applies. Read off the user's disc, never bundled.
3. Per frame: rpm → layer and pitch (`race_sound.c:89-101`) → **the game's own driver writes SPU2 registers**
   (`sound_driver.c:209-306`) → **SPU2 voices with ADPCM + ADSR + reverb are rendered** (`spu.c:51-267`).
4. The engine says it out loud on every race: `race audio: car N engine=NNNNN variant=N layers=…`
   (`race_audio.c:361`).

**So for a new car:** it inherits the donor car's engine sound until we say otherwise, because the row is what
names it. Two options, in order of cost: **(a)** point the new car's `ENGINE` row at any existing bank id (a
one-row edit — a Kia can sound like whatever we choose today), **(b)** author a bank at a new
`/sound/engine/<id>` when we have a bank writer (the format is the one `gt4_engine_bank_decode` already reads:
ADPCM layers + a loop/plateau table).

## 3. THE NUMBERS — where horsepower, weight and physics come from

The car's physics is **assembled from rows**, not from one blob. The row source is
`gt4_car_record_rows` (func_003F3B68, `record.c:634-696`); `record.c:647-665` shows the assembly:
`ROW(0x38,0x50) → engine()`, `ROW(0xb0) → turbo()`, `ROW_IF(0xd0) → power_parts()`, tyres, suspension, chassis.

The tables that matter for a car, out of the **43** the engine opens (`spectables.c:11-19`):

`GENERIC_CAR` · `CHASSIS` (wheelbase, at +4) · `ENGINE` (torque curve **and the sound id**) · `DRIVETRAIN` ·
`GEAR` · `SUSPENSION` · `BRAKE` · `BRAKECONTROLLER` · `LIGHTWEIGHT` · `TIRESIZE` · `TIRECOMPOUND` ·
`TIREFORCEVOL` · `FRONTTIRE` / `REARTIRE` · `DEFAULT_PARTS` (the parts bundle: chassis/suspension/tires/racing
modify ids, 208-byte row) · `DEFAULT_PARAM` · `RACINGMODIFY` (tread) · `WHEEL` · `WING` · `LSD` · `CLUTCH` ·
`FLYWHEEL` · `NITROUS` and the tuning rows (`NATUNE`, `TURBINEKIT`, `PORTPOLISH`, `ENGINEBALANCE`,
`DISPLACEMENT`, `COMPUTER`, `INTERCOOLER`, `MUFFLER`, `SUPERCHARGER`).

Those rows land in `gt4_car_params` (`car.h:199-240`: mass, engine, drive, tyres, drag, downforce) through
`gt4_race_init` → `gt4_car_record` → `gt4_car_params` (`init.c:1052-1069`, `:559`). The simulation itself is
**float, deterministic, fixed 1/60 s** with round-toward-zero and flush-to-zero each frame
(`curve.c:19-25`, `race_run.c:753-779`) — so a new car's numbers have to be in the same units as the disc's or
it will drive like nothing else in the game.

**Practical shape of the test car:** today a mod can already give an added car its own rows (our merge does
it) — so "500 hp, 1200 kg, 6 gears" is a **new `ENGINE` row (torque curve) + `GEAR` row + the mass in the
params row**, all of which we can write the same way `moonmod_addcar` writes `GENERIC_CAR`. What we cannot yet
do is *author* a curve from scratch with confidence — the honest first step is a **donor row plus edits**, and
say which numbers changed.

---

## WHAT THE TEST CAR NEEDS FROM THE CAPTAIN

| his part | what I do with it |
|---|---|
| the model (obj/gltf/kn5/fbx — anything) | convert to `CAR4`/`MDLS` with attachment tags; wheel/steering parts named or picked |
| the textures (PNG is ideal) | convert to `Tex1` (GS-legal PSM + swizzle + CLUT) |
| an icon image | **open question, measured next:** the disc has only `/car/menu|lod|open` — no icon folder. Where GT4 gets the small car picture in a dealer list has not been found yet, so I will not pretend the PNG has a home until I find it |
| the manifest (name, brand, price, description) | already supported: `mod.txt` (`docs/vulcan4/MOD-MANIFEST.md`) |

## OPEN QUESTIONS, NAMED

1. Which file, if any, holds a car's 2D icon in GT4's own dealer screens (the disc's `/car` has no icon tree).
2. Whether `CAR4`+`MDLS` can be written legally by us at all, or whether the mesh must go in as a *patched*
   existing model — the round trip in §1 answers this and nothing else does.
3. Which row carries a car's **mass** exactly (the params row is built in `init.c`, the source field is in the
   43 tables — to be read, not guessed).
