# W280 — the banana gets its own name, title, 666 PS and picture

**2026-10-17, `main` of the shared repo.** The mod at `/mnt/ssd/mods/moon-banana` (also
`%APPDATA%\MoonEngine\MoonEngine\mods\moon-banana` on Baki) ships the captain's banana as `moon_banana`
(id 1078), filed under Ford at 500 credits. Four jobs, all additive; the Ford Ka is untouched.

## JOB A — its OWN model name (fixes the Ka)

The mod shipped `/car/{menu,lod,open}/fdeu0006` — **the Ka's own model name** — and the overlay rule "a
file shadows" turned every car pointing at `fdeu0006` into a banana. `moonmod_addcar --model-name NAME`
now adds, all through the merge machinery:

- a new string in `american_StrDB.sdb` (GTST, index 3326 = "moonbanana"),
- a new `VARIATIONamerican.dbt` row (id 946) whose +0 points at it,
- this car's `CAR_VARIATION_american.dbt` row re-pointed at the new variation,
- the mesh placed at `/car/{menu,lod,open}/moonbanana`.

```
$ gt4car_survey "<disc>" --labels moon_banana      # with OPENGT4_OVERLAY=<mod>
showroom: moon_banana model=moonbanana color=8 triangles=80 textures=1 unsupported=0
$ gt4car_survey "<disc>" --labels ka_01            # with the mod ticked
showroom: ka_01 model=fdeu0006 color=1 triangles=11906 textures=53 unsupported=4
$ gt4car_survey "<disc>" --labels ka_01            # without the mod (baseline)
showroom: ka_01 model=fdeu0006 color=1 triangles=11906 textures=53 unsupported=4
```

The Ka is a Ka again — identical to the no-mod baseline.

## JOB B — a title in the list

`CAR_NAME_american.dbt` (GTDB, **width 16**) is keyed by **the car id** (the same id `GENERIC_CAR.idi`
gives the label). Measured with `gt4spec <disc> --carname --rows`:

```
  row 0 id 0x15  +0=230   "Isuzu 117 Coupe `68"           +8=230   +12=360
  row 1 id 0x16  +0=339   "Alfa Romeo 147 2.0 Twinspark `02" +8=339 +12=147
  ...
  ka_01  id 59  name="Ford Ka `01"  short="Ford Ka `01"  desc=435
```

+0 = name string, +8 = short-name string, +12 = description id (both into `american_StrDB.sdb`).
`moonmod_addcar --name "Moon Banana"` adds a `CAR_NAME` row keyed by 1078:

```
$ gt4spec "<disc>" --carname moon_banana   # with the overlay
 name="Moon Banana" short="Moon Banana" desc=0
```

## JOB C — 666 PS

Where the game reads power, measured:

- **The dealer's stock figure is STORED**: `nat_quick_catalog` (`menu_quick.inc:848`) reads
  `e[26] | e[27]<<8` — a u16 at **ENGINE row +0x1a**. Confirmed against known cars: Ford Ka +0x1a = **60**,
  Ford GT +0x1a = **558**.
- The menus' **current** (tuned) figure is DERIVED from the ENGINE torque curve
  (`career_vehicle.c peak_stats`, `P = T·ω` to the redline), not stored.
- The car reaches its ENGINE row through `GENERIC_CAR +0` (parts id) → `DEFAULT_PARTS` row → **+0x58** =
  `{table 0x0c (ENGINE), engine id}`.

`moonmod_addcar --power 666` sets **both**: the stored u16 (+0x1a = 666) and the torque curve
(`+0x1a`, `+0x1c` and the 24 u16 points at `+0x1e`) scaled by 666/60 = 11.10, so the stock and current
numbers agree. A **new ENGINE row** (id 2417) and a **new DEFAULT_PARTS row** (id 6734) keep the Ka's own
rows untouched.

```
$ gt4spec "<disc>" moon_banana ENGINE 0x58     # the row the dealer reads
  010: 19 00 00 00 1a 00 00 00 45 93 9a 02 65 2e 98 24
ENGINE +0x18 sound=37701  +0x1a PS(the dealer's figure)=666  +0x1c torque=11877
```

## JOB D — the picture

**The source is found, and it is a per-car asset keyed by the car label: `/piece/car_mini/<label>.img`**
(a 50x30 Tex1) and `/piece/car_pict/<label>.img` (160x100). Measured:

- The disc has `/piece/car_mini/` (1024-byte Tex1 each, ka_01 = 50x30), `/piece/car_pict/` (ka_01 =
  160x100), `/piece/car_logo{,2}/`.
- `gtmode.adc` (CarScroll/SelectBox) builds the path from **`MCarData.GetCarLabel`**:
  `image/piece/car_mini/` + label + `.png` (if the gpb bundles it) else `from_disk` `/piece/car_mini/` +
  label + `.img`.
- The engine's `ui.c image()` already maps an `image/piece/....png` path to `<vol>/piece/....img` and
  decodes it with `tex1_decode` — **overlay-aware**, so a mod file at `/piece/car_mini/moon_banana.img`
  is what gets loaded.

`tools/c/moonmod_img.c` converts the captain's PNG to a Tex1 `.img` with the engine's own writer. The mod
now ships `/piece/car_mini/moon_banana.img` (50x30) and `/piece/car_pict/moon_banana.img` (160x100), built
from `/mnt/ssd/vulcan4-assets/icon-banana.png`:

```
$ OPENGT4_OVERLAY=<mod> gt4img "<disc>" /piece/car_mini/moon_banana.img /tmp/modmini
/tmp/modmini_0.png: 50x30 (texture 64x32)          # the engine's own reader, through the overlay
```

**Not yet shown live.** A screenshot of the banana's own list entry was not captured: reaching the Ford
(`DealerUSA`) car list headlessly needs a longer carousel navigation than I drove, and existing cars'
minis are **packaged in the gpb** (shadowing their `.img` had no effect — the gpb wins), so only a *new*
car with no gpb entry exercises the disc path. The mechanism is decoded from the script and the loader is
proven; the on-screen proof is the piece still open.

## A REAL BUG FOUND AND FIXED ON THE WAY

`src/core/mod_merge.c` merged a mod pool entry as `off < entry_table ? 0 : off - entry_table`, which
clamped **every** offset below the mod's own entry table to the pool's first string. One string per mod
table hid it; the two-string `american_StrDB.sdb` (model name + title) exposed it — the title resolved to
"moonbanana". Fixed to treat a mod offset as pool-relative (still tolerating an absolute one). The
`tests/mods/test_mod_merge.c` suite passes (24 checks, including "the pool's second string survives").

## FILES

- `tools/c/moonmod_addcar.c` — `--model-name`, `--name`, `--power` (int price already fixed).
- `tools/c/moonmod_img.c` — PNG → Tex1 `.img` (new).
- `tools/c/gt4spec.c` — `--carname` measurement mode.
- `src/core/mod_merge.c` — the multi-string pool offset fix.
- `CMakeLists.txt` — the mod SDK tool targets.
