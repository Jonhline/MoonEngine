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
| graphics.daylight | Graphics | Procedural daytime atmosphere, solar surface lighting and nearby course shadow casting; source assets retained by Classic | Off (0) | On (1) | Next race/replay draw | No; presentation only | Options > PC Settings > Light & Shadow > Daylight | Secondary: yes; official: no | 2026-10-18 |
| graphics.solar_hour | Graphics | Equinox solar hour at the presentation model's fixed 35-degree latitude; 7..17 encoded as 0..10 | 12:00 (5), inactive | 12:00 (5) | Next draw | No | Options > PC Settings > Light & Shadow > Solar Time | Secondary: yes; official: no | 2026-10-18 |
| graphics.solar_cycle | Graphics | Fixed, real-time or 60x progression, reflected at 07:00/17:00; no night | Fixed (0) | Real Time (1) | Simulation frame clock; pause freezes it | No; presentation only | Options > PC Settings > Light & Shadow > Daylight Cycle | Secondary: yes; official: no | 2026-10-18 |
| graphics.hdr | Graphics | RGBA16F main scene, SDR filmic resolve and restrained bloom before UI/FSR | Off (0) | On (1) | Next scene; float attachment rebuild | No | Options > PC Settings > Light & Shadow > HDR | Secondary: yes; official: no | 2026-10-18 |
| graphics.sun_rays | Graphics | Off (0), retained screen-space shafts (1), shadow-mapped world-space volumetric integration (2); requires HDR/Daylight | Off (0) | Volumetric (2) | Next scene; choosing Volumetric enables HDR/Daylight | No; presentation only | Options > PC Settings > Light & Shadow > Sun Rays | Shared launcher API | 2026-10-18 |
| graphics.ambient_occlusion | Graphics | Optional depth-based cavity/contact shading; screen-space approximation requiring HDR/Daylight | Off (0) | On (1) | Next scene; selecting On enables HDR/Daylight | No; presentation only | Options > PC Settings > Light & Shadow > Ambient Occlusion | Shared launcher API and personal presets | 2026-10-18 |
| controls.touch_menu | Controls and input | Switch handheld and Android first tap selects/previews, Android pause cards use authored actor identities, second tap on the same item confirms, direct list/map drag, parameter bars and native confirmation/profile buttons; ignores duplicate mouse events; hides cursor artwork after touch and restores it on controller input; retains desktop scenario-specific hover between taps, uses second-press compression without added colored overlays or instructional text; original green Exit fallback appears only after touch on screens lacking an existing return control (including pause) | Off; controller navigation | On | After OK, menu input boundary | No; no simulation change | Options > Screen Settings > Touch Menus (Switch); Controls > Touch Menus (Android) | não | This change |
| controls.touch_race | Controls and input | Android-only multitouch race overlay routed through the existing pad configuration; external controllers hide and supersede it | Off; physical controller | On | After OK, Android input boundary | Resulting ordinary pad commands are recorded; layout is not simulation state | Options > Screen Settings > Controls > Race Touch Controls (Android) | no | Android foundation |
| controls.touch_size | Controls and input | Small/Medium/Large Android race buttons and steering area | Medium; overlay off | Medium | After OK, Android input/presentation boundary | No; ordinary commands are recorded | Options > Screen Settings > Controls > Control Size (Android) | no | Android foundation |
| controls.touch_opacity | Interface | Android race overlay opacity, 20–100 percent | 60%; overlay off | 60% | After OK, Android presentation boundary | No | Options > Screen Settings > Controls > Control Opacity (Android) | no | Android foundation |
| controls.touch_layout | Controls and input | Steering on the left or mirrored on the right; not a free-position layout editor | Steering left; overlay off | Steering left | After OK, Android input/presentation boundary | No; ordinary commands are recorded | Options > Screen Settings > Controls > Control Layout (Android) | no | Android foundation |
| controls.touch_steering | Controls and input | Continuous analog drag or digital left/right steering; no solver changes | Analog drag; overlay off | Analog drag | After OK, Android input boundary | Resulting ordinary pad commands are recorded | Options > Screen Settings > Controls > Steering (Android) | no | Android foundation |
| controls.desktop_menu | Controls and input | Independent hover/previews, manufacturer preselection, full painted-button clicks, viewport/authored-regulation wheel scrolling, pause pointer, slider click/drag with the disc hand and manufacturer-panel edge panning, coalesced hover sounds, fullscreen mouse confinement (released on focus loss) and WASD menu navigation | Off; existing pad-style adapters | On | After OK, menu input boundary | No; menu-only, no simulation change | Options > Screen Settings > Desktop Input | nÃ£o | This change |
| controls.gtmode_edge_pan | Controls and input | Pan GT Mode map near viewport edges without selecting landmarks; requires Desktop Input | Off | On | After OK, menu input boundary | No; no simulation change | Options > Screen Settings > Map Edge Pan | nÃ£o | This change |
| interface.smooth_mouse_cursor | Interface | Damped cursor motion, restrained inertial lean and clickable-target feedback; script icon crossfade | Off; direct mouse position when Desktop Input is enabled; original pad cursor with Classic | On | After OK, presentation/input boundary | No | Options > PC Settings > Controls > Smooth Cursor | nÃ£o | This change |
| interface.mouse_hover_feedback | Interface | Gently enlarge static Options icons/labels and the green Exit pictogram (including its authored alternate face); whole vertical list-card enlargement (background, text, price and swatch together), including event/championship ticket artwork and labels; light standalone paint-swatch enlargement, varnish and animated reflection; adjustable-bar hover; car-strip mouse hover uses authored blob magnification/interpolation; preserve existing game hovers | Off; authored widget faces only | On | After OK, presentation boundary | No | Options > PC Settings > Controls > Hover Feedback | nÃ£o | This change |
| interface.compact_car_strip | Interface | Draw car thumbnails at 80% of their disc dimensions; retain original assets and smoothly selected magnification | Off; disc geometry | On | After OK, presentation boundary | No; menu only | Options > PC Settings > Controls > Compact Car Strip | nÃ£o | This change |
| interface.graphics_swap_key | Interface | F7 in a race or replay swaps the picture between Classic graphics and the player's own, with a two-second label; nothing is saved and leaving the race restores the player's graphics (keyboard only) | Off; F7 does nothing in races | On | Immediately, race/replay presentation | No; presentation only, simulation untouched | Launcher Options > Interface > Graphics Swap Key; first-drive guide, Menus | não | This change |
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
INTERFACE and QUALITY use the guide's two columns, the rows on the left and the
focused option on the right with the guide's own words (what, look at, cost; the
Menus note for interface rows; the Options help for rows the guide does not cover).
A QUALITY row the guide compares gets its comparison: the same Tsukuba frame at the
original value left of a draggable divider and the current value right (Enhanced's,
marked as not chosen, while it is still original). Options apply at once, so there
is no draft. The course is read when the column first needs it and closed with the
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
button and run by the page's own playlist code. An empty Menu keeps the original
menu music; an empty Replay plays the Racing list. Classic sets it to 0.

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
