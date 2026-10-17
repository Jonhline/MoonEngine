# Input support and touch roadmap

Status recorded 16 October 2026. This page distinguishes implemented PC
menu input from planned touch support. It does not certify every screen
or announce an Android port. See [Options](OPTIONS.md) for authoritative
setting IDs/defaults and [Fidelity](FIDELITY.md) for bounded evidence.

## Current support

| Input/platform | Status | Scope |
|---|---|---|
| Original controller navigation | Retained | Original script actions, focus, authored cursor and transitions; Classic baseline |
| PC mouse and keyboard menus | Implemented, optional (P) | Main menus, settings, lists, native confirmations, profile selection and race pause |
| Switch touch | Planned, not implemented/validated by this work | Handheld menu interaction first; existing controller path remains available |
| Android touch | Planned, not implemented/validated by this work | Shared menu gestures after an Android platform/application foundation exists |
| Touch race controls | Proposed later phase | Separate optional input overlay; no vehicle physics changes |

Mouse events synthesized by a platform are not proof of deliberate touch
support. There is currently no finger-contact/gesture adapter in the menu
path documented here. Android packaging, lifecycle handling and device
validation are prerequisites, not delivered features.

### Using the PC menus

Enable **Options > Screen Settings > Controls > Desktop Input**. Move the
mouse to hover; release the left button to activate; release the right
button for Back. Arrows/Enter/Escape and controllers remain available;
Desktop Input also enables WASD menu navigation, without stealing text entry.

The wheel scrolls content independently of controller selection. Over a
parameter bar it changes that parameter; in an open dropdown it steps the
pending choice. Hovering a dropdown row lights it without committing it;
clicking commits the choice. Vertical lists clip within their viewport,
including when the wheel moves beyond the old controller selection.

Main-menu actions accept the whole horizontal row, including the ends of
Gran Turismo Mode. The optional expanded presentation retains the original
vertical labels, backgrounds and arrows, displaying five entries at once.
Car/course hover invokes existing preview callbacks without committing a
choice. Horizontal strips can pan at their edges. GT Mode map hover runs
the authored landmark animation; optional map edge panning moves the map
without locking onto a landmark.

| In-game Controls row | Store ID | What it does |
|---|---|---|
| Desktop Input | `controls.desktop_menu` | Mouse/menu routing and WASD aliases |
| Map Edge Pan | `controls.gtmode_edge_pan` | Independent GT Mode map edge panning; requires Desktop Input |
| Input Hints | `interface.input_hints` | Show button hints for the latest keyboard/mouse or controller input |
| Main Menu | `interface.main_menu_overview` | Five-entry original-style main menu viewport |
| Smooth Cursor | `interface.smooth_mouse_cursor` | Damped motion, inertial lean, clickable-target cues and icon crossfade |
| Hover Feedback | `interface.mouse_hover_feedback` | Small eased enlargement of otherwise-static Options icons/labels and Exit |

All six are Off in Classic and On in Enhanced. Presets fill values;
individual edits mark the set Modified. Settings use the shared store,
not a second mouse-specific configuration. The official launcher has not
implemented these controls; this work only extends the in-game Options UI.

Smooth Cursor uses damped presentation motion and restrained inertial lean,
with positional lag capped at 24 UI units and a 90 ms cursor-icon crossfade. New pointer/modal contexts start at the current mouse position.
Hit testing uses the actual input position; smoothing is presentation only.
Hover Feedback eases over 60 ms only where Options icons/labels lack authored
hover. Existing model/text/button hovers receive no extra box or tint.
Turning either off preserves the other
independently. Classic controller cursor animation remains the original path.

### Implementation boundaries and evidence

- `src/platform/sdl/plat_sdl.c/.h`: absolute mouse/button/wheel events and
  controller source classification; relative cinematic mouse remains separate.
- `src/app/desktop_input.c/.h`: desktop routing, inverse viewport projection,
  keyboard aliases, native-dialog input and hint updates.
- `src/core/menu.c`: semantic action targets, hover callbacks, full-row main
  actions and wheel/map routing. Hover does not replace controller selection.
- `src/core/ui.c/.h`: painted/clipped target bounds, authored prelight,
  optional feedback/easing, list viewport drawing and pointer transfer.
- `src/core/strobe_player.c`, `race_pause.c`, and `src/app/race_game_pause.inc`:
  pause button hit testing from the rendered STRB geometry and original actions.
  The pause cursor uses its own overlay; the menu smoothing/feedback tests
  below do not certify identical effects in that overlay.
- `src/app/video_settings.c` and `options_store.c`: drafts, Apply/Cancel,
  shared persistence, presets and menu-only option exposure.

Focused evidence: `desktop_pointer`, `desktop_navigation`,
`desktop_events`, `desktop_options_store`, their intentional negatives,
`desktop_settings_app`, and the existing `options_lifecycle` family.
The latest feedback/list change passed ten focused CTests plus the real
executable settings exercise; see the per-change fidelity record for scope.
Tests cover clipped/full-row targets, actual hover draw calls, popup prelight,
no popup pointer pull, 30/60 Hz position agreement, angle settling, cursor
crossfade alpha, last-row visibility, both settings values and persistence.
These are integration/presentation checks, not full PS2 pixel equality.

Remaining work includes slider dragging, configurable desktop bindings and
specialized mouse workflows in Photo/Logger. Full keyboard layout/IME and
all-screen/device coverage are not certified. Named player presets remain
planned in the shared options design. Report uncovered screens individually.

## Planned shared touch design

Implement touch at the platform/input/presentation boundary and reuse the
same menu actions, projected widget bounds and disc-owned visual elements.
GT4 and future Tourist Trophy should consume a shared input API; no gesture
code belongs inside game rules, career logic or the vehicle solver.

The proposed event model carries source/device/contact IDs, logical position,
phase (begin/move/end/cancel), timestamp and pointer kind. Each platform
adapter transforms physical coordinates through its actual viewport, DPI,
letterboxing and safe area. Suppress synthetic mouse duplicates from touch,
so one tap cannot activate twice. Exact API names and gesture thresholds
remain implementation decisions to document and test, not PS2 constants.

| Gesture | Planned behaviour |
|---|---|
| Tap | Press feedback on contact; activate once on release inside the valid target |
| Drag a list/strip | Capture that contact and scroll the content; cancel tap activation once dragging starts |
| Drag GT Mode map | Pan the map freely within content bounds, without selecting every crossed landmark |
| Tap a dropdown | Open it, highlight the touched row, commit on a valid release; permit scrolling long lists |
| Back | Provide a visible game-styled Back action; retain controller Back and map Android Back at the boundary |
| Extra contacts | Track by ID; do not steal an active menu gesture or cause extra activations |
| Cancel/focus loss | Release capture, clear pressed state and synthesize no activation |

Direct touch has no persistent hover. Finger-down feedback must therefore
not depend on mouse motion or a floating cursor. Do not require double taps,
long presses or mouse edge panning for basic navigation. Keep edge panning
as a mouse feature; direct map dragging is the touch counterpart. Touch
cursor visibility, feedback and enlarged targets must remain configurable.

### Delivery order and acceptance gates

1. **Shared pointer/contact foundation.** Separate pointer kind from menu
   actions, implement capture/cancellation and duplicate suppression. Add
   deterministic positive/negative tests before enabling platform gestures.
2. **Switch handheld menus.** Add its touch adapter without changing the
   existing renderer or controller navigation. Validate on hardware: tap,
   scroll, dropdowns, popups, map drag, pause, touchscreen edges and rapid
   controller/touch switching. Docking/undocking cancels active contacts;
   docked use must remain fully operable by controller.
3. **Android platform foundation and menus.** Establish build/package,
   graphics/audio/input, application lifecycle and user storage first. Then
   reuse the gesture layer. Validate multiple aspect ratios/DPI/safe areas,
   Android Back, background/resume, rotation/recreated surfaces, cancellation,
   and external controllers/keyboards. Record actual tested devices/backends.
4. **Optional touch race overlay and specialized tools.** Define steering,
   throttle/brake, gears and pause controls separately, including layout,
   size/opacity and simultaneous contacts. Feed the established pad/axis input
   boundary; never alter physics to compensate for touch. Prove that the
   resulting input stream can be recorded/replayed correctly before claiming
   race-control support. Photo/Logger gestures need separate acceptance tests.

Before each implementation, add its individual options to the
[Options registry](OPTIONS.md) in the same commit: stable ID, Classic value,
Enhanced recommendation, apply boundary, replay impact, menu location and
Launcher = no. Do not treat proposed touch options as implemented registry
entries. Classic keeps touch adaptations off and the original controller
path working; players can independently mix mouse, controller and touch
preferences. A platform lacking a feature must present its availability
honestly, rather than silently enabling an imitation.

Every phase needs tests for successful interaction and rejected/cancelled
input: outside/hidden/disabled targets, dragging beyond a control, contact
replacement, multitouch, duplicate mouse events, interrupted popups and
lifecycle transitions. Test viewport edges, clipping and the first/last list
items. Compare Classic controller behaviour before/after. Record real-device
validation separately from synthetic tests; an NRO/APK build alone does not
prove touch support. Mark the new behaviours P, with explicit remaining gaps.

This is a development plan only. It does not change Switch/Android code,
commit to a release date, or authorize publication of the project.



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

Manufacturer car strips retain the hovered preselection while keeping viewport
position independent. Clicking runs the original colour-selection action.
Regulation dialogs route the wheel through their authored scrolling handler.

Car strips now interpolate selection magnification at the original 60 Hz
ratio (002D5660/002D6868), including shrink on the previous selection.
Compact Car Strip is optional under Screen Settings > Rendering; Classic
disables it. Regulation geometry has no separate setting. The regulation
rows preserve original GS scissor rounding, confirmed from the slot 2
savestate and original 00215FE0/004A2DF8. Position and extent are truncated
separately on the 640x224 interlaced field; resulting gaps vary by row.
No fixed seam width, layout change or extra option remains.
The horizontal ScrollBox thumb now handles negative SelectBox offsets,
fixing its visual position on the shared PC/Switch UI path. No Switch
package or hardware validation is claimed.

Paint-swatch hover retains its selected index, including return to the first
colour after leaving the strip. Hover Feedback adds a subtle white clearcoat
(maximum alpha 12/255) and a 1.4-second reflected-light sweep (peak 44/255),
with the existing 60 ms hover envelope. No enlargement, new texture or RGB
change is applied to swatches. Classic/Off omits these presentation passes.
