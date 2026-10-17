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

## 6. What is NOT solved yet (do not promise these)

- **A pretty title in the list.** The car's display name needs a `CAR_NAME_american.dbt` row and that table's
  key scheme is **not decoded**. Today the list shows the label.
- **The icon on screen.** The loader reads both pictures; nobody has photographed it in the dealer yet.
- **Paint.** How an authored car should take the game's colours is undecided.
- **Wheel markers.** Intended design is markers in the model rather than hardcoded object names; not built.
- **Tracks.** `moonmod_addtrack` adds a label and copies a course's bytes — collision, timing and driving
  lines (issue #8) are a separate measurement.
