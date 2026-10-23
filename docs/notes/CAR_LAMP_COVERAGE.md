# Optional car-lamp coverage and bounded cost (20 October 2026)

P: this improves `graphics.car_lamps`, Classic Off / Enhanced On. It does not
change source model selection, brake callbacks, RNW4 floor glow, controls,
physics or replay data. Original callback 0 / 00453698 supplies the pressed
branch; the existing loader exposes it as `lamp=2`. Inferring a light source
from that surface is a PC adaptation, not recovered original optical metadata.

## Missing rear emitters

The initial survey failed: the F-150 had one rear emitter; BEAT, R390, GT-One
and the pace car had none. Their authored pressed brake surfaces are lit kind 1
and often shared between day and night. The previous filter accepted only
unlit kinds 0/5 or a night/day difference, thereby discarding real brake lenses.
Kind 1 pressed surfaces now qualify alongside kinds 0/5; reflective-only kind 2
copies remain excluded unless independently classified by night geometry.
Position, normal, side, height and longitudinal bounds still come from geometry.
No per-car coordinates or fallback positions were added.

| Night race model | Rear sources before | Front / rear now |
| --- | ---: | --- |
| focus_rs_02 | 2 | 2 / 2 |
| mustang_gt_05 | 2 | 2 / 2 |
| svt_f150_lightning_03 | 1 | 2 / 2 |
| gt40_race_69 | 2 | 2 / 2 |
| beat_91 | 0 | 2 / 2 |
| ka_01 | 2 | 2 / 2 |
| crx_sir_90 | 2 | 2 / 2 |
| _787b_91 | 2 | 2 / 2 |
| r92cp_92 | 2 | 2 / 2 |
| r390_race_98 | 0 | 2 / 2 |
| gt_one_race_exxon_99 | 0 | 2 / 2 |
| r34_gtr_v_gtcolor_pc_01 | 0 | 2 / 2 |

C for these detections and finite normalized outward directions only. Day loads
of the eleven ordinary cars have zero front and two rear candidates; the pace
car is explicitly loaded in its original always-night mode. Source removal is
a deliberate negative and fails. Four candidates do not prove correct lens
coverage, calibrated optics or every car material. Some decoded models report
an existing unsupported packet; this survey does not certify those packets.

## Ghost gate and all 24 tiles

A ghost receiver already ignored lamp shading but its pose still emitted light.
The source list now rejects ghost entries as well as inactive entries. A native
six-Focus Route5 scene checks 24 active emitters, all 24 atlas tiles, a ghost
reducing that to 20, opaque restoration to 24, source daytime to zero, and brakes
to 12 rear sources. Its negative makes the expected ghost opaque and fails.
The initial positive failed before the gate fix. Renderer work has no readback;
pose/driver snapshots are unchanged by the measured draws. This is a static
presentation fixture, not an independently driven full race/replay.

## Conservative spotlight bounds

Only perspective lamp maps use six model-space support-point clip tests instead
of eight transformed/divided corners per group. Directional solar shadow bounds
retain their previous path. The lamp planes retain boxes straddling the near
plane and discard only groups wholly outside a plane. A 20,000-box independent
transformed-corner reference checks 19,995 random poses/boxes and near-plane
cases (five samples within 1e-4 of the tolerance boundary are excluded); a changed
plane negative fails. Focus and BEAT captures before/after this optimization
are byte-identical for Classic, lamps and night-sky images on both APIs (12
comparisons). On the earlier close-camera fixture submissions fell from 5099
to 3982 per frame, with the same light tiles; this is not an FPS ratio claim.

## Captures and limits

Local gallery: `build/lamp-coverage-gallery/index.html`, 12 cars on each PC API,
with Classic, Car Lamps and enhanced sky modes. Capture driver:
`tests/render/lamp_survey.py`. The two-API gallery contains 24 successful native
inspections. `build/lamp-grid-<api>-six-cars.png` shows all six cars on the
original 100-metre grid; every car origin is checked inside the camera frustum.
`build/lamp-cull-image-proof.log` records the unchanged comparison images.

No equivalent PCSX2 captures, all-car coverage, calibrated lamp axis/intensity,
monthly moon phase, skinned driver/pit/particle casters, full HDR/Enhanced stress,
mobile/console GPU run, full-race timing or whole-frame backend equality is
claimed. Detecting lit brake surfaces does not itself extend the existing lens
emission shader to every lit material. Visual composition and that emission
coverage remain A/P. Do not infer a publishable finished game from this sample.

## Measured snapshot

GTX 1650, native 640x480, six identical Focus cars at their authored grid poses,
all origins inside the overview camera. No physics/AI updates, HDR, modern
lighting, daylight or other optional shadow pass. Three warmups, twelve frames
per mode, GPU completion waited explicitly, vsync and menu/frame pacing Off.
Values include CPU submission, GPU completion and presentation; they are not
GPU timestamp queries. They vary with clock/load and are not acceptance limits.

| API | Lamps Off, mean ms | Cones only, mean ms | With occlusion, mean / maximum ms |
| --- | ---: | ---: | ---: |
| OpenGL | 9.123 | 9.913 | 14.112 / 16.335 |
| Vulkan | 16.214 | 16.301 | 26.938 / 32.224 |

Cones-only test omits atlas generation. With occlusion the overview submits
4527 draws / 225111 triangles per frame, versus 1268 / 60362 without depth maps.
All 24 light tiles are generated; no measured renderer readbacks. The initial
close-camera measurement had menu pacing enabled and only the first car in
view; it is superseded by this wider, explicitly checked camera and unpaced
measurement. Vulkan is still materially more expensive here and exceeds a
16.67 ms frame budget; no 60 FPS promise or complete-race performance claim.

38 focused tests pass, including deliberate negatives. Logs:
`build/lamp-coverage-final-tests.log`, `build/lamp-coverage-test-details.log`.
The first survey and ghost positives failed before their fixes; these failures
remain in `build/lamp-survey-first.log` and `build/lamp-grid-first.log`.


## Direct atlas and shadow-only batching (20 October 2026)

P performance work within `graphics.car_lamps`, Classic Off / Enhanced On;
no quality reduction, source-light gate, physics or replay format change.
The same 24 256x256 light tiles are now rasterized directly into their
1536x1024 colour/depth framebuffer, with a per-tile viewport and scissor. One
clear and one receiver-target restoration replace 24 scratch-to-atlas copies.
`end_light` marks the tile; `bind` finalizes the batch/restores the receiver.
Restart/destruction also finish an open batch. Invalid sources remain unbound.
Directional sun maps keep the square-target path and do not enable batching.

The lamp pass merges only contiguous vertex ranges with the same texture,
active brake branch and alpha-test function/reference, after light-frustum
acceptance. Reflection copies and blended non-cutout glass are still excluded.
No primitive reordering, shader/material approximation or caster removal.
Native six-visible-car work falls from 4527 to 4080 submissions, with the same
225111 triangles. This is a submission reduction, not a proportional FPS claim.

C (bounded GPU reference): `lamp_atlas` embeds the renderer and retains the old
per-tile scratch/copy and unbatched submission in its test fixture. It compares
all 24 tiles with posed opaque, rejected-alpha, glass/reflection and alternate
brake-branch geometry. Both APIs retain identical coverage (159000 occupied
pixels, zero alpha mismatch); maximum packed-depth difference is 5.96046e-8,
within the declared 2e-6 tolerance. Omitting tile 23 in the optimized path is
the deliberate negative; it fails with 6606 alpha mismatches. The synthetic
fixture submits 456 batched draws versus 3888 reference draws. Four native
six-car before/after captures (two runs on each API) are byte-identical.

Two alternating runs of the old executable and the changed executable on the
same GTX 1650, with the previous unpaced 640x480 static overview fixture:

| API | Before, mean ms in each run | After, mean ms in each run |
| --- | --- | --- |
| OpenGL | 13.476 / 13.300 | 12.578 / 12.897 |
| Vulkan | 24.916 / 24.736 | 24.228 / 22.906 |

Logs/captures: `build/lamp-atlas-paired.log`,
`build/lamp-atlas-paired/measurements.json`, `build/lamp-atlas-comparison.html`.
These are small wall-time samples including GPU completion/presentation, not
GPU timestamp measurements or a statistical/general-hardware speedup proof.
Vulkan still exceeds 16.67 ms here. Complete races, HDR/other Enhanced passes,
long-term stability, mobile/console and equivalent PS2 captures remain open.

Tradeoff: the depth attachment now spans the atlas. Nominal colour+depth storage
is 18 MiB with RGBA8 + D32F/S8, versus 6.75 MiB for the former atlas plus 256px
scratch colour/depth (legacy D24/S8: 12 versus 6.5 MiB). Driver alignment and
allocation residency are not measured. The original depth precision is retained;
this change trades approximately 11.25 MiB in the tested depth mode for fewer
copies/submissions. Classic allocates neither optional lamp target.

42 focused automated cases pass (build/lamp-atlas-final-tests.log); a separate
legacy OpenGL-depth run and negative also pass their declared outcomes
(build/lamp-atlas-legacy-depth.log). No full-suite run.
