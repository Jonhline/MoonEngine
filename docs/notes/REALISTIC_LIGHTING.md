# Realistic sky and lighting reference pass (20 October 2026)

Status: P (optional PC rendering), not a photorealism certification. The user
rejected the earlier flat/grey result; these changes do not establish a final
look. No physics code, parameters or replay simulation fields changed.

## Primary references and what they establish

- Polyphony, *Driving Toward Reality: Physically Based Tone Mapping and
  Perceptual Fidelity in Gran Turismo 7* (SIGGRAPH 2025), pages 17–21 and
  26–29: sunlight/atmosphere, measured automotive paint, view-dependent solar
  highlights and scene-linear rendering followed by display mapping.
  https://www.polyphony.co.jp/publications/
  https://s3.amazonaws.com/gran-turismo.com/pdi_publications/s2025_PBS_Physically_Based_Tone_Mapping_GT7.pdf
  This is a reference for the approach, not a claim that Moon reproduces GT7.
- Filament, *Physically Based Rendering*: direct/indirect units, GGX/Smith/
  Schlick, image-based lighting, coating energy conservation and exposure.
  https://google.github.io/filament/main/filament.html
  The measured clear-sky noon table gives sun 105000 lux and sky 25000 lux.
  That ratio informs the fill/direct calibration, not absolute lux output.
- PBRT 4, *Transmittance*: Beer-Lambert extinction and ordered volume transport.
  https://www.pbr-book.org/4ed/Volume_Scattering/Transmittance
- Guerrilla, *The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn*:
  volumetric density/lighting instead of a flat sky texture.
  https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn

## Changes and calibration

The reference noon sun now has unit luminance (the former arbitrary 0.6 scale
made white car paint unnecessarily grey). Sky coefficient is 0.330863143:
(25000/105000)/0.719618498, where the denominator is the luminance of our
existing artistic noon sky colour. This preserves the measured *ratio*.
It is a relative radiance convention, not physical scene calibration. The
baked-course denominator changes with it, preserving clear neutral noon
rather than applying a second lighting/exposure pass to baked PS2 textures.
No global saturation multiplier was added. At the user's explicit request,
Realistic Lighting warms direct sunlight by relative linear RGB
(1.25, .98, .68), normalized to preserve its input luminance. Skylight and
moonlight keep their existing colours. This is intentional warm art direction,
not a measured noon spectrum or a claim that all sunlight is orange. Cloud cover was reduced after the
first native capture looked overcast; sun visibility and scattered fill remain
coupled. Density thresholds are artistic, not recovered GT4 weather.

The reflection overlay no longer uses a fixed 4–16% angular cap. Thirty-two
Hammersley GGX normal samples integrate environment radiance with correlated
Smith visibility and Schlick dielectric F0=.04. Sampling decodes each source
texel to linear before filtering. Authored strength/tint remain masks. Rough
surfaces broaden the reflection. This is not a complete automotive paint
model: separate original passes do not yet supply a shared coating-energy
budget, metal classification or measured paint/roughness data. Probes remain
half-range LDR sphere maps and cannot recover clipped sun radiance.

Realistic Skybox adds a 1500–2200m cloud slab, four density octaves, 32 view
steps, 3 solar steps, Beer-Lambert extinction .0034/m and approximate forward
scattering. A mid-plane optical-depth sample attenuates surface sunlight;
half of blocked direct light is redistributed into sky fill. That is a bounded
approximation, not a multiple-scattering solver. Clear-air volume rays are Off.
Wind uses the simulation-frame presentation clock, not wall time.

## Validation and remaining limits

GPU fixtures cover both OpenGL and Vulkan: analytic direct BRDF cases, an
independent incident-hemisphere quadrature for a uniform environment,
reflection roughness broadening, fill/direct contrast, baked noon preservation,
cloud visibility/motion/repeatability and exact Off restoration. Deliberate
negatives bypass the enhancement or restore excessive fill/sharp reflections.
The stripe variance fell from about 560.62 to 15.50 before the final exposure
normalization (reflection-only test is unaffected by that normalization).

Initial failures: the car/course fixture incorrectly called exposed white
radiance white albedo; it now supplies actual baked reference irradiance.
SDL's main macro prevented an include-based fixture from running its intended
entry point; daylight_baked and realistic_sky now explicitly select it.
The reflection fixture was first accidentally run without daylight, then
corrected to exercise the new path. Older reflection-cap assertions were
replaced by independent white-environment quadrature. These failures were in
fixtures, not evidence that a failed visual result was acceptable.

Local logs: build/realistic-warm-tests.log, build/realistic-lighting-ui-tests.log.
Local images: build/realistic-lighting-proof/*-warm-front.png and
*-after-benchmark.png. Paired Classic captures compare against the previous
local inspection binary; its precise source revision is not known, so this
is a bounded regression check, not a source-version or PS2 equivalence claim.
Each run also compares all 32 MiB of native race state. No screenshots or
reference PDFs belong in Git.

Outstanding: proper diffuse-only screen AO (current post AO still attenuates
complete radiance), prefiltered HDR probes, measured/material-aware paint and
metals, indirect bounce/occluded sky, longer-range solar cascades, full weather
and atmospheric multiple scattering, display/exposure calibration and broad
GPU/platform performance checks. Baked illumination cannot be removed from
original textures without additional asset work. Mesh silhouettes remain PS2.
No equivalent PCSX2 capture; original full-image fidelity stays A. No Android
or Switch GPU certification for these desktop shader changes.
