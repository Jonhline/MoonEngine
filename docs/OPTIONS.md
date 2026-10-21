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
| audio.launcher_music_volume | Audio | Secondary launcher music level, independently of game audio | 78/128 (host default) | 78/128 | Immediately in launcher | No | Launcher > Audio (explicit launcher-only request) | Secondary: yes; official: no | 2026-10-18 |
| audio.launcher_effects_volume | Audio | Secondary launcher sound effects level | 128/128 (host default) | 128/128 | Immediately in launcher | No | Launcher > Audio | Secondary: yes; official: no | 2026-10-18 |
| audio.launcher_music_muted | Audio | Mute only launcher music, retaining its level and audible effects | Off (host default) | Off | Immediately; M shortcut or Music button | No | Launcher > Audio / Music button | Secondary: yes; official: no | 2026-10-18 |
| graphics.antialiasing | Graphics | PC scene sampling: Original retains the previous renderer path (OpenGL up to 4x MSAA, Vulkan single sample); Off uses single sample; SSAA 150%/200% renders larger and downsamples; Edge AA filters final scene edges after HDR without touching UI | Original (0) | Edge AA (4) | After OK, next 3D scene; clears FSR and resets 3D scale | No; presentation only | Options > PC Settings > Graphics > Anti-Aliasing | Secondary: yes; official: no | This change |
| graphics.render_scale | Graphics | PC 3D resolution at 50/75/100 percent of output, independent of full-resolution UI/HUD | 100% (2) | 100% (2) | After OK, next scene; selecting clears SSAA and FSR | No | Options > PC Settings > Graphics > 3D Resolution | Secondary: yes; official: no | This change |
| graphics.texture_smoothing | Graphics | PC model point-sampling override; smooths formerly nearest-sampled surfaces, retaining mip/wrap rules | Off (0) | On (1) | After OK, next model draw | No | Options > PC Settings > Graphics > Texture Smoothing | Secondary: yes; official: no | This change |
| graphics.texture_filtering | Graphics | Independent PC trilinear/4x/8x/16x anisotropic filtering for ordinary repeat/clamp textures; special GS region addressing retains the original taps; device limits cap anisotropy | Original (0) | Anisotropic 8x (3) | After OK, next model draw; no texture/model reload | No; presentation only | Options > PC Settings > Graphics > Texture Filtering; shared launcher Quality list | Secondary: yes; official: no | 2026-10-18 |
| graphics.car_shadows | Graphics | Project body and posed wheels, including fitted body parts, onto the local receiver plane; GPU soft edge; retain authored contact blot | Original (0) | Projected (1) | After OK, next race/replay draw; no model reload | No; presentation only | Options > PC Settings > Light & Shadow > Car Shadows; shared launcher Quality list | Secondary: yes; official: no | 2026-10-18 |
| graphics.material_shading | Graphics | Evaluate authored lighting coefficients, shininess and reflection coordinates per pixel; preserve pass masks and opacity | Original (0) | Per-Pixel (1) | After OK, next lit-model draw; no model reload | No; presentation only | Options > PC Settings > Graphics > Material Shading; shared launcher Quality list | Secondary: yes; official: no | 2026-10-18 |
| graphics.reflection_quality | Graphics | Generated race/replay faces and sphere maps, plus studio panel maps; authored environment textures stay unchanged | Original 128 (0) | High 256 (1); Ultra 512 (2) is optional | After OK, rebuild generated targets on next draw; no model reload | No; presentation only | Options > PC Settings > Graphics > Reflection Quality; shared launcher Quality list | Secondary: yes; official: no | 2026-10-18 |
| graphics.shadow_maps | Graphics | Local car depth shadows on course geometry and car surfaces; alpha-cutout casters and weighted soft filtering | Off (0) | High 1024 (2); Low 512 (1) and Ultra 2048 (3) optional | After OK, next race/replay frame; targets resize lazily | No; presentation only | Options > PC Settings > Light & Shadow > Shadow Maps; shared launcher Quality list | Secondary: yes; official: no | 2026-10-18 |
| graphics.modern_lighting | Graphics | GGX direct material response, elevation-dependent daylight colour, sky fill and SDR filmic highlight compression | Original (0) | Modern (1) | After OK, next race/replay material draw | No; presentation only | Options > PC Settings > Graphics > Lighting; shared launcher Quality list | Secondary: yes; official: no | 2026-10-18 |
| graphics.daylight | Graphics | Procedural daytime atmosphere, relative relighting of baked course colours and nearby course shadow casting; source assets retained by Classic | Off (0) | On (1) | Next race/replay draw | No; presentation only | Options > PC Settings > Light & Shadow > Daylight | Secondary: yes; official: no | 2026-10-18 |
| graphics.solar_hour | Graphics | 24 manual solar hours plus Course; index24 selects source day/night default. Legacy day-only clamp when Night Sky is Off | 12:00 (5), inactive | Course (24) | Next draw | No | Options > PC Settings > Light & Shadow > Solar Time | Shared launcher API | 2026-10-20 |
| graphics.solar_cycle | Graphics | Sun / Moon Cycle: Fixed, Real Time, 60x. Full24h with Night Sky; legacy07..17 reflection otherwise | Fixed (0) | Real Time (1) | Recorded frame clock; pause freezes it | No | Options > PC Settings > Light & Shadow > Sun / Moon Cycle | Shared launcher API | 2026-10-20 |
| graphics.hdr | Graphics | RGBA16F main scene, SDR filmic resolve and restrained bloom before UI/FSR | Off (0) | On (1) | Next scene; float attachment rebuild | No | Options > PC Settings > Light & Shadow > HDR | Secondary: yes; official: no | 2026-10-18 |
| graphics.sun_rays | Graphics | Off (0), retained screen-space shafts (1), shadow-mapped world-space volumetric integration (2); requires HDR/Daylight | Off (0) | Volumetric (2) | Next scene; choosing Volumetric enables HDR/Daylight | No; presentation only | Options > PC Settings > Light & Shadow > Sun Rays | Shared launcher API | 2026-10-18 |
| graphics.ambient_occlusion | Graphics | Optional depth-based cavity/contact shading; screen-space approximation requiring HDR/Daylight | Off (0) | On (1) | Next scene; selecting On enables HDR/Daylight | No; presentation only | Options > PC Settings > Light & Shadow > Ambient Occlusion | Shared launcher API and personal presets | 2026-10-18 |
| controls.touch_menu | Controls and input | Switch handheld and Android first tap selects/previews, Android pause cards use authored actor identities, second tap on the same item confirms, direct list/map drag, parameter bars and native confirmation/profile buttons; ignores duplicate mouse events; hides cursor artwork after touch and restores it on controller input; retains desktop scenario-specific hover between taps, uses second-press compression without added colored overlays or instructional text; original green Exit fallback appears only after touch on screens lacking an existing return control (including pause) | Off; controller navigation | On | After OK, menu input boundary | No; no simulation change | Options > Screen Settings > Touch Menus (Switch); Controls > Touch Menus (Android) | não | This change |
| controls.touch_race | Controls and input | Android-only multitouch race overlay routed through the existing pad configuration; external controllers hide and supersede it | Off; physical controller | On | After OK, Android input boundary | Resulting ordinary pad commands are recorded; layout is not simulation state | Options > Screen Settings > Controls > Race Touch Controls (Android) | no | Android foundation |
| controls.touch_size | Controls and input | Small/Medium/Large Android race buttons and steering area | Medium; overlay off | Medium | After OK, Android input/presentation boundary | No; ordinary commands are recorded | Options > Screen Settings > Controls > Control Size (Android) | no | Android foundation |
| controls.touch_opacity | Interface | Android race overlay opacity, 20–100 percent | 60%; overlay off | 60% | After OK, Android presentation boundary | No | Options > Screen Settings > Controls > Control Opacity (Android) | no | Android foundation |
| controls.touch_layout | Controls and input | Steering on the left or mirrored on the right; not a free-position layout editor | Steering left; overlay off | Steering left | After OK, Android input/presentation boundary | No; ordinary commands are recorded | Options > Screen Settings > Controls > Control Layout (Android) | no | Android foundation |
| controls.touch_steering | Controls and input | Continuous analog drag or digital left/right steering; no solver changes | Analog drag; overlay off | Analog drag | After OK, Android input boundary | Resulting ordinary pad commands are recorded | Options > Screen Settings > Controls > Steering (Android) | no | Android foundation |
| controls.desktop_menu | Controls and input | Independent hover/previews, manufacturer preselection, full painted-button clicks, viewport/authored-regulation wheel scrolling, pause pointer, slider click/drag with the disc hand and manufacturer/mission-grid edge panning, coalesced hover sounds, fullscreen mouse confinement (released on focus loss) and WASD menu navigation | Off; existing pad-style adapters | On | After OK, menu input boundary | No; menu-only, no simulation change | Options > Screen Settings > Desktop Input | nÃ£o | This change |
| controls.gtmode_edge_pan | Controls and input | Pan GT Mode map near viewport edges without selecting landmarks; requires Desktop Input | Off | On | After OK, menu input boundary | No; no simulation change | Options > Screen Settings > Map Edge Pan | nÃ£o | This change |
| interface.smooth_mouse_cursor | Interface | Damped cursor motion, restrained inertial lean and clickable-target feedback; script icon crossfade | Off; direct mouse position when Desktop Input is enabled; original pad cursor with Classic | On | After OK, presentation/input boundary | No | Options > PC Settings > Controls > Smooth Cursor | nÃ£o | This change |
| interface.mouse_hover_feedback | Interface | Gently enlarge static Options icons/labels and the green Exit pictogram (including its authored alternate face); whole vertical list-card enlargement (background, text, price and swatch together), including event/championship ticket artwork and labels; light standalone paint-swatch enlargement, varnish and animated reflection; adjustable-bar hover; car-strip mouse hover uses authored blob magnification/interpolation; preserve existing game hovers | Off; authored widget faces only | On | After OK, presentation boundary | No | Options > PC Settings > Controls > Hover Feedback | nÃ£o | This change |
| interface.compact_car_strip | Interface | Draw car thumbnails at 80% of their disc dimensions; retain original assets and smoothly selected magnification | Off; disc geometry | On | After OK, presentation boundary | No; menu only | Options > PC Settings > Controls > Compact Car Strip | nÃ£o | This change |
| interface.graphics_swap_key | Interface | F7 in a race or replay swaps the picture between Classic graphics and the player's own, with a two-second label; nothing is saved and leaving the race restores the player's graphics (keyboard only) | Off; F7 does nothing in races | On | Immediately, race/replay presentation | No; presentation only, simulation untouched | Launcher Options > Interface > Graphics Swap Key; first-drive guide, Menus | não | This change |
| interface.controller_tiles | Interface | The game's Options > Controllers by player: Vibration, Controller 1P, Controller 2P (the DUALSHOCK 2 tiles, same page and script), Steering; the six per-model wheel tiles (Driving Force Pro, GT Force, Driving Force; 1P/2P) are hidden and the rows below move up. On the PC each port takes one pad or one wheel, any model, and a wheel's buttons reach the game through that port's button config | Original; the disc's ten tiles | By Player | The next time the Options open | No; menu only | Options > PC Settings > Controls > Controller Tiles; launcher Options > Interface | não | 19 October 2026 |
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

## Android input boundary (17 October 2026)

The Android Controls tab reuses the authored Screen Settings widgets and the
shared options store selected by the app's `--config-file`. It exposes Touch
Menus, Race Touch Controls, Control Size, Control Opacity, Control Layout and
Steering. Apply/Cancel retain their existing shared draft semantics. Classic
turns touch/menu and race overlays off, keeping controller navigation; a
physical controller is then needed on a phone. Display/Rendering remain the
shared pages, with Vulkan/fullscreen required by this Android foundation.

The mobile pad uses the existing DualShock button configuration: Cross gas,
Square brake, Circle handbrake, L2/R2 shifts and Select camera with defaults.
Changing original button assignments also changes these virtual buttons.
Steering supplies the ordinary left analog axis. Replays receive the normal
recorded commands, with no alternate physics or new replay format. The
current size and mirrored layouts do not implement per-button dragging,
gyroscope steering, profile sharing or haptics.

## PC scene quality

Vulkan and OpenGL expose the same SSAA, 3D scale and texture smoothing controls.
SSAA 200% shades four times as many scene pixels; 150% shades 2.25 times as
many. UI/HUD are composed after the scene, at display resolution. No temporal
history or motion vectors are used. Existing configurations retain their
previous sampling until the player chooses an option or reapplies Enhanced.
The Original entry preserves each backend's retained path; it does not claim
that the previous PC MSAA setup reproduces every PS2 GS sampling detail.
FSR and reduced 3D scale are performance choices; selecting one clears SSAA,
and selecting SSAA clears those reductions. Switch/Android keep their paths.
Alt+Enter (including keypad Enter and either Alt key) toggles desktop fullscreen;
it is consumed before game/menu confirmation. Screen Settings reads the current
window mode when opened, and OK persists it through the existing shared store.

### Secondary launcher quality controls (17 October 2026)

The current studio launcher exposes Anti-Aliasing, 3D Resolution and Texture
Smoothing under Options > Quality, using the same `video_setting` API and
shared store as Screen Settings. Changes apply at Start; the renderer choice
also remains available under Video. SSAA, reduced resolution and FSR keep
the same exclusions as the in-game menu. This user-requested secondary
launcher addition does not implement the planned official launcher.

## PC video page layout

Desktop Screen Settings separates Display (window and output), Rendering
(AA, 3D resolution, texture sampling, material shading, reflection quality and lighting),
Shadows (geometric ground shadows and local shadow maps), Daylight (solar time,
cycle, HDR and shafts), Presentation (UI
filtering, text, upscaling, spatial visibility and GPU reflections), and
Controls (desktop input and interface behaviour). L1/R1 cycles the six
pages. Each page holds at most seven rows, spaced 28 virtual pixels apart;
controls keep their authored height. This replaces the crowded eleven-row
Rendering page. Draft, Cancel, OK and preset semantics are unchanged.

## Personal preset files — first implementation (18 October 2026)

P: optional `interface.preset_tools` (Classic Off / Enhanced On), exposed as
Preset Files in Screen Settings > Presentation and the official studio's
Interface category. On Windows it reveals Load/Save Preset buttons in the game
and corresponding actions in the official studio's General category. The game
buttons reuse the authored rounded button style; the six existing tab labels
now have inset text and wider gaps rather than touching one another.

Save Preset creates a named `.moonpreset` file; saving under another name
provides a copy/export. Load Preset opens an existing file. The chooser starts
in the `presets` folder beside options.ini, and can also select another folder.
The Windows picker supplies file rename/delete operations; a dedicated in-game
preset library and explicit rename/delete buttons remain planned.

Files use bounded version-1 key/value text (`MoonEnginePreset=1`, `name=...`),
matching the existing options store conventions. This replaces the plan's
initial JSON proposal for this first portable implementation. Only allowlisted
presentation and desktop-menu options are included. Resolution, aspect, window
mode, renderer, paths, saves, physical-device assignments and original script
Options remain local. Switch/Android file pickers and platform-specific preset
fields are not implemented in this milestone.

The game imports into the existing draft: OK applies, Cancel preserves the
current values and file. The official launcher applies through the same
video_settings/store API. Personal names survive restart and show modified
when an option changes; Classic/Enhanced cannot be overwritten by a personal
file. Imports reject unknown versions/keys, duplicate keys, invalid values,
conflicting sampling modes and malformed files before touching the destination.
Changing the enabled tools does not change physics or the original save format.

Coverage: personal_presets, launcher_quality and desktop_options_store, each
with a negative; personal_presets_ui exercises real ScreenRoot Load/Save,
Cancel, Apply and export using an injected file choice in a hidden window.
The native file-dialog interaction itself is not automated. Visual captures:
build/personal-presets-proof. This is a first preset-file delivery, not a claim
that the complete settings/library redesign or a 1:1 Classic menu is finished.

## Optional native save transfer — 18 October 2026

P: `interface.save_transfer_tools`, Classic Off / Enhanced On. The independent
Save Transfer option is available in Screen Settings > Presentation and the
official launcher's Interface category. On Windows it changes the player-profile
footer to Import / Export / Media; Off restores the previous two import buttons.
The existing import picker now accepts `.ogt4` alongside PS2 archives/GAMEDATA.

Export writes the persisted active player using the existing checksummed native
save format. If there is unsaved progress, the UI offers to save it first;
cancelling leaves the export unstarted. Export does not switch the active player
or clear dirty state itself. The Windows destination check rejects paths in the
live save directory, avoiding accidental replacement of a live slot.

Native imports use an empty slot, validate the selected file's envelope and typed
state, and require a valid player name before writing. A malformed selected file
does not silently fall back to its neighbouring backup. The pre-existing PS2
importer remains the fallback for other formats. Photos, films and replay files
are separate media and are not included in this profile-only transfer; the UI
states this explicitly. A bundled media/archive export remains future work.

Evidence: profile_storage verifies payload-identical export/import with real
career/garage data, unchanged active/dirty state on export, occupied/corrupt
imports and the protected destination. Its transfer-specific negative changes
one payload byte and fails. launcher_quality checks independent persistence and
Classic/Enhanced defaults. Visual on/off captures: build/save-transfer-proof.
The updated preset UI fixture also verifies explicit navigation between the
Load/Save buttons and the existing footer. Host file picker interaction is not
automated. Switch/Android file selection remains pending.

## Complete player package — 18 October 2026

This supersedes the profile-only export limitation above. The optional Export
button now creates one `.moonplayer` package containing the selected saved
player and the entire shared `photos`, `replays`, `films` and `imports` library.
The original native profile format is retained inside the versioned ZIP package;
legacy `.ogt4` and PS2 imports remain supported. Ghosts, fitted-car replay data,
film streams, photo/replay playlists and auxiliary/backup files inside these
library folders travel with their original relative names and contents. Original
music/slideshow option blobs remain part of the serialized player profile.
Other player slots, host paths and disc/executable files are not exported.

The library is shared across local players, so the complete shared library is
included instead of guessing ownership. Import requires an empty player slot.
All package paths, profile state and member CRCs are validated in staging before
live library writes. Identical existing files are reused; a same-name file with
different bytes stops the whole import with a conflict message. No library files
are silently skipped or overwritten. Failed profile writes/activation remove
only files created by that import. A successful import invalidates cached photo
and replay list reads. Files are streamed instead of loading the full library
into RAM; the original access indicator stays active while the UI pumps.

Interchange limits: 16384 library files, 512 MiB per file, 16 GiB in total and
16 nested directories. Exceeding a limit, unreadable files or linked paths fails
the operation explicitly. The archive contains its own manifest and accepts only
its known roots; path traversal, aliases, duplicate/case-colliding names and
unsupported manifests are rejected. Empty directories carry no player data and
are recreated as needed. This is transaction safety for ordinary I/O failures,
not a power-loss recovery guarantee across the entire multi-file library.

Tests: save_package covers exact profile payload and media bytes, a decodable
replay and ghost, lists, film/auxiliary/legacy bytes, idempotent shared content,
conflicts, CRC damage, invalid paths/manifests/profiles and both profile-write
and activation failure rollback; its negative changes a photo byte and fails.
save_package_ui exercises actual Export/Import with a real career fixture and
generated media in separate save directories. Native picker selection alone is
injected in hidden-window fixtures; accented filenames are also covered. On-device Switch/Android picker validation
is still pending; the current visible picker is Windows.


### Post-disc first-drive guide (18 October 2026)

P: `launcher.guided_intro` (0/1, default 1) controls the optional introduction
once the selected SCUS-97328 disc is validated and its assets can be loaded.
`launcher.guide_seen` records dismissal; viewing the introduction preserves the
preset's modified flag. Neither key is a game setting or a portable preset.
F7 on the launcher reopens it on the Graphics page.

Five pages in the launcher's language (OPTIONS glass panel and tabs, dealer metal
band with BACK/SKIP/NEXT and the game's cursor, help strip): Welcome, Preset
(Start From Classic/Enhanced, or the player's own when already modified), Graphics
(Lighting, Material Shading, Reflection Quality, Car Shadows, Shadow Maps, Daylight,
HDR, Sun Rays, Ambient Occlusion, Texture Filtering, Anti-Aliasing), Menus (Desktop
Input, Input Hints, Smooth Cursor, Hover Feedback, Main Menu, Map Edge Pan, Compact
Car Strip, Aspect Ratio) and Ready (summary of changes, sound shortcuts).
Choices are a draft. Each graphics option is compared on its own: the same frozen
frame of the Gran Turismo Skyline GT-R (`r34_gtr_v_gtcolor_01`) on the Tsukuba grid,
the course every PC renderer addition was validated on, drawn by the race renderer
(`race_view_request`/`race_view_draw`, not the Photo Travel film path), the option at its original value
left of a draggable divider and the chosen value right (Enhanced's value, marked as
not chosen, while the option is still original), cropped where the effect shows,
with what it does, what to look at and its cost. Space/Y shows either look full
screen, Tab/X flips it. Sun Rays and Ambient Occlusion are compared with HDR and
Daylight on, as they need them, and say so. The frozen scene is only rendered into
GPU captures (`video_grab`): the backdrop when its look changes and the two sides once
the selection rests (~130 ms), at most one render per frame, so scrolling the list
does not stall even at SSAA 200%/HDR. The Menus page includes Graphics Swap Key. Rows use the Options' own labels and choices.
DONE (and OPTIONS, which then opens the launcher Options) writes the preset if it
changed and then every differing option through `video_setting_set`, so the result
reads as in Options (e.g. Enhanced (modified)). SKIP, Escape/B and closing write
nothing; the guide verifies the store is untouched. The comparison is scoped to
this scene; Classic is not a claim that the port is fully PS2-exact.

The launcher Options show the same thing (19 October 2026, P, no setting):
every tab uses the guide's two columns, the rows on the left and the focused option
on the right with the guide's own words (what, look at, cost; the Menus note for
interface rows; the Options help for rows the guide does not cover). A QUALITY row
the guide compares gets its comparison: a short clip of Tsukuba, the camera swinging
around the car (16 frames recorded once per look, played back and forth), at the
original value left of a draggable divider and the current value right (Enhanced's,
marked as not chosen, while it is still original). Theme, Light Colour and Scene's Room
show the launcher's own room rendered by the showroom in each look (light and dark; the
colour before, the chosen one and the next) from START's framing, taken only once the car
is fully there (behind a Scene its fade runs inside the render) and kept for every look
taken until the car or the window changes, so going through the colours never shows a gap
(`launcher_renders.inc`). Aspect Ratio, Controller Tiles, Desktop Input, Hover Feedback, UI
Filter and Text Clarity show the game's own Options menu printed with each of two values
(`launcher_prints.inc`): the launcher runs its own executable hidden, mute and without
mods, once a picture (`<menu> <root> --config-file --shot`, scripted input such as
`mouse:x:y` for our pointer), one at a time on a thread, started the first time OPTIONS
opens, the focused row first. The PNGs are kept in `launcher-prints/` next to launcher.ini
with a version stamp, so later runs only read them; nothing is shipped, every picture
comes from the player's disc. Graphics API and PC Preset show their named choices. Every
other row has no picture (a still cannot show VSync, latency, a frame limit or a folder
list), and no drawn stand-in. Card and side names shrink to their box. Under the words a gold line
appears only when it adds something: a change that waits (next start, next time the
Jukebox opens) or what Classic and Enhanced set when they differ (not on interface rows,
whose Menus note already says it). The comparison's remarks sit under the picture, not on
it; the two side names shrink to their half. When the column is short (a small window)
the picture keeps at least a third of it and the words drop Look at and Cost. The dock
captions shrink to their slot. Load Preset and Save Preset are buttons (Choose File...,
Save As...): Enter, A or a click opens the file window; Left and Right do nothing there.

Launcher Scene (19 October 2026, P; launcher.ini `scene=`, empty by default): GENERAL >
Scene chooses where the launcher's car stands. Room (the default) is the white or dark
sweep; the others are the disc's Photo Travel places (`/crs/ss_*`) and courses. The course
is read in the background with the player's car (its paint and fitted parts) and drawn live
by the race's renderer instead of the room: at a place the car stands on PhotoMode's spot,
on a course on the first grid slot. The launcher's cameras (dock framings, the idle
cinematic shots, the free rig) are written in the car's frame and become cameras of that
world. They stay 0.3 above the road and come in toward the car where the road rises across
the line. At a place they turn to the side PhotoMode's own camera stands on, which is known
to be clear of fences and walls, and stay within 1.1 times its distance. Walls themselves
are not in the course's road data, so a course camera can still meet one. The room's car
keeps loading behind it; the corner text gets a dark shade and light ink. Launcher only:
the game and its presets are not involved (Dante's idea of changing the menu room, as in
Euro Truck Simulator 2). `MOONSTUDIO_SCENE=<code>` sets it for captures and
`MOONSTUDIO_WINDOW=WxH` the window size. The car follows the road under its four wheels
(pitch and roll from the road heights there; a flat car on a slope sank on one side). On a
course the cameras also stay over the road, since walls, barriers and stands are off it,
and look from the car's side with more road between the car and them (the first grid slot
is often by the pit wall: Laguna Seca, Monte Carlo and Seattle had the camera in a wall).

Options apply at once, so there is no draft. The course is read when the column first needs it and closed with the
panel; each capture is restored to the player's look before the launcher's frame,
so the room behind keeps it. No full-screen view in Options.


### Jukebox Music folder (19 October 2026)

P: `audio.custom_music` (0/1, default 0) controls the local songs. Classic
sets it to 0. Off, the Jukebox is the disc's own page on its PS2 playlist blobs and
no local song is listed or played (19 October 2026; before, the page listed them
and editing a list turned the option on). On, the original GT Mode > Jukebox
Library discovers supported audio in `Music` next to the active options file,
including subfolders, on each entry. Its Racing and Slideshow folders are always
the "Race playlist" and "Slideshow playlist" lists; a different list an earlier
editor assigned to races or slideshows stays in the file, unused. All list controls/layout remain authored.
There is one Jukebox page, the one GT Mode opens; no second page is drawn over it.
The added Playlists/management screen, selectors, file dialogs and music F9 entry
have been removed. Existing catalog and playlist data remain compatible.

Files remain in place. The UTF-8 `<options path>.music` stores references, including
`gt4:` disc references. IDs do not enter PS2 blobs. Reopening deduplicates tracks.
Windows validates WAV/MP3/FLAC/OGG/M4A (compressed through existing FFmpeg).
Android/Switch scanner branches are unvalidated for storage/playback.
See [usage, tests and limits](notes/CUSTOM_MUSIC.md).

P: `audio.jukebox_folders` (0/1, default 1 except Classic) adds Menu and Replay
folders to the Jukebox, built from the authored Racing/Slideshow folder and add
button and run by the page's own playlist code. Menu replaces only the GT Mode map music (the authored MAP group), never
Arcade, Home, dealers or pre-race music. An empty Menu keeps the original
map music; an empty Replay plays the Racing list. Classic sets it to 0.

P: `audio.jukebox_groups` (0/1, default 1 except Classic) lists local songs by
their Music subfolder, the folder shown in place of the artist. Classic sets it to 0.

The three are rows of the launcher's Options > SOUND, after the volumes (Custom
Music, Jukebox Folders, Library Groups), and of the game's Options > PC Settings >
Music, each changed on its own; the preset then reads Modified. Before, only the
preset changed them.

### PC Settings section in the game's Options (19 October 2026)

P, no setting: the PC options left the Screen page's six tabs. The game's Options
gain a "PC Settings" section under General, built like the authored ones (the
hidden Group header and a row of tiles copied from the Screen tile with its
script): Graphics, Light & Shadow, Controls, Music, Presets. Each tile opens the
authored Screen page (pane, rows, dropdowns, Cancel/OK) showing only its own
settings under its own title; the Screen tile keeps the display (Aspect Ratio,
Window Mode, Resolution, VSync, Upscaling, UI Filter, Text Clarity). PC Preset,
Preset Files, Save Transfer and the Load/Save Preset buttons are on Presets.
Graphics Swap Key joins Controls; Custom Music, Jukebox Folders and Library Groups
are on Music. Tile icons are the disc's own art (Options' Camera, Album, Steering
and GT emblem; the Jukebox's Library discs); back from a PC page focuses its tile.
No tab buttons and no L1/R1 page switching (the game's Options have neither).
Android keeps its three tabs; Switch is unchanged.

### GT Mode pointer direction (19 October 2026)

P: existing `controls.gtmode_edge_pan` and `controls.smooth_cursor` also provide
map-edge cursor rotation, using the Arcade strip spring and directional feedback.
Axes that can pan determine eight-way arrows, including diagonals. A blocked axis
contributes no direction. Leaving an edge, changing page, disabling edge pan or
handing over to the pad restores the existing cursor behavior. No extra option.
`desktop_pointer` checks all eight directions, map bounds and disabled pan;
`gtmode_map_navigation` retains controller landmark navigation coverage.

Windows launcher Sound > Open Music Folder opens the Music directory beside the active options store. Startup prepares this folder even with audio.custom_music off; the button does not enable playback or change playlists.

### Authored night guard (20 October 2026)

The daytime-only `graphics.daylight` path preserves the loaded authored sky,
lighting and fog on nonzero Env0 time zones. This prevents noon procedural
replacement during racing while pregrid keeps the authored scene. No new option:
the lamp-shadow restoration is original behavior; full procedural night is the
separate requested follow-up. See [limits](notes/NIGHT_COURSES.md).

### Car Lamps (20 October 2026)

`graphics.car_lamps`: independent 0/1, Classic=0, Enhanced=1. In-game
PC Settings > Light & Shadow > Car Lamps and shared launcher Quality API.
Applies on next draw, presentation only; never serialized to physics/replay
commands. Personal presets include it. World-space cone lamps and emissive
lenses use the same shader on OpenGL and Vulkan, with existing HDR bloom.
Includes optional perspective depth occlusion while Car Lamps is On (24 maximum
256-pixel light tiles, 3 cm bias); no allocation/draw in Classic. Inferred lenses,
range, colours and depth filtering are artistic P. Full-race performance and
broad car coverage remain unverified. The later [12-model/24-light check](notes/CAR_LAMP_COVERAGE.md)
recovers lit brake emitters and excludes ghosts; it is bounded coverage, not a
full-car certification or complete-race benchmark. The same option now uses a
direct atlas and shadow-only batching at unchanged resolution; nominal depth
storage rises about 11.25 MiB in D32F/S8 mode. See the coverage note for the
copy-reference tests and bounded performance measurements. Emission follows
night-lens/pressed-branch tags for both lit and unlit materials; nearby paint
is excluded, with HDR bloom checked on both APIs.
Parameters, tests and limits: [renderer](RENDERER.md#optional-world-space-car-lamps-20-october-2026).

### Optional full-day procedural sky (20 October 2026)

P: `graphics.night_sky` is individual, Classic Off / Enhanced On, in PC Settings
> Light & Shadow > Night Sky and the shared launcher Quality API. It requires
Daylight; turning it on enables Daylight. Off retains the legacy daytime cycle
and the authored-night guard. No physics or recorded control format changes.

Solar Time now has all 24 hours; saved indices 0..10 still mean 07:00..17:00,
then 11..23 mean 18:00..06:00. With the default 12:00 setting, nonzero authored
Env0 time zones start at 22:00; explicit other hours override that default.
Fixed, Real Time (1/3600 hour per second) and 60x (1/60) use the existing recorded
frame clock, wrapping at 24. No wall clock or RNG. Lamp activation continues to
follow original night/safety-car and brake flags, independently of solar time.

The same shader on OpenGL/Vulkan adds a moon opposite the sun and a deterministic
integer-hash star field, blending with the existing daytime atmosphere over
sun elevation -0.12..0.08. Direct sunlight is zero below the horizon; solar
shadow-map casters are skipped there. Night ambient is artistic, not recovered
PS2 data. Course illumination is still baked into textures; this is not a fully
relightable nighttime world. No lunar ephemeris, moonlight shadow pass or weather.
Native Route5 captures are very dark away from the car lamps; visual balance
and more tracks still need review. Whole-frame API equivalence is not proved.

Tests: night_sky GL/Vulkan and their deliberate noon-as-night negatives;
2400 deterministic clock/ambient samples, repeat-frame byte equality, dark-night
and star output, default course time, wrap and legacy clamp. Initial tests failed
because no star samples survived the float-hash/threshold combination; the
integer hash and revised density pass. launcher_quality verifies persistence
and Classic restoration. No equivalent PCSX2 capture: original parity remains A.
Local captures: build/night-sky-<api>-enhanced-night.png; logs:
build/night-sky-final-tests.log and build/night-sky-regression-tests.log.


### Enhanced night: retain authored local radiance (20 October 2026)

P correction within graphics.night_sky / graphics.daylight, no new option.
The procedural night multiplied already-lit course radiance by near-zero solar
irradiance; modern car lighting could also erase the original local RNW4 response.
The new night_baked GPU positive fails before the fix on both APIs (source20,
night0). The fragment path now retains its original lit colour before modern
shading, blends back to that radiance during authored night, then adds optional
car lamps. HDR retains linear radiance, SDR retains encoded source colour.
Original fog remains after the mix; the procedural sky is separate.

The weight is 1-smoothstep(-.12,.08,sun elevation), and only nonzero original
Env0 time zones may retain authored night radiance. Day courses are not forced
bright at night; noon and Classic have weight zero. The weight depends on the
same recorded frame clock as the sky, never track labels, wall clock or physics.
This preserves existing baked artificial light; it is not light-source recovery,
new global illumination or removal of baked daytime shadows.

Twenty focused GPU tests cover night surfaces/cars, HDR/SDR, source night/day,
legacy restoration, the existing baked-day/noon path, sky, car lamps and authored
night guards, including deliberately disabled-source negatives. A car fixture
initially lacked its modern_light flag and was corrected without relaxing the
radiance equality. Retained synthetic radiance agrees within 1 byte/channel;
Off restoration is byte exact. Both APIs captured all 25 source environments
again: build/night-balanced-<api>/index.html. Route5 now retains lit buildings and
asphalt; the optional lamps remain additive. No equivalent PCSX2 frames, complete
source-scene photometry or console GPU certification. Overall visual grade A/P.
Logs: build/night-baked-before-tests.log, night-baked-final-tests.log and
night-baked-regression-tests.log. Initial test compile errors were fixture-only
(include entrypoint alias and light-field name), corrected before GPU comparisons.


### Full daily sun/moon cycle (20 October 2026)

P, optional PC presentation: graphics.night_sky plus graphics.daylight enable the
full daily path. The existing graphics.solar_cycle is exposed as Sun / Moon Cycle
(Fixed / Real Time / 60x), not a second competing clock. Real Time is one solar
hour per 3600 recorded-frame seconds; 60x is one hour per 60 seconds, so a full
cycle lasts 24 real minutes. The sky uses only the recorded frame index /60.
Sunrise/sunset follow the existing artistic equinox/35-degree model. The moon is
a stylized full moon opposite the sun, not a lunar ephemeris or monthly phases.
Stars rotate about that model's celestial pole and wrap with the same 24h clock.
Moon/star rays below the horizon are rejected before the daytime integrator's
horizon clamp. Moon radiance uses the existing RGB optical depths
(.0428,.1,.24414), elevation floor .08 and disc transition .009..008 radians.
These are presentation choices, not recovered original-game astronomy.

Solar Time now has 25 entries: the existing 24 explicit-hour indices keep their
mapping; new index24 is Course. Enhanced defaults to Course (12:00 for source day,
22:00 for nonzero original Env0 time zone); Classic retains inactive index5.
An explicitly selected 12:00 now really means noon even on a night course; the
former implicit 12:00->22:00 substitution is removed. Stored indices0..23 remain
manual hours. New/default presets select Course, not an inference that overwrites
old modified player choices. Personal presets accept index24. Startup and runtime
share video_settings_solar_hour, fixing the startup path's previous +7-only mapping
for late hours. No profile, vehicle command or physics/replay format is changed.

celestial_cycle checks 1441 clock samples over a day, opposite unit sun/moon
vectors, sunrise/moonrise and actual GPU disc contrast at six times. Star motion
and exact midnight repetition are checked on both APIs. Moon-disabled and
clock-held negatives fail their intended assertions. Initial disc comparison used
the window centre instead of the actual content viewport; querying the viewport
fixed the fixture without weakening its contrast threshold. The early CPU-negative
shutdown attempted video teardown before initialization; it now only tears down
a started device, and returns the intended assertion failure rather than crashing.

celestial_clock uses the actual app: 240 live and replay ticks both publish exactly
4 seconds / solar23.0666656, pause30 updates preserve the frame count and complete
scene bytes, and deliberately changed reported time fails. This is bounded clock/
pause integration, not an entire 24h simulated endurance or universal replay-image
proof. launcher_quality checks 25 choices, Course/manual-noon resolution, night-hour
persistence, Classic and Enhanced defaults. Focused sky/baked/lamps/guard regressions
remain in build/celestial-final-tests.log. Original-code comparison is inapplicable
to new astronomy; original Classic rendering remains the separate baseline.

Local previews on both APIs: build/celestial-preview-<api>/index.html and cycle.mp4,
97 fixed-camera Route5 samples at quarter-hour intervals. PNG0 and PNG96 are byte
identical. No original-game assets/captures are committed. Authored baked lights,
car lamp activation and callback7 model selection still follow source time-zone
rules: the optional daily sky does not certify fully dynamically relit tracks,
automatic day-car geometry swaps or monthly lunar phases. Full scene comparison A.


## Realistic lighting bundles (20 October 2026)

User-directed consolidation supersedes the earlier individual lighting rows:

| Visible option | Stable primary key | Classic | Enhanced | Contents |
|---|---|---|---|---|
| Realistic Lighting | graphics.modern_lighting | Off | On | per-pixel materials, GGX, sun/sky response, HDR scene/filmic resolve, car shadows, medium shadow maps and AO |
| Realistic Skybox | graphics.daylight | Off | On | atmosphere, clouds, twilight/night, moon/stars and real-time full solar cycle; authored course day/night selects the initial time |
| Realistic Car Lamps | graphics.car_lamps | Off | On | existing optional lamp illumination/emission; renamed, lamp behavior unchanged |

Location: PC Settings > Light & Shadow, and launcher Quality/first-drive guide.
The previous HDR/material/AO/shadow/hour/night/cycle IDs remain readable for
compatibility, but are hidden Legacy rows. Loading/importing normalizes them
from the primary switches; there is no separate visible Sun/Night/Time option.
Skybox On selects Course time and real-time cycle; Off selects fixed noon for
the procedural fallback while retaining original authored night presentation.
Lighting On sets clear-air rays Off (the old fog-volume effect is not enabled
by this bundle). The two bundles can be selected independently. Changes apply
after OK and are presentation only, never recorded in physics replay state.
[Calibration, references, test evidence and remaining limits](notes/REALISTIC_LIGHTING.md).


### Authored celestial placement and shading caches (20 October 2026)

Update within the existing Realistic Lighting/Skybox options; Classic Off,
Enhanced On, no additional switch. At the user's request, course sun/moon
placement now comes from retained source sky artwork, and lighting follows the
enabled Env0 direction. The stored cycle/hour IDs remain compatibility data;
there is currently no moving celestial orbit in course draws. Cloud motion
continues from simulation-frame presentation time. The old constant warm filter
is removed: high sun is neutral and low sun warmer. Pregrid/race share initialized
environment state. Cloud lookup at15Hz and linear-source reflection cache reduce
GPU work without lowering reflection sample count or disabling shadows/lamps.
[Tests, tradeoffs and measured limits](notes/LIGHTING_PERFORMANCE.md).


Photo Travel pointer correction (20 October 2026): the existing
`controls.desktop_menu` option also covers the original floating ScaleBar
controls. Wheel input adjusts the hovered focal control even when its popup
starts closed; dragging retains its decimal min/max/step and the original
script callback. Classic retains controller-only navigation. The preview
return fade correction is a base native adapter fix, with no new option.

### Launcher controller binding selection (21 October 2026)

The existing player controller mappings now use a choice list. Pointer hover,
keyboard/controller navigation and cancelling are presentation only; confirmation
applies the selected assignment to the selected player's existing pad settings.
An occupied destination is identified before confirmation. Editing one function
preserves other functions' alternate bindings. No new mapping store or gameplay
option: this repairs the secondary launcher's editor for the existing controls.

### Realistic Skybox repair (21 October 2026)

Existing skybox/night controls retain their configuration keys and Classic
defaults. Procedural sky replaces the full original background when enabled;
its surface reference irradiance remains authored. Disabling night replacement
also preserves the authored reflection sky. No new option or physics state.

### Realistic Car Lamps receiving-light repair (21 October 2026)

Existing graphics.car_lamps controls these changes; Classic remains off.
Front lamps use a generic soft low-beam cutoff and right-side upsweep; rear
lamps retain their existing brake-linked behavior. Radiance is an artistic
approximation, not calibrated lumens. No per-car constants or physics edits.

### Realistic Lighting reflected coat (21 October 2026)

Existing graphics.modern_lighting uses a smooth reflected coat independently
of legacy highlight shininess. No extra toggle: this repairs the existing
material conversion. Classic bypasses it; physical paint classification remains
unknown and no metallic-material inference is claimed.

### Authored sheen correction (21 October 2026)

Within Realistic Lighting, the original mixed-material sheen mask keeps its
authored strength; the global rough-asphalt assumption is removed. No new
option, no course-name conditions, no physical state changes. Classic unchanged.

### Realistic Lighting baked-light conversion (21 October 2026)

Existing graphics.modern_lighting preserves authored world detail at neutral
lighting in SDR and HDR; the retained authored-night world branch also keeps
its source display level in HDR. This corrects double host tone compression.
No new toggle, no track exceptions or physics state. Classic remains unchanged.
This does not add dynamic tunnel lamp emitters or certify original-game parity.


### GT Auto previews and direct solar coating (21 October 2026)

Correction: wheel/wing preview transitions belong to the base GT Auto
behaviour in Classic and Enhanced. The mistakenly added
`interface.part_preview_fades` preference has been removed; existing keys
have no effect. Exact original blend timing remains unverified.
The original mechanic camera fade is a base composition correction and
does not depend on that option.

`graphics.modern_lighting` (Realistic Lighting) also evaluates direct sunlight
on the existing authored reflective coating. Off keeps the original lighting
path. The coating remains a host approximation with the original mask; it
does not infer metallic paint types or replace the original model geometry.
Material reference: [Filament](https://google.github.io/filament/Filament.md.html).
