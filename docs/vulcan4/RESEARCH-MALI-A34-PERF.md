# RESEARCH — OPTIMISING FOR THE A34 / MALI IN GENERAL

Captain's ask (2026-10-19): *"maybe research some ways u can try to optimize the a34. Like u know. Mediatek
chips in general."*

The A34 is **MediaTek Dimensity 1080 (MT6877V, 6 nm), GPU Mali-G68 MC4 at ~950 MHz** — Arm **Valhall**,
4 shader cores, tile-based (TBDR). MediaTek is the vendor; **the rules that matter are Arm's**, because the
GPU is Arm. Everything below is from Arm's GPU Best Practices Developer Guide / Vulkanised talks, or Android's
own docs, and each lever is tied to a line in THIS tree.

---

## The one finding that explains our own measurement

**Arm explicitly says: do NOT use `vkCmdBlitImage` to upscale a low-resolution frame to native resolution** —
*"Especially if you render the UI or HUD directly on top of the frame with `loadOp = LOAD_OP_LOAD`, as this is
an unnecessary round trip to memory."*

That is exactly what we do:
- the frame is rendered offscreen, then **blitted at full extent** into the swapchain
  (`src/platform/sdl/vk_context.cpp:638`), and
- the swapchain is `TRANSFER_DST` only (`:553`), so the UI cannot be drawn into it in the same pass.

This is also the honest answer to *"why did 50% render scale change nothing?"*: the upscale path we rely on is
the one Arm tells you to avoid — a full-resolution round trip that the smaller 3D target does not remove.
**Replacing the blit with a textured full-screen quad in the same render pass as the UI is the single biggest
structural fix available to us.**

---

## Ranked levers (win / effort), all with our own line numbers

### 1. Load/store operations — Arm measured up to 36 % less external read, 62 % less external write
- `LOAD_OP_CLEAR` or `LOAD_OP_DONT_CARE` instead of `LOAD_OP_LOAD`; `STORE_OP_DONT_CARE` for the depth buffer
  (never needed after the pass); `STORE_OP_NONE` for read-only attachments.
- `vkCmdClearAttachments` is **not** free — never use it inside a pass; the clear must be the loadOp.
- WHERE: the render pass we build in `src/platform/sdl/vk_pipeline.cpp` (attachment descriptions) and the
  clear path in `vk_context.cpp`.
- COST: small, contained. **This is the next dish after layouts.**

### 2. Transient depth — zero storage, zero bandwidth
- A depth buffer that lives only inside one pass should be `TRANSIENT_ATTACHMENT_BIT` backed by
  `LAZILY_ALLOCATED` memory: no allocation, no load, no store.
- WHERE: depth image creation in `vk_context.cpp` (`info.usage` at `:159-161` today includes
  `DEPTH_STENCIL_ATTACHMENT` but no transient flag).
- COST: small. Note AFBC/transaction-elimination **requires** the colour image NOT to be transient — keep the
  two cases separate.

### 3. One render pass instead of two — "3D pass, then UI pass" costs a full 1:1 read
- Arm: *"3D Render Pass + UI Render Pass → 1:1 Read"*; append the UI to the existing pass whenever possible.
- WHERE: the split between `video_end_scene()` (blit + UI) and the 3D pass; see the present path in
  `vk_context.cpp:599-650`.
- COST: medium — this is the same work as lever 4 and they should be done together.

### 4. Delete the upscale blit (see the finding above)
- Draw the offscreen frame as a **textured full-screen quad** in the UI pass (a sampler read of an image that
  was just a colour attachment is the AFBC-friendly path), or render straight into the swapchain when the
  resolution matches.
- WHERE: `vk_context.cpp:638` (`vkCmdBlitImage`), `:553` (swapchain usage), the 2D path in `video_gl.c`.
- COST: medium. Unlocks the render-scale win we already shipped but never collected.

### 5. Barriers — stop using TOP_OF_PIPE / BOTTOM_OF_PIPE
- Arm: avoid `BOTTOM → TOP` dependencies; they serialise a TBDR pipeline that otherwise runs vertex and
  fragment work in parallel.
- WHERE: `vk_context.cpp:627-637` — the present barrier goes `TOP_OF_PIPE → TRANSFER` and then
  `TRANSFER → BOTTOM_OF_PIPE`. Replace with the real stages (`COLOR_ATTACHMENT_OUTPUT`, `TRANSFER`,
  `ALL_COMMANDS` where genuinely needed) and real access masks.
- COST: small, but it must be done carefully — this is where a wrong mask shows up as a corrupted frame.

### 6. Blending and overdraw — "do not keep blending always on"
- Split large UI elements into **opaque + transparent** parts so early-ZS / FPK can kill the overdraw beneath
  the opaque parts; monitor the blended layer count per pixel.
- WHERE: the 2D path (`video_gl.c`, the batch that sets `glBlendFuncSeparate` unconditionally at
  `video_flush2d`), and the launcher/HUD layers.
- COST: medium — needs one person measuring overdraw (Arm Mobile Studio or a simple counter), not guessing.

### 7. AFBC + transaction elimination (the layouts dish already fired)
- Safe layouts (`COLOR_ATTACHMENT_OPTIMAL`, `SHADER_READ_ONLY_OPTIMAL`, `TRANSFER_SRC_OPTIMAL`,
  `PRESENT_SRC_KHR`), `SAMPLE_COUNT_1`, `TILING_OPTIMAL`, no `STORAGE`, no `ALIAS`, no `MUTABLE_FORMAT`,
  and **no `TRANSFER_DST` on the swapchain** on some drivers (the Vulkan guide calls this out explicitly).
- COST: in flight (commit pending on the chef's diff).

### 8. Android-level: ADPF (Dynamic Performance Framework)
- ADPF gives the app **thermal headroom** and lets it report GPU work, so the platform can raise clocks
  before a stutter instead of after; Android exposes a sustained-performance mode as well.
- WHERE: new JNI in `android_host.c` / `GameActivity`, no engine changes.
- COST: medium (Java/NDK glue), and it only pays on long sessions — thermal throttling is real on an
  Exynos-class mid-range package under a sustained 3D load.

### 9. Panel/refresh-rate honesty
- The A34 panel is **120 Hz**; our swapchain is FIFO (vsync) and the engine paces for 60. That is fine, but a
  FIFO swapchain on a 120 Hz panel can show uneven cadence; MAILBOX (or an explicit 60 Hz presentation hint)
  is worth measuring once the frame cost is sane.
- COST: a one-line experiment, but DO NOT touch it before levers 1-5 — pacing a slow frame hides nothing.

---

## What NOT to do (Arm says so explicitly)

- Do not clear with a shader or `vkCmdClearAttachments`.
- Do not `LOAD_OP_LOAD` unless the algorithm needs the previous contents.
- Do not upscale with a blit.
- Do not blend on floating-point framebuffers.
- Do not assume push constants beat uniform buffers on Arm — they do not.
- Do not chase MSAA on a tile-based GPU: 1 sample + AFBC beats 4 samples every time.

## Suggested order for us

1. **Layouts** — already cooking (chef's diff pending a commit).
2. **Load/store + transient depth** — the cheapest measured win (36 %/62 % in Arm's own numbers).
3. **Kill the blit, merge the passes** — the structural fix, and the one that finally makes the render-scale
   knob pay.
4. **Barriers** — correctness-sensitive polish.
5. **Overdraw audit, then blending work** — needs a measurement, not an opinion.
6. **ADPF + present mode** — last, because it only matters once the frame is not 190 ms.

## The honest limit

Every lever above is a *bandwidth/overdraw* lever. None of them makes a 190 ms frame into 16 ms by itself if
something pathological is happening per draw call — that is what the (currently crashing) micro-benchmark was
for, and it stays parked until the phone can be plugged into Cortex. These levers are still correct, still
cheap, and they remove known, documented waste on this GPU family.

---

# ADDENDUM — measured against our own code (2026-10-19, second pass)

The first pass was Arm's rules. This pass is what OUR renderer actually does, read line by line, because the
difference decides what a safe fix even looks like.

## 1. Our render pass is the worst case Arm describes

`src/platform/sdl/vk_pipeline.cpp` (`Context::render_pass`, ~line 224-250):

```c
attachments[i].loadOp          = VK_ATTACHMENT_LOAD_OP_LOAD;      /* colour AND depth */
attachments[i].storeOp         = VK_ATTACHMENT_STORE_OP_STORE;    /* colour AND depth */
attachments[i].stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_LOAD;
attachments[i].stencilStoreOp  = VK_ATTACHMENT_STORE_OP_STORE;
attachments[i].initialLayout = attachments[i].finalLayout = ...OPTIMAL;   /* after the layouts dish */
```

So we pay, every pass: a **full-resolution read** of the colour target, a **full-resolution read** of the depth
target, and a **full-resolution write** of both. Arm's guide names `LOAD_OP_LOAD` as the thing to avoid and
measures up to **36 % less external read / 62 % less external write** when it is replaced by
`LOAD_OP_CLEAR`/`DONT_CARE` and `STORE_OP_DONT_CARE`.

**At 2267x1080**: colour RGBA8 = 9.8 MB, depth24/stencil8 = 9.8 MB. One pass therefore reads ~19.6 MB and
writes ~19.6 MB before any real drawing happens. That is the size of the prize.

## 2. Depth is a full-size, non-transient image

`vk_context.cpp` (~line 159): depth images are created with
`TRANSFER_SRC | TRANSFER_DST | DEPTH_STENCIL_ATTACHMENT`, `TILING_OPTIMAL`, `SAMPLE_COUNT_1` — **no
`TRANSIENT_ATTACHMENT_BIT`, no `LAZILY_ALLOCATED` memory**. So the depth buffer has real storage and is loaded
and stored every pass. Making it transient is a small, contained change - *if* nothing reads it back.

## 3. Vulkan 1.0 only - this constrains the fix

`vk_context.cpp:848`: `app.apiVersion = VK_API_VERSION_1_0`, and the only device extension enabled is
`VK_KHR_swapchain` (`:895-901`).

**Consequence: `VK_STORE_OP_NONE` is NOT available to us** (it is Vulkan 1.3, or `VK_EXT_load_store_op_none`).
A recipe that tells the chef to use it will fail or be ignored. The safe vocabulary here is exactly:
`LOAD_OP_CLEAR`, `LOAD_OP_DONT_CARE`, `STORE_OP_STORE`, `STORE_OP_DONT_CARE`.

## 4. The engine can begin the scene target MORE THAN ONCE PER FRAME

`video_begin_scene()` / `video_end_scene()` are per *layer*, and `main.c` tracks a `scene_layer` and closes it
later in the same frame. So:

- `LOAD_OP_DONT_CARE` on the **first** begin of a frame is correct (nothing to preserve - the pass is about to
  cover it).
- `LOAD_OP_DONT_CARE` on a **later** begin would erase the earlier layers. The later begins must keep
  `LOAD_OP_LOAD`.
- Depth is the same, and it must be **stored** between layers if a later layer needs its depth. Only the last
  layer can store `DONT_CARE`, and only if nothing reads depth afterwards (shadows/readbacks would).
- So the fix needs a **per-frame "target already has content" flag**, not a global constant flip. That is the
  one piece of state this dish must add.

## 5. The blit structure, precisely

`video_gl.c:1078 video_end_scene()`:
1. flush 2D → 2. blit `scene_fbo` → `scene_tex` (+ FSR resolve when upscaling) → 3. `glBlitFramebuffer` into
   the output `fbo`, upscaling to `output_w x output_h` → 4. the 2D/UI is then drawn into that same output
   `fbo` → 5. the present path blits the output into the swapchain (`vk_context.cpp:638`).

So a normal frame contains **two full-resolution blits** plus a full-resolution 3D target. That is why "kill the
blit and merge the passes" is a separate, bigger dish: it changes the frame's shape, not just its flags.

## 6. What this means for the recipe

- **Safe and big (this dish):** load/store flags + per-frame content tracking + transient depth where nothing
  reads it back. It touches flags and one boolean; it does not change what is drawn.
  **The recipe that implements exactly this is `/home/or/.hermes/recipes/moon-mali-loadstore.txt`** (written
  2026-10-19, parked awaiting the captain's go). Anything in this section that the recipe does not do was
  deliberately left for a later dish, not forgotten.
- **Bigger (next dish):** draw the 3D frame as a textured full-screen quad in the UI pass and merge the passes,
  removing both blits. This is what finally makes the render-scale knob pay.

## 7. Still unknown (needs the phone on USB)

- How many times a menu frame actually begins the scene target (1 or more) - measure, do not assume.
- Whether the driver reports `VK_EXT_load_store_op_none` anyway (it would be free to use if it does, but the
  recipe must not depend on it).
- Whether the frame is per-draw-call bound on top of all this - that is the micro-benchmark's job.
