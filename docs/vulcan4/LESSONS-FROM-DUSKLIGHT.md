# Lessons from Dusklight

*A study, 2026-10-18. Read for structure and discipline — no code taken, no files copied. Everything below was
read from the public project itself; each claim carries its source.*

## What it is (measured 2026-10-18)

`github.com/TwilitRealm/dusklight` — a native port of **The Legend of Zelda: Twilight Princess** (GameCube,
plus every commercial disc except the Wii Korean release). **5,925 stars / 444 forks**, C++, **CC0-1.0**, repo
created 2025-11-30, last push 2026-10-11. It is built on the five-year `zeldaret/tp` **decompilation**, wrapped
in **Aurora** (`encounter/aurora`, the GameCube/Wii reverse-engineering framework) — see the README's credits and
the "Powered by Aurora" banner.

Its own words: *"Dusklight is a reverse-engineered reimplementation of Twilight Princess. It aims to be as accurate
as possible to the original while also providing new options, enhancements, and tools to customize your
experience."*

**Every release is a platform matrix** — v2.0.3 (2026-09-30) shipped eight artifacts: `android-arm64.apk`,
`ios-arm64.ipa`, `linux-{arm64,x86_64}.AppImage`, `macos-{arm64,x86_64}.zip`, `win32-{arm64,x86_64}.zip`. Cadence
was rough weekly through September (v2.0.0 → v2.0.3 in eleven days). It requires the player's own dump; no assets
ship.

## The five things worth taking

### 1. The platform layer is directories, and the Android shell lives in the repo

`platforms/{android,freedesktop,ios,macos}` plus per-platform CMake export modules
(`cmake/AndroidExports.cmake`, `AppleExports.cmake`, `WindowsExports.cmake`, `WindowsTargetProcessor.cmake`).
The Android shell is a **whole Gradle project inside the repository** (`platforms/android/app/...`,
`DuskActivity.java`, `game_mode_config.xml`, `network_security_config.xml`, gradle wrapper committed) with its own
README: SDK Platform 37, NDK pinned via `ANDROID_NDK_VERSION`, JDK 17+, built with **CMake presets**
(`cmake --preset android-arm64`, then `cmake --build --preset android-arm64`).

*Our equivalent:* `src/platform/{common,sdl,switch,android}` + `cmake/AndroidTargets.cmake` + `android/` — the
same shape, reached independently. **Lift:** their Android shell README states its prerequisites and the two
preset commands; ours requires reading `tools/build_android.py`. A 20-line `platforms`-style README per platform
is a cheap improvement. Also note they add a `game_mode_config.xml` (performance/Do Not Disturb hints) and a
`network_security_config.xml` — both are things Android asks for that we have not thought about.

### 2. A mod API with three tiers — asset, native, script

Mods are **`.dusk` bundles** (zip archives) described by a `mod.json`
(`id`, `name`, `version`, `author`, `description`, `icon`, `banner`), and there are three kinds
(`docs/modding.md`):

- **Asset-only** — replace files on the game disc ("overlays") and provide replacement textures (texture packs);
- **Native (C++)** — interop with game code and **hook functions**, must be compiled per platform;
- **Script (Luau)** — simple, widely compatible, limited to provided services.

They can be enabled, disabled and **reloaded at runtime**, and the repo carries the evidence of use: `mods/` holds
`template_mod`, `luau_runtime`, `audio_mod`, `shadow_mod`, `flow_demo`, `custom_actor_demo`, `window_demo`, plus
a separate `TwilitRealm/mod-template` repository for authors. `cmake/ModSDK.cmake`, `ModExports.exp`/`.ver` and
`CopyModAssets.cmake` are the machinery; `cmake/PatchFunchook.cmake` is the function-hooking dependency.

*Our equivalent:* our overlay ("a file shadows") + list merge + `mods.ini` + the launcher's MODS page **is their
asset-only tier, already working**. What we do not have is the other two tiers: a **scripting runtime** and a
**native mod ABI** — which is exactly `G6.13` (modding framework). Their design is a free blueprint for the
shape of it: one bundle format, one manifest, tiers by capability, a mod-manager UI, a template repo for authors.

### 3. One release = every platform, from one CI file

`.github/workflows/build.yml` is the whole story: a matrix over runners and **CMake presets**
(`x-linux-ci-gcc` with ninja + mold, `x-macos-ci-arm64`, `x-ios-ci`, Windows), artifacts named per platform and
architecture, and iOS/tvOS presets present but commented out — i.e. they keep the matrix honest by commenting the
dead legs instead of deleting them.

*Our gap, and it is a real one:* we build Linux, Windows and Android by hand, each through a skill note, with the
pins living in prose. We have no CI file and no release matrix. That is the single most mechanical thing on this
page we could copy, and it costs no design debate.

### 4. Renderer backends by capability, with a named floor

Their floor is **D3D12, Vulkan 1.1+ or Metal**; **D3D11 and OpenGL ES (Android) are "best-effort" and will not
achieve full accuracy or performance"** — stated plainly in the README. Their README also names **known issues on
Adreno GPUs**.

*Our equivalent:* the same two-backend split (OpenGL on the desktop, Vulkan as the recommended one) and the same
conclusion we reached for Android — Vulkan only, GLES would be a rewrite. **Lift:** publish the floor in the same
way (one sentence in our README), and treat the Adreno note as a warning about the Odin 2 rather than a surprise.

### 5. Documentation as a per-subsystem practice

`docs/building.md`, `docs/code-conventions.md`, `docs/modding.md`, a deep **`docs/jaudio.md`** (the GameCube audio
engine: mermaid + SVG flow diagrams, plus **ImHex patterns** for the format in `docs/imhex/jaudio/*.hexpat`), and
`docs/ios-install-altstore.md`.

*Our equivalent:* `docs/vulcan4/research/` (e.g. `format-mesh-contract.md`) is the same idea. **Lift:** the ImHex
pattern files — a format that has a written parser pattern can be inspected by anyone without writing a probe.

## What they have that we do not (honest gaps)

| | Dusklight | Moon Engine |
|---|---|---|
| release matrix / CI | 8 artifacts per release, one workflow | manual, per-platform notes |
| mod tiers | asset + native + **script (Luau)** + runtime reload | asset-only |
| mod author tooling | `mod-template` repo, documented API | our own `moonmod_*` tools, no author docs |
| platform reach | Windows, macOS, Linux, **Android, iOS**, browser build | Windows, Linux, Switch, Android (in progress) |
| community | 5,925 stars, active modding scene | ours |
| contributing policy | **refuses contributions that are primarily AI-generated**, enforces code conventions | we build with agents, by design |

## What we have that they do not

- **The disc is ours to extend.** We re-render and re-drive GT4 with our own engine, so content that never
  existed — added cars, added tracks, custom race modes (`G6.26`), the single-car career (`G6.27`), the studio
  launcher — is *ours to invent*. They keep the original code (which is why their accuracy is free) and can only
  extend it where hooks exist; their headline changes are player-facing options, not new game systems.
- **A track/car intake pipeline** (`SDK-MAKING-A-CAR.md`, the mesh contract, `moonmod_glbcar`) — a content
  authoring path, not just a mod loader.
- **A Switch target**, which they do not ship.

## Two things to keep in mind while studying it

1. **Their accuracy is structural, not earned.** They run the game's own decompiled code; when we match a
   behaviour we have *proved* something, which is why our measurement gates are the project's spine and theirs can
   be lighter.
2. **Their mobile pain is our mobile pain.** Same API floors, same Adreno caveats, same "GLES is best-effort"
   judgement — arriving at the same conclusions independently is a good sign for our route, not a reason to copy
   theirs.

## Sources

- `github.com/TwilitRealm/dusklight` — README, `docs/modding.md`, `docs/building.md`, `platforms/android/README.md`,
  `.github/workflows/build.yml`, `cmake/`, releases v2.0.0–v2.0.3.
- `encounter/aurora` (the framework), `zeldaret/tp` (the decompilation), `TwilitRealm/mod-template`.
