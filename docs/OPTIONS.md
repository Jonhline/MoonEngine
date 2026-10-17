# Options and presets

Every change that is not the original PS2 behaviour is a setting the player
can turn on or off. This page holds the rules and the registry of those
settings. The plan behind it is in [Editions](EDITIONS.md).

## Presets

| Preset | What it sets |
|---|---|
| **Classic** | Every setting at the PS2 behaviour. The game is the original; fidelity ([FIDELITY.md](FIDELITY.md)) is measured with Classic and no mods. |
| **Enhanced** | The project's recommended improvements. |
| **Player presets** | Named combinations the player saves, loads, renames and deletes. |

A preset only fills in settings. After choosing one, the player can change
any setting; the set is then shown as modified (for example "Enhanced,
modified") until it is saved as a player preset. Classic and Enhanced
themselves cannot be overwritten.

## Rules for every non-original change

1. **It is a setting.** No improvement is hard-wired on. Its Classic value is
   the original behaviour, which must keep working and stay tested.
2. **One registry row.** Add it to the table below in the same commit.
3. **In-game menu first.** For now each setting appears in the in-game
   Options menu (the PC additions to the original Options project, like
   `src/app/video_settings.c` does for the Screen page), using the original
   visual language: fonts, frames, cursor, sounds, transitions. The launcher
   comes later (see "For the launcher").
4. **One store.** Settings are read and written through one shared settings
   store, never through a file of their own, so the menu, the secondary
   launcher and the official launcher all see the same values. Until that
   store exists, the first feature that needs it must include its implementation
   and documentation here; existing `video.ini` display settings are migrated into it then.
5. **Boundaries only.** Settings are read at presentation, input, audio,
   mod loading and configuration boundaries, never scattered through game
   logic (see [Editions](EDITIONS.md)).
6. **Simulation.** A setting that can change a race result (rival driving,
   assists, rules) must be recorded in replays, ghosts and records, because
   they are re-simulated. Vehicle physics is not forked: "physics" settings
   are not planned unless Jonhline decides otherwise and records it here.
7. **Fidelity record.** The non-original behaviour is graded P in
   [FIDELITY.md](FIDELITY.md); its Classic value keeps the original grade.
8. **Tests.** Each setting has a test that runs it in both values; the
   Classic value must reproduce the original behaviour exactly.

## Categories

Group settings so a player can take one area without the others, for example
gameplay improvements without the new graphics:

| Category | Examples |
|---|---|
| Graphics | resolution and aspect, filters, shadows, reflections, renderer |
| Presentation and HUD | widescreen placement, HUD scale, PC-only screens |
| Audio | mixing, music choices beyond the original |
| Controls and input | keyboard and mouse, remapping, dead zones |
| Gameplay and quality of life | menu shortcuts, faster flows, assists defaults |
| Interface | new menu items and PC pages |
| Modding | mod loading and content packs |
| Tourist Trophy | settings of the Tourist Trophy part, when it exists |

## Registry

One row per setting. `id` is stable (it is the key in the store and in
player presets): lowercase, dotted by category, for example
`graphics.soft_shadows`.

| id | Category | What it changes | Classic | Enhanced | Applies | Recorded in replays | Menu location | Launcher | Commit |
|---|---|---|---|---|---|---|---|---|---|
| controls.desktop_menu | Controls and input | Independent hover/previews, manufacturer preselection, full painted-button clicks, viewport/authored-regulation wheel scrolling, pause pointer and WASD menu navigation | Off; existing pad-style adapters | On | After OK, menu input boundary | No; menu-only, no simulation change | Options > Screen Settings > Desktop Input | nÃ£o | This change |
| controls.gtmode_edge_pan | Controls and input | Pan GT Mode map near viewport edges without selecting landmarks; requires Desktop Input | Off | On | After OK, menu input boundary | No; no simulation change | Options > Screen Settings > Map Edge Pan | nÃ£o | This change |
| interface.smooth_mouse_cursor | Interface | Damped cursor motion, restrained inertial lean and clickable-target feedback; script icon crossfade | Off; direct mouse position when Desktop Input is enabled; original pad cursor with Classic | On | After OK, presentation/input boundary | No | Options > Screen Settings > Controls > Smooth Cursor | nÃ£o | This change |
| interface.mouse_hover_feedback | Interface | Gently enlarge static Options icons/labels, including Exit; subtle paint-swatch varnish and animated reflection; preserve existing game hovers | Off; authored widget faces only | On | After OK, presentation boundary | No | Options > Screen Settings > Controls > Hover Feedback | nÃ£o | This change |
| interface.compact_car_strip | Interface | Draw car thumbnails at 80% of their disc dimensions; retain original assets and smoothly selected magnification | Off; disc geometry | On | After OK, presentation boundary | No; menu only | Options > Screen Settings > Rendering > Compact Car Strip | nÃ£o | This change |
| interface.main_menu_overview | Interface | Show five existing entries in the original vertical style, with mouse prelight, full-row clicks and viewport wheel scrolling | Off; original scrolling three-entry presentation | On | After OK, presentation boundary | No; menu layout only | Options > Screen Settings > Main Menu | nÃ£o | This change |
| interface.input_hints | Interface | Adapt button legends to the latest keyboard/mouse or controller input | Off; original PS2 symbols | On; automatic keyboard/Xbox/PS labels | After OK, input/presentation boundary | No | Options > Screen Settings > Input Hints | nÃ£o | This change |
| graphics.ui_filter | Graphics | Existing optional UI sampling; migrated from ui_filter | Original (2) | Smooth (0) | Presentation, after OK (draft preview) | No | Options > Screen Settings > UI Filter | nÃ£o | Migrated in this change |
| graphics.text_sharpness | Presentation and HUD | Existing text sharpening; migrated from text_sharpness | 0 | 0 | Presentation, after OK (draft preview) | No | Options > Screen Settings > Text Clarity | nÃ£o | Migrated in this change |
| graphics.switch_scene_resolution | Graphics | Switch-only internal 3D size; output/HUD remain full resolution. Classic 640x448, Balanced 960x540 portable / 1280x720 docked, Performance 640x360 | Classic (0), PS2-sized render target | Performance (2) | After OK at the presentation boundary; targets rebuild on next 3D scene | No; presentation only, physics and race results unchanged | Options > Screen Settings > 3D Resolution; Classic/Enhanced preset | não | This change |
| graphics.upscaling | Graphics | Existing optional FSR scene scaling; migrated from upscaling | Native (0) | Native (0); player may select FSR | After OK | No | Options > Screen Settings > Upscaling | nÃ£o | Migrated in this change |
| graphics.spatial_visibility | Graphics | Reject conservative spatial blocks before individual group tests; retain authored course visibility lists, primitive order and flat fallback | Off; retained flat group traversal | On | After OK, renderer boundary | No; presentation only | Options > Screen Settings > Spatial Visibility | Not yet | Scene pipeline |
| graphics.resident_reflections | Graphics | Keep the six reflection views and their sphere-map remap on the GPU in OpenGL and Vulkan | Off; retained CPU remap | On | After OK, renderer boundary | No; presentation only | Options > Screen Settings > GPU Reflections | Not yet | Scene pipeline |


## PC settings pages

The native Screen Settings extension uses the original Options project's
fonts, checkerboard, pane, dropdowns, cursor and confirmation buttons. PC
controls are split into three tabs instead of shrinking a single long list:

- **Display:** aspect ratio, window mode, resolution and VSync.
- **Rendering:** UI filtering, text clarity, 3D upscaling, spatial visibility
  and GPU reflections.
- **Controls:** desktop input, map edge pan, input hints and main menu overview.

The PC preset remains visible on every tab. Tab buttons accept normal
pointer and controller navigation; L1/R1 (Q/E on the keyboard) also cycle
tabs when a dropdown is closed. The selected tab has a gold label.
Changing tabs preserves the shared draft. OK applies and saves all tabs;
Cancel discards their changes and restores the current quality preview.
Missing desktop-input preferences follow the selected preset, so older
Classic files do not enable mouse/keyboard improvements implicitly.
Reopening the page starts on Display. The Switch page retains its existing
seven rows. This PC page organization is an intentional adaptation (P).

`desktop_settings_app`, `scene_settings_app` and its deliberate negative
exercise the actual executable with isolated preferences and hidden windows.
They cover independent options, presets, reload, cross-tab edits and Cancel.
Screenshots check the three tabs in 16:9 and the retained 4:3 presentation.

## Shared PC store (16 October 2026)

`src/app/options_store.h` / `.c` is the shared, instance-owned preferences
API. The normal PC path is `%APPDATA%/MoonEngine/MoonEngine/options.ini`.
The first read imports the previous `video.ini` if the destination does not
exist; it leaves the original file intact. Existing video keys migrate to
`graphics.<legacy-key>` and unknown keys are retained. Writes replace a
complete temporary file atomically. `--config-file` remains an isolated path
override for tests/existing callers; it accepts both legacy and dotted keys.
No launcher source or Switch preference flow is changed in this work.
The official launcher can use the same API and path when Dante integrates it.
The secondary launcher's existing explicit file override remains supported;
its own settings UI has not been migrated to this API yet. The normal
secondary-launcher handoff of legacy video.ini uses the canonical store
after migration, so a later launcher rewrite cannot erase the new settings.
Once migrated, the in-game store takes precedence over that legacy file.

Original script-owned Options are still held by the existing game options
service and its existing sidecar; its path is preserved during the video
migration, so units, pad mappings and other original preferences survive.
This commit consolidates **PC additions**, without changing the original save
or replay formats. Menu widgets keep drafts; Cancel does not persist them.

The Screen Settings extension includes Desktop Input and a PC Preset row.
Classic and Enhanced fill the registered mouse/sampling settings; changing
an individual setting displays Modified. Host resolution, monitor mode and
backend are retained. Named player presets and bringing every historical PC
addition under the registry remain follow-up work; this is not a claim that
the complete preset design is finished.

Desktop Input adds mouse hover/left-release activation/right-release Back,
viewport wheel scrolling (parameter steps for hovered bars), and WASD menu navigation.
Existing arrows/Enter/Escape and pad adapters remain available in Classic.
Text entry is not remapped to navigation. The pointer uses the user's disc
cursor texture, original angle animation, widget focus sounds and authored
hover faces; its tip tracks the host mouse immediately (P). No pointer asset
is packaged. Clicks use current painted bounds with clipping/list offsets;
hidden, insensitive and unpainted widgets are excluded. Menus, authored
modal loops, native confirmations, player profile selection and race pause are wired.
Hover runs authored preview callbacks without committing list selection. Map
landmarks retain their original hover rotation. Opening a dialog preserves
the mouse position. Horizontal strips pan at their edges under Desktop Input;
GT Mode map panning has its own Map Edge Pan switch. These deliberate P
values are 12% viewport per wheel unit, a 10% edge band and a maximum of
1.5 viewport widths/heights per second, clamped to authored content bounds.
Input Hints has a separate switch; it transforms displayed legends only,
leaving original text and bindings intact.
Dragging sliders, remappable desktop bindings, mouse driving/camera additions
and mouse-specific Photo/Logger tools are not implemented by this first step.

Tests: `desktop_options_store` (+ intentional negative), `desktop_pointer`
(+ negative), `desktop_navigation` (+ negative), `desktop_events` (+ negative), `desktop_settings_app` and the
existing `options_lifecycle` family / `menu_navigation`. The app test uses
isolated files and checks both values, OK/Cancel, reload, Modified and both
built-in presets. No user profile is changed by the tests.

## For the launcher (Mr. Dante)

The official launcher will show the same settings and presets. It should:

- read this registry for ids, categories, Classic/Enhanced values and how
  each setting applies (immediately, next race, after restart);
- use the shared settings store, not a copy, so changes made in the game's
  menu and in the launcher are the same values;
- offer the same presets (Classic, Enhanced, player presets) with the same
  "modified" rule;
- keep the plain `launcher*` names in the player data folder; the existing
  launcher is the secondary one and uses `secondary_launcher*`
  ([LAUNCHER.md](LAUNCHER.md)).

The "Launcher" column of the registry says whether each setting is already
in the official launcher.


Mouse feedback (16 October): Smooth Cursor uses damped presentation motion
and restrained inertial lean. Positional lag is bounded to 24 UI units;
cursor icon crossfade remains 90 ms.
A new mouse/modal context starts at the current mouse position. Hover
Feedback eases small icon/text enlargement over 60 ms only on otherwise-static
Options tiles/Exit. It adds no rectangular overlays; GT Mode model animation,
main-menu text prelight and authored hover faces remain unchanged.
Hit testing continues to use the actual input position. Main menu actions
accept the entire row, beyond the authored narrow label client. Screen
Settings uses a wider, continuous light-gray PC pane with the existing
frames, fonts and buttons. Dropdown hover lights the pointed row without
committing it. Vertical lists clip to their viewport and wheel scrolling
draws the rows in the viewport independently of controller selection.


See [Input support and touch roadmap](INPUT.md) for current controls, source
boundaries, known gaps and the planned Switch/Android gesture work. Touch
settings enter this registry only with their implementation; this plan adds
no enabled touch behaviour or duplicated settings store.



Cursor motion refinement (16 October): Smooth Cursor retains clickable-target
feedback but replaces discrete target zones with a continuous resting tilt
(default-alignment targets: 0 to -16 degrees across their width). Hover grows
the cursor by up to 6%, anchored at its tip, easing over 60 ms. Free-pointer
resting angle, explicit authored alignments, edge direction and icon changes
remain distinct context signals. Position and rotation use critically damped
presentation springs (rates 70/s and 45/s). Motion/acceleration adds at most
12 degrees of transient lean and relaxes at rest. This supersedes the earlier
30/55 ms easing and nine-zone mapping; icon crossfade remains 90 ms and lag
is still bounded to 24 UI units. No vehicle physics is involved. Raw hit
testing and Classic controller animation are unchanged.


GT Mode Map Edge Pan accepts the full UI screen, including the black text/
tooltip footer. Pointer positions in those bands exert bounded edge pressure
on the map viewport. Icon hover/click bounds remain clipped to the visible
map, so panning over text does not activate a hidden landmark. The same
setting controls this behaviour; Classic/Off and content limits are retained.

The former `interface.regulation_separators` key is retired and ignored.
Regulation presentation follows the same baseline geometry in every preset.
Regulation rows use the original GS scissor conversion (00215FE0 ->
004A2DF8), truncating scaled origin and extent independently. Slot 2
confirms the variable gaps; no fixed two/four-unit row cut remains.
This is baseline presentation, including Classic; row allocation and
scroll are unchanged. The current adapter covers retail NTSC interlaced
640x448 presentation; other console display modes are not certified.
Older configuration files remain readable.

## Switch presentation store

Switch uses the same `moon_options` store in `sdmc:/switch/opengt4/options.ini`.
On first use, existing video preferences are migrated from `config.ini`; ISO
and save bootstrap paths there remain unchanged. Unknown keys are preserved.
Migration retains Balanced 3D resolution rather than lowering existing quality.
Classic sets PS2-sized 640x448; Enhanced recommends Performance 640x360. The
player can select any resolution independently; changes mark the preset modified.
The original 16:9 output and full-resolution HUD remain active on Switch.
