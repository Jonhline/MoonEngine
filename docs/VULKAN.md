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
One frame remains in flight. Queue completion still protects command-pool,
staging-memory and presentation-semaphore reuse. Readback and swapchain
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
The renderer still has one frame in flight and performs a completion wait
before reusing its resources. Physics remains at its existing 60 Hz rate.
With VSync enabled on a 60 Hz display, presentation remains limited to 60 Hz.

Thirteen focused cases passed, including deliberate descriptor, submission,
framebuffer, geometry, pixel and simulation-state failures. The synthetic
frame, paired running-race comparison and framebuffer negative also passed
with the Khronos validation layer enabled, without reported API errors.
The full test suite was not run.
