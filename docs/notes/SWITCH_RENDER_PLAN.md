# Switch rendering optimization plan

Research and source audit, 2026-10-14. This is an implementation plan, not a
claim that any proposed optimization has reached 60 FPS. No runtime changes
were made for the initial research; subsequent native implementation is recorded
separately in SWITCH.md. Desktop rendering and simulation remain reference
paths. Switch quality/performance tradeoffs are grade P and need their own
settings and console measurements.

## Target and existing measurements

60 FPS allows 16.667 ms per presented frame. Supplied log 4 records 41.597 ms
for Ka/Laguna (24.04 FPS), 49.300 ms for Escort/Tsukuba with opponents (20.28 FPS,
only 82 physics-active frames), and 35.683 ms for Escort/Tsukuba Time Trial
(28.02 FPS). Reaching the target would require roughly 53-66% less frame time
in these samples, not another small percentage improvement. Physics takes
0.773-1.644 ms on average. These are CPU wall-clock spans, not GPU timestamps.
The 13-18 ms scene spans at Tsukuba include GL operations outside draw_mesh.

Log 4 measures the shader build, not commit ab0f73c's later draw-state cache.
Different cars, grids, cameras and routes invalidate direct speedup claims.
There is no new docked sample or GT Mode map sample in that log. First showroom
readiness remains 2387.294 ms, with a 1017.82 ms first submission; this loading
problem must be measured separately from steady rendering.

## What the current code already does

This table records the audited Mesa baseline. The later native backend has its
own implementation and limitations in SWITCH.md.

| Technique | Source evidence | Remaining work |
|---|---|---|
| View-frustum rejection | render3d_frustum.h builds six planes; render3d.c draw_mesh rejects group AABBs before group state/draw | A car/actor has no single early hierarchy rejection before its lighting, pilot and wheel preparation; all groups are scanned |
| Authored course visibility | course.c gt4_course_vision_open/select validates Vls0, selects a grid candidate/model list; race_view.c draw_scene_box submits it | Audit pass coverage and measure real selected/culled/submitted counts; it is already wrong to say the whole course is always drawn |
| Separate race models | car_model.c selects car/menu versus car/lod or car/open, plus race wheel/wing assets | This file choice is not distance-adaptive LOD. draw_car has no screen-size LOD switch; audit original LOD metadata/branches before deriving alternatives |
| Resident geometry/textures | render3d.c creates static VBOs and uploads textures on load | Geometry is an expanded triangle list: 19 floats/static vertex, 27/skinned vertex, glDrawArrays rather than indexed reuse |
| Small material groups | render3d.c caps groups at 384 vertices for culling, with restricted adjacent opaque merging | Real showroom logs report zero merged groups; synthetic 3-to-1 batching is not representative of those cars |
| GPU environment remapping | race_view.c course_reflection plus switch_render.c avoids six CPU readbacks | Still redraws six 128-square sky/scenery views per scene call, then remaps them every frame |
| Reduced 3D scene targets | video_gl.c/switch_render.c caps full showroom/race at 960x540 handheld, 1280x720 docked, single sample | Ordinary direct CarFace and map ModelFace rendering do not uniformly use this reduced scene path |
| Studio preparation | showroom.c render_layers/render_panels makes static room and panel images once | Software preparation remains on the drawing thread when first available |

Visibility is per view. Rear-view mirrors, reflections, shadows, photos and
replays may require objects behind the main camera. A global forward-half-space
cut would break them. Keep physics, AI, collisions and progression active even
when their visuals are not submitted.

## Selected direction

After this audit, the user selected immediate deko3d migration and explicitly
allowed Switch-specific graphics rather than reproduction of the original
passes. The first integrated native build is documented in SWITCH.md. The
sequence below remains a performance research roadmap, not a requirement to
finish every GL optimization before starting the native backend. Gameplay,
physics and the desktop renderer remain preserved.

## Recommended work order

### 1. One controlled benchmark and per-pass measurements

Use the same saved replay/camera, car paints, opponent grid and settings on the
same console, separately handheld/docked. Record warm average and p95/p99 frame
times; preserve loading spikes in a separate cold/second-entry measurement.
Measure menu, navigating/stationary GT map, dealership CarFace, full showroom,
Tsukuba and a heavier track. Replay controls presentation repeatability;
validate a live race separately to include simulation cost.

Add sparse, delayed GPU timer queries when supported by the runtime, with a
ring of results and no wait for unavailable data. Isolate CPU and GPU time for
reflection generation, road sheen, main world, cars/pilots/wheels, shadows,
map/CarFace and final composition. Count draws, primitives, state changes,
target switches and DATE snapshots over the complete frame, not only the last
r3d_begin pass. The existing passive profiler cannot prove Mesa CPU overhead
versus GPU saturation. Timer queries can themselves perturb a driver; retain
uninstrumented confirmation and never force glFinish.

Diagnostic A/B workloads should independently freeze/bypass reflection
*production*, simplify shading, lower only 3D resolution, and replace detailed
geometry. MOON_NO_REFLECT currently skips reflective mesh groups but does
not bypass course_reflection's six producer views: it alone cannot measure
that producer's cost. Such bypasses are diagnostic controls, not release fixes.

### 2. Reduce repeated passes, especially reflections

Measure course_reflection first: six auxiliary views run before the primary
world every scene call. Cache the world-space capture while it remains valid,
remap it for the current camera, and compare amortized face updates against
full refresh. Camera rotation alone does not move the world-space capture
origin; capture invalidation must consider car movement and changing lights/
scenery. Publish a completed cube consistently to avoid face seams. Lower-detail
geometry for reflection/shadow passes can save work even when the main car
must stay detailed. Scheduling and reduced update frequency are P changes;
check fast turns, tunnels, night lighting, reverse view and camera cuts.

Measure road_sheen's mask, target rendering, blur passes and composite
separately. Avoid redundant full-size target copies or attachment changes only
where dependencies permit. DATE reads preceding destination alpha; removing or
merging snapshots across an intervening alpha write is incorrect. Preserve the
existing blend/mask/alpha-fail reference and its image/depth regressions.

### 3. Reject complete invisible actors before preparing their parts

Build conservative bounds for body, wheels, wing, driver and the relevant
shadow envelope. Test each view before expensive per-actor light/pose/draw
preparation. A hidden body does not prove its shadow is invisible. Maintain
animation state needed for correct reappearance. Reuse the authored course
visibility selection first, then a spatial hierarchy/BVH to avoid scanning
unneeded groups. Preserve original draw order among retained leaves.

Occlusion culling is a later addition for genuinely hidden, expensive chunks
behind buildings/hills. Start with conservative static visibility or large
occluders; test an asynchronous hierarchical scheme only if geometry/overdraw
measurements justify it. Do not issue and immediately wait for one GPU query
per small object. Open circuits and the single-car showroom may gain little.

### 4. Fix geometry/submission organization

Introduce indexed VBO/IBO rendering using exact full-attribute vertex equality,
including normal, UV, colours, alpha, skin and morph data. Preserve triangle
order initially; compare full RGBA/depth with the expanded stream. Measure
actual vertex reuse and upload size before adding packing/quantization.

Batch compatible materials within proven ordering boundaries. Texture arrays
or safe atlases may reduce switches; native wrap, mip regions and alpha rules
must survive. Instancing is useful for repeated identical scenery assets, not
for arbitrarily different cars. Separate simple opaque pipelines from masked,
blended and DATE cases, and compile all selected variants before interaction.
Front-to-back ordering or a depth prepass should apply only to independently
reorderable opaque geometry and only after showing that reduced overdraw pays
for extra submission. Globally sorting PS2 material streams would break masks.

### 5. LOD based on projected size, including pass complexity

Audit authored body/wheel/pilot LOD metadata and original selection first. Merely
loading car/lod is not an implementation of distance-based selection. Keep
close player/showroom/photo models detailed; use lower-detail variants for
small opponents, distant props and tiny map icons. LOD should reduce draw/state
count and secondary passes, not just triangles in the same number of batches.
Use projected screen size/error with stable transitions, rather than a single
arbitrary world-distance cutoff. Keep all needed levels resident when feasible;
never simplify meshes synchronously while driving.

If authored levels cannot supply the required budgets, evaluate an optional
load/cache-time simplifier preserving UV/material seams, normals and appearance.
Do not mistake CAR4 auxiliary shadow models for lower-detail visible bodies.
Derived LODs are P, must preserve installed wheels/wings and silhouettes, and
are never substituted into physics or saved gameplay state. Independent images
at transitions, negative wrong-level tests and checks in mirrors/photos are
required. No fixed distances, triangle percentages or gains are established yet.

### 6. Apply a consistent 3D pixel budget

Keep UI/text native while rendering map icons and direct dealership/garage
CarFaces into appropriately sized targets. Preserve the page under a car and
its alpha/mask dependencies; downscaling the entire already-rendered page would
also blur the UI. Reuse static icon/background renders while inputs are
unchanged; turning/highlighted icons must still animate. If pixel workload is
proven dominant, add bounded dynamic 3D resolution based on delayed GPU timing,
with stable recovery. It will not fix CPU submission bottlenecks. Hardware mip
sampling and texture formats/compression can be evaluated after bandwidth is
measured; alpha encodes behavior and cannot be casually compressed/remapped.

### 7. Native backend when submission remains the limiting stage

If measured CPU command generation/driver time still exceeds the available
budget after reducing passes/draws, deko3d is the strongest Switch-specific
backend candidate. It targets Tegra X1, compiles shaders at build time and
supports reusable native command lists. It does not automatically remove
shader/overdraw/bandwidth cost. The SDK/library/uam are already installed.

Implement a complete representative scene slice (world, car, textures,
alpha/blending/depth/DATE, secondary targets and UI composition), then compare
the same scene through both backends. Track framebuffer/texture ownership,
barriers, resource retirement and frames in flight explicitly. A standalone
triangle or swapping a library is not a game renderer migration. Desktop GL
remains available. Do not assume mesh-shader-era GPU features on Maxwell.

### Loading work in a separate track

Move static showroom room/panel raster preparation to worker-owned scratch
storage, then publish completed immutable results under synchronization.
Do not concurrently rasterize into showroom's live pixels/depth arrays. Decode
textures/models and read/decompress large ISO blocks off the render thread;
GPU uploads stay on the render thread with a bounded budget. Versioned derived
resource/font caches on SD reduce repeated opening costs. These measures target
hitches and loading, and are not claimed to turn a steady 24 FPS race into 60.

## Acceptance and decision gates

- Test each stage against a fixed baseline; report milliseconds saved, not a
  predicted FPS multiplier. Retain changes only for a measured benefit or a
  necessary structural prerequisite.
- At 60 FPS, the sustained CPU critical path and GPU workload must each fit the
  frame budget, with headroom for worst visible sections. They overlap, so do
  not simply sum CPU submission time and GPU timestamps.
- Stage 1 chooses whether pass reduction, actor/LOD work, pixel cost or a native
  submission backend receives implementation priority. There is no safe basis
  for promising that LOD alone, occlusion alone or deko3d alone yields 60 FPS.
- Image/depth comparators with deliberate negatives cover changes intended to
  preserve output. Intentional quality changes remain P and need visual review.
- Run only affected tests. This research changes documentation only; no game
  build, runtime test or new console measurement was performed for it.

## Primary references

- [NVIDIA: Graphics Pipeline Performance](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-28-graphics-pipeline-performance): isolating the bottleneck, batching, vertex reuse and pass/LOD costs.
- [NVIDIA: OpenGL ES Programming Tips](https://docs.nvidia.com/jetson/archives/r35.6.4/DeveloperGuide/SD/Graphics/GraphicsProgramming/OpenglEsProgrammingTips.html): batching/state grouping, texture arrays, sparse environment-map updates and alpha ordering. General Tegra/OpenGL guidance, not a measured Horizon Mesa result.
- [NVIDIA: Efficient Occlusion Culling](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-29-efficient-occlusion-culling): query stalls and asynchronous scheduling.
- [Khronos: Query Objects](https://wikis.khronos.org/opengl/Query_Object): timer-query semantics and avoiding unavailable-result waits.
- [meshoptimizer: simplification](https://github.com/zeux/meshoptimizer#simplification): projected error, attribute/material boundaries and limitations; candidate only, not integrated.
- [devkitPro: deko3d](https://github.com/devkitPro/deko3d) and [Primer](https://github.com/devkitPro/deko3d/blob/master/Primer.md): Switch-native resource/command model and reusable lists. Counter reports can flush tiled caches; use profiling sparingly.
