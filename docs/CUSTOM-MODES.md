# Custom Modes — implementation & asset log

Everything needed to add **another tile/icon** or **another race mode** without re-deriving it.
Measured on this repo; every number here came from a live capture or the authored widget tree.

Last updated: 2026-10-27 (Elimination shipped, banners in review).

---

## 1. Where the code lives

| File | What it owns |
|---|---|
| `src/app/custom_modes.inc` | Tile creation, icon face, navigation, page attachment, row build, mode entry |
| `src/app/main.c` | GT Mode entry path, where `custom_modes_attach()` must be called |
| `src/core/menu_catalog.inc` | Event/hall catalog, opponent picker limits |
| `src/core/menu_engine_objects.inc` | `menu_native_page` (line ~217) |
| `src/core/menu_widgets.inc` | Widget copy/bind behaviour |
| `src/core/ui.c` | `CarFace` / `ModelFace` |
| `src/core/physics/race_run.c` | Countdown gate, grid placement, rank comparator |
| `src/app/race_game.c` | Field size, phantom count, result board |
| `src/core/physics/session.c` | Arcade collision/contact context |

Reference research: `docs/vulcan4/RESEARCH-CUSTOM-GAME-MODES.md` (mode concepts), `docs/vulcan4/GOAL-MORE-THAN-6.md`.

---

## 2. Adding a new map tile / icon

### Geometry (GT Mode map, 640×480 UI space)

| Item | Rect |
|---|---|
| Home tile | `595,537,88,72` (child 51 of 56 under `TopRoot`) |
| Custom Modes tile | `600,465,88,72` (directly above Home) |
| Model face size | `88×72`, `normal=0` |

### Icon model — the part that went wrong three times

- Map icons are **`ModelFace`** and load from the **map package**: `image/gtmode/map/*.mdl`.
- The car-shaped icons already exist: `car_JPN.mdl`, `car_USA2.mdl`, `car_GER.mdl`, `car_FRA.mdl`, … plus `car_90a.mdl` (generic).
- **Do NOT** point a map tile at the showroom car path (`/car/menu/<model>`). It resolves in the car viewer, not here, and draws nothing — a silent blank tile.
- Regional models can render as a flat flag block at this size; `car_90a.mdl` is the safe generic car.

### Spin / focus behaviour (copied from Home)

| State | yaw | speed |
|---|---|---|
| Unfocused | 30 | 0 |
| Focused | 108 | 2 |

Hover label sits below the tile; navigation: `up` from Home focuses the tile, `down` returns.

### The attach trap (cost two playtests)

`custom_modes_attach()` originally ran only on the **startup** path. Entering GT Mode later (the normal route) never created the tile — the widget tree said `visible=1`, the screen had nothing.

**Always attach on GT Mode entry**, not only at boot. Verify with a pixel diff, never with an attach log.

---

## 3. Adding a new mode page

### Page shell

- Page is a **runtime child of `EventHallRoot`** — do not create a new UI root.
  `ui_project` roots are fixed at load (`nroots=35`), `ui_widget_copy` sets `script=NULL`, and `bind_tree` is name/class-bound, so a new root gets a blank module. A true script clone needs runtime root registration + deep `mp_node` clone + exposed script binding — not available yet.
- Background: `image/gtmode/event/hall.swf`, authored rect `0,0,640,480`.
  Clip facts: **300 frames @ 29.969 fps, 2 bitmaps** — bitmap0 at frame 0 (GT logo), bitmap1 at frame 150. Freeze at **5.2 s** for the darker hall art.
  Known gap: the real hall's **city/transition layer is not inside `hall.swf`**; it comes from the hall's own script. Until the script-clone API exists, our background stays GT-logo art.
- Music: `menu_request_music` with **`HALL`** → plays the hall BGM.
- Loading: pressing accept starts the normal GT loading screen; do not roll your own.

### Transition (the "fade" every menu has)

- Authored behaviour: `MColorTransition.panOutIn`, **period `0.250000` s**.
- Implement: fade to black (0.25 s) → switch page → fade in (0.25 s). Same on back, and before a race starts.

### Event row — authored `EventItem` geometry (640×480)

| Element | Rect |
|---|---|
| Row | `0,100,640,132` (pitch 132) |
| Ticket (art) | `43,0,296,132` |
| Stub (`TicketStubLong`) | `339,0,258,132` |
| `ticket_long.png` | `0,0,258,96` |
| **A-spec** `long_A.png` | `4,44,112,24` (absolute `343,144`) |
| **B-spec** `long_B.png` | `4,72,112,24` (absolute `343,172`) |
| Title text | `7,3,238,28` |
| Type / mode text | `46,100` |

**The A-spec / B-spec cells are IMAGES, not text widgets.** The words "A-spec"/"B-spec" are baked into `long_A.png` / `long_B.png` (112×24). The compact variant (`TicketStubShort`, `short_A/B` at 38×24) draws only a bare letter and reads as a stretched block — do not use it for a full event row.

Only two `TextFace`s exist in the row: `label` (title) and `type` (mode).

### Adding a second mode later

1. Duplicate the row build in `custom_modes.inc`, change title/type/thumbnail and the mode id.
2. Keep the row pitch 132 and the same x-columns; a second row occupies `y=232`.
3. Each mode's accept handler must set its own `laps` / rules before `race_start`.
4. Add the mode id to the page's focus chain, then verify navigation with a live capture.

---

## 4. Elimination mode (first custom mode)

- **Infinite laps** — the lap count is gone; the lap line is the *checkpoint trigger*.
- Each checkpoint: last-place car (ghost or official) is struck by thunder (Minecraft thunder sound + flash + bolt) and shows "`<CAR NAME>` HAS BEEN ELIMINATED".
- Race ends when one car remains, or the player is eliminated (result board).
- Field size comes from the >6-car (phantom) work: official conductor stays six, phantoms live in a second conductor; classification is merged natively (live position, finish order, results, prize/points).

---

## 5. Event banner art (the icon Boss asked for)

- Slot: the **Ticket** image, **296×132**, replacing the player-car render.
- Build at **2× (592×264)** and downscale with LANCZOS.
- Source: a **real in-game Elimination race frame** (original art, not AI) — capture at 1280×960 and crop a 2.24:1 band.
- **Crop around the HUD**: the frame's HUD lives at the top ~6–14 % and below ~70 %. A safe band is **y ≈ 0.29–0.65 of the frame height**, centred horizontally. A band that includes the top shows the race timer; one that includes the bottom shows "FULL THROTTLE" and the speedo.
- Three house styles produced so far (all 296×132 + 592×264):
  - **A — storm**: blue-steel tint, vignette, two lightning forks, white condensed wordmark + accent rule + subtitle "LAST CAR OUT · NO FINISH LINE".
  - **B — inferno**: warm red tint, sparse motion streaks, subtitle "SURVIVE · EVERY LAP".
  - **C — GT clean**: desaturated, dark top/bottom bars, "CUSTOM MODE" tag, big condensed "ELIMINATION", red rule + "LAST PLACE IS OUT".
- Typeface used: **Liberation Sans Narrow Bold** (closest available condensed sans to the GT UI face).
- Working script: `/tmp/make_banners.py` (regenerate by editing `SRC` and the `crop` band).
- QC at **296×132**, not at 592×264 — a banner that reads at 2× can turn to mush at game size.

---

## 6. Evidence rules (project law — `AGENTS.md`)

1. Live entry path only: launcher → GT Mode → the screen. Probe screenshots prove nothing.
2. Every visual change ships a **crop + measured diff** (changed pixels + bounding box) against the same crop before the change.
3. `visible=1`, `attached`, `drawn N` are state claims, **not** rendering proof.
4. Report what was not verified, explicitly.
5. A failed gate is a result. Never dress it as success.

---

## 7. Commit trail (Custom Modes)

| Commit | What |
|---|---|
| `31f87e3` | tile + page POC |
| `27de1aa` | elimination mode |
| `9c025d3` | thunder/flash/bolt |
| `964cedf` | tile moved above Home |
| `b2fa0cd` | **attach on GT Mode entry** (the real tile fix) |
| `9d2d55e` | car icon instead of house |
| `7fe7248` | spin only when focused |
| `73c64e7` | hover name |
| `457108c` | page from championship template |
| `c3d5345` | eliminated-car result board |
| `90615f4` | car face `88×72 normal=0` |
| `6a4609f` | page child of `EventHallRoot` |
| `a34e40f` | icon via `ModelFace` + SpecDB |
| `76342de` | hall background + HALL music |
| `d85a2e1` | map car model `car_90a.mdl` |
| `06a7e47` | authored ticket art in the row |
| `b821869` | row rebuilt as a hall list row |
| `4db5c30` | neutral car thumbnail + later hall frame |
| `36aba43` | 0.25 s fade + A/B rects |
| `9f1d648` | compact A/B (later reverted) |
| `8df1b7b` | **authored long stub** — final A-spec/B-spec |

Six-car regression anchors: CRC `fa2b3669` (120 frames), `b28c9af8` (600 frames).
`EventHallRoot` widget-tree hash must stay `7102d95650678b559650222c38a16e2c2f3d44835eb63e7aa382f2a9dcd920c9`.
