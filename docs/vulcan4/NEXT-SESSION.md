# MOON ENGINE — where the mod work stands, and what is parked for the next session

**Parked by the captain 2026-10-16 ("lets do it tomorrow. park it and log everything").** Next session starts
by handing Sanji the two prompts below — they are written, not remembered.

---

## WHAT IS DONE (all committed on `main`, nothing pushed)

| commit | what |
|---|---|
| `0fb999b` | **the switch** — `OPENGT4_OVERLAY`: a mod folder shadows the disc. Off = byte-identical (`68d8eb83…`) |
| `313ac1d` | `gt4table_dump` — a table's header, which is how we learned every car table has **compressed rows** |
| `6a9f6fa` | **lists merge** (`src/core/mod_merge.c`) + `moonmod_addcar` / `moonmod_addtrack` |
| `83441a3` | a mod's cars join the game's **Arcade list** (`mod.txt` → `mod_manifest.c`) |
| `4a1cbca` | the manifest's `price` written into the car's own row (500 credits, verified `500.0`) |
| `1261523` | **mods live in the user's folder** (`%APPDATA%\MoonEngine\MoonEngine\mods` + `mods.ini`) |
| `a88fb84`, `5a451bf` | the launcher's MODS page (list + card + SPACE/D/DEL) and **zip install** |
| `cf19e5e` | `--launcher-page` / `--launcher-shot` — a page and a picture with no mouse |
| `9fa5fb3` | **a mod's car reaches the dealership** (`NewCarData.ford_carlist` 8 → 9) |
| `7cd579c`, `4608c3e`, `59265ae` | the launcher's rules, the car-translation research, the SDK intake contract |

Gates that pass today: `mod_source` 13 checks, `mod_merge` 24 checks (no disc), `gt4car_survey` **722 → 723
checked, 0 failed loads**, the added car drawn in the studio, the dealership catalog gaining the car, and a
6,820-byte zip unpacking into a working mod.

## THE TWO BUGS THE CAPTAIN FOUND (measured, causes known)

1. **The Ford Ka became a banana.** `mods/moon-banana` ships `/car/menu/fdeu0006` — *the Ka's own model
   name*. The overlay rule is "a file shadows", so the banana is served to every car pointing at that name.
   Fix = the added car must have its **own model name** (new string + `VARIATION` row + `CAR_VARIATION` row,
   all additive, all already writable by our merge).
2. **The banana has no title.** A car's display name comes from `CAR_NAME_american.dbt` (+ a string); its
   **key scheme is not decoded**, so this is a measure-first job, not a guess.

Also reported: **666 hp did not take** — nobody has measured which value the dealer/menus read for power.

## PARKED, READY TO PASTE (tomorrow, in this order)

**Prompt 1 — the banana's own name, its title, 666 hp, and the icon.** (Full text was handed to the captain in
chat on 2026-10-16, plus this add-on for the icon:)

```
JOB D -- THE CAR'S ICON IN THE MENU (the captain sent an icon and it is not there):
  A PNG was supplied for this car and it appears nowhere. Our engine has no path that consumes a car's 2D
  icon: the disc's /car holds only menu|lod|open, and the dealership's small picture has never been traced.
  MEASURE where GT4 gets it: check the car's menu model for an embedded thumbnail, the packages
  (/image/..., .gpb) and the gtmode/arcade scripts for an icon field. Then make the mod's icon appear there
  through the mod source -- never by editing the disc.
  Icon file: /mnt/ssd/vulcan4-assets/icon-banana.png
  GATE: the car shows the supplied picture where the game shows other cars' pictures. If the source cannot be
  found, stop and report the paths you searched and what you read -- do NOT invent a replacement.
```

**Prompt 2 — the camera.** `BuyCarRoot` in GT Mode draws with the launcher's rig camera, because
`showroom_clear_rig` (`showroom.c:107-113`) clears the rig's *car offset* but not the camera, and
`main.c:325-327` routes `BuyCarRoot` through the studio path. Provenance: the routing is **upstream's**
(`09d05d6`, the fork import), the rig is **ours** (`fa351a3`), the half-fix is **ours** from today
(`0a3b91d`). The captain's instruction: **use the original camera for BuyCarRoot; do NOT touch the launcher
scene.** No freeze/test hooks — a camera print per surface plus two screenshots is the gate.

**Prompt 3 — the mesh writer (the real wall).** Round trip first (decode a disc car → re-encode with our own
writer → identical triangles and texture bytes), then a car of ours the engine loads. Contract for the intake
is `docs/vulcan4/SDK-MODEL-INTAKE.md`.

## PUSHING TO JONHLINE — THE MEASURED STATE (2026-10-16, after he moved)

The captain asked to push. **It cannot be a plain push, and here is exactly why:**

- Our `main` is **651 commits ahead** of his — our own internal history, which by standing rule **never
  crosses**. What goes across is a handful of *feature* commits, replayed on top of his `main`.
- He moved: his `main` is now **`bcd1871`** (two "Update to the current development state" commits, 353 files
  — a **Vulkan** stack in `third_party/vulkan`, release tooling, and the installer renamed
  `opengt4.iss → moonengine.iss`). His work is fetched and parked in `moonengine/main`.
- Replaying our commits onto his new head: **3 applied, then it stops.**
  - `2881e4c` the switch (vol.c overlay) — applied (one docs conflict, resolved)
  - `ada3879` gt4table_dump — applied clean
  - `a92fb49` lists merge — applied after resolving a **one-hunk** conflict in `src/core/vol.c` (he had
    refactored the same function into `read_entry`, with `__SWITCH__` timing)
  - **the next commit (`83441a3`, the game's own lists) conflicts hard in `src/core/menu.c`** — his two
    commits rewrote regions our line had also changed.
- The WIP branch is kept: **`push/features-1016`** (based on his `main`, carrying the 3 replayed commits).
  `main` is untouched and clean.

**Files that clash, with both sides' deltas against his merge base:** `src/core/menu.c` (his +83 / ours
+5,793), `src/app/main.c` (+102 / +2,853), `src/platform/sdl/showroom.c` (+22 / +878), `src/app/launcher.c`
(+611 / +41), `CMakeLists.txt` (+1,089 / +96). Nineteen files conflict in a plain merge — it is a real
integration, not a rebase.

**The plan for tomorrow (or Sanji's next session):** replay the remaining feature commits onto
`push/features-1016` one at a time, resolving `menu.c`/`main.c` by hand (our additions are small and additive:
`mod_cars_into_catalog`, the Arcade append, the mods dir, the camera separation), then **build both trees,
run the tests, and push the replayed commits to his `main`** — the one-file-per-idea commits he can read.
Nothing of ours has to be lost: his work is upstream of ours in the tree, not in conflict with what the mods
layer *does*.

## ASSETS (on the SSD, never in the repo)

| what | where |
|---|---|
| the captain's banana model | `/mnt/ssd/vulcan4-assets/low_poly_banana.glb` (80 tris, glTF 2.0, 1 embedded PNG, not centred, no wheels) |
| the icon PNG he sent | `/mnt/ssd/vulcan4-assets/icon-banana.png` |
| the working mod (as built) | `/mnt/ssd/mods/moon-banana` + the live copy on Baki |
| the proof pictures | `/mnt/ssd/moon-mods-proof/` |

## OPEN QUESTIONS (unchanged, listed so nobody re-derives them)

1. Where GT4 consumes a car's **2D icon** (the disc's `/car` has only `menu|lod|open`).
2. `CAR_NAME_american.dbt`'s **key scheme**.
3. Which row holds a car's **mass**, and which value the menus show as **power**.
4. Whether `CAR4`/`MDLS` can be written by us at all — the round trip answers it.

## STATE OF THE MACHINES

- **Baki**: new `moonengine.exe` (with all of the above), old one kept as `moonengine-before-mods.exe`;
  `mods/` and `mods.ini` live in `%APPDATA%\MoonEngine\MoonEngine\`; the studio rig is restored to
  `car_code ford_gt_05` (backup `studio-rig.txt.beforemod` still there).
- **Cortex**: `/home/or/vulcan4` on `main`, clean tree, build trees `/mnt/ssd/vulcan4-build` (Linux) and
  `/mnt/ssd/vulcan4-win` (Windows cross).
