# Native renderer

Implementation: `src/platform/sdl/video_gl.c and src/platform/sdl/render3d.c`.

SDL owns the window and graphics context. The OpenGL backend renders decoded model geometry with texture addressing, alpha tests, blend state and depth state derived from disc data. Course visibility lists limit scene submission. UI drawing and 3D drawing share host output but use different projection rules. Whole-frame comparison remains separate from tests of individual draw states.

## Car material lighting

Surface highlights and environment reflections are separate authored passes.
The renderer retains each vertex's material specular colour and shininess;
reflection strength also uses authored vertex colours and destination-alpha
masks. It does not assign a single gloss coefficient to the whole car.

The specular-only path (`func_0049EB90 +0x84..0x94`) consumes the specular
colour block without ambient or diffuse lighting. Its contribution must be
selected before the race lighting path saturates the vertex colour. Clearing
the separate specular output first used to erase that layer entirely.
Environment geometry uses VU1 program `0x215`, independently of the surface
light blocks; the shared PC shader must not overwrite its authored colour
with ambient/diffuse lighting when no angular environment override is active.

`car_material_opengl` and `car_material_vulkan` render 21 cases each: three
specular strengths for diffuse and specular-only surfaces in both lighting
paths, plus three reflection colours under three specular strengths. Expected
channels allow one byte of GPU quantization. Both deliberately corrupted
material tests must fail. The previous shader failed the specular-only race
and environment-colour cases in both APIs. These focused checks cover pass
separation, not complete PS2 image equivalence: car appearance remains A
pending comparisons of actual glass and paint with the original game.

## Shared PC material submission

OpenGL and Vulkan use the same mesh-local material and texture cache. A new
mesh submission starts with an invalid cache; alpha-fail actions invalidate
material state again. Destination-alpha snapshots retain their original
placement in the draw sequence. Nothing is cached across meshes or views.

Compatible material ranges are computed once when an immutable mesh is
created. Per-view visibility checks and model-list limits still decide which
parts may be submitted. Diagnostic environment switches are sampled at the
view boundary, rather than looked up again for every mesh.

After visibility checks, consecutive opaque VBO ranges with identical
material, texture, source model, lamp and wheel-blur attributes can share a
draw. Alpha-tested ranges are also eligible when their fail action is KEEP:
failing fragments are discarded without a second pass. Their triangle order
is unchanged. Blended, multipass alpha-fail, destination-tested, skinned and
ghost draws remain separate; model-list
boundaries and the existing visibility granularity are retained. This is a
PC resource optimization, graded P, rather than a new PS2 rendering proof.

`MOON_RENDER_REFERENCE=1` retains uncached, unbatched PC submission for
comparisons. `MOON_RENDER_NO_BATCH=1` disables only merging. The existing
`MOON_VK_REPEAT_COMMANDS` diagnostic still disables the Vulkan material
cache independently of batching. These switches support verification; they
do not change physics, resolution, material parameters or LOD selection.

The synthetic `shared_batches_opengl` and `shared_batches_vulkan` tests
exercise two adjacent ranges with alternating passing/failing alpha and
vertex colours. Opaque and alpha KEEP ranges must merge; alpha fail writes,
destination testing and blending must remain separate, with exact RGBA and
triangle counts. Each has a deliberately disabled-batching negative.

The `shared_render_opengl` and `shared_render_vulkan` tests compare dry
Tsukuba, wet Tsukuba and a recorded-race replay against that reference.
They require exact final RGB, identical 32 MiB simulation state, identical
triangle and visibility totals, and fewer material/texture updates. Draw
reduction must match merged-group accounting, with actual merging in the
dry-course case. Each backend has a negative that forces the reference in
the optimized branch and must fail the reduction gate. `MOON_SHARED_FRAMES`
sets the measured length; `MOON_SHARED_CASE` isolates race, wet or replay.
The driving probe cycles chase, onboard and roof cameras every 240 ticks;
its update remains fixed at 60 Hz. Lifetime work counters include nested
mirror and reflection views, unlike the existing per-view counters.

The probe drains GPU work at the boundaries of its measured interval.
OpenGL reports CPU wall time; it explicitly reports that GPU timer data is
unavailable rather than presenting Vulkan's zero counters as a measurement.
The checks establish invariance within each backend, not equality between
OpenGL and Vulkan or equivalence to PS2 output. Final-frame images and
work totals also do not certify every intermediate frame or every course.

### Measured work and performance limits

A 600-frame, 1920x1080 comparison submitted 30,988,185 triangles in the dry
course in both branches. Draw calls fell from 859,312 to 818,972; material
applications fell from 845,808 to 202,764. The uncached reference is an
ablation of these shared optimizations, not the preceding Vulkan release:
Vulkan already had mesh-local material caching before this change.

An initial scenario run overlapped another MoonEngine process; its wall
times are excluded from performance conclusions. After closing that process,
three alternating previous/current executable pairs measured 600 frames per
run on a GTX 1650 at 1920x1080, VSync off. The final implementation computes
compatible ranges at load time and samples diagnostic choices per view.
The values below are medians of three run averages, not frame percentiles.
All previous/current captures (including PNG bytes) and 32 MiB states matched.

| Backend | Previous median | Current median | Frame-time reduction |
| --- | ---: | ---: | ---: |
| OpenGL | 18.069 ms | 17.845 ms | 1.2% |
| Vulkan | 19.791 ms | 19.305 ms | 2.5% |

Run-to-run variation is substantial: OpenGL previous/current averages range
16.993-18.261 / 16.721-18.012 ms; Vulkan ranges 18.213-20.016 /
18.335-19.598 ms. The first Vulkan pair was slightly slower. These small
scenario gains do not establish a universal or sustained FPS improvement.
Frames above 16.667 ms remain, and 300 FPS is unverified. No physics
tick-rate change, render-frame interpolation, quality reduction or
resolution change is part of this work.

The final precomputed ranges also reproduce the saved 600-frame wet-course
and replay reference captures/state in each backend, with the same triangle
and visibility totals. The registered paired tests can reproduce these
comparisons directly with `MOON_SHARED_FRAMES=600`.

### Driver validation

Khronos synchronization validation passed eight focused cases: shared Vulkan
batching and its negative, the synthetic frame/resize fixture and its
negative, retained-command comparison and its negative, and the shared
submission comparison and its negative. The disc comparisons used 60
measured dry-course frames per branch; the separate uninstrumented dry,
wet and replay checks covered 600 frames. No validation errors or
synchronization hazards were reported. Material tests for both PC APIs and
the existing shared Switch-renderer regression checks also passed. Only
focused renderer tests ran, not the full suite.

## Evidence and limitations

The PC renderer choice and native Vulkan implementation are documented in
[PC graphics backends](VULKAN.md). OpenGL is the default; Vulkan draws menus
and 3D scenes and remains experimental pending broader coverage.

The [development record](notes/RENDERER.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.

## Optional spatial visibility and resident reflections

The original course Vls0 selection and the port's existing flat per-group
frustum checks remain available. Two independent settings in Options >
Screen Settings select additional PC work. Classic disables both; Enhanced
enables both. They persist in the shared options store, apply only after OK,
and have no solver or replay-input effect. The renderer's existing fidelity
limitations remain; selecting Classic does not certify all PS2 imagery.

`graphics.spatial_visibility` builds a bounding-volume hierarchy over immutable
rigid mesh groups at load time. It partitions group indices spatially without
reordering geometry or materials. Union boxes reject subtrees; planes that
fully contain a box are omitted for its descendants. Exact leaf support-point
tests retain the previous tolerance and decisions. Small, mutable, skinned
and authored-model-list submissions keep flat traversal; the original course
list is already a useful restriction and querying an unrestricted tree for
every list entry would repeat work. Allocation failures and nonfinite boxes
also fall back to flat checks. Both backends share the implementation.

This is conservative frustum acceleration, not occlusion culling or new
geometry LOD. It neither chooses lower-detail meshes nor hides objects behind
other objects. Full original car-body LOD selection remains incomplete.
Future occlusion or generated LOD must be separate options, preserving the
original path and carrying their own image and performance evidence.

`graphics.resident_reflections` uses the existing GPU sphere-map remapper in
both PC APIs. The six 128x128 scene views and their draw order are retained;
OpenGL no longer reads each view to CPU memory and uploads the remapped image
back. Vulkan's resident path already existed; it now has the same explicit
setting and CPU fallback. No lower resolution, skipped view or reduced
refresh rate is introduced. The CPU/GPU bilinear result can differ through
float/UNORM rounding; tests allow at most 2/255 in the final RGB comparison.

`MOON_FLAT_VISIBILITY=1` and `MOON_REFLECTION_CPU=1` force the retained paths
for diagnostic comparisons. Scene settings are read at the renderer boundary,
never from physics. Switching resident reflections off also selects CPU
binding when an old GPU environment image remains allocated.

### Validation

`spatial_visibility` compares the hierarchy with flat tests for 1,000
randomized cameras over 4,096 boxes, including mirrored views, narrow model
ranges, degenerate bounds and the tolerance boundary. Invalid boxes reject
hierarchy creation. The negative flips a visibility result and must fail.

`scene_pipeline_opengl` and `scene_pipeline_vulkan` independently compare
flat/CPU, spatial/CPU, flat/GPU and spatial/GPU cases. Hierarchy-only RGB must
be exact; CPU/GPU remaps have the stated 2/255 bound. All four branches must
retain draw counts, triangles, original visibility totals and the complete
32 MiB simulation state. Hierarchy work must decrease, and resident rendering
must remove all six reflection-face readbacks per frame. Their negatives force
both reference paths in every branch and must fail. `MOON_SCENE_CASE` selects
race, wet or replay; `MOON_SCENE_FRAMES` sets measured length. These disc-image
comparisons use Pillow in the configured Python interpreter (`python -m pip
install Pillow`); it is a test dependency, not part of the executable.

The settings fixture exercises both values independently, apply/cancel,
persistence, preset modification and Classic/Enhanced. A negative corrupts
the Classic result. Existing desktop settings tests cover the extended page.

Six 600-frame scenarios (dry/wet/replay in both APIs), each with all four
setting combinations, passed. Final resident RGB differed by at most 1/255
from CPU reference in those captures; hierarchy-only images were exact.
Dry-course support-point box tests fell from 2,601,141 to 1,042,772,
wet-course tests from 2,405,062 to 1,001,815, and replay tests from 2,634,920
to 1,026,101. The 3,600 CPU reflection readbacks per 600-frame run became
zero in resident mode. Primitive submission and simulation were unchanged.
The Classic-return check runs after allocating/using the GPU image and
requires the CPU reference's exact final RGB and full simulation memory.

These counts establish less synchronization and visibility work, rather
than a guaranteed frame rate. Scenario timings vary with camera, shader
warm-up, driver presentation and other system work. OpenGL gains resident
reflection remapping here; Vulkan already had that path, so comparing its
CPU reference to GPU mode is not a speedup over the previous default.
The spatial hierarchy has its own traversal/mask overhead and may not improve
every scene's wall time. No universal or minimum FPS claim is made.

The PC options page keeps authored fonts, frame and dropdown content but
uses fixed native row positions. Allowing the original VBox/FBox packer to
restore template row heights clipped the added controls. Its compact rows
now remain within the pane, including Cancel/OK, as checked in a generated
capture. This layout is a PC adaptation, not original-screen equivalence.

Focused validation also covers runtime return from resident GPU reflections
to Classic after allocating the GPU image. The final dry OpenGL/Vulkan
120-frame comparisons and their deliberate negatives passed. Vulkan's
four 60-frame modes plus the Classic return passed with Khronos validation
and synchronization validation enabled; the driver reported no validation
errors or synchronization hazards. These checks supplement the six
600-frame dry/wet/replay comparisons above; they do not establish a universal
FPS increase.

## Optional PC texture filtering

`graphics.texture_filtering` is independent of Texture Smoothing. Original
retains the previous shader taps, minification and region-address rules.
Trilinear and anisotropic 4x/8x/16x use `textureGrad` for ordinary repeat/clamp
surfaces, including existing authored mip chains. This matters because
manual `texelFetch` taps otherwise bypass the driver's anisotropic sampler.
No new mip levels are synthesized for incomplete authored chains; existing
mip limits remain in force. Special region addressing stays on its prior
shader path. Sampling state is updated at texture submission only when the
option changes, including restoration of the original wheel-blur sampler.

Vulkan enables the optional samplerAnisotropy device feature when present,
caps the sampler factor to maxSamplerAnisotropy and includes the factor in
the sampler cache key. Dynamic descriptor caching already keys the sampler
handle, so changing filtering cannot reuse a stale descriptor set. Devices
without that optional capability retain trilinear filtering. No extra CPU
texture copies, readbacks or model reloads are required when toggling it.

Focused texture_filtering_opengl/vulkan tests and their deliberately disabled
negatives exercise distant stripe contrast, exact Original restoration and
region-addressed invariance. This is P visual enhancement, not original-game
image equivalence. Player controls and presets are registered in OPTIONS.md.

## Optional geometric car shadows on PC

`graphics.car_shadows` selects Original or Projected, independently of
texture filtering and AA, on OpenGL and Vulkan. Classic retains the old
`draw_shadow` path. Projected replaces the simplified cast volume with a
silhouette of the current body and independently posed wheels. Fitted wings
already belong to the assembled body. The original contact blot remains.
Direction, tint, ground frame and sun/shade attenuation come from the same
course/pose inputs as Original; peak authored cast-volume vertex alpha caps
the replacement opacity. Physics and replay data are not modified.

The silhouette is unioned into a reusable GPU mask with blending disabled:
intersecting body and wheel triangles cannot repeatedly darken the ground.
A separable three-tap filter softens the edge. Composition reconstructs the
local receiver's camera-space depth, including reversed-Z, and depth-tests
against the existing scene without writing depth or changing target alpha.
Foreground cars and scenery therefore occlude the projection. Targets use
half output dimensions, capped at 1024 pixels wide, with no CPU geometry
projection or readback. The race view owns and destroys these resources;
uniform locations are cached at creation. Failed setup falls back to the
original shadow path.

This is **P**, a local-plane projection rather than a shadow map. It does
not provide self-shadowing, wall receivers, arbitrary curved terrain
conformity, glass transmission or alpha-cutout silhouettes. The caster uses
rigid body/wheel geometry, not driver skinning or a separate pit-crew caster.
Those limitations are explicit; world shadow mapping remains future work.

`car_shadows_opengl/vulkan` checks projection, soft edge, overlap union,
zero opacity, moving caster, foreground depth and exact disabled restoration.
Its negative removes the cast pass and must fail. Local Tsukuba captures,
replay comparisons and timing samples are in build/car-shadow-proof; see
FIDELITY.md for the measured scope. The optional pass costs GPU work and
is not an FPS optimization.

## Optional per-pixel material shading

`graphics.material_shading` selects Original or Per-Pixel on both PC APIs.
Classic selects Original; Enhanced selects Per-Pixel. It is independent of
texture filtering, car shadows, resolution and AA. It applies at the lit
model submission boundary, including race, replay and car previews. Unlit
course/UI layers and contact/cast-shadow rendering stay on their old path.
The shared options store, game Rendering page and launcher Quality list
expose the same value, with live application and no model reload.

The source already separates material coefficients: `model.c` reads the
80-byte MDLS material rows, retaining ambient at +0, diffuse at +16,
specular at +32 and shininess at +64. Original func_0049EB90 +0x84..0x94
loads specular-only colour vectors; func_0049E918 +0x100..0x150 selects the
race's 4/8/16 specular powers. VU1 0x215 handles environment layers apart
from diffuse lighting. These original reading rules are unchanged.

Per-Pixel interpolates transformed normals, camera-relative positions and
the authored coefficients, then evaluates the surface light terms in the
fragment shader. The view ray comes from that fragment's position. Race
power selection, light colours, vertex-limit saturation and specular-only
pass separation remain in use. Studio lights retain the decoded exponent
and existing specular scale. Reflection layers evaluate sphere-map
coordinates and the existing course angular colour factor per pixel,
retaining their own vertex colour, alpha tests and destination-alpha masks.
A zero specular coefficient stays zero; no generic gloss is assigned to
paint, glass, rubber or metal. Material names/classes are not inferred.

This **P** enhancement changes sampling and view-dependent evaluation; it
is not a new physically based material format. It adds no invented
roughness/metalness/IOR, lights, replacement environment assets or global
colour grading. Existing low-resolution environment maps, authored normals,
paint masks and geometry still limit the image. Full PBR, roughness-filtered
environments, refraction and complete PS2 material-image parity are not
implemented by this change. Original keeps vertex lighting and interpolated
sphere-map coordinates.

The focused material_shading tests run 60 analytical surface cases per API
(specular strengths, powers and separate passes), reflection gradient and
strength checks, zero-specular isolation, unchanged flat/unlit layers and
byte-exact Original restoration. An independent CPU calculation checks the
selected fragment with a two-byte allowance; measured maximum error is zero.
The curved-normal fixture has an interior highlight absent from vertex
sampling; its deliberate disabled-mode negative must fail. Previous material,
shadow and filtering regressions remain separate focused tests. Real-disc
Ka/Ford GT/M5 captures and state comparisons are local to
build/material-shading-proof; measured scope is recorded in FIDELITY.md.

## Generated reflection quality on PC

`graphics.reflection_quality` offers Original 128, High 256 and Ultra 512
independently of per-pixel material shading, filtering, shadows and AA.
Classic selects Original; Enhanced selects High. The game Rendering page
and launcher Quality list share the persisted setting. It changes generated
environment dimensions, not source textures, light intensity or materials.

The race still renders the same six views from the existing car-relative
reflection camera and remaps them with the existing sphere-map convention.
Only face/map dimensions grow. Resident targets stay on the GPU in both
APIs; disabling GPU reflections uses the existing CPU remapper at the chosen
size. Targets and CPU buffers are rebuilt only when their size changes.
No simulation state or reflection cadence changes. The 128 path remains the
retained PC reference, not a new assertion of complete PS2 equivalence.
Original context: func_0044E528/func_0044EEA8; the native cube remap already
approximates the game's three 120-degree views.

Studio panel maps use the selected sphere-map size; their existing 448-pixel
software-rendered source faces remain the detail ceiling. Optional larger
maps use GPU four-layer composition when GPU reflections are enabled,
retaining the authored Alpha0..3 weights. Original retains CPU composition.
Unsupported/disabled GPU composition falls back to the same CPU sum. The
initial panel camera basis is retained across rebuilds, so changing quality
does not bake a later animated camera into the Original map. Authored static
environment images are not enlarged or described as higher-detail assets.

This is **P**. It increases target storage and fill work; it is not an FPS
optimization, new reflection geometry, a roughness filter, screen-space
reflections or a change to reflection masks. Synthetic reflection_quality
cases resolve angular stripes at 256/512 that collapse at 128, test all
sizes and exact restoration, and include a deliberately wrong-resolution
negative on both APIs. Real race/showroom captures, live quality cycles,
physics comparisons and timing samples are in build/reflection-quality-proof.

## Local car depth shadows on PC

`graphics.shadow_maps` is independent of the retained geometric-plane setting:
Off restores that setting's existing path; Low/High/Ultra allocate 512/1024/2048
maps on either PC API. Enhanced selects High, Classic selects Off. A 32-metre
orthographic light volume follows the viewed car, with its origin snapped to
light-space texels. Casters wholly within the guard band use their current body
(including fitted wing) and posed wheels. Other cars retain the previous shadow
path; the original contact blot remains. Course light direction and authored
course/caster opacity feed the new pass. This is a **P** enhancement.

An RGBA8 texture stores 24-bit light-space depth and caster opacity, alongside
a real depth attachment. The pass disables dithering while packing depth and
restores framebuffer, viewport and scissor. Reflection layers do not cast twice;
blended glass is excluded from opaque casting. Ordinary and GS region-addressed
alpha cutouts sample the uploaded native alpha, retaining the alpha test and
selected brake branch. No per-frame CPU readback is required.

Receivers compare against sixteen Gaussian-weighted taps with 9-cm spacing in
world light space, so increasing resolution sharpens geometry without removing
the soft edge. A receiver-plane depth gradient corrects each tap before the
quarter-texel raster bias. Derivatives are evaluated before the non-uniform
volume-boundary return: evaluating them after that return caused false triangular
self-shadows. The flat/sloped self-surface fixture detects this (20 falsely
shadowed pixels before the repair, zero afterward).

GPU tests on each API cover all resolutions, moving and duplicate casters,
zero opacity, a raised body receiver, flat/sloped self surfaces, transparent glass,
alpha-cutout shadow energy, invalid levels and exact Off restoration. A deliberate
disabled-map negative must fail. Scope is local rigid car casting, not an entire
course shadow system: track buildings/trees and skinned drivers do not cast into
this map. Glass transmission, area-light contact-hardening and cascaded world
shadows remain pending. The fixed soft kernel is an intentional presentation
choice, not the original game's penumbra model.

## Optional modern race lighting

`graphics.modern_lighting` selects Original/Modern, independently of material
sampling and shadow resolution, on OpenGL and Vulkan. Enhanced selects Modern;
Classic selects Original. It applies at the race/replay view boundary and does
not change course assets, vehicle state, replay data or the studio launcher.

Modern evaluates direct material response in linear light using GGX, correlated
Smith visibility and Schlick Fresnel, following the [Khronos BRDF equations](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation).
Authored diffuse, ambient, specular and shininess remain separate inputs; additive
specular-only passes receive no second diffuse/ambient contribution. Existing
environment masks retain their authored Fresnel rather than being attenuated a
second time. The conversion `roughness = sqrt(2/(shine+2))`, clamped to 0.08..1,
and dielectric `F0 = 0.04 * clamp(specular,0,1)` are explicit PC approximations:
the original assets do not provide modern roughness/metalness maps.

The first light, when above the horizon, uses relative Rayleigh transmittance at RGB
wavelengths 680/550/440 nm, optical depth 0.1 at 550 nm and inverse-elevation air
mass capped at 12.5. Luminance normalization retains source light power while
low elevations warm the direct light. A hemisphere sky/ground tint fills ambient
light (ground tint 0.85/0.78/0.68, sky tint 0.78/0.88/1.0); shadow visibility attenuates direct illumination while leaving ambient
fill. These daylight parameters are a single-scattering presentation model, not
measured weather or a forced golden-hour preset. Source direction and light
colour remain inputs; neither time of day nor the sun trajectory is changed.

Highlight display compression uses the [Narkowicz filmic fit](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/)
and sRGB transfer functions. This is per-material SDR compression, not a complete
HDR frame pipeline or ACES colour-management implementation. Baked course vertex
lighting/textures, sky artwork, fog and original pass blending remain; they cannot
be described as fully dynamically relit. Global illumination, reflection
roughness filtering, full-scene HDR exposure and atmospheric scattering remain
future work. The setting is **P**, not PS2 fidelity E.

The independent CPU comparator covers 120 material/light/shine/specular cases per
API with maximum measured error 1 byte/channel (allowed 2), exact opacity,
unlit invariance and exact Original restoration. Deliberately disabling Modern
fails the negative. Real race/replay state, previous-build Original images and
GPU samples are recorded locally in `build/shadow-map-proof/comparison.json`.

Validation for this stage: 28 distinct focused renderer/store/menu tests pass,
including the deliberately failing variants. Previous-build Original images
match with zero changed pixels on both APIs; all 33554432 simulation bytes
match for race/replay comparisons. Vulkan High-shadow and High-shadow/Modern
samples measured 5.136 and 5.105 ms of GPU time respectively (120 frames each).
Other desktop work was active, so these samples are not an isolated FPS
benchmark or evidence that Modern makes rendering faster. The passes add work;
no per-frame CPU readback was measured in the resident path.

## Optional daytime atmosphere and HDR scene

This stage supersedes the retained-contact-blot and local-car-only limits of
[the first depth-shadow implementation](#local-car-depth-shadows-on-pc) when
Daylight is enabled. All additions are **P**, independent shared preferences;
Classic selects Daylight/HDR/Sun Rays off. No original sky asset or simulation
code is replaced. The original path still draws authored sky/environment data.

The Enhanced sky is generated from eight single-scattering view samples and
four solar optical-depth samples over a spherical Earth (6360/6460 km radii).
Rayleigh RGB coefficients 0.005802/0.013558/0.0331 per km, molecular/aerosol
scale heights 8/1.2 km, Mie coefficient 0.003996 per km and phase asymmetry 0.76
follow the Earth example and phase formulation described by
[Bruneton](https://ebruneton.github.io/precomputed_atmospheric_scattering/index.html)
and [PBRT](https://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions).
The implementation is an original reduced single-scattering integration, not
Bruneton's full precomputed multiple-scattering renderer. Solar radiance 20,
disc radiance 4 and the 0.0042..0.0052-radian disc transition are presentation
parameters. Ozone, multiple scattering, clouds, rain, snow, weather transitions
and night are not implemented by this stage. It is the clear-day atmospheric
foundation for those systems, not a complete weather simulation.

Solar Time selects 07:00..17:00 using an equinox model at fixed latitude 35
degrees. This is an explicit presentation scenario, not the real date/location
of every course. Cycle is Fixed, Real Time or 60x, evaluated from the existing
read-only simulation frame count. At the daylight bounds it reflects smoothly
in hour (the hour's derivative reverses); there is no jump to night or wall-clock
advance while paused. The same configuration/frame produces the same daylight
in replay. These visual preferences are not serialized into replay physics.

The solar direction drives opaque car material illumination, a replacement
procedural sky in the scene and reflection faces, and a 128-metre nearby shadow
volume containing the body, wing, wheels and intersecting course draw groups.
Light-space bounds reject unrelated world groups. This is not a multi-cascade
whole-course shadow system. Parts outside the volume retain the previous
shadow route; skinned drivers/gadgets are not newly added as casters here.
Baked course shadows/vertex colours remain in the diffuse base and cannot be
claimed to have been removed. Course diffuse relighting is an intentional
approximation (sky fill 0.8, direct 0.6); the atmosphere and surface model are
not a calibrated photometric scene reconstruction.

Depth comparisons now interpolate four comparison results per weighted tap
(64 comparisons), rather than snapping each tap to one texel. This provides a
continuous penumbra without interpolating packed RGB depth bytes. The kernel
uses 0.18-metre spacing for the larger daylight volume; the retained local
32-metre mode keeps 0.09. The hard authored contact geometry is suppressed for
cars covered by an active map: drawing it over the filtered map retained a
polygonal black edge. Off restores the authored path. Receivers retain the
pre-branch derivative fix, and flat/sloped self-surfaces remain unchanged in
the GPU fixture. The shadow fixture measures 19/19/16 intermediate penumbra
levels at 512/1024/2048 on each API, alongside cutouts and deliberate negatives.

HDR allocates an RGBA16F main scene on both APIs. The final pass performs SDR
filmic compression, sRGB encoding and restrained highlight bloom (5x5 taps,
threshold 1, gain 0.08) before optional FSR and full-resolution UI/HUD. The HDR
route currently uses one sample; SSAA/reduced-scale sizing is retained.
It is an HDR scene pipeline with SDR monitor output, not HDR10 output. Legacy
reflection/sheen targets remain encoded LDR; a framebuffer-boundary test
ensures they do not accidentally receive linear HDR radiance. Procedural
reflection sky retains the legacy half-intensity convention. This avoids
washing out metallic reflections. Authored material shininess is retained;
normal-derivative variance only reduces subpixel specular aliasing, without
the discarded extra broadening that made paint look matte. Blended legacy
windows/overlays retain their original light contribution rather than being
lit as opaque paint. This does not implement new metalness textures or
roughness-prefiltered environment maps.

Sun Rays project the same sun and integrate 32 depth-mask samples toward it.
The depth attachment is copied on the GPU; covered pixels block shafts instead
of using image luminance as a proxy. The effect is screen-space: off-screen
occluders, volumetric world shadows and multiple scattering remain limits.
The current single-view projection is not a verified per-viewport solution
for split-screen/mirrors; those views remain validation debt. The bloom/shaft
parameters are artistic presentation choices, not measurements of GT4.

Independent positive/negative fixtures check solar geometry/daylight bounds,
clock progression, float headroom, nested reflection-target isolation,
procedural sky colour/time response, exact Original restoration and real depth
occlusion. Float input 2/4 resolves to 246/252 rather than both clipping at
255. The shaft fixture reads 185 with open sky and 149 with either an opaque
depth mask or shafts disabled; its dedicated negative disables the effect.
Procedural zenith RGB is 86/141/192 at noon and 65/113/164 at 08:00 on both
APIs. These are integration checks, not an atmospheric radiometry oracle.
Real-disc image/state/timing evidence lives locally in build/daylight-proof.

Final focused validation: 34 renderer/store cases and two real options-menu
cases pass, including deliberate negatives. Float-to-RGBA8 snapshot copying
is checked numerically (0.25/0.5/2 -> 64/128/255), so retained UI snapshots
convert formats instead of copying incompatible pixel bytes.

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


## Wheel rim and brake visibility audit (18 October 2026)

The brake attachment and spinning rim are distinct submissions. Original:
func_00453698 +0x124 calls func_00454110 for the brake's hub/steer/camber
transform; +0x1AC calls func_004541A0 for rim spin. The brake representation
therefore does not acquire the rim's spin merely because both share a hub.
This source inspection does not prove a particular photographed disc is
correctly visible or that all cars have identical coverage. A circular blurred
rim can also resemble a solid disc; no speed-based disc fade is established
by these paths.

The existing exposure/bank oracle is now registered with CTest: 50000
original exposure cases are bit-identical (E for these scalars/selection),
50000 deformation samples compare within 0.000001, and 2816 accumulation
command graphs match the original 00453BB0 sequence. Rotated quad XY tolerance
is 0.00002 at the shared libm boundary. The GPU accumulation fixture compares
48 images per API with independent CPU affine rasterization: maximum channel
error 4/255, the declared tolerance. This is C for the PC transport, not GS
rasterization equivalence. All comparator negatives fail deliberately.

Real-disc Ka, Legacy B4 2.0GT and Esprit V8 2002 fixtures compare fixed-pose
sharp versus synthetic angular exposure, on OpenGL and Vulkan. Changed pixels
are 17067/17067, 33083/33083 and 21280/21274 respectively. Each uses 48 GPU
accumulation passes; cached redraw needs zero new passes and is byte-identical.
Scene draw/triangle counts and all 33554432 simulation bytes remain unchanged.
Each model/API has a negative that removes exposure and fails the comparison.
All 20 focused audit cases pass. Evidence: build/wheel-visibility-proof logs
and build/wheel-visibility-<api>-<car> captures.

The whole moving-wheel/brake appearance remains A until compared against a
matched original-game image. No gameplay renderer, brake visibility, geometry,
lighting or wheel-exposure formula was changed for this audit. Only automated
test registration and the probe's full-state invariance assertion were added.

## Packed HDR depth transport

The HDR scene depth copy now uses matching packed depth/stencil formats:
DEPTH32F_STENCIL8 for reversed depth and DEPTH24_STENCIL8 for normal depth.
Previously the destination was depth-only DEPTH_COMPONENT32F, which violates
OpenGL depth-blit format requirements (OpenGL 4.6, section 18.3.1):
https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf
A rejecting driver could leave the depth sampled by ambient occlusion and
volumetric lighting invalid. Vulkan accepts the same packed target formats.
The original preset with HDR disabled does not use this copy.

C (PC depth transport only): six focused hdr_depth cases pass, including
negative comparators, both APIs and both OpenGL depth conventions. Four
resolution/AA modes are checked. Maximum copied depth error is zero with
reversed depth and 2.98023224e-08 with normal depth (tolerance 2e-6).
The 18 daylight regression cases also pass. Evidence is in
build/suzuka-shadow-proof/depth-details.log and depth-tests.log.
The reported black asphalt patches near Suzuka's first corner have not been
reproduced locally; this fix does not establish that report's cause or visual
resolution on the affected computer.

## Optional world-space car lamps (20 October 2026)

P: graphics.car_lamps, Classic Off / Enhanced On. The GPU receives up to 24
world-space emitters from six cars, four each. Candidate front surfaces are
identified by comparing the authored night and day body streams (material,
texture, attributes and geometry); rear candidates also include unlit pressed
brake branches. Area-weighted centroids and normals are grouped by side and
front/rear using bounds, not per-car coordinates. Pair-averaged horizontal normals
set aim, with an explicit artistic .03 downward slope for headlights. This is
inference, not recovered optical lamp metadata. Missing candidates remain absent.

Front range 60m, 25-degree outer cone, quadratic range fade and 1/(1+.06*d²)
attenuation; rear 8m and 65 degrees. White front radiance (6,5.4,4.5); red rear
(1,.015,.005) times .35 normally or 3 under braking. Nearby unlit lens fragments
within .22m add emissive output, with gain 3. Headlights/tails require the original
Env0 night flag or safety-car exception; brake emitters follow the existing
per-entry pedal presentation. Curved lenses without normals use triangle normals.

The same GLSL illuminates course, walls and car surfaces on OpenGL/Vulkan. Linear
addition enters HDR before the existing bloom; SDR uses sRGB encoding without
re-tonemapping the existing picture. The later optional car-lamp occlusion entry
adds rigid-world/car depth blocking to this pass. Geometry identification is approximate
and not surveyed over all 600 cars. Daylight car colours stay on their own option.

car_lamps_opengl/vulkan test cones, range, HDR output, off restoration, emitter
coordinates and deliberate disabled-light negatives. launcher_quality checks
persistence and Classic/Enhanced. Real Focus/Route5 produces four emitters and
changed pixels on both APIs; exact whole-frame API equality is not established
because the existing backends have other raster differences. Captures remain
local build/car-lamps-<api>-enhanced-lamps.png. Original PS2 image parity remains A.

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


## Optional car-lamp occlusion (20 October 2026)

P: the existing independent `graphics.car_lamps` option now includes a perspective
shadow atlas for its additive light. Classic Off / Enhanced On; no new setting,
physics, replay controls or source time-zone/brake gates. The original RNW4 lamp
shadow/model-3 floor glow passes remain separate. This is a PC adaptation, not a
claim that these spot shadows reproduce PS2 passes 0038F168/00451EE0.

Each active light gets a 256x256 depth tile (24 maximum, 1536x1024 RGBA8 atlas,
one reusable depth target). Source geometry supplies the emitter pose. Resident
course/scenery, posed rigid car bodies/wheels and visible live gadgets are
submitted from the light's view, independently of the camera visibility list.
Ghosts, reflection copies and blended non-cutout surfaces are excluded; authored
alpha rejection is retained. Packed linear depth, four comparisons and a 3 cm
receiver bias affect added light only; emissive lenses and baked lighting stay.
Atlas entries are invalidated on each new lamp frame. No GPU readback.

C within bounded tests: lamp_occlusion OpenGL/Vulkan checks an opaque blocker
behind the viewing camera, movement restoring illumination, transparent glass,
alpha rejection, final atlas slot 23, HDR and byte-exact Off restoration. The
negative omits the atlas and fails the same blocked-surface assertion. Real
Focus/Route5 integration changes 7587 OpenGL / 7604 Vulkan pixels (max 30/channel)
and restores Classic byte-for-byte, with sampled car/driver state unchanged;
its no-caster negative fails. 26 focused cases pass including existing solar
shadow, night radiance, sky and car-lamp regressions. The initial build lacked
the fixture's miniz dependency; repaired without relaxing assertions. Local log:
build/lamp-occlusion-regression-tests.log. Native comparison images:
build/lamp-occlusion-<api>-{classic,clear,blocked,restored}.png.

A/P limits: 256-pixel tiles can alias; caster coverage omits skinned drivers,
pit crews and particles; light inference still needs a broad car survey. No
complete-race FPS/memory benchmark, 24-lamp race stress test, console GPU coverage,
whole-frame API parity or equivalent PCSX2 capture. This closes missing rigid
world/car occlusion in the tested path, not every transparency or shadow case.


## Car-lamp model coverage, ghosts and bounded cost (20 October 2026)

P/C: `graphics.car_lamps` (Classic Off / Enhanced On) now accepts lit kind-1
pressed-brake geometry, recovering missing rear emitters on F-150, BEAT, R390,
GT-One and the pace car in the 12-model survey. All twelve night models produce
two front/two rear candidates; day models retain rear candidates, with the pace
car's source always-night selection. These counts do not certify optical axes,
all materials or full PS2 images. Ghost entries no longer emit physical light.
A native six-car fixture checks all 24 depth tiles, ghost/opaque/day/brake gates,
pose/driver state preservation and no renderer readback. The initial survey and
ghost positives failed before their corrections; deliberate negatives fail.

Lamp-only support-point frustum bounds reduce unnecessary shadow submissions;
19,995 generated-box decisions match an independent corner reference, excluding
five numerical-boundary cases. Focus/BEAT Classic/lamps/night images before and
after culling are byte-identical on both APIs (12 comparisons). 38 focused cases
pass. Native gallery: build/lamp-coverage-gallery/index.html, 24 car/API runs.
Bounded six-visible-car timing on GTX 1650 is about 14.1 ms OpenGL / 26.9 ms Vulkan
with occlusion, native 640x480 and other enhanced passes Off. This is a static,
unpaced snapshot, not a full race FPS or all-options benchmark; Vulkan remains
a performance concern. Physics and replay formats are unchanged. No equivalent
PCSX2 capture; full appearance A/P. Details, failures, timings and limits:
[coverage note](notes/CAR_LAMP_COVERAGE.md).


## Direct car-lamp atlas and shadow batching (20 October 2026)

P: performance-only change inside graphics.car_lamps, Classic Off / Enhanced On.
All 24 256px depth tiles remain; one atlas framebuffer replaces 24 scratch/copy
transfers. Contiguous caster ranges merge only with identical shadow-relevant
texture/alpha/brake state and retained visibility, preserving primitive order.
Directional solar maps retain their previous path. No physics/replay changes.
42 focused automated cases pass; legacy OpenGL depth also passes the reference
and deliberate negative. Logs: build/lamp-atlas-final-tests.log and
build/lamp-atlas-legacy-depth.log.
C: GPU copy-reference fixture on OpenGL/Vulkan retains 159000 occupied pixels,
zero alpha mismatches and max depth error 5.96046e-8 (limit 2e-6). A missing-last-
tile negative fails. Four native before/after six-car images are byte-identical.
Native submissions fall 4527 to 4080 with the same triangle count. The modest
paired timing gain is hardware/sample-specific; Vulkan remains above 16.67 ms.
Depth storage grows approximately 11.25 MiB in the tested D32F/S8 mode; full-race
FPS, memory residency, broader GPUs, Enhanced combinations and PS2 imagery are
not certified. [Reference, measurements and limits](notes/CAR_LAMP_COVERAGE.md#direct-atlas-and-shadow-only-batching-20-october-2026).


## Tagged emissive lamp materials (20 October 2026)

P: graphics.car_lamps, Classic Off / Enhanced On. Lens emission now follows the
loaded night-lens/pressed-brake geometry tags, independently of lit/unlit material
kind. Group creation and draw merging retain that tag, preventing nearby paint
from acquiring lens emission. Brake release keeps the authored released branch;
no source geometry, original lighting, physics or replay command change.
34 focused cases pass on OpenGL/Vulkan, including missing-emission negatives,
lit/unlit night and pressed branches, released lamps, adjacent-paint exclusion,
HDR bloom outside the lens and byte-exact Off restoration. Four native BEAT/F150
API captures retain Classic byte-exact against prior images. Local captures:
build/lamp-emission-<api>-<car>-enhanced-lamps.png; logs:
build/lamp-emission-final-tests.log and build/lamp-emission-native.log.
A/P: generic source inference/axis/output and .22m emission falloff remain; no
all-car material or equivalent PCSX2 image certification. This extends emission
to tagged lit lenses, not arbitrary untagged emissive surfaces.


## Realistic sky / lighting reference pass (20 October 2026)

P, optional desktop shaders: coherent Realistic Lighting and Realistic Skybox
switches replace the fragmented visible controls. Realistic Car Lamps names
the existing independent lamp switch. Clear-air rays default Off. Sky-only
and lighting-only draws both initialize their required solar uniforms.

Sun luminance is normalized to one; sky fill uses the measured clear-noon
25000/105000 ratio, adjusted for the existing sky tint. Baked surfaces retain
the neutral noon denominator. Cloud volume/shadow and 32-sample GGX sphere-map
integration replace cloudless sky/fixed reflection cap. These are bounded
improvements, not measured GT4 albedo recovery, complete clearcoat, HDR probes
or certified photorealism. Post AO still needs indirect-only separation.
See [full references, equations, limitations and failures](notes/REALISTIC_LIGHTING.md).


## Authored sky and Enhanced shading cost (20 October 2026)

This supersedes the constant warm filter/artificial course orbit above. Retain
source sky geometry/texture for sun/moon placement and use normalized primary
Env0 light direction (00398210) for surface light. Pregrid initializes the same
solar fields as racing; it previously enabled sky shading with zero light data.
Course celestial cycling is suspended, cloud motion remains frame-deterministic.
High sun uses neutral direct light; excess optical air mass warms lower sun.

The cloud volume is integrated into a256x128 RGBA16F hemispheric image at15Hz,
plus128x128/8192m ground shadow field. The same32/3 density integrations remain;
angular/spatial/time lookup is approximate. Reflection source colours decode to
a source-sized RGBA16F texture per revision, preserving32 GGX samples while
using hardware interpolation of linear radiance. Units5..8 are reserved for
these persistent inputs. Reference bypasses are local test diagnostics.

Forty-two focused cases passed on both backends. The bounded local1600x900
sample reduced total frame time about44% GL/38% Vulkan and Vulkan GPU time58%;
Vulkan submission cost and broader performance coverage remain open. Classic
pixels and native race memory stayed exact against the copied pre-task binary.
[Full evidence, failed attempt and limits](notes/LIGHTING_PERFORMANCE.md).


### Lighting submission follow-up (20 October 2026)

View uniforms are applied once per mesh; the existing material-state comparison
also gates lens-tag updates. Vulkan links an index list for sampler uniforms,
then descriptor submission visits only that list while retaining dynamic units,
keys, transitions and descriptor writes. Native Classic/Enhanced pixels are
identical on both backends, with32 focused checks and unchanged native race
memory. This removes CPU submission work; the GPU shading is unchanged. A short
local run shows a small5%/3% GL/Vulkan total-frame improvement, not a universal
FPS promise. [Evidence and remaining tint question](notes/LIGHTING_PERFORMANCE.md#20-october-2026--submission-overhead-follow-up-and-tsukuba-building-tint).

### 21 October: full procedural sky cache

The sky no longer composites over the authored sky texture. Source irradiance
remains available for surface normalization. A 256x128 RGBA16F full-sky lookup
shares unit 5 with the raw cloud volume according to the draw pass; no added
sampler is required. The existing 15 Hz deterministic cache key is retained.
Night celestial detail and the solar disc use the direct path. Deep night
skips daytime scattering integration. This is part of Realistic Skybox.

### 21 October: reflected coat and legacy highlights

Realistic Lighting uses roughness 0.1 for the authored environment-overlay mask,
independently of the original highlight exponent. The original separates these
passes, and the exponent is not a measured coat parameter. Direct GGX highlights
retain their existing conversion. This is a generic dielectric coating, not a
new metallic material classifier. See [Filament clearcoat](https://google.github.io/filament/Filament.md).

### 21 October: preserve mixed-material authored sheen

The source 003411D0 sheen coverage is not an asphalt roughness map. The shared
compositor retains authored masked radiance in modern mode as well as Classic.
The world-up/F0 multiplier was removed rather than applied to water and unknown
surfaces. HDR still decodes the intermediate once. This does not introduce a
physically based water shader or certify water parity.

### 21 October: display-referred authored world lighting

Authored course textures/vertex colours contain baked light and are display
values, not measured reflectance. The modern world path lifts decoded source
colour through the inverse of the HOST filmic fit before relighting; final
compression then preserves the neutral source image instead of crushing its
shadow-side detail again. The inverse solves the existing host rational curve
per channel; it does not assume the PS2 used that curve. It also applies to
retained authored-night WORLD colour in HDR. Procedural sky, direct car BRDF
and untagged fixtures keep their own radiance paths. No extra sampler or blur
pass. This remains a P mapping with baked shadows, not a de-lit PBR asset set.
