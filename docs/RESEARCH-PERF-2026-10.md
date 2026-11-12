# PERFORMANCE RESEARCH — where the frame time actually goes (2026-10-25)

Measured on Cortex (Linux, Ryzen 5 5600X, GTX 1060 6 GB, OpenGL path), a real
Nürburgring race, 1920×1080, 38–76 draws/frame, 93–211 quads/frame.
Method: `MOON_VIDEO_PROFILE=1` for frame cost, `perf record -F 499 -g` for the
CPU profile (12 K samples, IPC counted), both on the same binary.

## 1. HEADLINE

The engine paces itself at **60 Hz** and the pacer **burns a core while it
waits**. 22% of all sampled cycles are inside the vDSO `clock_gettime` called
from the busy-wait in `plat_video_begin_frame` (`video_gl.c:744-757`):

```c
while (now < deadline) {           /* SDL_Delay(0) + clock_gettime, in a loop */
    ...
}
```

On a 60 Hz target that loop spins for most of every frame. It looks like "low
CPU %" in Task Manager because it is spread across cores, but it is real work
stolen from the render thread and it adds latency.

**First lever, cheapest of all:** sleep `remaining - 1 ms` with `SDL_Delay`,
then spin only the last millisecond. Same frame timing, ~20% of the profile
gone.

## 2. THE PROFILE (top symbols, children excluded)

| overhead | symbol | note |
|---------|--------|------|
| 22.00% | vDSO `clock_gettime` | the pacing spin above |
| 10.59% | `render_synth` | our synthetic-render path; the biggest real consumer |
| 8.05% | `0xa0f93c` (driver, no symbols) | Mesa/NVIDIA inside GL calls |
| 2.09% + 1.69% + 0.73% | `__fminf` / `__fmaxf` / `fmaxf@plt` | ~4.5% in min/max, much of it PLT calls |
| 2.06% | `gt4_glyph_sdf_presented` | text is rasterised/presented every frame |
| 1.95% + 0.85% + 0.50% | `ps2f_mul`, `ps2f_do_add`, `ps2f_div` | software PS2 float emulation, ~3.3% |
| 1.49% | `r3d_shadow_map_mesh` | shadow pass |
| 1.22% + 0.44% | `edt_line`, `edt` | distance-field line work |
| 1.15% | `tinfl_decompress` | inflate running during the race — why? |

## 3. LEVERS, RANKED BY MEASURED COST

1. **Fix the pacing spin** (22%). Sleep to `deadline - 1 ms`, spin the rest.
   Zero visual change.
2. **`render_synth`** (10.6%). Needs a read of its loop; if it is CPU raster or
   per-vertex work that the GPU already does, it is duplicated effort.
3. **The `fminf`/`fmaxf` cluster** (~4.5%). Most are called through the PLT
   (`fmaxf@plt`). Inline the scalar min/max, or compile the hot files with
   `-ffast-math`-style intrinsic mapping. Also check for `fminf(fminf(a,b),c)`
   chains that should be a single clamp.
4. **`gt4_glyph_sdf_presented`** (2%). Text redraws every frame. Cache the
   glyph atlas and only re-upload when the string changes.
5. **`tinfl_decompress`** (1.2%). If a texture or record is being inflated per
   frame, hoist it to load time and cache by key.
6. **PS2 float emulation** (~3.3%). Fine for fidelity, but the hot multiply/add
   pair (`ps2f_mul` + `ps2f_do_add`) can be fused behind a feature flag for
   "performance" mode — the maths is the same, only the rounding edge cases
   differ.

## 4. WHAT IS NOT THE PROBLEM (measured)

- **Draw count**: 38–76 draws and 93–211 quads per frame. That is nothing for
  any GPU of the last 15 years. Batching is not the win here.
- **VSync alone**: with `--vsync off` the frame cost stayed at ~16.07 ms and
  ~61 fps — the 60 is our own pacing, not the display.
- **The GPU doing too much**: the driver symbols are 8%, and the GPU-side cost
  measured through `glFinish` sits under the pacing window.

## 6. CORRECTION (measured 2026-10-25, later the same night)

My first read of this was wrong and the post to issue #26 has been corrected.
The engine is **not** locked to a 60 Hz tick. With vsync off *and* the pacer
off, the same race gives:

| window | pixels | ms/frame | fps |
|--------|--------|----------|-----|
| 640×480 | 307 k | 12.3 | 79 |
| 1280×720 | 922 k | 20.4 | 47 |
| 1920×1080 | 2.07 M | 32.5 | 31 |

Draw count is identical (41–43) at every size, so this is **pixel cost, not draw
cost**. The earlier "exactly 60 / exactly 30" reading came from vsync: at 60 Hz
a frame under 16.7 ms shows 60, a frame over it halves to 30. That is a vsync
ladder, not a tick lock.

The number that matters: **12.3 ms at 640×480 with 41 draws.** 307 k pixels
should cost well under a millisecond on a GTX 1060. Fit the three points and the
curve is roughly `8–9 ms fixed + ~11 µs per 1 k pixels`:

- the fixed ~8–9 ms is CPU-side or per-frame driver work (matches the 22%
  `clock_gettime`, 10.6% `render_synth`, and the `glFinish` inside the profiler),
- the per-pixel part is genuinely heavy — two full-resolution blits per frame
  (scene→texture, then output→swapchain) plus the shadow pass and SDF text.

So: not a tick lock, and not "the GPU is too slow" either. It is a heavy
per-frame fixed cost plus an expensive per-pixel path.

## 7. FOR JONHLINE (Vulkan, issue #11)

His "struggled to hold 60 on a GTX" is consistent with the CPU side above, not
with the GPU: the pacing spin plus text redraw plus min/max PLT calls are all
CPU, and Vulkan will inherit every one of them because they are outside the
renderer. Fix the CPU first, then compare backends — otherwise a Vulkan vs GL
comparison measures the pacer, not the API.

Open question worth answering with a build flag: does the game *render* once per
60 Hz tick even when the display and the GPU could go faster? If yes, any
benchmark above 60 fps is impossible by design and the "fps" complaint is really
a frame-time-variance complaint.
