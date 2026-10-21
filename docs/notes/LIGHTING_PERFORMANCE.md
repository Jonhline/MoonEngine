# Authored sky correction and graphics cost (20 October 2026)

P, presentation only. This supersedes the constant warm filter and synthetic
course sun/moon orbit in REALISTIC_LIGHTING.md. Physics, input tapes and stored
race state are unchanged. No new player switch: work stays inside Realistic
Lighting / Realistic Skybox. Classic disables the extra rendering.

## Original sources and correction

Local splat/asm/core/text.s:
- 00398210 selects enabled Env0 lights (flags +10, records from +D0, stride80).
  The course loader already retains their direction, diffuse and ambient fields.
- 00398438/00398758 provide the existing shadow direction fallback (+A70/+A74).
- 0044E528 uses Env0 +48/+28 for authored environment-sky rotation.

The native course draws now keep the original sky mesh/texture as their base.
No procedural sun disc, opposite-sun moon or stars are superimposed over that
artwork. Cloud scattering/transmittance is composed over it. Surface lighting
uses the normalized first enabled original directional light, rather than an
invented latitude/time azimuth. Night retains its authored radiance. Keeping
sky artwork does not prove its whole raster is PS2-identical: existing texture,
projection, fog and composition approximations remain.

The pregrid bug was a mismatched condition: sky replacement was enabled there,
but the direction/colour uniforms were initialized only outside pregrid.
Both now use the same initialized course environment. A deliberately corrupted
source direction fails the native fixture before the race draw.

The fixed linear RGB (1.25,.98,.68) warm filter is removed. Direct-sun chroma
uses atmospheric excess air mass min(1/y, 12.5)-1 with RGB optical depths
(.0428,.1,.24414), normalized to unit luminance. Overhead sunlight is neutral;
low sunlight warms increasingly. This remains an approximate RGB transport
model. With course positions fixed, it follows authored elevation, not a
running morning/noon/evening clock. Cloud movement continues from recorded
presentation frames. Course celestial cycling is intentionally suspended at
the user's request; the legacy stored cycle/hour IDs are not an active orbit.

## Caches and work retained

1. A 256x128 RGBA16F hemispheric cloud lookup stores radiance/transmittance.
   It uses the same four-octave density, 32 view and 3 solar integration samples
   as the reference shader. Angular interpolation replaces per-screen-pixel
   integration. It updates at deterministic 15 Hz; drift is <.47m per update.
   A separate 128x128 shadow lookup spans 8192m around a quantized origin.
   Ground shadows approximate y=0; distant/out-of-bounds samples are clear.
   The sky volume assumes the original eye-centred dome, not camera-height
   parallax/cloud interiors. No CPU readback or wall-clock cache keys.
2. A source-sized RGBA16F environment image decodes half-range sRGB once per
   source revision. The 32 GGX samples, roughness, weights and original
   reflection resolution are retained; hardware bilinear sampling replaces
   four per-tap fetch/decode operations. Reserved texture units5..8 avoid
   rebinding the persistent images for every model.

Maximum added float-image storage is about 2.4 MiB at a 512px environment.
The optimization trades finite lookup interpolation/15Hz weather refresh for
less repeated shading; it is not an exact full-resolution cloud integrator.
No shadows or lamps are disabled to obtain these measurements.

## Focused checks and failures

40 area tests passed in build/sky-performance/final-tests.log; 6 additional
source-sky checks passed in source-tests.log (four overlap: 42 unique cases).
Both OpenGL and Vulkan cover:
- cloud cache vs direct full-resolution integration: mean channel error <1.5,
  <1% channels differ by more than8 in the bounded upward-facing fixture;
  changed-time negative fails; repeated frames/Off restoration are exact;
- linear environment cache vs direct per-tap decoding: max byte error <=2;
  bypassed-lighting negative fails; analytic BRDF/roughness checks still pass;
- neutral overhead/warm low sun; a substituted low-sun negative fails;
- retained sky-source colours under the display curve; replacing them with
  a synthetic atmosphere fails;
- pregrid and later race light match normalized enabled Env0 direction in
  Tsukuba, Laguna Seca and Route5. One-hour presentation offset does not move
  it. Native direction corruption negatives fail.

Initial Vulkan allocation used unsupported GL_FLOAT input for a null image;
it produced an empty lookup and the movement check failed. Replaced with the
backend-supported null GL_UNSIGNED_BYTE allocation while retaining RGBA16F
storage. All measurements taken before this repair are invalid. A preliminary
run overlapped another GPU test / an open game and was discarded for delivery.

## Measured local result, not a universal FPS promise

GTX1650, 1600x900 window, Tsukuba six-car grid, FordGT,90 measured frames after
20 warmup; hidden window, dummy audio, unpaced, source/render options matched.
Final paired logs: build/sky-performance/delivery-benchmarks.log and
 delivery-comparison.json. No other engine window was running.

| Renderer | Before mean frame | After mean frame | Before/after median | Before/after GPU |
|---|---:|---:|---:|---:|
| OpenGL | 29.979ms | 16.797ms | 29.260 / 16.483ms | unavailable |
| Vulkan | 35.424ms | 21.937ms | 34.507 / 21.904ms | 24.216 / 10.077ms |

About44%/38% less total frame time in this sample. Vulkan GPU time falls58%;
CPU/submission cost remains significant. This does not establish60fps on every
track, resolution or GPU. Classic pixels are byte-identical for both renderers
against the saved pre-task binary; all32MiB race memory matches for Classic and
Enhanced runs. Classic timing fluctuates (GL8.539→9.343ms; Vulkan12.231→12.616ms),
so there is no Classic performance-improvement claim.

An additional same-new-code run with both caches bypassed confirmed the cost
was repeated shading (GL32.030→18.791ms; Vulkan36.106→23.873ms), before the final
15Hz refresh/persistent binding reduction. This isolates caches from the colour
and authored-direction changes; it is a different timing run.

Local native captures: build/authored-<api>-tsukuba-pregrid.png,
build/authored-<api>-route5-pregrid.png and sky-performance/*-final-benchmark.png.
The free-camera Laguna integration image is a diagnostic view with incomplete
visibility/pose framing, not a visual-quality deliverable. No equivalent PCSX2
camera capture: full-image fidelity A; enhancements P. No Android/Switch build,
long race/thermal/memory test, dynamic sun cycle, all-weather or full-course
celestial survey certified. App Release updated locally, no push.


## 20 October 2026 — submission overhead follow-up and Tsukuba building tint

The Tsukuba pit-building comparison uses the same native pregrid camera, first
Enhanced then Classic, with no simulation advance. Local images:
`build/building-compare-pregrid.png` / `build/building-compare-classic.png`.
The mauve wall tint is visible in both. That establishes that the latest
Enhanced sky did not introduce the tint, not that the Classic raster already
matches PS2. No equivalent PCSX2 frame or raw-albedo proof was obtained, and no
building-specific desaturation/tint correction was applied. Source celestial
artwork still supplies visible sun/moon placement; enabled Env0 directional
light supplies the surface light. Those are separate evidence scopes.

P implementation optimization, identical pixels: view-level modern/daylight/
authored-environment/time uniforms are sent once per mesh, instead of again for
every material. Lens emission uniform changes only when the existing material
state changes; state equality already includes both lens tags. Vulkan stores
sampler-uniform indices when linking a program, and descriptor draws visit only
that list (at most8), instead of scanning all matrices/material/light uniforms.
Sampler units remain dynamic; descriptor keys, transitions and writes retain
the existing path. No shading samples, textures, lights or draw order change.
No new option is needed for an invariant submission path.

32 focused GL/Vulkan tests passed in `build/submission-baseline/area-tests.log`,
including missing-shading/paint/sky/reflection/lens-emission negatives. Paired
native Classic/Enhanced captures on each backend are byte-identical against a
pre-follow-up binary, and all32MiB race state is unchanged. Logs and exact
comparisons: `build/submission-baseline/indexed-comparison.json`. An earlier
uniform-only measurement was inconsistent on Vulkan; no gain is claimed for
that intermediate change by itself.

Bounded GTX1650,1600x900 Tsukuba grid,120 frames after20 warmup:

| Renderer | Previous mean frame | With both submission changes | GPU before/after |
|---|---:|---:|---:|
| OpenGL | 17.808ms | 16.863ms | unavailable |
| Vulkan | 21.667ms | 20.967ms | 9.913 / 9.892ms |

Roughly5%/3% in this short sample; small timing differences can include normal
run-to-run variation. It is a CPU submission reduction, not a new GPU-shading
speedup or whole-game60fps certification. Classic timings and broader
GPU/track/thermal coverage are not a guaranteed improvement. Overall PS2 pixel
fidelity and the origin/correctness of the existing mauve building tint remain A.
