# PC graphics backends

Moon Engine draws menus and 3D scenes through **OpenGL** or **Vulkan**.
Select the API in the launcher's Video page, under **Graphics backend**.
Vulkan is marked experimental. OpenGL remains the default. A Vulkan
initialization failure reports an error rather than silently selecting OpenGL.

The choice persists as `renderer=opengl` or `renderer=vulkan` in `video.ini`.
Older files retain OpenGL, and in-game display settings preserve the key.
Explicit invocations can override it:

```powershell
.\build\Release\opengt4.exe "C:/discs/GT4.iso" --renderer vulkan
.\build\Release\opengt4.exe "C:/discs/GT4.iso" --renderer opengl
```

Unknown command-line names are rejected. For rendering tools without that
argument, `OPENGT4_GRAPHICS_API=opengl` or `vulkan` overrides the descriptor
at the platform boundary, taking precedence over its backend field.
`OPENGT4_WINDOW_HIDDEN=1` supports offscreen tests. Switch keeps its renderer.

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
.\build\Release\opengt4.exe --vulkan-check
```

It verifies device creation and a submitted transfer through 1,024 readback
words. It does not check image output.

## Verification

```powershell
cmake --build build --config Release --target vulkan_device_tests vulkan_frame_tests renderer_backend_tests launcher_tests ui_blend_probe showroom_probe opengt4
ctest --test-dir build -C Release -R "^(vulkan_|renderer_|launcher_config)" --output-on-failure
```

- Device and missing-loader cases check initialization and transfer readback.
- `vulkan_frame` compares a synthetic frame against OpenGL within two channel
  levels. It covers texture/capture, blend/depth/write-mask state, clipping,
  matrix/array uniforms, winding, partial depth clears, offscreen float depth,
  resize/presentation and flat-color FSR output. Its negative corrupts a pixel.
- `vulkan_blend` runs the existing viewport/blend probe through Vulkan,
  including all 129 fixed blend coefficients; its negative changes the result.
- `vulkan_disc`, registered with `OPENGT4_TEST_DISC`, compares a Ka in Tsukuba,
  the showroom entrance and Options through both APIs. Restarting the entrance
  selects the same studio light-animation time. Limits: mean RGB difference
  at most 1/255; at most 0.1% of channels differing by more than 32 levels.
  Its negative inverts the comparison image.
- `vulkan_race` renders 60 Tsukuba frames with GPU and CPU reflection
  composition. The GPU path must perform zero readbacks and exactly two
  submissions per frame (render and presentation). Pixels must differ by no
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
has been exercised. There is no performance improvement claim.

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
`OPENGT4_REFLECTION_CPU=1` before startup selects that reference for Vulkan.
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
texture bindings. `OPENGT4_VK_STREAM_GEOMETRY=1` retains the previous streamed
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
