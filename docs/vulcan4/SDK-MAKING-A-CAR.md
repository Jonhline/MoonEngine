# MAKING A CAR PACKAGE — the runbook, command by command

**Written 2026-10-17** because the truth was embarrassing: the whole pipeline that produced the banana lived
in code comments and one results file, and **no document mentioned the converter at all**. This is the manual
the GUI will wrap and the CLI's own manual. Every command here is the real one, and every number is measured.

The worked example throughout is the captain's banana, which is a Ford/`US` car, 50 credits, 666 hp, no
wheels, own model name `moonbanana`, label `moon_banana`.

---

## 0. What you need

- The retail disc, untouched: `Gran Turismo 4 (USA) (v2.00).iso` (md5 `6e03af5873c30e418021553a8832ca42`).
  Spec II is **not** this engine's disc yet.
- The built tools (in the engine repo, `tools/c/`): `moonmod_addcar`, `moonmod_glbcar`, `moonmod_img`,
  `moonmod_addtrack`, plus the readers (`gt4car_survey`, `gt4table_dump`, `gt4vol`).
- A source model: `.glb` (metres, Y-up, car facing −Z). Contract: `SDK-MODEL-INTAKE.md`.
- A source car on the disc to inherit the boring parts from (physics shape, sound, paints): the banana uses
  the Ford Ka, label `ka_01`.

## 1. The car's catalogue entry (the label, its tables, its manifest)

```sh
moonmod_addcar disc.iso ka_01 moon_banana out/ ford 500 \
    --model-name moonbanana --name "Banana" --power 666
```

What that one command writes, all **additively** (the source car is never touched):

| written | what it is |
|---|---|
| `out/specdb/GT4_US2560/GENERIC_CAR.idi` | label `moon_banana` → a **new** id |
| `out/specdb/GT4_US2560/GENERIC_CAR.dbt` | the catalogue row |
| `out/specdb/GT4_US2560/VARIATIONamerican.dbt` | a row pointing at the new model-name string |
| `out/specdb/GT4_US2560/CAR_VARIATION_american.dbt` | car id → that variation |
| `out/specdb/GT4_US2560/american_StrDB.sdb` | the new strings (`moonbanana`, the display name) |
| `out/specdb/GT4_US2560/ENGINE.dbt` | power (`--power`) and the donor's sound id |
| `out/specdb/GT4_US2560/DEFAULT_PARTS.dbt` | what the car wears when bought |
| `out/mod.txt` | the manifest: `name`, `source`, `car <label> maker=ford` |

`--model-name` is the flag that makes the car **its own car**. Without it the package points at the source
car's model name and *every* car that references that name becomes your model — that is how the Ka briefly
became a banana. Never ship a package without it.

## 2. The geometry (three levels of detail)

```sh
moonmod_glbcar banana.glb out/car/menu/moonbanana --length 4.4
moonmod_glbcar banana.glb out/car/lod/moonbanana  --length 4.4
moonmod_glbcar banana.glb out/car/open/moonbanana --length 4.4
```

- `--length` is the car's **real length in metres** (default 4.4). GT4 draws car models at 1/4096; the
  converter does that scaling so the source stays human.
- Three copies is honest for now: the same mesh in the three slots the engine asks for by path
  (`/car/menu/<model>`, `/car/lod/<model>`, `/car/open/<model>`). Real LOD work is a later feature.
- Attachments: triangles tagged `body` = id 0, `wheel_fl/fr/rl/rr` = 1–4. A body-only car is legal and drives —
  the banana has no wheels by the captain's own call. Wheel-less means `wheel_parts=0` in the survey, and the
  physics still drives it.

### What the converter does to a modern source (measured on a 199,757-triangle Escort, 2026-10-17)

- **Facing:** the parts' names (lights, tyres, bumpers) say where the front is; the model is turned to the
  engine's −Z (a disc car's front hubs sit at z < 0). `--forward` or the rig's `forward` only decide when the
  names do not, and a rig that contradicts the names is reported.
- **Textures:** glTF's V is used as it is (no `1 − v`: that drew every image upside down); each triangle is
  moved into [0,1) whole; power-of-two images repeat. Alpha follows the material (OPAQUE ignores it, MASK cuts,
  BLEND keeps) and is written in the GS's 0..0x80 range (255 read as 2.0 and drew the body black).
- **Wheels:** without rig wheel lines, tyre/rim/hub/brake parts are found by name and given their corner. With
  `--wheels-from` they are left out (the donor's rim/tyre take their place) and measured: the body is centred on
  its axles and `car/wheels/<model>` (wheelbase, tread, radius, width) is written beside the model. The engine
  draws the wheels there; the physics keeps the donor's tables.
- **Cabin:** kept (it shows through the glass); `--no-interior` leaves it out.
- **Glass:** window materials are drawn as the disc's cars draw theirs, dark (the cabin a silhouette), with the
  GS's alpha blending on (the packet's PRIM ABE bit; without it nothing a mod drew was ever see-through) and after
  every opaque part. Lenses and gauge glass keep their own colour.
- **Textures at their own size:** the CAR4 keeps the PS2-sized copies (4 MB of GS memory, 1024 a side) as the
  fallback; `car/hires/<model>` carries every texture the GS had to shrink at its authored size (power-of-two
  sides, zlib-packed), and the engine swaps them in on PC (not on Switch). The Escort: 23 textures, 50 MB unpacked,
  5.7 MB on disk.
- **Decimation is optional, and the cabin goes first.** The whole car loads (222k in the engine with the cabin and
  the borrowed wheels; the disc's cars are 9-12k). Measured on the Escort, 2026-10-17, front-three-quarter
  pictures side by side: `--no-interior` alone gives 105k and looks the same; then `gltfpack -si R -sv -kn -noq`
  gives 41k at 0.35 (the same), 27k at 0.2, 21k at 0.12 (a few bumper-chrome specks). Without `-sv` the chrome and
  door frames tear; with `-slb` it stops at 49k with the bumper dented; `-sa` (the SDK's old 2 %) tore the UVs
  across the atlas. The SDK's "Compression" and "Cabin" fields drive this.

## 3. The pictures

```sh
moonmod_img icon.png out/piece/car_mini/moon_banana.img 50 30
moonmod_img icon.png out/piece/car_pict/moon_banana.img 160 100
```

Sizes are the ones measured on the disc's own car pictures (small 50×30, large 160×100). Output is a Tex1
image; the engine's loader reads it through the package. **On-screen proof in the dealer list is still owed**
(Job D) — do not claim it works, claim the loader reads it.

## 4. Verify BEFORE anyone plays it

```sh
# the car loads as its own name, the source car is untouched
gt4car_survey disc.iso --labels moon_banana
gt4car_survey disc.iso --labels ka_01          # with and without the overlay: must print the same line

# the engine's full gate
bash tools/scripts/smoke.sh
```

The claims a package has to survive: it loads as `model=moonbanana`; the source car's survey line is
byte-identical with and without the package; no table id collisions at load (one summary line per table); the
disc still assembles **722 cars, 0 failed loads**; the label reaches the **Arcade** list and the **dealership**
catalog (`menu: NewCarData.ford_carlist gains "moon_banana", list now 9`); price and power read back as
authored.

## 5. Install it where the engine looks

Drop the folder (or a zip) into the player's own mod folder and tick it in the launcher's MODS page:

- Windows: `%APPDATA%\MoonEngine\MoonEngine\mods\<name>` (+ `mods.ini`)
- Linux: `~/.local/share/MoonEngine/MoonEngine/mods/<name>`

## 6. Solved since (2026-10-17, photographed in the Arcade's Ford list)

- **The title:** `--name` writes the `CAR_NAME_american.dbt` row (keyed by the car id, +0 name, +8 short name);
  the list shows "Banana".
- **The maker's badge and flag:** `addcar <maker>` copies GENERIC_CAR +14 (country) and +15/+16 (maker) from the
  first car the disc's dealership lists for that maker; a Ford on a BMW donor no longer shows the BMW roundel.
- **The Arcade:** the car joins `CarData.<maker>_carlist` there too, not only the dealership's.
- **The pictures:** render them from the car itself --
  `MOON_PICTURE=<prefix> MoonEngine <disc> --preview <label>` writes `<prefix>-black.png` and `<prefix>-white.png`
  (the car alone, front three-quarter), and `moonmod_img --matte <prefix>-black.png <prefix>-white.png
  out/piece/car_pict/<label>.img 160 100` (and `car_mini ... 50 30`) mattes, crops and fits them.

## 7. What is NOT solved yet (do not promise these)

- **Paint.** How an authored car should take the game's colours is undecided.
- **Wheel markers.** Intended design is markers in the model rather than hardcoded object names; not built.
- **Tracks.** `moonmod_addtrack` adds a label and copies a course's bytes — collision, timing and driving
  lines (issue #8) are a separate measurement.
