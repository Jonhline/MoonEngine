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
