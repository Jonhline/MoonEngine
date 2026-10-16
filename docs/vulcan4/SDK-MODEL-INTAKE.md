# What a model must be, and how it gets in — the SDK's intake contract

**Started 2026-10-16** from the captain's answers, so the first test car has no ambiguity. This is the file the
SDK will implement; every rule here is a decision, and the ones marked *(later)* are not built yet.

## THE FIRST TEST CAR, AS DECIDED

| | |
|---|---|
| the model | **a banana** (the captain's own offer) — a real, silly, unmistakably-not-GT4 shape |
| brand | **Ford** (filed in the Ford dealer, US) |
| nation | **US** |
| price | **50 credits** |
| sound | the donor car's engine for now (`ENGINE` row +0x18 → `/sound/engine/%05u`) |
| icon | the captain sends a PNG; **where GT4 consumes a 2D car picture is still unfound** — an open hunt, not a promise |
| wheels | the captain's own rim meshes if he has them; otherwise the disc's |
| paint | unknown how GT4 should do it for an authored car — to be decided by measurement, not taste |
| goal | **buy it and drive it** — everything else is a step to that |

## THE INTAKE (what the SDK accepts, and why each rule exists)

| rule | why |
|---|---|
| **`.glb` (glTF binary)** first; `.obj` + `.mtl` + PNGs as fallback | both are simple to parse without a dependency; `.fbx`/`.blend` are containers with a decade of variants, and a converter that half-reads them lies about its failures |
| **metres, Y-up, car facing −Z** | glTF's own convention. GT4 draws car models at **1/4096** (`model.c:12`), so the converter scales; the source stays in real units so a wheelbase is readable |
| **separate objects named `body`, `wheel_fl`, `wheel_fr`, `wheel_rl`, `wheel_rr`** | GT4 tags every triangle with an attachment id — 0 body, 1-4 wheels, 5+ steering (`model.h:17`). A mesh without those tags renders with **dead wheels**, and there is no fallback mesh. The name is the cheap, human way to say which is which |
| **wheel objects centred on their own origin**, at nominal size | the game fits the rubber itself from `TIRESIZE` (`car_model.c:52-56`); the rim is the modder's, the tyre shell is the game's |
| **PNG textures, power-of-two, UVs in 0..1** | the GS wants PSMCT32/24 or PSMT8/4 with page/block swizzle and a CLUT; non-power-of-two is not a thing the hardware does |
| **≤10k triangles** for the first car (the disc's Ka is 11,906 with wheels) | keeps the round trip debuggable |
| **one mesh for `menu`/`lod`/`open`** for now | LODs are three files on the disc; splitting them is an optimisation, not a first step |
| no interior, no cockpit, no shadow/hull mesh yet | the game has them; nothing needs them to prove a car can be driven |

## THE WHEEL-MARKER IDEA (the captain's, 2026-10-16 — *(later)*)

Rather than requiring named objects forever, the SDK should let the author **place wheel markers** on the model
in a preview: pick a point (and a direction) for each of the four corners, and the converter writes the
attachment tags itself. That turns "name your objects correctly" into "click where the wheels are", which is
the difference between a tool for us and a tool for everyone. The model above still has to work, because it is
what a marker UI writes into.

## THE ROAD IN — in order, each one a gate

1. **Round trip.** Decode a disc car, re-encode the `CAR4`/`MDLS` bytes with our own writer, and require the
   re-encoded file to decode to **the same triangles and the same texture bytes**. Until this passes, nothing
   else is honest: it is the only test that answers "can we write the format at all".
2. **A car of ours that the engine loads** — same rows as today's add-car path, but its mesh is a file *we*
   wrote rather than the disc's. The engine's own loader is the judge
   (`showroom: <label> model=<name> triangles=… wheels=…`).
3. **The banana**, with the captain's textures, bought and driven.
4. Only then: the texture/CLUT round trip for a foreign PNG, and the SDK's own front end.

## STILL OPEN (named, not guessed)

- Where a car's **2D icon** is consumed by GT4 (the disc's `/car` holds only `menu|lod|open`).
- Which row carries a car's **mass** (the params row is assembled in `init.c`, the source field is in the 43
  tables).
- How GT4 expects an authored car's **paints** to work (Pat0 vs `VARIATIONamerican` colour rows).
- Whether a mod's mesh must be a real `/car/...` file **plus a VOL index entry** — the overlay makes the path
  resolve without touching the disc, which is the whole point of the two-source design
  (`docs/vulcan4/MODS-DESIGN.md`).
