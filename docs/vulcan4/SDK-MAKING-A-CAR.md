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
    --model-name moonbanana --name "Banana" --power 666 --sound m5_05
```

What that one command writes, all **additively** (the source car is never touched):

| written | what it is |
|---|---|
| `out/specdb/GT4_US2560/GENERIC_CAR.idi` | label `moon_banana` → a **new** id |
| `out/specdb/GT4_US2560/GENERIC_CAR.dbt` | the catalogue row |
| `out/specdb/GT4_US2560/VARIATIONamerican.dbt` | a row pointing at the new model-name string |
| `out/specdb/GT4_US2560/CAR_VARIATION_american.dbt` | car id → that variation |
| `out/specdb/GT4_US2560/american_StrDB.sdb` | the new strings (`moonbanana`, the display name) |
| `out/specdb/GT4_US2560/ENGINE.dbt` | power (`--power`) and the selected sound id (`--sound`) |
| `out/specdb/GT4_US2560/DEFAULT_PARTS.dbt` | what the car wears when bought |
| `out/mod.txt` | the manifest: `name`, `source`, `car <label> maker=ford` |

`--model-name` is the flag that makes the car **its own car**. Without it the package points at the source
car's model name and *every* car that references that name becomes your model — that is how the Ka briefly
became a banana. Never ship a package without it.

`--sound <disc label>` selects only the ENGINE soundNum, a little-endian u16 at
`+0x18`. The runtime reader is `src/app/race_audio.c` (original `00445518`).
Measured on the retail disc: `ka_01` has bytes `45 93` (37701); `m5_05` has
`79 cd` (52601). The new ENGINE row changes only those two bytes relative to
the same export without `--sound`, including when `--power 666` is supplied.
Without engine-spec overrides, power and the torque curve remain inherited. Without
`--sound`, sound remains inherited. Invalid disc labels fail the export.
Both SDK creation/export paths forward this optional field and save it in
the project. These are package overrides; neither source car is modified.

### Independent vehicle specifications (18 October 2026)

All fields below are optional: omission inherits the physics donor. The SDK
Specs inspector stores them in the project and both addcar paths forward
only nonempty fields. `--power` is **PS**, not mechanical HP. Stock physics
code is unchanged; the package authors new data rows and DEFAULT_PARTS keys.

```sh
moonmod_addcar disc.iso ka_01 moon_banana out/ ford 50 \
  --model-name moonbanana --name Banana --sound m5_05 \
  --weight 1100 --power 150 --torque 200 --torque-rpm 4500 --redline 7500 \
  --drivetrain FR --gears 6 --final 3.900 --ratios 3.5,2.2,1.6,1.2,1,0.8
```

These are explicit test specifications for a custom car, not measurements
of the source mesh's real-world vehicle. Existing wheelbase/tread/ride-height
flags can be supplied alongside them; weight and wheelbase share one new
CHASSIS row. No original row ID is reused.

| Flag | Disc field and runtime reader | Measured retail limits (USA v2.00) |
|---|---|---|
| `--weight KG` | CHASSIS u16 +6 (physics, 00441B20) and +12 (catalogue, 004460C8) | 265..3500 whole kg |
| `--torque NM` | ENGINE u16 curve +0x1e, 24 slots; 00441C58 -> setup.c engine_params | curve units 71..21840; each unit becomes 0.098 Nm |
| `--torque-rpm RPM` | ENGINE byte knots +0x55; 00441C58 -> 00361908 | 100..18000, steps of 100 RPM |
| `--redline RPM` | ENGINE byte +0x51 -> ENGINE_REDLINE; setup.c | 600..18000, steps of 100; must exceed idle and contain torque peak |
| `--drivetrain FR/FF/MR/RR/4WD` | DRIVETRAIN byte +0x10; 00441E28 -> 0034BC98 | disc codes 0/1/3/4/2 respectively |
| `--gears N` | GEAR byte +0x24; 00441E78 -> drive_params | 1..8 forward gears |
| `--final RATIO` | GEAR u16 +0x18/+0x1a/+0x1c; 00444A88 | 1.400..5.923, steps of .001 |
| `--ratios r1,r2,...` | GEAR u16 +0,+2,...; 00444A88 | .407..6.750, steps of .001; strictly decreasing |
| `--power PS` | ENGINE curve, with +0x1a catalogue metadata | 1..995 whole PS |

Limits are scanned from pristine stock GENERIC_CAR -> DEFAULT_PARTS rows on
the actual disc, not guessed from integer storage capacity. Each point of
an authored curve is range-checked. Explicit ratios infer the gear count
when `--gears` is omitted; otherwise the counts must match. Increasing the
count beyond the donor's populated ratios requires explicit ratios. Count
or ratio overrides disable GEAR +0x25 auto-generation so authored ratios
survive garage initialization. The final-drive min/default/max all become
the requested ratio, preventing the original clamp from changing it.

Two independent evidence paths are recorded in `tools/c/moonmod_specs.h`:
native readers plus actual `gt4spec` rows. Ka/M5/NSX/BTR mass is 965/1715/
1340/1180 kg at both offsets. Their drivetrain bytes are 1/0/3/4; Lancer
Evolution VI Rally Car is 2. Ka ratios are 3.333,1.955,1.286,.926,.733,
final 4.056; M5 has seven gears and final 3.620. Changing a Ka to 4WD also
copies centre fields +0x11..14 from `evo6_wrc_99` (02 0a 32 1e); retaining
the Ka's FE sentinel would not describe a 4WD centre. Other inertia and
braking fields remain those of the chosen physics donor. This centre
selection is an explicit authoring policy, not a universal 4WD simulation.

The original peak scan (`career_vehicle.c`, 0034F3F8) uses the runtime curve
at 100-RPM intervals. Power-only changes now scale against the actual curve
peak, rather than the rounded catalogue PS. Torque-only changes scale the
donor curve; torque-RPM changes warp its knot positions about the peak.
Unrepresentable/collapsed knots and inconsistent limits fail with a message.

When **both power and torque** are specified, the converter intentionally
authors a three-point, two-segment curve: an ascending section ending at
the torque peak, then a tail solved against the same 100-RPM power scan.
This does not reconstruct a real engine's full torque curve. Incompatible
PS/torque/RPM requests are rejected; numeric peaks must fit the table's
0.098-Nm quantization and 100-RPM grid (tested within .099 Nm and .5 PS).
The RPM labels are new strings appended to UnistrDB, referenced by ENGINE
+0x10/+0x14; donor strings are not rewritten. The displayed catalogue
power/torque metadata at +0x1a/+0x1c is updated from the authored curve.

Focused validation:

```sh
ctest --test-dir build -C Release -R '^mod_car_(specs|rig_sound)(_negative)?$' --output-on-failure
node gui/tests/specs.mjs
node --check gui/src/main.js
cargo build  # SDK gui/src-tauri
```

Four CTest cases pass, including deliberately corrupted camera/mass data.
The specs case also executes a deliberate missing-UI-payload negative.
It checks four entire stock rows and the assembly survey of Ka, M5, NSX,
BTR and Lancer with/without the package; all are identical. It exercises
all five drivetrain selections through 1200 native race ticks, blank
inheritance and rejected ranges, NaN, gear/RPM granularity and impossible
peak combinations. The SDK test executes the actual project serializer,
restore assignments and shared flag builder for all eight new fields.

Measured example: native UI 150.002 PS, 200.018 Nm at 4500 RPM, 1100 kg,
redline 7500; the original Arcade CarRoot displays
`Type:FR Power:150PS/7300rpm Torque:200.02NM/4500rpm`.
An OpenGL capture of those actual text widgets is stored locally under
`build/sdk-specs-proof/arcade-specs.png` (the capture harness has no 3D
showroom bridge). A 1200-frame silent race uses FR, six gears and final
3.900: overall ratios 13.65/8.58/6.24/4.68/3.90/3.12. Its mass is 1170 kg
after the original driver addition, not a failed 1100-kg override.

### Per-slot inheritance and the prepared camera project

A camera object in the SDK project may set `inherit: true`. Its array slot
keeps its index but writes no camera directive: the donor bytes survive,
including sentinel lenses. A normal authored object writes that index.
Dragging its gizmo or changing its pose/lens marks it authored again. The
project and rig serializers preserve this choice; invalid authored poses
still fail. This lets one adjust camera 6 while retaining all other rows.

The installed Banana now uses the full source, gltfpack
`-si .35 -sv -kn -noq`, and `--no-interior`. The converter removes 59 cabin
parts and reports 35561 body triangles; the assembly reader adds 7232
borrowed wheel triangles (42793 total, no unsupported/nonfinite models).
The old pre-decimated mesh is backed up locally and is no longer the input.

The rig adjusts only camera 6 (the original roof view, 0037B580). Source
bounds transformed by the converter put the roof at 1.459313 m. The eye is
1.559313 m, with the existing camera renderer's 0.1-m near plane as explicit
clearance. Positive geometry checks pass within 1e-5 m; the original
1.306150-m eye fails the same clearance check. All other 24 rows are exact
copies of the Ka in menu/lod/open. A silent 720-frame race and before/after
OpenGL captures verify this custom framing, not PS2 camera equivalence.

The local `Banana-camera-check.moonproj` contains the full source, 35%
compression, Cabin Drop and the sparse rig. It is ready for File > Open
Project, File > Export package in the freshly built moonsdk.exe. Actual
interactive export remains pending manual confirmation; CLI export and
serializer tests are not described as a UI-click test. Logs, source-bound
measurements and images remain under build/sdk-interior-proof, not Git.

### Active package verification

On 18 October, before this change, the installed folder already contained
25 cameras at 0x488 in all three slots; the adjacent ZIP still contained
376 at 0x1c8. `mods.ini` selected `moon_banana` (the folder), and a default
reader trace with MOON_OVERLAY unset confirmed that path. Both were backed
up to `build/sdk-specs-before` before reexport. The three installed model
slots were reexported with Ka cameras; the adjacent archive was rebuilt
from the same folder. Both paths now read 25 @ 0x488. The earlier validated
temporary export is at `build/sdk-camera-sound-proof/moon_banana`.

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

### Cameras and the rig sidecar (18 October 2026)

Add `--wheels-from disc.iso ka_01` to each converter command to inherit the
donor's wheels **and camera table**, without changing any camera row. The Ka
has 25 rows at CAR4 offset `0x488`: 800 bytes. Without a donor or an authored
camera the previous encoder behavior remains unchanged (verified by identical
SHA-256 output against the previous converter).

`--rig rig.json` consumes the SDK's text sidecar; despite its filename this
is not JSON. The SDK writes a camera line for each array index:

```text
camera 1 pos=0,-1.7,-6 turns=0,8,0 lens=2.69900727 spare=0
```

`pos` is the negative eye position in source/viewer units; `turns` contains
Z,X,Y Euler angles in degrees. Camera rows replace donor rows by index;
untouched rows remain byte-identical. Without a donor, indices must be
contiguous from zero. Duplicate, out-of-range or malformed camera rows fail.
The SDK validates poses before replacing the sidecar and passes `--rig` for
camera-only projects as well as projects with wheel assignments.

Authored cameras follow the mesh's resolved facing, horizontal centering,
floor translation, uniform length scale and axle-centering shift. Their
orientation follows the same Y-axis facing rotation, composed with ZXY Euler
rotation. CAR4 camera positions are **metres**; only MDLS vertex positions
use 4096 units/metre. Multiplying camera rows by 4096 is incorrect.

Proof: `ctest --test-dir build -C Release -R '^mod_car_rig_sound(_negative)?$'
passes both the positive test and the deliberately corrupted-camera negative.
The test checks all 800 donor bytes, four facing axes, translated/scaled
positions, orientation matrices (tolerance `2e-5`), lens/spare and untouched
rows; sound checks compare whole ENGINE rows and unchanged donor surveys.
The SDK's `cargo test rig_json` passes three tests, including invalid poses
that must leave the old sidecar intact.

The local `moon_banana` package was exported in all three model slots with
Ka cameras (25 @ `0x488`) and M5 sound 52601. A hidden, silent PC race probe
completed 720 rendered frames, switching cameras 1, 0 and 6; its runtime log
opened sound bank `52601`, while the Ka opponent opened `37701`.
This proves loading and switching, not every camera's framing or an auditory
match: an unchanged Ka interior eye can intersect a larger custom body's
geometry. Adjust it with the rig when authoring that model.

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

- **Camera framing and listening checks.** All 25 donor rows are preserved, but only three camera modes were exercised in the PC race probe. The Banana roof view is now authored and checked on the 35% no-interior model; other custom bodies need their own positions; an audible comparison against the selected sound donor remains pending (tests used dummy audio).
- **Paint.** How an authored car should take the game's colours is undecided.
- **Wheel markers.** Intended design is markers in the model rather than hardcoded object names; not built.
- **Tracks.** `moonmod_addtrack` adds a label and copies a course's bytes — collision, timing and driving
  lines (issue #8) are a separate measurement.

### Automatic export pictures (18 October 2026)

Export now uses the same picture pipeline as Make Pictures: blank PNG inputs
render a black/white pair and convert it to car_mini (50x30) and car_pict
(160x100). Any renderer or converter failure stops packing; manual inputs
remain supported individually. Offline capture explicitly selects OpenGL,
without changing player settings: the measured Vulkan capture exits with
0xC0000374, while OpenGL exits zero and produces both frames. The underlying
Vulkan capture fault remains unresolved. Banana conversion produces 6100-byte
and 64100-byte Tex1 files. Evidence: build/sdk-picture-proof; SDK focused
pictures.mjs tests cover automatic, manual, mixed and failed generation.

### Vulkan picture capture shutdown correction (18 October 2026)

The capture failure above is resolved: Context destruction calls close before
member teardown, preserving target_cache until image deleters finish. Three
Banana Vulkan runs exit zero and produce byte-identical pairs, with unchanged
pixels from the pre-fix capture. Four focused explicit/implicit shutdown
CTest cases pass, including deliberate image-corruption negatives. SDK capture
no longer forces OpenGL. The limitation in the preceding historical entry
is superseded by this proof (build/sdk-picture-proof/render-vk-fixed-*.log).

### Interactive export artifact verification (18 October 2026)

The user completed File > Export in moonsdk.exe. The resulting moon_banana.zip
(timestamp 13:25 local, 40087118 bytes) passes ZIP CRC validation and contains
car_mini/car_pict Tex1 assets (6100/64100 bytes). Both assets match a repeated
conversion of the fresh project PNG pair. All three CAR4 files match the
project build and retain 25 camera rows at 0x488; the rig overrides only slot 6.
The project retains cabin Drop and compression .35. UI operation was performed
by the user; these checks validate its artifact, not automated UI interaction.

### Functional custom-car authoring (18 October 2026)

The inspector now exposes an optional own rim/tyre GLB pair, released/pressed
brake-light mesh lists (Add selected), and an own sound-definition JSON.
These choices persist in project.json. Both conversion entry points consume
them. Keep-mode sound import copies the definition and all referenced WAVs
into the project; reference mode retains external dependencies.

- Wheels: the original native CAR4 rim/tyre path is reused. The pair replaces
  embedded wheel meshes, keeping independent camera inheritance via
  `--cameras-from <disc> <label>`. Radius/fit conventions remain those in
  model_write.h and wheel_geometry.h; vehicle tyre sizes still come from
  the configured specification rows. A synthetic authored pair assembles
  four wheels (1024 wheel triangles) with the Banana's 35561 body triangles.
- Lamps: `brake released part=<mesh>` and `brake pressed part=<mesh>` retain
  original callback-0 states; unassigned body geometry stays shared. Duplicate
  or missing mesh bindings are errors. The writer now preserves wheel-frame
  tags too. KHR_materials_unlit selects unlit authoring explicitly; braking
  never forces a lit material to become unlit.
- Sound: sound/car/<label>/engine.es and exhaust.es take precedence only for
  that car. Missing overrides retain the original sound-number lookup;
  corrupt overrides are diagnosed instead of silently selecting another car.
  WAVs become native ENGN/SPU ADPCM, preserving the retail controller's RPM
  selection, pitch, gain, transitions and race/replay mixing. No audio I/O
  or decoding is added to the real-time callback. Clearing the JSON field
  removes the two previously generated overrides at the next build.

Example sound.json (WAV paths relative to this JSON):

```json
{
  "engine": [
    {"wav":"idle.wav", "rpm":1000, "minimum":200, "maximum":1800, "gain":16384},
    {"wav":"high.wav", "rpm":5000, "minimum":3000, "maximum":32767, "gain":16384}
  ],
  "exhaust": [
    {"wav":"exhaust.wav", "rpm":3000, "minimum":200, "maximum":32767,
     "loop_start":0, "loop_end":2800}
  ]
}
```

Inputs: mono PCM16 WAV, 8–192 kHz; 1–256 ordered RPM plateaus per bank,
nonoverlapping plateaus (the gaps use the original transition), positive
recorded RPM, nonnegative signed-16-bit gain/ranges, at most 64 MiB encoded
per bank. Loop start is aligned to 28 decoded samples. Loop end is exclusive;
when not divisible by 28, the encoder holds the final sample to complete the
last block. Author loop endpoints for continuity; no automatic loop repair
or resampling is claimed. Gain 16384 is unity in the existing controller.

Proof: mod_car_functions and mod_car_function_export with deliberate
negatives, the focused original brake-branch/material regressions, five Rust
rig tests, two Rust sound tests and JavaScript save/CLI tests. Independent
native decoding of the synthetic 2800-frame WAV gives 56.32 dB SNR. A silent
Vulkan race and OpenGL replay both load authored engine/exhaust layers and
the four authored wheels; the Ka remains unchanged. Local evidence:
build/sdk-functions-proof. These are P authoring features, not an E claim
for an entire custom car.

Still outside this implementation: an audio timeline/layer editor, custom
per-event gear/skid/backfire samples, explicit steering-wheel/pilot rig
export, wing installation/animation authoring, and arbitrary animated parts.
The physical specification bundle still begins with a donor; exposing every
remaining physical table and replacing all dependencies is unfinished.
