# PC launcher

Since 16/10/2026 the launcher is Mr. Dante's official one (front-end studio
room, first-run wizard and `launcher_ui`), integrated from his MoonEngine
commits 89a0be0 and ab5b249; the earlier secondary launcher was removed.
Its own design notes are in `docs/vulcan4/` (`LAUNCHER-ARCHITECTURE.md`,
`LAUNCHER-V2.md`, `STUDIO-ROOM.md`). The text below describes the earlier
launcher and is kept as history until it is rewritten for the official one.

The Windows launcher prepares the disc, display, audio, controls and active
save before starting the native game. Its home screen is a PC presentation
(grade **P**), rather than a reconstruction of a PlayStation 2 screen.

## Disc-backed visual language

With a validated disc selected, the launcher reads the original Options
panels, checkerboard, cursor and bitmap fonts at runtime. The bottom dock
uses the original Options icons for its navigation artwork. Visible alpha
bounds determine their large display size, excluding transparent padding.
Icons extend above the gray panels, while labels use
the original font tables. The runtime cache stores 34 decoded images and
font data; an obsolete or damaged cache is rebuilt from the selected disc.
No game artwork or font is distributed with the executable.

Before a valid disc is available, the picker uses host text and a procedural
background. The eight home actions remain Play, Game disc, Video, Audio,
Controls, Player data, Startup and Exit. Arrow keys, D-pad and pointer select
the same actions. Existing settings pages retain their authored control
geometry within the wider window.

## Live car presentation

The preview restores the active PC save in a separate process and reads its
riding car code and paint index. Missing or invalid garage selection falls
back to the Ford GT '05. The preview does not write the save. Installed
wheels and the wing of the active setup sheet are read through
the career body decoder and passed to the native showroom model loader.
Stock or invalid part IDs retain the stock appearance.

Eighteen custom camera shots form a 216-second sequence, with twelve
seconds per shot. Front/rear quarters,
both side profiles, front and rear views, wheel, headlight and tail details,
roof and elevated views alternate broad framing and close-ups. Each shot
moves slowly between two poses. A 1.5-second smoothstep dissolve blends
the last outgoing frame into the incoming shot, including the loop back
to the first camera. The outgoing frame stays on the GPU, with opaque
alpha; only one car scene is rendered per update. These are authored PC cameras, not original GT4 camera data.
Their framing is not individually certified for every car.

The native showroom renderer supplies the model, animated studio lights,
environment reflections and floor reflection. A launcher-only presentation
flag adds soft fill and brighter highlights. Rendering at 2488 by 1400 and reducing on the GPU supplies the 1244 by 700
launcher image. The host reads only this final image.
The checkerboard has a faint sepia tint. The home heading uses light
letters with a dark shadow, without a pale backing over the car;
menu-dependent background changes remain subtle. These
lighting and composition choices are deliberate PC additions, not PS2
fidelity claims.

The worker uses the selected OpenGL or Vulkan backend. A local shared-memory
mapping transfers finished pixels to the Win32 window. It pauses on settings
pages and when minimized, and shuts down before Play or when its parent
exits. Its renderer lifetime is separate from the game, so the preview's
resources and presentation settings do not carry into races. The preview
targets up to 60 updates per second; this is not a race FPS limit.

## Focused checks

`launcher_skin` compares all 34 runtime/cache images with disc decoding and
checks the original font tables. Its negative alters an Options icon byte.
`launcher_scene_data` checks saved-car and paint selection, empty/invalid
fallbacks, the full camera cycle and finite camera values; its negative
changes an expected pose. `launcher_scene_parts` restores a saved fixture
with a replacement wheel and wing in the second setup sheet, checks their
disc labels, then verifies stock/invalid-part fallback. Its negative changes
the expected wheel label. `launcher_scene` opens the actual launcher with
isolated preferences, renders OpenGL wide/detail shots and a Vulkan frame,
checks non-flat scene content and detects a camera change. Its negative
requires an impossible changed-pixel count. `launcher_window` retains the
settings, keyboard navigation, Apply/Cancel and persistence checks.

These certify the stated PC contracts. They do not certify complete image
agreement with the PS2, universal car framing or a frame-rate guarantee.

## Preview performance

The presentation still uses GPU rendering and the same 2x render target.
The GPU now resolves it to launcher size before a reusable readback buffer
is copied. Vulkan keeps readback storage separate from its upload arena,
prefers CPU-cached coherent memory when available, and copies RGBA rows in
bulk. Devices without that memory combination retain a coherent fallback.

The window caches premultiplied image/font bitmaps. Unskewed text uses cached
glyphs whose rows are expanded with exactly the previous row selection,
then blends each glyph once. Skewed text retains the row path. Background
work hidden by a complete preview is skipped, and the timer requests a new
preview paint only when a new image arrives. Cached objects belong to the
skin and are released when it closes.

A local GTX 1650 Vulkan measurement found the original full-size readback
averaging 385.675 ms and parent paint averaging 16.506 ms (60-frame run).
After the fixes, a 180-frame run including startup took 3.933 s; worker means
were 4.123 ms submission, 11.479 ms read/resolve and 0.750 ms conversion.
Parent paint averaged 6.132 ms, with 112 paints. These are measured portions
of one launcher run, not a universal or steady 60 FPS claim. Framing,
lights, car paint, wheels and wing remain the same presentation feature.

`launcher_raster` compares all output pixels for 34 composed image/text
fixtures against uncached, per-row drawing and verifies no warm bitmap
rebuilds. Its negative flips a pixel. `launcher_scene` also compares the GPU
resolve against CPU four-sample averaging on both OpenGL and Vulkan, with
maximum channel error 1/255 and a fourfold reduction in transferred pixels.
The fixed preview freezes after 24 deterministic updates so these captures
compare the same animation state. `launcher_resolve_negative` alters a captured color channel to prove that
the resolve comparison detects corruption; the camera negative remains active.
Vulkan synthetic and disc scene comparisons cover the separate read buffer.

## Camera transitions and settings composition

`launcher_scene_data` checks the 216-second loop and the fade's endpoints,
midpoint and monotonicity at all eighteen cuts. `launcher_transitions`
compares the midpoint's GPU pixels against the independently captured
outgoing and incoming views on both backends (maximum channel error 3/255).
Its negative substitutes the outgoing view for the blend. The same test
compares complete settings captures before and after showing the live car;
`launcher_viewport_negative` corrupts that comparison's right edge.

Settings drawing temporarily shifts the GDI viewport to center the original
900-pixel canvas in the wide launcher. Paint and print now restore that DC
state before copying the backing bitmap or drawing child controls. This
prevents a shifted source copy and stale home-screen pixels at the right.
These durations, fades and typography are grade P presentation choices.

## Atomic native control painting

The animated home background repaints its child controls frequently. Native
Win32 BUTTON background erasure filled a white rectangle before the custom
owner draw. Disc-backed buttons now consume `WM_ERASEBKGND` without writing
pixels. Dropdowns also bypass the native interim paint and compose their
background, Options frame and label in an off-screen bitmap before one copy.
Input and dropdown selection messages continue through the native procedure.

`launcher_control_paint` sends 200 real erase messages to button/dropdown
windows and checks that every backing pixel remains unchanged. It also
compares the completed dropdown RGB pixels against an independently composed
background, frame and label at zero tolerance. The negative calls the former
native button eraser and fails when its white fill changes the pixels.
This is a tested PC presentation fix (P), not a PS2 fidelity claim or proof
that every possible source of flicker has been eliminated.


## Frame queue and refresh limit

The Video page includes **Vulkan frame queue**: Balanced uses two independent
submission slots, while Low latency waits for the current slot. **Frame
limit** offers Unlimited, 60, 90, 120, 144, 165 and 240 FPS. VSync can impose
a lower display limit. The cap applies to race/replay rendering on either
PC backend; native menus retain 60 Hz pacing. These choices persist in the
video preferences and are preserved by the original in-game settings.
The queue setting affects Vulkan only. See [Vulkan performance and tests](VULKAN.md#frame-queue-preparation-and-frame-pacing)
for resource lifetime, cache preparation, timings and limitations. Grade P.
