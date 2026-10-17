# THE SDK PACKAGE SCHEMA — draft 1, for agreement before code

**Started 2026-10-17.** Jonhline's issues **#7 (custom-car workflow)** and **#8 (custom-track workflow)** both
say the same thing in their test plan: *"Agree package schema and supported compatibility first."* This is that
document. Both interfaces named there — `interface:car-package`, `interface:content-catalog`,
`interface:course-package` — are defined here. Nothing in here is built yet; everything in here is a decision
the engine and the SDK must make the same way.

The SDK ships as **one tool with two faces**: a `moonmod` **CLI** (scriptable, what tests and the loop call)
and a **GUI app** that drives that same CLI. One implementation of every format, two front doors — the same
rule the engine already follows with its launcher and its game.

---

## 1. WHAT A PACKAGE IS

A package is **a folder** (optionally zipped) that sits in the player's own mod folder
(`%APPDATA%\MoonEngine\MoonEngine\mods\<name>` on Windows, `~/.local/share/MoonEngine/MoonEngine/mods/<name>`
on Linux). It holds:

```
<package>/
  mod.txt                     the manifest (below) -- the ONLY file the engine is told to read by name
  car/{menu,lod,open}/<model> authored geometry, one file per level of detail
  piece/car_mini/<label>.img  the small dealer picture
  piece/car_pict/<label>.img  the large picture
  specdb/<region>/<table>     table DELTAS: only the rows this package adds or overrides
```

Rules that are not negotiable, because the engine already works this way (measured):

- **The disc is never written.** Everything here is additive over `GT4.VOL` (`docs/vulcan4/MODS-DESIGN.md`).
- **Tables are deltas, not copies.** A package carries only its own rows; the merge decodes the disc's table,
  appends, and rebuilds a readable table (`src/core/mod_merge.c`, one summary line per table).
- **A row id that the disc already has is either identical (ignored) or an override (the package wins).**
  Silently keeping both is the bug we just removed.
- **The package is self-describing.** The engine cannot read intent out of merged bytes, so `mod.txt` says
  what the package *adds*: the Arcade list entry, the dealer entry, the course label.

## 2. `mod.txt` — the manifest (implemented 2026-10-16)

Plain text, one directive per line, `#` comments, `key=value` options after the value.
Detail: `docs/vulcan4/MOD-MANIFEST.md`. Read by the engine, the launcher's mod manager, and the SDK — one
source of truth, no second list to keep in sync.

```
# MOON ENGINE mod manifest
name        <human name>
version     1.0
author      <who made it>
description <one line, shown on the mod card>
image       card.png
price       500
car         <label> model=<model> maker=<maker> nation=<US|JP|EU> [power=<hp>] [sound=<donor label>]
course      <label> [name=<human name>]
```

`car` and `course` are the *catalog* declarations: they are what the engine appends to the game's own lists.
A package with no `car`/`course` line adds no content — it may still override files.

## 3. `interface:car-package`

### 3.1 The three mandatory model files

| path | what it is |
|---|---|
| `car/menu/<model>` | the showroom/dealer model (the one the player looks at when buying) |
| `car/lod/<model>` | the driving model |
| `car/open/<model>` | the far/low level model (drawn at a distance) |

`<model>` is **a new string**, authored by the package — e.g. `moonbanana`. It must **not** be a model name
that already exists on the disc: pointing a package at an existing name (we did it once with `fdeu0006`,
the Ford Ka) turns that car into the package's car for every list that references it. Measured, and the reason
`--model-name` exists in `tools/c/moonmod_addcar.c`.

Resolved by the engine as: `GENERIC_CAR.idi` label → id → `CAR_VARIATION_american.dbt` row → variation id →
`VARIATIONamerican.dbt` row (+0 = string index) → `american_StrDB.sdb` → the model name → the path above.

### 3.2 The tables a car package writes (all additive)

| table | the row it adds | why |
|---|---|---|
| `GENERIC_CAR.idi` | label → new id | the car's name in the catalogue |
| `GENERIC_CAR.dbt` | the car's row | the catalogue entry itself |
| `VARIATIONamerican.dbt` | a row whose +0 points at the new `<model>` string | makes the model name resolvable |
| `CAR_VARIATION_american.dbt` | car id → variation id | binds the car to that model |
| `american_StrDB.sdb` | the new strings (model name, display name) | the string pool |
| `ENGINE.dbt` | the engine row | power and the sound id (+0x18 = sound id, +0x1a = power; 666 measured as 0x029A) |
| `DEFAULT_PARTS.dbt` | the default parts row | what the car wears when bought |
| `CAR_NAME_american.dbt` | the display name row | **key scheme not yet decoded** — see §6 |

### 3.3 The pictures

`piece/car_mini/<label>.img` (small, measured Tex1 50×30) and `piece/car_pict/<label>.img` (large, 160×100).
The loader reads them through the package; on-screen proof in the dealer list is still owed (Job D).

### 3.4 Geometry rules (the intake contract)

Full detail in `docs/vulcan4/SDK-MODEL-INTAKE.md`. In short: `.glb` (metres, Y-up, car facing −Z), objects
named `body`/`wheel_fl`/`wheel_fr`/`wheel_rl`/`wheel_rr`, wheels centred on their own origin, PNG textures
power-of-two, ≤10k triangles. The game scales 1/4096; the converter does that, the source stays human.

## 4. `interface:content-catalog`

What the package declares, and where the engine files it (both are the game's own lists, not ours):

- **Arcade**: the label joins `US_carlist.adc`'s `InitData.car_carlist` handling in
  `src/core/menu_car.inc` (`nat_car_available`).
- **Dealership (GT Mode)**: the label joins the project's `NewCarData` / `CompleteCarData` / `CarData`
  containers under `"<maker>_carlist"`, and `"<maker>_carnum"` is bumped to the new length — a script that
  walks its own list by count never sees an addition otherwise (`mod_cars_into_catalog()`; being ported into
  the merged tree now).
- **Maker/country**: `GetManufacturer` answers from the manifest for a car the disc never knew.
- **Courses**: a new label in the course label table; the course file itself is read from the package.

A car with no `maker=` is deliberately **not** filed anywhere: a brand-less car stays out of the dealership
rather than being invented into a random brand.

## 5. THE VERIFY CONTRACT (`moonmod verify`)

A package is not "done" because it exports. It is done when these hold — the same list the engine's own smoke
gate runs (`tools/scripts/smoke.sh`), which is why they belong in the SDK as one command:

1. the model loads **as its own name** (`model=<model>`), and the car it was based on is byte-identical with
   and without the package (the Ka test);
2. **no table id collisions** at load (one summary line per table);
3. the disc still assembles **722 cars, 0 failed loads**;
4. the label **reaches the dealer list** and the Arcade list, with the count bumped;
5. price and power **read back** from the tables as authored;
6. the pictures are present at both sizes, and the engine's loader finds them.

## 6. OPEN QUESTIONS (honest list, do not guess in code)

- `CAR_NAME_american.dbt`'s key scheme: **not decoded**. Until it is, a package's car can load and drive but
  its title in the list is the label. Measure before writing a row.
- **Paint**: how an authored car should take colour from the game is undecided.
- **Wheel policy**: today wheels are optional (the banana has none). The intended rule is wheel *markers* in
  the model rather than hardcoded object names; not built.
- **Course package internals**: label merge is proven; the collision/timing/driving-line requirements in
  issue #8 are a separate measurement.
- **Schema versioning**: this document is `schema 1`. A package that declares a directive the engine does not
  know must be **refused with a named error**, never ignored.

## 7. VERSIONING AND COMPATIBILITY

A package declares the schema it was authored against. The engine refuses a package whose schema is newer
than it knows, and says so in one line. The SDK warns when it is about to write something the installed
engine cannot read. Neither side ever "tries anyway" — that is how a mod silently half-loads.
