# The GLES experiment — `android-gles` branch

*Written 2026-10-19. Status: **parked** until the A34 can be attached to a machine for adb. The branch is local,
unpushed, and is not part of `main`. Nothing here is a promise to ship.*

## Why it exists

On the A34 the Vulkan renderer is not a Vulkan renderer: it is a **GL command stream translated into Vulkan**
(`vk_commands.cpp` re-implements 86 GL entry points as `vkgl*`). Measured on the phone, that path costs about
**1 ms per draw call**, and two rounds of Vulkan-level fixes (optimal image layouts, skipping the first-pass
attachment loads) moved the frame rate by **zero**. The translation layer, not the flags, is the suspect.

So the experiment asks a cheaper question first: what does the same engine do when it speaks **GLES natively**?

## What the branch contains (4 commits, `main..android-gles`)

| commit | what |
|---|---|
| `4c11f32` | ES 3.0 context, `#version 330 core` → `#version 300 es` rewrite in `gl.c`, `GLESv3`/`EGL` link, `renderer` key in `video.ini` |
| `187538e` | its own `applicationId` (`org.moonengine.android.gles`) and label **Moon Engine OpenGL**, so it installs **beside** the game |
| `6a1661b` | GLES load fix: `glClearDepth` → `glClearDepthf`, `glGetTexImage` optional, photo export guarded |
| `f2466ce` | drop the two full-frame copies (scene direct, textured-quad upscale), overlay counters, Android-only GL state cache |

Every piece is inside `#ifdef __ANDROID__`, so desktop behaviour is unchanged by construction. The branch has not
been pushed; `main` does not contain any of it.

## What it measured

| build | fps | frame | CPU | RSS |
|---|---|---|---|---|
| Vulkan (translated) | 5.2 | 193.92 ms | 43% | 1112 MB |
| **GLES (this branch)** | **13.2** | **75.96 ms** | **104%** | **798 MB** |

Same phone, same showroom scene, 2267×1080, Mali-G68 MC4. The hardware is now saturated instead of idling — which
is what a real renderer looks like.

## How to build and run it

```sh
git checkout android-gles
JAVA_HOME=/mnt/ssd/android-dl/jdk/jdk-17.0.20.1+1 \
  python3 tools/build_android.py --sdk /mnt/ssd/android-sdk \
  --gradle /mnt/ssd/gradle-8.14.3/bin/gradle
# APK: android/app/build/outputs/apk/debug/app-debug.apk  →  moonengine-android-gles.apk
```

Flip back to Vulkan without rebuilding: put `renderer=vulkan` in `video.ini` inside the app's user folder
(`.../org.moonengine.android.gles/files/MoonEngine/MoonEngine/video.ini`).

Log lines that prove which path ran:
- `android: renderer=opengl (GLES)`
- `OpenGL: glClearDepth -> glClearDepthf`
- `video: scene direct (no blit)` / `video: scene upscaled via quad N%`

## What is still unmeasured (the reason it is parked)

1. **Is it fill-bound or CPU-bound?** Needs render scale 50% and 35% on the device while watching fps. Those
   controls exist and are untouched by the branch.
2. **Are the frame copies actually gone?** Needs a frame trace on the device, not a code read.
3. **Does the state cache help?** It needs the overlay's new `state skips` counter read off the phone.
4. **Draw batching** — the next lever if the CPU is still the limiter.

Everything above needs `adb` (logcat + per-frame timing + screencaps); guessing from the desktop has already cost
two wasted rounds.

## Decision rule

If GLES beats Vulkan on the phone, it does **not** become a fork. The plan is: rebase onto `main`, keep it behind
`graphics.renderer` (Vulkan stays the default), then lift the Android-only parts into
`src/platform/android/video_gles.c` — the same shape Jonhline used for the Switch in `deko_video.cpp`. If it does
not beat Vulkan, the branch is deleted and the finding stays in
`docs/vulcan4/RESEARCH-MALI-A34-PERF.md`.
