# WHAT A GT4 CAR CAN AFFORD — measured budgets, for the SDK's "too complex" check

**Measured 2026-10-17 by Caine**, not estimated. Two sources: a full survey of every car model on the retail
disc, and the engine's own texture code. Command used:

```
gt4model_survey "/mnt/ssd/gt4/Gran Turismo 4 (USA) (v2.00).iso" --each
```

## GEOMETRY — the real numbers from the disc's own cars

| | triangles per car model |
|---|---|
| models surveyed | **807** |
| minimum | 3,612 |
| median | **4,950** |
| 90th percentile | 5,831 |
| total across all models | 4,085,038 |

**Read this against a modern source.** An Assetto Corsa or Sketchfab car is commonly 100,000–500,000 triangles,
and some are past a million. That is **20× to 100× the median car this game ever shipped**, and it is the
number the SDK exists to surface. A package that does not decimate is a package that will not hold a frame rate.

Also measured: **2,608 unsupported strips/textures** across the 807 models (≈3 per car — and a typical car
reports `4 unsupported` in the engine's own line). Those are the model commands this engine does not implement;
they are the honest reason a converted model can look wrong even when it loads.

**Command usage across the 807 models** (how many cars use each opcode) gives the shape of a real car file:
a core set used by essentially all of them (`0, 4, 6, 8, 12, 13, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
38, 43` — 805–807 cars each), and genuinely rare ones: `21` (3 cars), `20` (20), `16` (44), `9` (46), `18` (88),
`17` (148). A converter that handles the core set handles every car; the tail is where a specific car breaks.

## TEXTURE MEMORY — 4 MB, and it wraps

The PS2's GS has **4 MB of texture memory**, shared with the framebuffer — the engine's own constant:
`src/platform/sdl/tex1.c:17` → `#define VRAM_SIZE (4u * 1024 * 1024)`, and its address math at `tex1.c:74`
notes **"GS memory wraps"** when it runs past the end.

Formats the GS (and this engine) speak — `tex1.c:19`:
`PSMCT32 = 0x00` (32-bit RGBA), `PSMCT24 = 0x01`, `PSMT8 = 0x13` (8-bit paletted), `PSMT4 = 0x14` (4-bit paletted).

Consequence for an author: a 1024×1024 RGBA texture is **4 MB on its own** — the entire texture memory for one
image. That is why the disc's own car textures are small and paletted. A modern car's textures will not fit
until they are converted and shrunk, which is the second thing the SDK has to tell a user honestly.

## DECIMATION — proven, offline, fast (measured 2026-10-17)

The pipeline needs to shrink a modern car down to what the game can afford. Measured with
[meshoptimizer](https://github.com/zeux/meshoptimizer)'s `gltfpack` **v1.3** (a single static 4.1 MB binary,
released for Windows/Linux/macOS — nothing to install):

```
gltfpack -i heavy.glb -o heavy-5k.glb -si <ratio>        # -si = target triangle ratio
```

| source | result | time |
|---|---|---|
| 81,920 tris → `-si 0.061` | **4,996 tris** (1,475,288 B → 50,796 B) | **0.031 s** |
| 327,680 tris → `-si 0.0153` | **5,012 tris** | **0.136 s** |

So a 300k-triangle modern car reaches the disc's own median budget in about a seventh of a second, in-process,
offline, and it stays a `.glb` — the same format our converter reads. Relevant flags: `-si` ratio, `-se` error
limit (default 1% deviation), `-sa` aggressive, `-slb` lock borders (avoids gaps on split meshes).

**Open, not assumed:** whether gltfpack preserves the *named objects* our wheel assignment depends on
(`body`, `wheel_LF`, …). That has to be measured on a real named model before the SDK relies on it — if it does
not, the rig map (`rig.json`, schema §3.5) becomes the authority instead of the names, which is what it is for.

## WHAT THE SDK SHOULD SAY (no invented thresholds)

- **Geometry:** per-model triangles, shown next to the disc's own range (median ≈ 5,000, p90 ≈ 5,800). Over
  ~20,000 it is worth a warning; the honest sentence is "this is N× the median car this game shipped", not a
  made-up hard limit.
- **Textures:** count, dimensions and format per image, with the 4 MB texture-memory budget as the context, and
  a plain warning when a single image is 4 MB or more on its own.
- **Unsupported:** the converter's own count of commands this engine does not implement, per model. That number
  is the difference between "it loads" and "it looks right".

## STILL OPEN (do not guess these)

- What the **GPU cost of a triangle** actually is in this engine at race time — needs a real frame-time
  measurement with N cars on track, not arithmetic.
- Which of the unsupported opcodes are **fatal** (a wrong vertex) versus **cosmetic** (a missing strip).
- Whether the engine can stream geometry (the disc's own `.texstream` files say it can stream textures at least).
