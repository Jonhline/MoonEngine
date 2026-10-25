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

## 6. THE BIGGER FINDING — THE FRAME RATE IS QUANTISED (measured 2026-10-25)

Same race, same draw count (43 draws / 191 quads at every size), NVIDIA GTX
1060:

| window | ms/frame | fps |
|--------|----------|-----|
| 1280×720 | 16.34 | 60.1 |
| 1920×1080 | 33.09 | 29.8 |
| 2560×1440 | 33.14 | 29.9 |

1080p costs **twice** 720p in reported frame time while submitting **the same
43 draws**, and the result lands exactly on 30.0 — not 41, not 27. The engine
is not reporting a measured frame rate; it is running a **fixed 60 Hz tick and
dropping to every second tick when a frame overruns**. 60 or 30, nothing
between.

That is also the answer to "100 fps sometimes 50": on a 100 Hz display the same
ladder gives 100 and 50. And it explains "GPU 50%, CPU 10%" — the engine is
waiting, not working.

**Consequences for anyone benchmarking this engine:**

1. Any fps number is a step function. Comparing 58 vs 61 is meaningless; it is
   60 or 30.
2. A micro-optimisation that saves 2 ms inside a 16.7 ms frame shows **exactly
   zero** improvement until it crosses the whole tick. Measure ms, never fps.
3. The real fix is architectural: decouple drawing from the 60 Hz tick (draw as
   fast as the display allows, step physics on the tick). Until then nothing can
   exceed 60 and every missed frame costs half.
4. The pacer's busy-wait (`video_gl.c:744-757`) is visible as 22% of samples in
   `perf`; it was tested with a 1 ms sleep headroom and made no difference to
   throughput, because the tick — not the pacer — is the limiter.

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
