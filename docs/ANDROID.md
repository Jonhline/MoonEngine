# Android

Status recorded 18 October 2026. **Planning and measurement only: no Android build exists, nothing here
has been validated on a device.** It records what the desktop and Switch work already gives us, the route
we intend to take, and the milestones with the command that decides each one. See
[Nintendo Switch](SWITCH.md) for the port that this one copies, and [Input support](INPUT.md) whose
*Android platform foundation and menus* is the third phase of the touch plan.

## Why Android is the next platform

Every hard part of a new platform has already been done once, for the Switch:

| what a platform needs | what already exists (measured 2026-10-18) |
|---|---|
| a second platform in the build | `if(NINTENDO_SWITCH)` branches in `CMakeLists.txt` + `cmake/SwitchTargets.cmake`; the host tools/tests are marked `EXCLUDE_FROM_ALL` there |
| a renderer that is not desktop GL | `moon_vk_render` (`src/platform/sdl/vk_*.cpp`) and `moon_vulkan` (`vulkan_device.c`) are already separate targets, selectable at run time (`MOON_RENDERER_VULKAN`, "recommended" in the settings UI) |
| shaders on a new backend | `vk_pipeline.cpp` compiles GLSL **at run time** through glslang (`EShClientVulkan`, SPIR-V 1.0). glslang is compiled in — no external `glslangValidator`, no pre-baked `.spv` — and desktop GLSL is valid input for a Vulkan target, so the Vulkan path needs **no shader rewrite** |
| movies without spawning a program | the Switch solved exactly this: `cmake/SwitchTargets.cmake` swaps `movie.c` for `src/platform/switch/movie_ffmpeg.c` + `movie_decode.c` and links **libavformat/libavcodec/libavutil**, gated by `MOON_SWITCH_MOVIES` |
| a per-platform file and profile layer | `src/platform/common/switch_file.c` + `switch_profile.c` + `plat_pref_dir()` (`file_stdio.c`) |
| a non-x86 arithmetic environment | `src/core/physics/fpu_env.c` already carries an `__SWITCH__ && __aarch64__` branch next to the x86 `_mm_setcsr` one |
| a startup path of its own | `MOON_LAUNCHER_SOURCES` already switches to `src/app/switch_startup.c` + `switch_fpu.c` instead of the PC launcher |
| a private test package | `tools/package_switch.py`, `switch_install.ps1`, `switch_transfer.cs` |

And the graphics layer is the one part that is *already* platform-neutral: `vk_context.cpp` touches only
`SDL_Vulkan_GetInstanceExtensions`, `SDL_Vulkan_CreateSurface`, `SDL_Vulkan_GetDrawableSize` and
`SDL_LoadObject`/`SDL_LoadFunction`. SDL2's Android backend implements all of those, so the Vulkan
renderer arrives on Android with the surface handed to it.

## Devices

| device | what it is | why |
|---|---|---|
| **Ayn Odin 2** | Snapdragon 8 Gen 2, Adreno 740, Android 13, 6" 1920×1080 60 Hz IPS, 8/12/16 GB, built-in gamepad, active cooling | the primary target: mature Vulkan drivers with custom-driver support, a real controller, no touch requirement to drive (the Odin 2 **Portal** variant is the same SoC with a 7" 120 Hz OLED) |
| **Samsung A34 5G** | Exynos 1280, Mali-G68, One UI 7 | the secondary target: an ordinary phone, so it needs the touch/gesture layer and it is the honest performance floor |

Exact Odin 2 variant, installed Vulkan loader/driver version and free storage: **to be read off the device**
(`adb shell getprop`, `adb shell cmd gpu vkjson`) before any performance claim.

## Route

**Vulkan only, arm64-v8a only, through SDL2's own Android project.** The reasons are measured, not
preferences:

- The OpenGL path cannot be reused: `render3d.c` is `#version 330 core` GLSL and `fsr.c` needs GLSL 4.x
  (it already refuses to run without it). Android would want GLES 3.0 semantics — a rewrite of the desktop
  renderer, for no gain, since the Vulkan renderer is the recommended one and is already portable.
- SDL2, not raw `ANativeWindow`: SDL already owns window creation, input, audio, lifecycle and the Vulkan
  surface on Windows/Linux here, and its `android-project/` is the supported Gradle + NDK path
  (`SDLActivity` → `SDL_main`). The engine's `main()` keeps working.
- libav rather than an `ffmpeg` executable: Android will not spawn a bundled binary the way Windows does
  (`movie.c` uses `CreateProcessW`), and the Switch already proved the linked-decoder route.

## Milestones

Each milestone is done when its gate passes on a device — not when it compiles.

1. **Toolchain.** NDK 27.x, `platforms;android-35`, `build-tools;35.0.0`, SDL2 2.30.9 source with its
   `android-project`, all under a local SDK root (`/mnt/ssd/android-sdk`; `~/Android/Sdk` is a broken
   symlink to a drive that is not mounted).
   *Gate:* `aarch64-linux-android<api>-clang` builds a trivial shared library, and SDL's
   `android-project` produces an installable APK.
2. **A window on the screen.** SDL + Vulkan, no game code: open a window, create the swapchain, clear.
   *Gate:* the APK installs on both devices; the log names the Vulkan device and API version; the surface
   is cleared; rotating and backgrounding/re-foregrounding does not kill it.
3. **The engine links for arm64.** Every target that a device needs, `moon_vk_render` linked in, the
   Android startup path in place, and the launcher's first screen drawn.
   *Gate:* `MainActivity` start → launcher UI visible; no undefined symbols; no x86-only code left
   (`fpu_env.c` extended to `__ANDROID__ && __aarch64__`).
4. **The studio draws.** The real renderer, the real disc data.
   *Gate:* the studio's white room and the selected car, as a screenshot, on the Odin 2.
5. **The disc on the device.** Scoped storage, so the disc is read from the app's own files directory
   (dev: `adb push`), with `plat_pref_dir()` answering Android's files dir for mods/saves/settings.
   *Gate:* the car survey reports the same 722 cars / 0 failed loads as desktop.
6. **Input.** The Odin 2's built-in pad first (SDL gamepad), then the touch layer
   (`src/platform/switch/touch_contact.h` is the existing work) for the A34, including Android Back.
   *Gate:* a race started and paused with the pad; the same on the A34 by touch.
7. **Movies.** Link libav for arm64 and reuse the Switch decoder sources, or ship without the menu movie
   and say so in the UI.
   *Gate:* `movie frames=N` on device, or one honest "movies unavailable" line.
8. **Packaging.** Gradle release APK, an install helper, and a README that states the disc requirement.
   *Gate:* a clean device installs from the artifact alone.

## Hazards to keep in view

- **Surface loss is the default, not the exception.** Android destroys and recreates the surface on
  rotate, background and split-screen; the swapchain must be recreated rather than assumed.
- **Storage.** The captain's disc is a 5.3 GB image (md5 `6e03af5873c30e418021553a8832ca42` is the desktop
  reference); copying it onto a handheld takes time and the free space must be checked first.
- **The file layer is not `fopen` anymore.** Scoped storage means either a copied file in the app's own
  directory (v1, simple, large) or a `content://` descriptor through the platform layer (later).
- **No 32-bit, no emulated FPU.** arm64-v8a only; `-ffp-contract=off -frounding-math` already applied to
  the physics targets must be kept, and the frame arithmetic must match the desktop reference, not merely
  look right.
- **Performance is a measurement, not a promise.** The Odin 2 is the target where 60 fps is plausible;
  the A34 is where we find out what the floor is. Publish what the device logs say.

## Coordination

The Android port is a platform lane, the same shape as the Switch lane: engine/platform code, one
`cmake/AndroidTargets.cmake`, one `docs/ANDROID.md`. It touches no fidelity rule and changes no game
behaviour on the existing platforms. As with every other change, nothing is pushed to the shared
repository without the captain's word, and the port is announced on the issue tracker before it is
built.
