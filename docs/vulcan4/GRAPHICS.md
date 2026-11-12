# GRAPHICS — fabulous without slopifying it

**Written 2026-10-15, on the captain's instruction.** Wishlist entry: `G6.38` (render presets). The one-line
brief from him: *"Think we can make the game look fabulous? Without. U know. Slopifying it?"*

---

## THE RULE THAT DECIDES EVERYTHING

Every proposed change is classified before it is built. The class decides the default:

| class | what it does | ships as |
|---|---|---|
| **ACCURACY** | makes the image **more correct** — same look, fewer artifacts | a quality option; may be on |
| **RESTYLE** | adds a **look** the original never had | a **preset, off by default** (`LAW 12`) |

If a change cannot be put in one of those two boxes, it is not ready to be built.

## WHAT WE ALREADY HAVE (measured, not hoped for)

- **The renderer is ours**: OpenGL 3.3 core, offscreen framebuffer, our own shaders that reproduce the GS's
  behaviour (alpha as a coefficient, destination-alpha test, per-tap texture addressing, per-pixel spherical
  environment mapping on cars). `docs/RENDERER.md`.
- **Resolution is already free**: 1280×720 → **3840×2160** and *Custom*, windowed or fullscreen, 4:3 or 16:9.
- **FSR 1 is already integrated** (`third_party/fsr1`, MIT) with five modes in the video settings — Native
  through Performance. That is *output resolution vs GPU cost*, not detail.
- **Text is already sharp at any resolution**: `src/core/glyph_sdf.c` converts the game's glyph atlases to
  **distance fields** (Mitchell-Netravali edge tracing) and the shader reconstructs them.
- **Studio/showroom shadow shaders exist** already.

## TIER 1 — ACCURACY (the cheap, safe, biggest-per-buck wins)

1. **Mipmaps + anisotropic filtering.** The road ahead currently shimmers; this is the single most visible
   improvement available in a driving game and it changes nothing about the art.
2. **MSAA (or SSAA) on the offscreen target.** Real edges, instead of smeared ones — deliberately *not* a
   sharpening filter, which invents halos.
3. **Vertex-snap removal (PGXP-style).** GT4's geometry wobbles and jitters because of PS2 vertex snapping;
   removing it is *accuracy*. GT2Recomp, one console down, already ships DuckStation-grade PGXP — the
   precedent exists and users treat it as a headline feature.
4. **Z-stability.** Reduce z-fighting, another GS-era artifact.
5. **Frame pacing / stable 60.** Not shading, but it reads as quality more than any filter does.
6. **UI art upscaling.** Panels, buttons, logos, maps and HUD plates are `.gpb` image packages
   (`menu.c:766`), and a community **HD HUD & UI pack** already exists for them — taken as the *user's* files
   through the `G6.23` loader. Text needs nothing (see above).

## TIER 2 — LOOK, AS PRESETS (off by default, and restrained)

A single post-processing pass (colour grade, gentle bloom, tone curve, motion blur, depth of field), each
individual effect toggled separately — photo mode is the natural showpiece for DOF and framing.

**Restraint is the feature here.** The default preset is *nothing*. A "Cinematic" preset exists for people who
want it, and the gate below still applies to it.

## TIER 3 — RE-LIGHTING (the project, and the one that can change the game's soul)

- **Real shadow maps** replacing the game's faked shadow geometry.
- **Environment reflections** replacing the spherical mapping.

Both are honest improvements *and* honest risks: they will make it look like a modern game, which is not the
same as looking like GT4. They stay last, behind a preset, and only after tiers 1–2 are proven.

## THE NO-SLOP GATE — checkable, not vibes

Every candidate frame is compared **side by side against PCSX2**, which this project already treats as the
oracle:

1. **Palette/hue deviation** from the reference frame must stay inside a stated threshold (per-channel mean and
   histogram distance).
2. **Any glow, contrast or light the reference does not have is a REJECT.** No effect may invent illumination
   that is not in the source.
3. **The floor:** with everything off, the reference frame (`md5 912bdc63bddc791afece574c7475eac8` on GL /
   `531acb651f5c8552f3540e9a57fdf08f` on Vulkan) must not move a byte.

**By the same rule, these are rejected outright** — they are the ingredients of slop, not of quality:
bloom-everywhere, fake-HDR oversaturation and crushed blacks, sharpening halos, chromatic aberration, film
grain, smeared fake ambient occlusion applied as a blanket, and AI texture upscaling that hallucinates detail
that was never in the asset.

## THE HONEST CEILING

Shaders polish; they cannot invent detail. GT4's textures are 2005-era, so the resolution half belongs to
**user-supplied HD texture packs** (`G6.23`) — the renderer's job is to present them correctly, not to
manufacture them. A "fabulous" build is therefore: **correct rendering + high output resolution + the user's
own better assets + restrained, optional effects.**

## SEQUENCING

1. **Tier 1 first** — accuracy only, verifiable, and it needs no taste decisions.
2. **Tier 2** only once there is a picture to judge effects on.
3. **Tier 3** last, as its own project.

Everything here is judged by **pictures**, not by commits: the captain looks at a frame and says *"that's it"*
or *"that's wrong"*. Nothing is called improved until he has seen it.

## Current implementation direction (2026-10-18)

The current alpha priority is general graphics quality. SDK feature work is
paused. Improvements remain independent presentation settings on both PC
renderers; Classic retains the previous rendering path. Current project
presets in docs/OPTIONS.md supersede the historical blanket-default wording
above: this does not authorize changing the retail simulation or removing
any Original setting.

The first addition is graphics.texture_filtering: Original, Trilinear,
Anisotropic 4x/8x/16x. Original keeps manual authored mip/nearest/region taps;
the alternatives use explicit texture gradients and hardware filtering for
ordinary repeat/clamp materials. Vulkan now requests samplerAnisotropy only
when supported and caches each anisotropy value separately. The effective
factor is capped to the device limit; unsupported devices retain trilinear.
GS REGION_CLAMP/REGION_REPEAT retain their existing rules in every mode.

This is a P enhancement. It adds no grading, bloom, light or texture assets.
Tests measure minor-axis contrast under an elongated texture footprint,
Original restoration and region-addressed invariance on both APIs. Real
course captures and simulation-state comparisons live locally in
build/texture-filter-proof; narrower final measured results are recorded in
docs/FIDELITY.md. Shadow mapping and re-lighting have not been implemented
by this filtering change.

### Geometric car-shadow option (2026-10-18)

The next graphics addition is graphics.car_shadows: Original / Projected.
The shared PC renderer builds a GPU silhouette from the assembled body and
posed wheels, softens its border and composites against receiver depth.
Course colour/direction/ground-lighting inputs and the contact blot remain
in use. Classic still calls the previous cast-volume renderer. Desktop
Screen Settings now separates Rendering and Presentation to keep controls
from overlapping; this is a UI adaptation, not a change to game scripts.

This is deliberately a local-ground projection, not world shadow mapping.
Self-shadowing, walls, alpha-cutout/transmissive materials and full curved
receiver coverage remain outside this step. See docs/RENDERER.md and the
fidelity record for proof and limits. SDK work remains paused.

### Per-pixel material response (2026-10-18)

The next optional PC step is graphics.material_shading (Original / Per-Pixel).
It uses the existing authored ambient/diffuse/specular coefficients and
shininess, sampling normals, highlights and environment coordinates per
fragment on both APIs. Classic retains the prior vertex-lit path. It does
not replace materials with guessed paint/glass/rubber presets and does not
introduce new light rigs or a generic gloss multiplier. Material strength,
pass masks and opacity continue to come from disc data.

Sixty analytical surface cases per API, reflection-coordinate checks,
deliberately disabled negatives, retained-path image comparisons and
real-disc/replay probes define the verified scope. See the renderer document
and fidelity record. Full PBR, glass refraction and roughness-filtered
reflections remain future work; the new setting is graded P.

### Generated reflection resolution (2026-10-18)

The independent graphics.reflection_quality setting exposes Original 128,
High 256 and Ultra 512 on both PC APIs. Classic retains 128; Enhanced uses
256. The existing race views/remap and studio panels are sampled into larger
generated targets, with no source-asset upscaling or new lights. Studio High
and Ultra compose authored panels on the GPU when resident reflections are
enabled, avoiding a larger CPU sum each frame. The CPU path remains available.

This is P and adds GPU work. Original image restoration, generated-detail
fixtures, menu persistence, race/replay state and timing evidence define
its scope; see the renderer document and fidelity record. Roughness filtering,
refraction and world shadow mapping remain future work.

### Local depth shadows and modern materials (2026-10-18)

Both PC backends now expose local car shadow maps (512/1024/2048) and a
separate Original/Modern lighting setting. Classic disables both; Enhanced
uses 1024 and Modern. Shadows use actual body/wing/wheel geometry, alpha-cutout
casting and a resolution-independent soft kernel, and can receive on the car
and course surfaces. Boundary derivatives are evaluated before early returns
to prevent false triangular self-shadow patches.

Modern adds GGX direct response, directional daylight colour, sky ambient fill
and filmic SDR highlights while retaining authored reflection masks. These are
P adaptations, with explicit conversions from PS2 material coefficients. They
do not replace baked course lighting, create a sunset, implement global
illumination or constitute full HDR/atmospheric rendering. Course/driver
casting, rough reflection filtering and full-scene HDR remain pending. See
[implementation, tests and limits](../RENDERER.md#optional-modern-race-lighting).

### Daytime atmosphere foundation (2026-10-18)

Enhanced now has an optional procedural clear-day sky, solar hours 07:00..17:00,
fixed/real-time/60x presentation progression, nearby course depth casting and
an RGBA16F main scene. Depth-occluded shafts, bloom and filmic SDR output happen
before the UI. Daylight, HDR and Sun Rays have independent shared-store toggles;
Classic retains the authored sky and renderer. Both PC APIs use the same code.
The atmosphere also enters generated car reflection faces with their original
half-intensity convention; nested LDR targets do not receive HDR scene encoding.

This is P. The reduced single-scattering model is a foundation for cloudy/rainy
conditions, not their implementation. No night, cloud simulation, rain, wet
physics, HDR10 output, global illumination or full-course cascades are claimed.
Track baked illumination and legacy reflection formats remain limitations.
See [implementation and proof](../RENDERER.md#optional-daytime-atmosphere-and-hdr-scene).

The final real-disc capture compares Original against the pre-change renderer:
zero changed pixels on OpenGL and Vulkan for this Ford GT/Tsukuba fixture.
Morning/noon/afternoon and replay images change as expected while all
33,554,432 simulation-state bytes remain identical in each paired comparison.
This is scoped regression evidence, not proof for every car, course or view.
Both build and dist executables were refreshed; SHA-256
067726ad89ece47f90ec07c0958ca02f898ddb879f183c4027dc4914634a5d60.

### Daylight contact regression (18 October 2026)

The daylight map previously reused the course's blended-shadow alpha and
attenuated only direct sunlight. After removing the authored hard contact
geometry, unoccluded sky fill washed out the replacement shadow. Opaque rigid
casters now use full solar occlusion in Daylight; the retained non-daylight
route still uses the course factors from func_003983E0. Alpha cutouts and glass
caster rejection remain intact.

The same filtered comparisons also accumulate nearby light-space occlusion
for ambient fill. Its exponential two-metre decay and 0.8 ambient strength are
explicit P presentation parameters, not PS2 measurements or full hemispherical
AO. It shares the directional map and therefore cannot model every ambient
occluder, especially at low solar elevation. The former polygonal contact
mesh stays suppressed for mapped cars; no hard overlay was reintroduced.

Six focused shadow-map cases pass across OpenGL/Vulkan, including a separate
contact negative. The synthetic opaque-caster fixture measures 38 pixels below
70% brightness, with minimum 51.4%; disabling the map gives zero such pixels
and 100%, causing the dedicated negative to fail. Flat/sloped self-surface,
cutout, glass, opacity, soft-edge and restoration checks remain in the fixture.
Evidence: build/contact-shadow-proof/tests.log and real-disc captures.

F-150/Tsukuba disc verification: the fixed front-grid underbody crop
(x=520..696, y=511..543 at 1280x896) changes mean RGB from 42.53 to 32.05
on both APIs (75.3% of the previous value). This is an image regression
measurement, not a photometric target. Chase captures change 200856 pixels
on OpenGL and 200775 on Vulkan; Classic changes zero pixels on each.
All 33,554,432 simulation bytes match in the paired comparisons. Twelve
additional focused modern-lighting/daylight cases pass (18 focused cases total).
The executable was linked to a separate output directory; dist and
build/Release were then updated with matching SHA-256 after the running app closed.

### Scene edge filtering and solar post fixes (18 October 2026)

Edge AA is a new independent value (4) of graphics.antialiasing, shared with
Screen Settings and personal preset interchange. Values 0..3 retain their
meaning. Enhanced selects Edge AA; missing preferences use the preset default,
while explicitly saved personal values remain authoritative. It is a single
frame luminance-directed filter, after tone mapping and before UI. Flat regions
are skipped; bounded along-edge taps smooth contrast transitions. It requires
no motion vectors or history. It cannot remove all temporal/specular/texture
aliasing and can soften small scene details. SSAA remains an alternative.
The retained Original sampling path with HDR uses one sample; Vulkan's retained
path also has no MSAA. FSR reduces scene resolution, so FSR + Original AA can
look strongly aliased. Selecting Edge AA returns sampling to native resolution
and clears FSR through the existing mutually exclusive controls.

The main scene now restores depth writes before clearing, because glClear
honours the depth write mask left by transparent draws. Rear-mirror rendering
no longer replaces the main-view solar projection; split-screen shafts are
explicitly disabled until they have separate viewport post buffers. Solar
projection expires after the final scene resolve, preventing stale shafts in
menus. The screen-space shaft integration now uses 48 samples, angular decay
5 and artistic scattering strength 0.3. These P parameters are not PS2 values
or calibrated volumetric transport. Rays depend on solar/view direction and
visible sky; no arbitrary beams are attached to the camera. Off-screen
occluders, full world volumetrics and cloudy/rainy weather remain missing.

Positive/negative GPU fixtures check LDR/HDR edge filtering, flat preservation,
Off restoration, float headroom, real depth occlusion and expired projection.
The diagonal fixture gains 476 transition pixels versus zero without AA;
flat interior remains 255/255. Shaft response is now 199 with open sky versus
149 with blocked depth or the effect disabled. Dedicated disabled negatives
fail the corresponding assertions. The shared launcher store also checks
Edge AA persistence and personal-preset round trips.

Real-disc F-150/Tsukuba captures on both APIs verify independent AA and shafts
on/off with identical 33,554,432-byte simulation states. The low-sun camera at
07:00 shows the depth-occluded radial effect. Classic comparison has zero changed
pixels on both APIs for this fixture. Evidence: build/aa-rays-proof, including
AA/ray image pairs and JSON comparisons. This is P, not PS2 imagery fidelity.

Final validation passes 16 focused cases: ten daylight/AA GPU cases, two
launcher-quality cases, two personal-preset store cases and two actual
Screen Settings Edge AA cases (including deliberate negatives). The menu
fixture selects value 4, cancels edits without changing the file, and reloads
value 4 on both APIs. The separately linked proof build and dist have SHA-256
001b624afa619c5f8071bd2502c12a2ce07c434640fbcae65d1b51b44c306de2.
After the running app closed, the build/Release shortcut executable was also
updated and its SHA-256 matched the dist executable.
Explicit player preferences were preserved; an existing FSR + Original AA
preset must select Edge AA to use the new filter.

### World-space sun volume and ambient depth (18 October 2026)

Sun Rays now has Off (0), the retained radial Screen Space effect (1), and
Volumetric (2). Enhanced selects 2; explicit saved values retain their meaning.
Ambient Occlusion is an independent boolean under Shadows, enabled by Enhanced
and disabled by Classic. Selecting either new effect enables its HDR/Daylight
dependencies in the shared launcher API and actual Options widgets. Personal
preset files persist both settings; source rendering, physics and replays use
the same data regardless of their values.

The main viewport publishes its camera axes, focal/depth coefficients and the
resident packed light-space depth map. The HDR resolve reconstructs each
pixel's surface position, then integrates 32 world-space samples between the
eye and that surface, capped at 80 metres. Each sample queries the light map,
so rigid objects and nearby course geometry interrupt scattering. Extinction
is 0.0015 per metre; a relative Henyey-Greenstein phase uses g=0.6 and scattering
scale 0.15, with an exponential upward density falloff 0.04 per metre. These
are explicit artistic P parameters for a clear-day presentation, not recovered
PS2 numbers or calibrated meteorology. The first high-density experiment was
not distributed; it washed out the whole car. Extinction and scattering now
use the same optical density. No CPU depth/map readback is required.

The volume is limited by the existing 128-metre local shadow map. Samples
outside that map assume unoccluded sunlight; there are no whole-track cascades,
multiple scattering, clouds or rainy weather. Unshadowed volume can render
without a map, but then objects cannot form world-space light shafts. Mirrors
and split-screen retain the existing guarded projection route; separate
viewport volume buffers remain unimplemented. The depth-reconstruction route
also cannot recover transparent surfaces absent from the scene depth.

Screen-space AO samples eight directions at two radii, bounded to 1.5 metres,
and reduces indirect-looking cavity/contact brightness. It is a final-scene
approximation: it attenuates composited direct/reflected energy too, cannot see
off-screen geometry, and is not GTAO, ray-traced AO or global illumination.
Derivatives are computed before the varying sky return to avoid undefined
quad derivatives at silhouettes. Small cavities without model geometry cannot
be reconstructed from a diffuse texture.

Daylight sky fill now uses a hemispherical normal factor, 0.12 + 0.88 *
(0.5 + 0.5 * normal.y), rather than equal sky illumination on the roof and
underside. The 0.12 ground-fill floor is artistic. Reflection overlays keep
legacy shading but now participate in the optional near-caster occlusion;
windows are not relit as opaque body paint. Opaque course casters now block
all direct light instead of retaining the previous 0.5 shadow-map alpha.
The source vehicle mesh, normals and silhouette are unchanged. Polygonal
silhouettes and missing geometric detail still require a separate geometry
solution; shading and AA do not create vertices or recover body details.

Focused GPU negatives disable each new effect independently. The controlled
volume fixture reads 155 lit versus 148 blocked by the light map; its disabled
negative gives 149/149. The AO corner fixture has 512 darkened pixels versus
zero when disabled. The hemisphere fixture verifies brighter upward-facing
surfaces while keeping the retained rendering path reachable. Real Options
fixtures verify AO, Volumetric, automatic HDR/Daylight dependencies, Cancel and
reload on both APIs. See local build/volume-proof logs and image/state pairs.
All these changes are P, not PS2 fidelity claims.

Final disc regression: F-150/Tsukuba AO changes 80622 pixels on OpenGL and
80630 on Vulkan; volume changes 1144243/1144254 relative to AO-only. Classic
comparisons change zero pixels on both APIs. All 33,554,432 simulation bytes
match in every paired comparison. The 34 focused renderer/store cases and
two actual Options cases pass (36 total), including deliberate negatives.
The executable is updated in build/Release and dist with matching hashes.
The active personal presentation file now enables HDR, Daylight, Volumetric
and AO, preserving the other choices; its prior bytes are backed up locally
in build/volume-proof/player-options-before.ini. No player save was modified.


## HDR sheen and daylight reflection balance (18 October 2026)

Grade P: optional PC presentation. The retained course sheen pass
(Original: func_003411D0 +0x0) produces encoded LDR colour. Its final additive
copy now converts that colour to linear light when entering the HDR scene;
blur intermediates remain encoded. Coverage and sheen source passes do not
receive daylight surface lighting. This prevents HDR from exaggerating pale
road/runoff patches without disabling the authored sheen mask.

Daylight vehicle environment overlays now recover the half-intensity source,
decode it, preserve authored tint and bounded strength, and use a dielectric
Schlick Fresnel term with a 0.04 normal-incidence floor. This is an artistic
material mapping, not recovered PS2 behaviour. Legacy rendering remains
reachable with daylight disabled. Roughness-prefiltered environments and
separate metallic/chrome classification are not implemented; course textures
and vertex colours still contain baked lighting, so complete photometric
agreement between cars and track is not claimed.

Both APIs pass the controlled reflection fixture: 33/255 at normal incidence
(expected 33.51 within two bytes) and 195 at grazing incidence. HDR sheen reads
153 versus a 154 linear-reference value, and zero with a zero coverage mask.
Deliberate reflection and sheen negatives fail as required. The 24 focused
modern-lighting/daylight tests and 14 material/reflection/shadow regressions
pass. Peugeot 206 RC/Tsukuba comparisons reduce the measured runoff crop mean
from 175.14 to 138.25 on OpenGL and 175.03 to 138.06 on Vulkan. These are image
measurements, not reference-game accuracy scores. Classic before/after images
change zero pixels on both APIs; all 33,554,432 simulation bytes match in each
paired run. Local evidence: build/reflection-fix-proof/comparison.json and logs.


## Shared daylight irradiance for cars and courses (18 October 2026)

Grade P. The daylight car BRDF previously used full sun and 0.5 sky strength,
while the course pass used 0.6 sun and 0.8 sky strength. These independent
exposures exaggerated the distinction between dynamic vehicles and baked
course surfaces. Both paths now call the same direct and hemispherical ambient
irradiance helpers, retaining the existing course calibration (0.6 and 0.8).
Those coefficients remain artistic PC choices. Material diffuse/ambient,
specular and environment coefficients remain separate material properties;
Classic, non-daylight lighting and simulation are unchanged.

The GPU fixture compares neutral course/car surfaces under sky-only and
sun-only illumination. OpenGL and Vulkan both read 200/200 for sky and
204/204 for sun, against independent analytic expectations 200.34 and 203.86
(tolerance two bytes). Its dedicated negative disables car modern shading and
fails at 102/200. Reflection and hemisphere negatives now reach their own
assertions instead of failing prematurely in the generic BRDF matrix.
All 26 focused modern-lighting/daylight cases pass. Local evidence:
build/light-integration-proof. This corrects a measured lighting inconsistency;
it does not remove baked course shadows, recover physical track albedo or
prove complete visual integration for all vehicles and courses.


## Bounded paint overlays and wheel contact (18 October 2026)

Grade P. This supersedes the unrestricted grazing-angle overlay described in
the HDR reflection balance entry. The retained additive environment layer is
not a complete energy-conserving paint model: allowing its Schlick weight to
reach one made it dominate the base surface. Daylight now uses 0.04 +
0.12 * (1 - N.V)^5, preserving authored tint and strength. The 0.16 upper
bound is an artistic PC clearcoat limit, not measured material reflectance.
The controlled grazing fixture changes from 195 to 80/255 on both APIs;
its normal-incidence value remains 33. The test explicitly rejects an
unbounded grazing overlay. Classic environment weighting is unchanged.

An isolated Ka/Tsukuba comparison confirms wheels already contribute to the
directional depth map (19380 changed pixels with wheel casters removed in the
front-view diagnostic). The missing visual cue was the small contact footprint,
not an absent wheel assembly. Daylight plus the existing optional
`graphics.car_shadows` now adds four depth-tested soft footprints. Their centres
use the visible wheel pose/CPRM snapshot and their size uses loaded tire width
and radius. Each footprint projects onto the current ground receiver plane.
It fades between zero tire clearance and half a tire radius above that plane.
The 0.65 peak opacity, radial falloff, footprint proportions and 0.02-metre
receiver lift inherited from the existing shadow route are artistic parameters.
One cached 64x64 procedural mask and two triangles per wheel are used; no game
assets or physics data are changed. Source: func_00454110 +0x28 for hub placement
through the retained wheel_place helper. The original contact mesh/path remains
available with Classic.

The four-contact fixture measures 24 dark pixels and minimum 46/255 in each
wheel region (128 baseline), 96 dark and 88 partial-edge pixels in total.
A raised opaque receiver hides every footprint. The dedicated negative omits
the contacts and fails. All 22 focused car-shadow, shadow-map and modern-lighting
tests pass, including deliberate negatives, on OpenGL and Vulkan. Real Ka front
views show 526 pixels changed by enabling contacts on each API. Local evidence:
build/shadow-artifact-proof. This is a planar ambient-contact approximation,
not ray-traced tire occlusion; discontinuous ground, detached pit wheels and
full material/environment accuracy remain outside these tests.


## Daylight paint albedo and ambient body footprint (18 October 2026)

Grade P. Daylight diffuse sky illumination now uses the same diffuse paint
albedo as sunlight; the original material ambient coefficient remains in the
non-daylight path. Texture modulation is normalized from the retained GS
128-white convention before sRGB decoding in the daylight car BRDF (255/128
for textured fragments, clamp to one). Previously the course had its encoded
normalization while the car's texture was decoded at half intensity. This is
a PC albedo conversion, not a claim of recovered physical paint reflectance.
It can clip authored texture values above 128; no independent metalness or
photometric material calibration is recovered. Original: func_0049EA18 +0x0
(retained car material path); the existing renderer uses 128-based GS colour
units and the course's separate doubled colour route. Local Ka source audit:
/car/lod/fdeu0006 model 0 texture 7 has RGB maxima 70/70/104 and 550 normal-bearing
kind-1 triangles. Temporary survey instrumentation was removed after measuring.

The existing daylight/car-shadow option now draws a broad, soft underbody
footprint in addition to the four tire contacts and directional shadow map.
Its centre uses projected wheel hubs; extent uses the loaded assembly bounds;
coverage averages wheel-ground contact, so lifting the vehicle fades it out.
The radial mask is shared with tire contacts. Peak body opacity 0.45 and
extent factor 0.6 are artistic parameters. This is a sun-independent planar
ambient-occlusion approximation, not an independent indirect-light buffer:
alpha composition also attenuates any direct/reflected contribution at the
same pixels. It does not replace the directional shadow or physically trace
sky visibility. Slopes/discontinuous ground, detached pit wheels and full
material matching remain limitations.

Both APIs pass 28 focused modern-lighting, car-shadow and shadow-map cases,
including dedicated texture, paint, ambient-footprint and retained negatives.
A texture sample 64 normalizes to 0.5 and reads 97/255 against 96.62 analytic
expectation (two-byte tolerance); the disabled-path negative reads 26.
Sky-only coloured paint with zero legacy ambient reads 45/63/169 against
44.94/62.94/169.52; its disabled negative fails. The broad body footprint centre
reads 79 against 128 without it, and a raised receiver occludes the footprint.
Ka/Tsukuba front-view crop mean changes 28.93 to 52.17 on OpenGL and 28.92 to
52.19 on Vulkan. This crop includes background and is only a repeatable visual
measurement, not a PS2 or physical accuracy score. Local logs and screenshots:
build/ambient-paint-proof.


## Rough road reflection composition (18 October 2026)

Grade P, part of the optional daylight route. The retained course sheen
(func_003411D0 +0x0, mask at +0x34 and reflection set/sky at +0x24/+0xC8)
adds its projected image through destination alpha. That retained additive
intensity is not a modern rough asphalt response. Daylight composition now
weights the decoded reflection by 0.04 + 0.06 * (1 - abs(N.V))^5. This uses a
rough dielectric IBL approximation with F0=0.04 and artistic roughness 0.9;
F90 is max(1-roughness,F0). Ground normal is world up, so banked/sloped road
normals and inferred wetness are not supported. No physical asphalt roughness
was recovered from the disc. The original colour, mask and source blur remain;
Classic calls the existing unweighted path. Blur intermediates explicitly
reset rough weighting so the factor is applied only once at composition.
HDR applies the factor after linear conversion. LDR applies it in the retained
encoded compositing space and is not a radiometric reference.

Both APIs pass all 18 focused daylight cases, including a deliberate
rough-road negative. Controlled original/rough-normal/rough-grazing values
are 153/10/17 with zero output for zero mask; an independent linear reference
predicts 10 for the normal-angle case. The negative bypasses rough weighting
and reads 153 instead of 10. Real Peugeot 206/Tsukuba paired 1200-frame captures
at 640x448 reduce the measured apron crop mean from 138.11 to 91.64 on OpenGL
and 137.92 to 91.62 on Vulkan. This is an image measurement, not original-game
accuracy certification. Classic images change zero pixels on both APIs and
all 33,554,432 simulation bytes match in every paired comparison. Local evidence:
build/road-roughness-proof.

Wheel-disc visibility is a separate unresolved image report: model/track
identification is pending. Source inspection confirms func_00453698 +0x124
calls the brake hub transform 00454110 without spin, while +0x1AC calls
004541A0 for wheel spin. No guessed disc fade or geometry suppression was added;
that model's actual rim coverage/blur still needs comparison.
