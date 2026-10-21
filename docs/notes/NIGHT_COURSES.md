# Authored night environments — 20 October 2026

Task 1, Classic first. Source: local disassembly 0038E7F8 (RNW4 light
selection), 0038F168 (shadow direction/tint/alpha), 00451EE0 -> 004524B8
(model-1 projected shadow), 00340DC0..00340E40 (pass count), and 00387FB0
(Env0 time-zone flags). No physics source or simulation state is modified.

## Changes and evidence

The existing stable top-four RNW4 light selection now also supplies shadow
parameters. Direction is minus the selected normalized direction times Env0
+A78; RGB is lamp +130 divided by Env0 +38; alpha is lamp +13C times its
point/spot attenuation. Directional lamps do not attenuate. The cast volume
uses these parameters in the ground frame, .02 height and .005 depth bias.
The authored contact/glow remain separate. With fewer than two selected lights
the original pass caller skips lamp volumes; up to four for the primary car,
up to two for other cars. No lamp geometry is fabricated by course name.

The original-upload oracle compares 192 inputs, directions/tints/alphas within
2e-6, using generated local original code only as test evidence. The existing
lighting negative and a dedicated altered-alpha negative fail. Initial positive
failed because alpha attenuation was missing; this was corrected before delivery.

Route 5 is already dark with Classic graphics. The reproducible discrepancy was
Daylight replacing its sky at noon during racing while pregrid bypassed Daylight.
Until the full-night procedural path is implemented, Env0's time zone prevents
this day-only replacement. Authored atmosphere/fog/sky remain loaded as before.
GPU night_course tests compare the complete RGB fingerprint of the same scene
with Daylight off/on on OpenGL and Vulkan. Negative clears the time-zone flag
and fails against the resulting daytime image. This is not PS2 pixel parity.

## Per-course captures

`tests/render/night_courses.py` writes local build/night-audit/index.html,
manifest.json and four images per course: race, front lamps, pregrid and
procedural-daylight guard. 25 source environments are covered: hongkong,
lasvegas, motegi/east/west, paris_rally, their existing r_ variants, route5/club
and reverse, seattle/reverse, sarthe/sarthe24/sarthe_old24, daytime sarthe_old
control, and ss_brooklyn/fremont/gion/sanmarco/shibuya/times/tsukiji.
No r_ Photo Travel files exist in this disc's /crs directory. Brooklyn and Tsukiji
have night flags but explicitly author ground-glow alpha zero. Motegi and
Sarthe have night model flags despite a bright authored sky; the flag is not
proof of a dark photographic time of day. Captures preserve those assets.

## Remaining verification

C applies only to compared light/shadow parameters and GPU guard behavior.
Complete shadow integration/image remains A: ground-triangle lookup is the
existing immutable +5m query, not the original contact-cache interpolation;
primary-pass identity uses the existing visible-entry detail path, not a new
EE scene scheduler oracle. Contact/light volume passes, all-track GPU coverage
and the whole raster have no equivalent-frame PCSX2 comparison. Captures are
static native rendering of loaded race assets, not completed races on all
courses. No whole-game or whole-night fidelity claim follows from this audit.
