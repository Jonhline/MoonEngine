# PC graphics backends

Moon Engine draws menus and 3D scenes through **OpenGL** or **Vulkan**.
Select the API in the launcher's Video page, under **Graphics backend**.
Vulkan is marked experimental. OpenGL remains the default. A Vulkan
initialization failure reports an error rather than silently selecting OpenGL.

The choice persists as `renderer=opengl` or `renderer=vulkan` in `video.ini`.
Older files retain OpenGL, and in-game display settings preserve the key.
Explicit invocations can override it:

```powershell
.\build\Release\MoonEngine.exe "C:/discs/GT4.iso" --renderer vulkan
.\build\Release\MoonEngine.exe "C:/discs/GT4.iso" --renderer opengl
```

Unknown command-line names are rejected. For rendering tools without that
argument, `MOON_GRAPHICS_API=opengl` or `vulkan` overrides the descriptor
at the platform boundary, taking precedence over its backend field.
`MOON_WINDOW_HIDDEN=1` supports offscreen tests. Switch keeps its renderer.

## Implementation

- `vk_context.cpp` owns the Vulkan device, graphics/present queue, SDL surface,
  swapchain, images, transfers, readback and synchronization. Resize, changed
  presentation mode and out-of-date surfaces recreate the swapchain. Minimized
  windows defer presentation.
- `vk_pipeline.cpp` compiles the shared renderer shaders to SPIR-V with
  glslang. Both stages share an explicit uniform layout, including arrays
  and transposed matrices. Pipelines are cached by shader and draw state.
- `vk_commands.cpp` implements the existing renderer command vocabulary
  directly with Vulkan resources and commands. `gl.c` loads either these
  entry points or the OpenGL driver entry points at the graphics boundary.

The Vulkan path creates **no OpenGL context**, which the rendering test checks.
Sharing the frontend retains 2D batching, filtering, clipping, captures, model
and course submission, destination-alpha copies, shadow targets and FSR passes.
Depth-only float and depth/stencil attachments map to Vulkan depth formats.
Alpha-only clears preserve RGB, and depth clears respect clipping. Internal
rows retain the frontend convention and are flipped for presentation.

The initial resource model uses one frame in flight, a mapped staging arena
and fence waits before reuse. Compatible draws share a render pass; transfers
and target changes end it. Deleted/replaced images are retained until the
submission fence completes. Interleaved attributes share both an upload and
a staging reservation per draw.
This bounds resource ownership; it is not a claim that Vulkan is faster.

## Building

The PC build fetches official Vulkan-Headers v1.3.296, volk
vulkan-sdk-1.3.296.0 and glslang 14.3.0. No Vulkan SDK installation is required.
The system loader and a compatible graphics driver are needed at runtime.
Dependency notices are in `third_party/vulkan` and accompany binary packages.
These dependencies are not built for Switch.

A separate diagnostic needs neither a disc nor a window:

```powershell
.\build\Release\MoonEngine.exe --vulkan-check
```

It verifies device creation and a submitted transfer through 1,024 readback
words. It does not check image output.

## Verification

```powershell
cmake --build build --config Release --target vulkan_device_tests vulkan_frame_tests renderer_backend_tests launcher_tests ui_blend_probe showroom_probe moonengine
ctest --test-dir build -C Release -R "^(vulkan_|renderer_|launcher_config)" --output-on-failure
```

- Device and missing-loader cases check initialization and transfer readback.
- `vulkan_frame` compares a synthetic frame against OpenGL within two channel
  levels. It covers texture/capture, blend/depth/write-mask state, clipping,
  matrix/array uniforms, winding, partial depth clears, offscreen float depth,
  resize/presentation and flat-color FSR output. Its negative corrupts a pixel.
- `vulkan_blend` runs the existing viewport/blend probe through Vulkan,
  including all 129 fixed blend coefficients; its negative changes the result.
- `vulkan_disc`, registered with `MOON_TEST_DISC`, compares a Ka in Tsukuba,
  the showroom entrance and Options through both APIs. Restarting the entrance
  selects the same studio light-animation time. Limits: mean RGB difference
  at most 1/255; at most 0.1% of channels differing by more than 32 levels.
  Its negative inverts the comparison image.
- `vulkan_race` renders 60 Tsukuba frames with GPU and CPU reflection
  composition. The GPU path must perform zero readbacks and one
  submission per steady-state frame (render and presentation together). Pixels must differ by no
  more than 2/255; the measured maximum was 1/255. Its negative restores CPU
  reflection readback and must fail the residency check. The synthetic frame
  also checks that 128 compatible draws share a pass without submission and
  that deleting a sampled image does not force a wait.
- Selection, startup and configuration tests check names and persistence.

On a GTX 1650 the disc comparison recorded mean RGB differences of
0.020918/255 (Tsukuba), 0.103857/255 (showroom), and 0 (Options). Showroom and
Tsukuba draws also completed under the Khronos validation layer without
reported API errors. Captures and diagnostic binaries stay local. These
are host-backend comparisons, not full-game PS2 certification. Twenty focused
cases passed, including corruption negatives and existing OpenGL regressions;
the full suite was not run. This addition
has grade **P**; existing renderer fidelity limits still apply.

Current limitations: single-sample scene attachments, no anisotropic filtering,
no adapter picker, no HDR or device-loss recovery, and no Linux runtime
validation yet. Not every menu, car, circuit, replay or Photo mode combination
has been exercised. Measured performance changes are scoped to the fixtures below.

API references: [Vulkan specification](https://docs.vulkan.org/spec/latest/index.html),
[official headers](https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.3.296),
and [SDL surface creation](https://wiki.libsdl.org/SDL2/SDL_Vulkan_CreateSurface).

## Race rendering performance correction

The initial Vulkan implementation opened and synchronized a render pass for
individual draws and waited on image deletion/replacement. Race reflections
also used the PC CPU composition path: six synchronous face readbacks per
frame, followed by CPU sphere-map composition and texture upload.

Vulkan now reuses the existing GPU reflection helper, keeping all six faces
and the sphere map resident. The helper's name (`switch_render.c`) reflects
its earlier platform use; this change enables it on PC without changing the
Switch renderer. OpenGL retains CPU composition as a reference. Setting
`MOON_REFLECTION_CPU=1` before startup selects that reference for Vulkan.
No reflection faces, draw distance or physics updates are removed.

In the hidden Ka/Tsukuba rendering probe at 640x448 on the GTX 1650, 120
frames without VSync measured 46.165 ms/frame before these changes and
11.804 ms/frame after them. This measures course/car rendering and
presentation, excluding the gameplay simulation, and is not a guarantee
of a particular frame rate in every race or resolution. The full-frame
comparison against CPU reflection composition differed by at most one
channel level. This optimization remains grade **P**.
Eight focused Vulkan cases passed, including deliberate image/residency
failures. The synthetic-frame and race comparisons and their negatives also
passed with the Khronos validation layer enabled, without reported API errors.

## Resident static geometry and running-race measurement

Static vertex allocations now have device-local Vulkan buffers, uploaded
once at first use. Initial `BufferSubData` assembly remains static; subsequent
edits invalidate the resident version and use streaming. Replaced/deleted
buffers survive until the fence completes. Interleaved attributes share a
resident-buffer lookup, and descriptor writes cover only the program's
texture bindings. `MOON_VK_STREAM_GEOMETRY=1` retains the previous streamed
path as a comparison reference. This changes graphics resource management,
not meshes, visibility, animation, game rules or physics.

`vulkan_simulation` runs the actual race update/draw path: Tsukuba, six cars,
player acceleration, audio updates and HUD, at 1280x896. It advances 600
simulation ticks, warms 20 displayed frames, then measures 120 frames at a
fixed simulation step with VSync and menu pacing disabled. Loading is excluded.
It compares the final 32 MiB physics state byte-for-byte and final RGB pixels
exactly between resident and streamed paths. The negative flips a state bit.
No game state or captures are distributed.

The final paired run on the GTX 1650 measured:

| Geometry path | Update | Draw submission | Present/wait | Total | Vertex bytes uploaded over 120 frames |
|---|---:|---:|---:|---:|---:|
| Resident | 0.137 ms | 6.576 ms | 5.456 ms | 12.171 ms | 3,427,332 |
| Streamed reference | 0.170 ms | 7.803 ms | 6.164 ms | 14.138 ms | 756,293,364 |

These are wall-clock averages from the running-race fixture, not isolated
GPU timestamp measurements or a guarantee for other circuits/resolutions.
An earlier run before this optimization measured 15.350 ms/frame. Frame times
vary between runs; the automated gate checks upload reduction, unchanged
simulation memory and pixels rather than a machine-specific FPS threshold.

The synthetic fixture additionally checks static allocation assembled by
subranges, nonzero vertex/attribute offsets, 128 draws without another upload,
and editing/deleting a buffer while its preceding draw is pending. Disabling
residency deliberately fails `vulkan_geometry_negative`. This optimization
has grade **P**; byte equality here proves invariance between PC resource
paths, not additional PS2 fidelity coverage.
Eleven focused Vulkan cases passed, including the deliberate streaming,
image and simulation-memory negatives. The geometry and running-race
comparisons also passed with Khronos validation enabled, without API errors.

## Submission and descriptor reuse

Rendering and presentation now share a command buffer and submission during
steady-state drawing. A barrier orders the screen image writes before its
presentation blit; the acquire semaphore is waited at the transfer stage.
The original single-frame mode waits before resource reuse. The configurable
two-frame path described below now retains separate resources for each slot. Readback and swapchain
recreation retain explicit completion. Descriptor pools, cached sets and
retired images/buffers are reset only after the submission completes.
`MOON_VK_SPLIT_PRESENT=1` retains the former separate render/present submissions.

Uniform data still has its own aligned slice for every draw. Dynamic uniform
offsets allow draws with the same texture views, samplers and uniform range
to reuse an immutable descriptor set until the submission pool resets.
Changing texture/sampler state selects a different set; updating a texture's
contents preserves its view. `MOON_VK_UNCACHED_DESCRIPTORS=1` retains one
allocation/update per draw for comparison. The fixed-size pipeline key and
attribute bookkeeping also avoid per-draw temporary heap containers, and
one vertex-buffer bind describes all used attribute slots.

`vulkan_submission` compares both resource paths in the actual six-car
Tsukuba race at 1280x896 and 1920x1080, after loading and warm-up, with VSync
disabled. It requires identical draw counts, final RGB pixels and the full
32 MiB physics state. The optimized path uses 120 submissions over 120
measured frames; the reference uses 240. Descriptor update and framebuffer creation counts must fall.
The negative restores split submission and must fail that gate. The
synthetic frame checks descriptor reuse over 128 compatible draws; its
negative disables caching. These are PC invariance/performance checks (P),
not new PS2 fidelity claims or a guaranteed FPS on every course/device.

Framebuffer objects are cached by their immutable attachment views instead
of recreated at each render-pass boundary. An image's deferred destruction
removes its cached framebuffers before destroying its views. Resizing and
texture replacement therefore cannot retain dangling attachments. Setting
`MOON_VK_TRANSIENT_TARGETS=1` restores per-pass framebuffer creation;
`vulkan_targets_negative` enables it and must fail the creation-count check.

On the GTX 1650, three alternating runs of the pre-change executable and
optimized executable at 1280x896 gave median frame times of 12.871 and
12.341 ms (about 77.7 and 81.0 FPS, a 4.1% frame-time reduction). Each run
measured 120 frames after 20 warm-up frames; all three pairs retained exactly
the same final pixels and physics state. These timings exclude loading and
use VSync off. An earlier single baseline measurement was 13.848 ms; the
paired medians are the comparison used here.

At 1920x1080, the six-car fixture measured 14.204 ms/frame (about 70.4 FPS).
This is a scenario measurement, not a guaranteed minimum or a pre-change
1080p executable comparison. The resource-reference run measured 14.588 ms.
Over 120 frames the optimized path performed 30,134 descriptor updates and
240 framebuffer creations; the retained reference performed 119,245 and
12,393, with the same 119,245 draw calls in both. Timing varies with system
load, so automated gates use resource counts and image/state invariance.
These earlier measurements used one frame in flight; the configurable
queue described below supersedes that restriction. Physics remains at its existing 60 Hz rate.
With VSync enabled on a 60 Hz display, presentation remains limited to 60 Hz.

Thirteen focused cases passed, including deliberate descriptor, submission,
framebuffer, geometry, pixel and simulation-state failures. The synthetic
frame, paired running-race comparison and framebuffer negative also passed
with the Khronos validation layer enabled, without reported API errors.
The full test suite was not run.


## Frame queue, preparation and frame pacing

The Video page now offers **Vulkan frame queue** (Balanced, two frames, or
Low latency, one frame) and **Frame limit** (Unlimited, 60, 90, 120, 144,
165 or 240 FPS). The defaults are Balanced and Unlimited. VSync remains a
separate limit; a 60 Hz display with VSync cannot present above 60 Hz.
These are PC presentation choices (grade P); simulation still advances
at its existing fixed rate. Menus retain their 60 Hz pacing.

`vk_frames.cpp` owns two independent command pools, command buffers, fences,
acquire semaphores, descriptor pools, uniform/upload arenas and timestamp
query pools. Balanced rotates slots after presenting and waits only before
reusing a slot. Low latency waits for the just-submitted work. Captures,
resource exhaustion, swapchain recreation and explicit drains still wait
when needed. Presentation wait semaphores belong to swapchain images,
following [Khronos's semaphore reuse guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).
There is no per-frame queue-idle call. Each arena reserves 128 MiB, so the
second slot increases staging capacity and memory use by 128 MiB.

The race inspection probe reports CPU wall-clock update/render/present
intervals separately from GPU timestamp duration, fence-wait duration and
frame median/p95/p99/worst. CPU intervals can include driver waits and must
not be interpreted as isolated CPU execution or added to GPU time. GPU
queries are collected only after their slot fence completes; devices with
no queue timestamp support leave the GPU sample count at zero. Timings
exclude loading, drain the warm-up before measurement, and include tail
completion in the average.

`vk_cache.cpp` persists the driver's pipeline cache beneath the application's
preference directory, in `vulkan/`, keyed by vendor/device/driver. It checks
header size/version, device IDs and pipeline UUID before passing cached
bytes to Vulkan. Invalid or rejected data falls back to an empty cache;
it never modifies save data. `MOON_VK_CACHE_DIR` isolates test caches and
`MOON_VK_NO_PIPELINE_CACHE=1` disables caching. This follows the
[Khronos pipeline cache guidance](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_cache/README.html).

On Vulkan startup, the existing render warm-up exercises the main 3D,
wheel, sprite, shadow, layer and image-filter paths behind loading. The
480-line menu view is restored after the loader's 448-line view. Race asset
installation uploads the loaded static vertex buffers before returning
ready; texture installation already performs its uploads there. Shader
variants are still created on demand when a previously unseen draw state
is encountered; this is not a promise that every first-use stall is gone.
`MOON_VK_SKIP_PREPARE=1` retains lazy preparation for comparisons.

The limiter uses absolute rational deadlines, sleeps before drawing and
yields near the deadline. Changing the cap or resuming after a long stall
reanchors it; it does not accumulate a burst of catch-up frames. It does
not change game timers, physics or replay input. Both new settings persist
in `video.ini` as `low_latency` and `fps_limit` and survive applying the
original in-game video settings.

### Focused verification

- `frame_pacing` checks one hour of 144 Hz deadlines, cap changes, unlimited
  mode and stall recovery. Its negative injects accumulated rounding drift.
- `vulkan_frame` exercises sixteen textured frames, both queue modes,
  resizing and texture deletion with pending work, then compares against
  OpenGL. Existing pixel/resource negatives remain active.
- `vulkan_cache` compares cold, warm and deliberately corrupt cache loads
  with exact output bytes. Its negative disables the warm cache.
- `video_performance` applies and reopens both launcher choices, checks
  persistence, and requires exact Options pixels with preparation on/off.
  Its negative corrupts the persisted latency setting.
- `vulkan_queue` compares 1920x1080 running Tsukuba grid, six-Ford-GT Hong
  Kong, Nürburgring, wet Tsukuba and an actual recorded-race replay across
  both queue modes. The serial reference also disables geometry preparation.
  Every pair requires exact final RGB and all 32 MiB of simulation memory;
  its negative forces the wrong queue mode. `MOON_VK_QUEUE_CASE=replay`
  (or `grid`, `city`, `long-course`, `wet`) isolates a scenario for instrumented
  reruns; the default covers all five, with a 900-second test deadline.
  These are PC invariance checks, not comparisons against PS2 rendering.

On a GTX 1650, the first five-scenario run measured the following averages
with VSync off, twenty warm-up frames and 120 measured frames. Both modes
used eager geometry preparation in this measurement. GPU times are for
Balanced; CPU wall-clock averages include rendering and presentation.

| Scenario, 1920x1080 | Balanced | Low latency | Balanced GPU |
| --- | ---: | ---: | ---: |
| Tsukuba grid | 12.380 ms | 12.150 ms | 3.723 ms |
| Hong Kong, six Ford GTs | 10.079 ms | 9.801 ms | 3.567 ms |
| Nürburgring | 10.383 ms | 10.749 ms | 3.373 ms |
| Wet Tsukuba | 10.682 ms | 10.826 ms | 3.823 ms |
| Tsukuba replay | 12.301 ms | 12.502 ms | 3.701 ms |

There is no consistent queue-mode speed advantage on this driver. These
averages exceed 60 FPS in the measured scenarios, but occasional frames
still exceed 16.67 ms (Nürburgring's worst Balanced frame was 29.137 ms).
They do not guarantee a minimum FPS, all-car/course coverage or flawless
frame pacing on another machine. Both modes remain selectable.


Twenty-seven focused cases passed, including deliberate failures, OpenGL
regressions and launcher persistence/painting. Khronos synchronization
validation reported no API errors in the synthetic resize/queue fixture
and the five race/replay scenario pairs. The instrumented combined matrix
hit its former 600-second deadline during the last replay; the isolated
replay rerun completed successfully in 143.59 seconds. The test deadline is
now 900 seconds, with an optional scenario filter for targeted reruns. These
instrumented durations are not gameplay benchmarks. No full suite was run.


## Command recording and known pipeline variants

The Vulkan boundary now omits redundant pipeline, viewport, scissor,
blend-constant and vertex-buffer commands. All pipelines built here declare
the three dynamic states; their values survive render-pass changes and are
invalidated at every command-buffer begin. Descriptor sets still bind for
every draw because each uniform block has its own dynamic offset. No draw,
material, alpha-fail pass, destination-alpha snapshot or primitive is removed.

Static vertex bindings retain their base offsets and use the draw's first
vertex instead of rebinding a shifted range for each material. Procedural
`gl_VertexID` programs retain the previous indexing path. Disabled attributes
share the same immutable `(0,0,0,1)` value inside each command arena; resetting
the arena invalidates that offset. This also permits resident bindings to
remain unchanged between material groups. Mixed/streamed buffers retain the
subrange-upload path. `MOON_VK_REPEAT_COMMANDS=1` restores repeated commands,
per-draw defaults and shifted static bindings for comparison.

The Vulkan 3D path also reuses the existing mesh-local texture/material
state cache. It starts empty for each mesh, restores state after alpha-fail
passes and retains the separate ghost handling. Opaque-group merging remains
outside this PC change. OpenGL keeps its previous material path; the Switch
branch is unchanged. These implementation changes preserve PC output; their
record is P, not additional PS2 fidelity certification.

The driver's pipeline cache is now accompanied by a `.variants` file. It
contains shader fingerprints, attachment formats, topology, vertex formats
and raster/blend/depth descriptions, with no resource handles, vertex data,
textures or game assets. A bounded versioned header, exact file size and
payload checksum reject damaged metadata. Known descriptions are compiled
when the matching shader program links, using a temporary vertex description;
the frontend's state is restored and no draw or simulation update is issued.
The driver/device cache filename also scopes this metadata. Up to 4,096
unique descriptions are retained; later unknown states still compile on use.
`MOON_VK_SKIP_VARIANTS=1` disables this preparation and metadata persistence.
Programs that themselves link late can still incur their work on first use.
Deleting a program drains both submission slots before destroying pipelines.

`MOON_VK_PROFILE_DRAWS=1` enables optional CPU timing for vertex/uniform
preparation, descriptors, pipeline/pass lookup and command emission. Normal
rendering does not take those per-draw clock samples. The race probe also
reports actual command counts, pipeline creation time/count after warm-up,
and the number of measured frames above 16.667 ms. `PROBE_FRAMES` supports
60-3,600 measured frames; `PROBE_WARMUP` selects 0-120 warm-up frames.

### Longer comparison and limits

`vulkan_commands` runs 1,200 measured frames for six-car Tsukuba, wet Tsukuba
and an actual recorded-race replay at 1920x1080 with VSync off. The driving
cases cycle chase/onboard/roof views every 240 steps; the replay retains its
TV camera sequence. Each pair requires the same draw count, exact RGB pixels
and all 32 MiB of simulation memory, with strictly fewer pipeline, dynamic
and vertex bind commands. The negative forces repeated commands and must
fail the reduction gate. `MOON_COMMAND_CASE` isolates `race`, `wet` or
`replay`, and `MOON_COMMAND_FRAMES` shortens an instrumented rerun.
Each default run covers twenty seconds of simulated time, not an entire race.

One GTX 1650 comparison measured:

| Scenario | Cached average | Repeated average | Cached p95 | Repeated p95 |
| --- | ---: | ---: | ---: | ---: |
| Tsukuba, changing views | 14.675 ms | 15.012 ms | 21.147 ms | 21.653 ms |
| Wet Tsukuba, changing views | 12.900 ms | 13.739 ms | 19.490 ms | 20.612 ms |
| Replay | 13.043 ms | 13.297 ms | 19.571 ms | 20.166 ms |

These are individual scenario measurements, not a statistically established
speedup across systems. Cache learning and driver/system load vary. The
longer view changes expose frames above 16.67 ms despite averages above 60
FPS; the Tsukuba cached run's worst frame was 28.235 ms. This work does not
eliminate all stutters or guarantee a minimum FPS.

For Tsukuba's 1,445,862 draws, pipeline binds fell from 1,445,862 to 330,865,
dynamic-state commands from 4,337,586 to 56,570, and vertex binds from
1,400,742 to 68,075. The render CPU wall interval fell from 6.022 to
5.697 ms/frame in that pair. GPU work and presentation remain substantial.
The reference followed the cached run and could use its learned variants;
the comparison is gated on command counts and unchanged output, not FPS.

`vulkan_variants` independently compares cold, warm and checksum-corrupted
metadata using identical synthetic output bytes. Warm shader linking must
prepare recorded pipelines; its negative disables preparation. The existing
cold/warm/corrupt driver-cache tests remain separate.


Twenty-three focused positive/negative cases passed for this change. Khronos
synchronization validation also passed the synthetic frame/resize fixture,
variant cold/warm/corrupt comparison and a paired 60-frame wet-course command
comparison. The three full 1,200-frame pairs ran without instrumentation;
there is no claim of full-race or every-camera coverage. No full suite ran.

## Shared OpenGL and Vulkan submission

Both PC backends now reuse mesh-local material state and merge consecutive
visible single-pass opaque ranges. Alpha KEEP is eligible because it has
no fail-write pass; multipass alpha-fail, DATE, blending, skin and ghosts
retain separate draws. See [shared submission and its tests](RENDERER.md#shared-pc-material-submission).

`MOON_RENDER_REFERENCE=1` disables these shared optimizations in both APIs.
This differs from `MOON_VK_REPEAT_COMMANDS`, which still compares Vulkan
command emission while retaining the same batching choice in each branch.
Work counters cover nested views in both APIs; Vulkan-only timer and command
counters are explicitly unavailable on OpenGL.
