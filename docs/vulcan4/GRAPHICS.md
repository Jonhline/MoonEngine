# GRAPHICS — fabulous without slopifying it

**Written 2026-10-15, on the captain's instruction.** Wishlist entry: `G6.38` (render presets). The one-line
brief from him: *"Think we can make the game look fabulous? Without. U know. Slopifying it?"*

---

## THE RULE THAT DECIDES EVERYTHING

Every proposed change is classified before it is built. The class decides the default:

| class | what it does | ships as |
|---|---|---|
| **ACCURACY** | makes the image **more correct** — same look, fewer artifacts | a quality option; may be on |
| **RESTYLE** | adds a **look** the original never had | a **preset, off by default** (`LAW 12`) |

If a change cannot be put in one of those two boxes, it is not ready to be built.

## WHAT WE ALREADY HAVE (measured, not hoped for)

- **The renderer is ours**: OpenGL 3.3 core, offscreen framebuffer, our own shaders that reproduce the GS's
  behaviour (alpha as a coefficient, destination-alpha test, per-tap texture addressing, per-pixel spherical
  environment mapping on cars). `docs/RENDERER.md`.
- **Resolution is already free**: 1280×720 → **3840×2160** and *Custom*, windowed or fullscreen, 4:3 or 16:9.
- **FSR 1 is already integrated** (`third_party/fsr1`, MIT) with five modes in the video settings — Native
  through Performance. That is *output resolution vs GPU cost*, not detail.
- **Text is already sharp at any resolution**: `src/core/glyph_sdf.c` converts the game's glyph atlases to
  **distance fields** (Mitchell-Netravali edge tracing) and the shader reconstructs them.
- **Studio/showroom shadow shaders exist** already.

## TIER 1 — ACCURACY (the cheap, safe, biggest-per-buck wins)

1. **Mipmaps + anisotropic filtering.** The road ahead currently shimmers; this is the single most visible
   improvement available in a driving game and it changes nothing about the art.
2. **MSAA (or SSAA) on the offscreen target.** Real edges, instead of smeared ones — deliberately *not* a
   sharpening filter, which invents halos.
3. **Vertex-snap removal (PGXP-style).** GT4's geometry wobbles and jitters because of PS2 vertex snapping;
   removing it is *accuracy*. GT2Recomp, one console down, already ships DuckStation-grade PGXP — the
   precedent exists and users treat it as a headline feature.
4. **Z-stability.** Reduce z-fighting, another GS-era artifact.
5. **Frame pacing / stable 60.** Not shading, but it reads as quality more than any filter does.
6. **UI art upscaling.** Panels, buttons, logos, maps and HUD plates are `.gpb` image packages
   (`menu.c:766`), and a community **HD HUD & UI pack** already exists for them — taken as the *user's* files
   through the `G6.23` loader. Text needs nothing (see above).

## TIER 2 — LOOK, AS PRESETS (off by default, and restrained)

A single post-processing pass (colour grade, gentle bloom, tone curve, motion blur, depth of field), each
individual effect toggled separately — photo mode is the natural showpiece for DOF and framing.

**Restraint is the feature here.** The default preset is *nothing*. A "Cinematic" preset exists for people who
want it, and the gate below still applies to it.

## TIER 3 — RE-LIGHTING (the project, and the one that can change the game's soul)

- **Real shadow maps** replacing the game's faked shadow geometry.
- **Environment reflections** replacing the spherical mapping.

Both are honest improvements *and* honest risks: they will make it look like a modern game, which is not the
same as looking like GT4. They stay last, behind a preset, and only after tiers 1–2 are proven.

## THE NO-SLOP GATE — checkable, not vibes

Every candidate frame is compared **side by side against PCSX2**, which this project already treats as the
oracle:

1. **Palette/hue deviation** from the reference frame must stay inside a stated threshold (per-channel mean and
   histogram distance).
2. **Any glow, contrast or light the reference does not have is a REJECT.** No effect may invent illumination
   that is not in the source.
3. **The floor:** with everything off, the reference frame (`md5 68d8eb83dc75b6fe6e03cc2372ca186d`) must not
   move a byte.

**By the same rule, these are rejected outright** — they are the ingredients of slop, not of quality:
bloom-everywhere, fake-HDR oversaturation and crushed blacks, sharpening halos, chromatic aberration, film
grain, smeared fake ambient occlusion applied as a blanket, and AI texture upscaling that hallucinates detail
that was never in the asset.

## THE HONEST CEILING

Shaders polish; they cannot invent detail. GT4's textures are 2005-era, so the resolution half belongs to
**user-supplied HD texture packs** (`G6.23`) — the renderer's job is to present them correctly, not to
manufacture them. A "fabulous" build is therefore: **correct rendering + high output resolution + the user's
own better assets + restrained, optional effects.**

## SEQUENCING

1. **Tier 1 first** — accuracy only, verifiable, and it needs no taste decisions.
2. **Tier 2** only once there is a picture to judge effects on.
3. **Tier 3** last, as its own project.

Everything here is judged by **pictures**, not by commits: the captain looks at a frame and says *"that's it"*
or *"that's wrong"*. Nothing is called improved until he has seen it.
