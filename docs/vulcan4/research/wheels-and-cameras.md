# Wheels and cameras — what the disc actually carries

**2026-10-17, measurement only** (no code changed). Disc: GT4 (USA) v2.00. Readers: `/mnt/ssd/moon-build`
(`gt4spec`, `gt4car_survey`, `gt4vol`) plus the engine's own code and logs. Every number below was read
from the disc; where nothing was found, it says so.

---

## QUESTION A — WHEELS

### A1. The tag: how the game knows which mesh is a wheel

A triangle's **attachment id** is authored **inside the model's MDLS opcode stream**, not in a table:

- `src/core/model.h:24` — `uint8_t attachment; /* 0 body, 1..4 road wheels, 5+ steering frames */`
- `src/core/model.c:609-610` — while the car's command stream runs, the car callbacks **11..14** set
  `attachment = callback - 10`, i.e. **1, 2, 3, 4** (the four road wheels):
  ```c
  if (r->car && op == 6 && u16(r->d + p) >= 11 && u16(r->d + p) <= 14)
      attachment[depth] = (uint8_t)(u16(r->d + p) - 10);
  ```
- `src/core/model.c:583-592` — car callback **8** registers a **steering frame**, tagged `5+frame`.

So a wheel is a mesh **drawn under callback 11..14**; anything drawn outside them is body (0). The tag is
carried by the geometry, so it travels with the CAR4 file.

### A2. Where a wheel's SIZE and POSITION come from — the tables (measured)

The wheel's position is **computed**, never the mesh's own origin. `car_model.c`:

| what | code | row | Ka value |
|---|---|---|---|
| wheelbase | `car_model.c:62-64` `u16(row+4)*.001` | **CHASSIS** (Ka row 0x5a1) | `+4 = 0x0990 = 2448` → **2.448 m** |
| tread (track), front/rear | `car_model.c:89-91` `u16(row+2)` / `u16(row+4)` | **RACINGMODIFY** (0x5a1) | `0x0570=1392` → **1.392 m**, `0x0583=1411` → **1.411 m** |
| ride height, front/rear | `car_model.c:67-68` `(int16)u16(row+4)` / `+10` | **SUSPENSION** (0xea5) | `0x91=145` → **0.145 m** both |
| tire size | `car_model.c:94-100` → `showroom_tire_size` (`showroom_math.h:58-65`) | **TIRESIZE** | id **654** = `0b 00 82 21` |

TIRESIZE row 654 = `0b 00 82 21`: `size[0]=0x0b=11` (aspect), `size[1]=0`, `size[2]=0x82=130`
(diameter: ≥61 and a multiple of 10 → `d/10` inches = **13"**), `size[3]=0x21=33` (width ÷ 5 → 165).
`showroom_tire_size` gives **width 0.165 m, rim radius 0.16510 m, radius 0.25585 m**.

The size chain is `FRONTTIRE`/`REARTIRE` row **+0 = TIRESIZE id**; Ka FRONTTIRE row 0x9c2f `+0 = 0x028e = 654`.

**Position it uses** (`car_model.c:107-108`):
```c
float hub[3] = { sign*tread[axle]*.5f, radius, (axle?1:-1)*wheelbase*.5f };
```
→ front hubs `(±0.696, 0.256, −1.224)`, rear `(±0.7055, 0.256, +1.224)` (metres). **The mesh's own origin
is the wheel centre**; the tire is deformed about it (`gt4_tire_fit_prepare/_position`,
`src/core/wheel_geometry.h:10-30`) and the rim scaled by the nominal rim radius (`car_model.c:186-194`).

### A3. The wheel MESH — where the rim and rubber come from

- The CAR4 carries the **rim at +0x20**, the **front tyre at +0x2c**, the **rear tyre at +0x30**
  (`car_model.c:155`: `off = u32(data + (piece?44+axle*4:32))`). Ka: `+0x20=0x68700, +0x2c=0x78cc0,
  +0x30=0x80900`.
- A **replacement rim** is an external file `/wheel/<menu|lod>/<name>` (`car_model.c:468`), named by the
  **WHEEL** table.
- **Spin and steer** are rotations of that mesh: `race_view.c:1522 wheel_place` builds
  `RotateY(steer) · RotateZ(camber)` and `RotateX(spin)` (comment: "hub, RotateY(steer), RotateZ(camber);
  RotateX(spin)"). So a wheel **spins about its local X (the axle)** and **steers about Y**.

### A4. What a MOD must provide (practical answer)

All three, for a spinning, steering wheel:

1. **a tagged wheel mesh** — triangles drawn under car callbacks 11..14 (attachment 1..4) in the body's
   MDLS, **or** a CAR4 with the tyre/rim meshes at `+0x2c/+0x30/+0x20`;
2. **the rim/tire meshes** themselves (or a donor's, referenced by `+0x2c/+0x30/+0x20`);
3. **the size rows** — a `DEFAULT_PARTS` row whose `CHASSIS/RACINGMODIFY/SUSPENSION/FRONTTIRE/REARTIRE`
   point at real `TIRESIZE` etc. rows (a mod can reuse a donor's bundle).

The banana has **none of these** — its body is all attachment 0, its CAR4 points `+0x20/+0x2c/+0x30` past
the file, and no wheel meshes exist — so `add_wheels` counts 0 and adds no wheel, by design.

---

## QUESTION B — CAMERAS

### B1. The per-car camera table lives in the CAR4 model file

- `src/platform/sdl/car_model.c:412-419` reads **CAR4 header `+0x80` = count, `+0x84` = offset**, 32 bytes
  (8 floats) per entry, into `car_model.cam_point[32][8]`:
  ```c
  unsigned n = u32(data + 0x80), at = u32(data + 0x84);
  for (i < n && i < 32 && at + (i+1)*32 <= size) → cam_point[i][0..7];
  ```
- `src/platform/sdl/car_model.h:32-36` names it: *"the race cameras' points on the car … entry 1 the chase
  camera's, (0, -1.7, -6)"*.

**Raw, the disc Ka (`/car/menu/fdeu0006`): count = 25, offset = 0x488.** First entries (x, y, z, a, b, c,
lens, spare):

```
point  0:  0.0000  -0.8922  -0.1269   0.0000   0.0000  0.0000  2.0503  0
point  1:  0.0000  -1.7000  -6.0000   8.0000   0.0000  0.0000  2.6990  0    <- chase
point  3:  0.9600  -0.6700   0.3500   4.0000 -180.0000 0.0000  2.0503  0
point  4: -0.9600  -0.6700   0.3500   4.0000 -180.0000 0.0000  2.0503  0
point  5:  0.0000  -0.6000   1.7340   0.0000   0.0000  0.0000  2.4142  0
point  7:  0.0009  -1.3955  -0.0207  10.4247  -0.0816  0.0000  1.7321  0
point  9:  0.0000  -0.6000  -1.6259   0.0000 -180.0000 0.0000  2.4142  0
```

### B2. Field meanings and how the engine uses them

Fields, from the readers: `[0..2]` = the point's **position on the car**, `[3..5]` = its **turns**
(z, x, y in degrees — `race_run.c:1200`, `turns(point[5], point[3], point[4])`), `[6]` = the **lens**
factor (`race_game.c:2059`: `sx = view==0||view==1 ? 1.73205f : 1/tanf(atanf(1/point[6])*4/3)`), `[7]`
unused on this path.

The engine places the race camera from this row plus the car's own pose:

- `race_view_camera_point` (`race_view.c:1139-1145`) returns the player car's point `i`.
- `gt4_race_run_camera` (`race_run.c:1164-1200`) turns that point + the car's drawn position/yaw/pitch/roll
  into `eye`/`rows` (`view = Rz·Rx·Ry·(world − p) + point`).
- `split_camera` (`race_game.c:2039-2065`) picks `view = g->camera`, computes the lens from `point[6]`.

So a car's views (chase / bumper / cockpit / side …) are **rows of this table**, chosen by the view index.

### B3. The banana's cameras ARE broken — and this is why

Our CAR4 writer (`model_write.c:431-438`) emits a **0x40-byte header** and writes `+0x18/+0x1c/+0x20/+0x2c/
+0x30` only — **it never writes `+0x80`/`+0x84`**. Those bytes then fall inside the MDLS, so the reader sees
garbage:

```
disc fdeu0006  : camera table count=25  offset=0x488   (the Ka's real cameras)
mod moonbanana : camera table count=140 offset=0xdc    (MDLS bytes read as a camera table)
```

count 140 clamps to 32, and the 32 anchors are MDLS bytes — so the banana's race cameras have no valid
points. This is the "broken cameras" the captain saw.

### B4. Searched, and found NOTHING (all negative — do not invent a table)

- **SpecDB**: dumped all **172** files under `/specdb/GT4_US2560` (`.dbt/.idi/.sdb`). **No camera table.**
  No name contains `cam`, `view`, `shot`, `bumper`, `chase`, `cockpit` or `hood`.
- **GT Mode / Arcade scripts**: the only "camera" strings in `gtmode.adc` are `usbcamera_connected` (the
  USB/EyeToy camera) and, in `gtmode.mproject`, `camera_trans`/`camera_rotate`/`view_angle` — those are the
  **showroom CarFace widget camera** (the dealer page), not a per-car chase/bumper camera.
- **`docs/MODELS.md`**: no camera field documented for CAR4.
- **The studio `camera.mot`** (`/menu/select/camera.mot`, `docs/SHOWROOM_SCENE.md`): the showroom's single
  constant camera, **not per-car**.

`src/app/race_game_cameras.inc:75` and `race_game.c:2045,2121` are the engine's own callers, and they read
only `race_view_camera_point` — i.e. the CAR4 table above. There is no other per-car camera source.

---

## WHAT THE SDK CAN HONESTLY OFFER TODAY

- **Wheels — read and tag**: read each triangle's `attachment` (`gt4_model`), show which are 1..4, and let the
  user mark a mesh as a wheel; read the size tables (CHASSIS `+4`, RACINGMODIFY `+2/+4`, SUSPENSION
  `+4/+10`, FRONTIRE/REARTIRE `+0` → TIRESIZE) to show the computed hub so the marker UI can pre-place the
  wheels. **Write**: author the tag into the MDLS (callbacks 11..14) — this is what the writer work (C3) is.
- **Cameras — copy or author the CAR4 `+0x80` table**: the rows are the game's own format; a mod can carry a
  donor's rows verbatim or let the user place points. Nothing else on the disc holds car cameras.

## WHAT NEEDS THE GAME (OUR CODE) TO CHANGE

1. **The CAR4 writer must emit the `+0x80` camera table** (`src/core/model_write.c` currently does not), or
   every authored car has broken cameras (the banana's `count=140` garbage proves it).
2. **A wheel mesh alone is not enough**: without the **tag** (attachment 1..4) and real size rows (or a CAR4
   with the tyre/rim meshes at `+0x20/+0x2c/+0x30`), the game cannot spin or steer it.
3. The **published test car has no wheels by design**; the moment the SDK adds wheels it also has to add the
   tag, the rim/tire meshes and the camera table — three things the SDK must write, not one.
