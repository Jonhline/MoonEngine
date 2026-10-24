# RESEARCH — THE ASSIST LINE (G6.15 / "the brake point"), 2026-10-24

Boss's brief: *"people say the license tests have an entire assist line system. so thats kinda what
they want."* He is right, and it is more useful than expected: the disc does not merely draw a line —
it ships **32 authored line meshes**, one per National B and National A licence test.

Everything below was measured on the captain's own retail disc
(`Gran Turismo 4 (USA) (v2.00).iso`, 5,314,478,080 B) with `tools/vol_file.py` and
`build-linux/gt4model_write`. No emulator, no save state, no guessing.

---

## 1. What the disc actually has

**Directory: `/crs/bestline/`** (the only non-course entry under `/crs`; 129 files total)

| region prefix | sets |
|---|---|
| `usl0a`, `usl0b` | 16 + 16 files — USA |
| `jpl0a`, `jpl0b` | 16 + 16 — Japan |
| `ukl0a`, `ukl0b` | 16 + 16 — UK/PAL |
| `krl0a`, `krl0b` | 16 + 16 — Korea |

Naming: `<region><licence code>` — `usl0b0001` is exactly the code the engine already carries in
`last_desc.license_code` for B-1 (`l0b0001`). `0a` = National A, `0b` = National B. No `0c`..`0f`,
no `l1a` — the authored system stops at A, which is precisely what the community says
(*"this feature only works on the B and A license tests"*).

**US set measured: 32 files, 641,394 compressed bytes, 2,271 B (B-1) to 82,529 B (A-10).**

Format: each file is a **single MDLS model** (the same container as a car body), decoded with the
engine's own reader:

```
/crs/bestline/usl0b0001 : 2,271 bytes ->   50 triangles, 0 textures, 1 unsupported element
/crs/bestline/usl0a0016 : 34,401 bytes -> 1,002 triangles, 0 textures, 1 unsupported element
```

- **0 textures** → the line is coloured by **vertex colours**, not a texture.
- Saturated colours present in the raw data: `00 00 ff ff` (blue, the acceleration guide) and
  `f4 24 3d ff` (GT red, the braking guide). That is the blue/red split players describe.
- Vertices sit in **world coordinates** (e.g. x ≈ −973, z ≈ −1586 for B-1), i.e. the model is
  authored in the same space as the course, and both a flat layer (y = 0.00) and a raised layer
  (y = 0.04) appear — the ribbon has a top and a bottom.

So the "assist line system" is: **an authored ribbon mesh per licence test, drawn at road level,
blue where you accelerate and red where you brake.** Not a runtime algorithm, not a spline, not a
shader.

## 2. How the game turns it on

Two strings in the game's own executable (`CORE.GT4.dec.bin`):

```
crs/bestline/%s          <- the loader path
BUTTON_LICENCE_BEST_LINE <- the input action that shows it
```

That action is already modelled in our tree: `src/core/pad_config.h:22 GT4_PAD_BEST_LINE`, and the
game's own action-name table is transcribed at `src/core/pad_config.c:12-17` in the guest's order.
The button name `R3` exists as a key (`GT4_PAD_R3`, PS2 bit 0x2000, mapped in
`src/platform/sdl/pad_sdl.c:226`).

**The gap is one default binding.** `gt4_pad_defaults()` (`src/core/pad_config.c:34-61`) binds
L1/L2/R1/R2/START/SELECT/cross/square/triangle/circle/up-left-right and **never binds R3 to
`GT4_PAD_BEST_LINE`**. So in our engine the guest is never told the button was pressed, and the
guest's own best-line path never runs. That is why the licence tests show no line today — not a
missing asset, not a renderer gap: a missing default.

## 3. What that does NOT give us

`crs/bestline/` is **licence-test data**. A free run, an arcade race or a GT Mode event has no file
to load, because the disc never authored one. So "the line in every track and race" needs the line
**generated**, and the 32 authored meshes become the only honest yardstick for whether a generated
line is right.

## 4. Generating a line — what the geometry can and cannot do

Measured on `crs/deepforest` (RNW4 block at 0x69cc80):

```
length 3576.6 m, grids 6, road vertices 7,074, road triangles 17,292
```

- **There is no ordered centreline in the data.** Triangle adjacency is surface soup (chef's
  measurement: Tsukuba 12,100 tris, 17,309 shared edges, 1,538 boundary, 47 non-manifold,
  3 components). A walk from grid 0 does not close a loop.
- **But the file order is locally coherent**: consecutive triangle centroids step a median of
  **2.90 m** (mean 10.3 m), with only **888 of 17,292** jumps over 20 m. So the triangles are laid
  out in runs along the track, broken into sections — enough to chain, not enough to trust blindly.
- The runway header carries further blocks we do not parse yet (offsets at `+0x58`, `+0x7c`,
  `+0x80`, `+0x84`, `+0x8c`, `+0x90`, `+0x9c`, with a count `0x16e2` = 5,858 at `+0x88`). One of
  them may be a path table; that is worth one measurement pass before writing a skeletonizer.

**Recommended generator, in order of honesty:**

1. **Occupancy grid.** Sample the road surface (`gt4_course_surface_at`, already written) on a 2 m
   grid over the course bbox → a binary "is road" raster. This is order-free and immune to the
   soup.
2. **Skeletonise** (thinning) → a 1-cell-wide path through the road's medial axis.
3. **Order it** from `gt4_course_grid(0)` by nearest-neighbour walking, closing the loop.
4. **Smooth** (moving average, then curvature-limited) → the racing line.
5. **Brake zones**: curvature above a threshold, scaled by the car's deceleration, exactly as
   `WISHLIST.md` G6.15 says ("derived from physics we already compute").

Gate for all of it: **regenerate the 32 authored licence lines and compare**. Same track section,
same world coordinates, so the test is quantitative — mean lateral distance between generated and
authored line in metres, and brake-zone overlap. If the generated line for B-1/B-4/A-16 is not
within a few metres of the authored one, it is not an assist line, it is decoration.

## 5. Rendering

Already solved by the engine:

- `r3d_mesh_create(const gt4_model *)` — `src/platform/sdl/render3d.h:158`; a synthesised
  `gt4_model` (positions + vertex colours) becomes a drawable mesh.
- Race scene draw path: `r3d_begin`/`r3d_draw` at `src/platform/sdl/race_view.c:2541,2557,2564`,
  inside `race_view_draw` (`:2736`), called from `src/app/main.c:6528` and `race_game.c:2873`.
- The course world is already drawn `unlit` with vertex colours (`r3d_view.unlit`), which is exactly
  how the authored best-line model wants to be drawn — no lighting, no texture, no new shader.
- The course object is reachable in a race: `race_asset` owns `gt4_course` (`race_view.c:68`),
  exposed by `race_view_course()` (`:1460`), used at `race_game.c:1273,1943,1986`.

## 6. The fence

- Off by default, one switch in the Tweaks tab (`tweaks.assist_line`), global: every track, every
  race mode, licence tests included.
- It may not touch AI, physics, or any disc-authored data. It draws; it does not drive.
- The authored licence meshes come from the user's disc at runtime — **nothing from the disc is
  ever committed**, same rule as every other asset.

## 7. Verdict

The feature is buildable in two stages, and the first stage is small:

- **Stage A (hours):** bind R3 → `BUTTON_LICENCE_BEST_LINE` as a default, add the switch, and prove
  the authored line appears in a B/A licence test. This validates the *system* the captain pointed
  at, with zero new geometry.
- **Stage B (days):** the generator above, gated against the 32 authored lines, then the switch
  makes the line appear on every track and race.

Stage A is worth shipping on its own: it is the most authentic possible version of the feature in
the only place GT4 ever had it.
