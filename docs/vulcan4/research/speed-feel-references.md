# Sense of speed — what shipped racing games actually do

**Research note for the opt-in "sense of speed" feature. 2026-10-23.**
Every number is (a) quoted from a primary source with a URL, (b) measured off this repo / the GT4
disc with the method shown, or (c) marked **UNVERIFIED** / **FOLKLORE**. Where a well-known claim
has no primary source, it says so instead of inventing one.

Short version: **FOV and peripheral optic flow dominate; motion blur is the weakest celebrated
technique; frame-rate stability beats frame-rate magnitude; audio carries a measurable share of
speed discrimination.**

---

## 1. What the player actually perceives

- **Optic flow**, not speed itself: *global optical flow rate* = ground speed / eye height,
  *optical edge rate* = texture edges crossing a reference point per second. Edge rate has the
  stronger effect on perceived speed and is what FOV and texture density change.
  ([Clemson thesis, ch. 2](https://tigerprints.clemson.edu/all_theses/978))
- **Periphery is load-bearing.** Jamson (2000), cited in the Iowa sim study, put the requirement at
  **≥120° horizontal** for correct speed perception. Raising FOV moved speed production from
  **127% → 121%** of target (average speed) and **165% → 152%** (25 mph); >100% = speed overproduced.
  ([Driving Assessment 2017](https://doi.org/10.17077/drivingassessment.1419))
- **Narrow FOV makes people drive faster.** 60° vs 135° geometric FOV: drivers "highly underestimate
  their driving speed" at 60° and drove closer to real-world behaviour at 135°.
  ([Procedia CS 2020](https://www.sciencedirect.com/science/article/pii/S1877050920304270))
- **Calibratable, and unnoticed.** Over GFOV/display-FOV ratios 0.83–1.33, visual speed was
  underestimated by a mean **10%** at ratio 1.00; regression put zero error at **1.22:1**. A change
  of **0.15** in the ratio was significant at p<0.001, and *no subject noticed the FOV had changed*.
  ([Paillot 2010 PDF](https://iutchalon.ube.fr/wp-content/uploads/2013/03/Damien_paillot_2010.pdf),
  [preprint](https://doi.org/10.13140/rg.2.1.2407.1526))
- **Vection scales with speed.** 120 mph laps gave significantly higher vection and presence than
  60 mph; expanding flow beats contracting/translational cues.
  ([Sci. Rep. 2024](https://www.nature.com/articles/s41598-024-80778-4))
- **Why a static camera reads slow:** the camera supplies the car's optic flow but none of the body's,
  and at 54–60° a large part of the flow field — the fast periphery — is simply off-screen.

**For a 4:3-era engine:** hFOV ~54–60° sits far below the 120° the literature wants, so the first
10–15° of widening are both a real gain and (per Paillot) unnoticeable.

---

## 2. FOV and camera placement — numbers per game

### 2.1 GT4's own camera, measured from the disc (ground truth here)

From `docs/vulcan4/research/wheels-and-cameras.md` (CAR4 `+0x80` table) and the code consuming it:
chase row = position **(0, −1.700, −6.000)**, turns 8°/0°/0°, **lens 2.6990**.
Projection: `sx = 1/tan(atan(1/lens) * 4/3)` (`src/app/race_game.c:2206`), `sy = sx * 1.30122`
(`:2209`), and `sx = cot(hFOV/2)` because the renderer's focal is `sx * (H*4/3)/2`
(`src/platform/sdl/race_view.c:2469`). Views 0 and 1 bypass the lens: hard-coded `sx = 1.73205`.

Computed (`python`, atan/tan, no approximation):

| view | lens | sx | **hFOV** | **vFOV** |
|---|---|---|---|---|
| views 0/1 as the engine draws them (hard-coded) | — | 1.732 | **60.0°** | 47.9° |
| chase point's own lens (CAR4, *not used* for view 1) | 2.6990 | 1.954 | 54.2° | 43.0° |
| cockpit-ish (point 5) | 2.4142 | 1.732 | **60.0°** | 47.9° |
| point 0 / 3 | 2.0503 | 1.446 | **69.3°** | 56.0° |
| wide (point 7) | 1.7321 | 1.192 | **80.0°** | 65.6° |

**Correction (measured after the first draft):** the chase camera is drawn at **60° h / 47.9° v**, not
54.2°. The CAR4 chase point (0, −1.7, −6) carries lens 2.699, but `race_game.c:2287` bypasses the
point's lens for views 0 and 1 and uses the captured projection `sx = 1.73205` — so 54.2° is what the
row *would* give, not what the player sees. The conclusion survives, slightly weakened: 60° is still
half of the ≥120° the perception literature wants, and it is still the narrowest camera the game
draws for the car.[^chase]

[^chase]: Verified at HEAD `dbcd5bf`: `race_game.c:2287` is the only place views 0/1 get a projection,
and it is the constant. The CAR4 lens is used for view ≥2 only.

**DERIVED (first-order):** for a fixed world direction, per-pixel flow scales with `1/tan(hFOV/2)`,
so 60° → 70° *reduces* central per-pixel motion ~18%. The gain is that 30–35°-off-axis scenery
(where angular rate is highest) comes on screen and edge rate rises. FOV trades centre clarity for
periphery — it is not a free multiplier.

### 2.2 Modern titles

- **iRacing** — horizontal FOV from screen width and eye distance, built-in calculator in
  Settings → Display; wider "adds distortion"
  ([support](https://support.iracing.com/support/solutions/articles/31000157467-iracing-camera-tool)).
  A coaching site puts the single-monitor default at **~80° hFOV**
  ([Coach Dave](https://coachdaveacademy.com/tutorials/the-ultimate-guide-to-fov-in-iracing/)) — secondary.
- **Assetto Corsa** — vertical FOV, **default 56° vFOV** cockpit, said to give "a more satisfying
  sense of speed" than the geometrically correct ~20–26°
  ([Kunos forum](https://www.assettocorsa.net/forum/index.php?threads/fov-field-of-view-heres-what-i-think.19426/)).
  **UNVERIFIED** as a shipped default (forum claim, no Kunos doc).
- **F1 (Codemasters/EA)** — slider spans **37°–117° hFOV**
  ([simracingcockpit](https://simracingcockpit.gg/fov-calculator/)); the setting is an offset from a
  built-in default of roughly **77° hFOV**, community-derived from game files, "players don't all
  agree on it" ([fovcalc](https://fovcalc.xusf.xyz/guide/game-fov-settings)). **FOLKLORE-adjacent.**
- **Forza Horizon 5** — vertical FOV, per-camera sliders (chase / far-chase / driver / dash / hood /
  bumper); users report a **40° vFOV** floor and call stock "claustrophobic"
  ([walkthrough](https://youtube.com/watch?v=8b-KOMFb8To), [fovcalc](https://fovcalc.xusf.xyz/guide/game-fov-settings)).
  Stock values **UNVERIFIED**.
- **Gran Turismo 7** — **no FOV slider**; only "Adjust Cockpit View" with Height and Depth on a
  **0–5** scale plus a **Wobbling Type** selector
  ([racinggames.gg](https://racinggames.gg/article/can-you-adjust-fov-in-gran-turismo-7)).
- **Project CARS 1/2 (Madness engine)** — horizontal FOV, and the only mainstream sim documented as
  applying **dynamic (speed-linked) FOV by default**
  ([mysimrig](https://mysimrig.nl/en/blog/simracing/sim-racing-fov-guide/)). **The curve itself is
  not published.**

### 2.3 On speed-linked FOV curves

No primary source (GDC/SIGGRAPH/dev blog) publishes a FOV-vs-speed curve with numbers. Every
"add N degrees at top speed" figure I found was **FOLKLORE**. What *is* published is the perceptual
target: perceived speed rises roughly in proportion to the GFOV/PFOV ratio (Paillot), so tune
against the 1.22:1 result instead of copying a number.

---

## 3. Camera dynamics — spring, lag, lead

Primary source with structure but no printed constants: **Criterion Games, "Vehicle Feel
Masterclass", GDC 2018**
([slides PDF](https://media.gdcvault.com/gdc2018/presentations/Harris_Matthew_VehicleFeelMasterclass.pdf)).
Sprung-camera parameters, in Criterion's own slide words:

- **Convergence** — "Move Camera to Target"
- **Momentum** — "Faster Camera Movement"
- **Damping** — "Camera Speed Limit"
- plus a **lookahead distance** ahead of the car, and a camera-led vs vehicle-led choice
  ("Player controls Camera, Vehicle Follows" vs the reverse). Their verdict for a racing title:
  **"Vehicle-led Sprung Camera"** — camera-led was rejected for "Loss of handling Precision /
  Unexpected Motion".
- On landings: "Use **Momentum with low Damping** for exaggerated motion" + shake. The under-damped
  case is deliberate for impact, not for steady state.

Published smoothing parameters that *do* exist:

- **Asymptotic averaging** (Nesky, GDC 2014): `x += (target - x) * w` per frame, `w = 0.01` "nice and
  slow", `0.1` "reasonably fast", `0.5` "incredibly fast", all stated **at 60 FPS**
  ([GDC Vault](https://www.gdcvault.com/play/1020460/50-Camera-Mistakes)). Frame-rate dependent —
  at 120 fps the same `w` halves the time constant. Never ship a raw per-frame lerp.
- **SuperTuxKart** (read from source): `m_position_speed = 8.0f`, `m_target_speed = 10.0f`, smoothing
  as `delta = dt / max(dt, smooth_time)` → exponential with τ = **0.125 s** position, **0.1 s** target
  ([camera_normal.cpp](https://raw.githubusercontent.com/supertuxkart/stk-code/master/src/graphics/camera/camera_normal.cpp)).
  FOV there is fixed config (`stk_config->m_camera_fov`)
  ([camera.cpp](https://raw.githubusercontent.com/supertuxkart/stk-code/master/src/graphics/camera/camera.cpp)).
- **Steady-state trailer lag — DERIVED** (textbook): a critically damped follower with natural
  frequency ω tracking a target at constant speed v settles **2v/ω** behind. At 200 km/h (55.6 m/s):
  ω=10 → **11.1 m**, ω=6 → 18.5 m, ω=4 → 27.8 m. GT4's chase point is already 6.0 m behind the car
  origin, so an extra 11 m would be a different camera — lag should be **acceleration-driven and
  small**, not speed-proportional.
- **Settling times** (open implementation with tests): stiffness 400 → 90% in **0.200 s**, 99% in
  **0.333 s**; stiffness 140 → 0.333 / 0.567 s
  ([third-person-camera-rig](https://github.com/m2-md/third-person-camera-rig-threejs)). Anchor:
  settle in ~0.2–0.35 s, i.e. visible for ~12–20 frames at 60 Hz.
- **Lead under braking** is the mirror of trail under acceleration in the same model; no shipped
  title publishes a value. **UNVERIFIED.**

Yaw-rate context for "why the chase cam feels glued": 90 km/h through a 50 m radius = **28.6°/s**;
a camera lagging 0.2 s sits 5.7° off-axis.

---

## 4. Shake

- **Trauma model** (Nesky, GDC 2014): trauma in [0,1]; events add +0.2 or +0.5; linear decay; apply
  as **trauma²** (or ³) so small traumas vanish. Quoted: trauma **0.30 / 0.60 / 0.90 → 3% / 22% /
  73%** shake. Use **Perlin noise**, not per-frame random; in 3D prefer **rotational only**;
  "tread carefully in VR" ([slides](https://www.gdcvault.com/play/1020460/50-Camera-Mistakes)).
- **Criterion's landing shake** is event-driven (touchdown), amplitude scaled by impact, layered on
  the low-damping momentum response (GDC 2018).
- **Frequency bands.** No racing title publishes its shake spectrum. Physical anchors: engine firing
  = `rpm/60 × cylinders/2` (4-cyl: 100 Hz at 3000 rpm, 26.7 Hz at idle); kerb/road input a few Hz to
  ~20 Hz; whole-body vibration standards weight **0.5–80 Hz** peaking near 4–12.5 Hz
  (**ISO 2631-1**, not fetched here — exact weighting **UNVERIFIED**). Practical reading: engine
  shake *fast and tiny* (tens of Hz, ≤0.2°), kerb/impact *slow and larger* (2–8 Hz, 0.5–2°).
- **Per-title amplitudes: UNVERIFIED.** Community presets land near **~2° RMS pitch** at max trauma
  with ω≈8 (a non-racing Roblox preset: sigma 0.055 → "~2° RMS") — sanity range only.

---

## 5. Blur and post

The headline changes the priority order:

> **Simulated motion blur did not change perceived speed.** *Split/Second* was tested with and
> without it: players *could reliably detect* the effect, but lap time, **perceived speed** and
> enjoyment did not move. The authors still recommend it for anti-strobing and "the look".
> (Sharan, Neo, Mitchell, Hodgins, ACM MIG 2013 —
> [PDF](https://people.csail.mit.edu/lavanya/PDF/sharanetal13_MIG.pdf) /
> [summary](https://people.csail.mit.edu/lavanya/motionblur.html))

- **Split/Second** (Ritchie, Modern, Mitchell, SIGGRAPH 2010): image-space velocity field +
  texture-space (mip-chain anisotropic) blur, "**less than one sample per pixel**"; the paper itself
  claims "motion blur is key to delivering a sense of speed" ([ACM](https://doi.org/10.1145/1837026.1837048)).
  Frame budget was **33.33 ms** (30 Hz) with blur one line item among bloom, grade, flare, HDR
  ([DF interview](https://www.digitalfoundry.net/articles/digitalfoundry-split-second-tech-interview?page=2)).
- **Basic quality = 2–16 samples/pixel** in single-frame rasterisation (Mitchell 2001, cited in the
  MIG paper). The standard cheat is half-res + few taps: UDK's blur runs at **half resolution** with
  a gather of "a few texture lookups", warning that ~5 lookups at high velocity "can appear as 5
  ghost images" ([UDK doc](https://docs.unrealengine.com/udk/Three/MotionBlur.html)).
- **Per-object vs camera-only.** UE's modern path is a **velocity GBuffer, one vector per pixel** —
  camera + per-object in one pass, but blind to shadows/reflections and it breaks on fast crossings
  ([UE docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-motion-blur)).
  GT4 has no velocity buffer and no spare MRT, so camera-only radial/zoom blur is the only realistic
  option (UDK describes radial blur as a separate, cheaper effect).
- **Shutter angle → blur length (DERIVED, exact):** 180° exposes half a frame = **16.7 ms** at 30 fps,
  **8.33 ms** at 60, **4.17 ms** at 120. At 200 km/h that is **0.46 m** of scene travel at 60 fps —
  a few pixels near the vanishing point, tens at the edge.
- **Defaults:** UE `MotionBlurAmount = 0.5` ≈ 180° shutter, and `MotionBlurTargetFPS` "makes motion
  blur independent of actual frame rate … a value of zero makes the motion blur dependent on the
  actual frame rate" ([API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Engine/FPostProcessSettings/MotionBlurTargetFPS)).
  Community measurement says the shipped target defaults to **30**, quadrupling blur at 120 fps
  ([forum](https://forums.unrealengine.com/t/motion-blur-is-wrong-by-default/137777)) — the "30" is
  community-observed, the 0–120 clamp is documented.
- **Cost on Android:** half-res at 1–2 taps is the only band that fits a phone at 4:3/60, and the MIG
  result says the perceptual return is ~zero — so this sits **last** in priority.

---

## 6. Frame rate, pacing, interpolation

- **Locked beats high.** DriveClub's director Paul Rustchynsky: "the most important thing for a
  racing game is a locked frame-rate. Whether it's 30fps or 60fps … every controller input you make
  is consistent" ([DF](https://www.digitalfoundry.net/articles/digitalfoundry-2014-frame-rate-vs-frame-pacing)).
  DriveClub shipped **rock-solid 30 fps** with camera *and* object blur "used extensively"
  ([DF vs DriveClub, mirror](https://gamrconnect.vgchartz.com/thread.php?id=191411)).
- **Unlocked is the failure mode.** DF: frames arrive at 16 ms or 33 ms depending on v-sync, giving
  "on-screen judder and inconsistency in the controls".
- **120 fps is real for racing.** GT7 patch 1.31's frame-rate 120 Hz mode "manages to hit a locked
  120fps the overwhelming majority of the time while racing, even in intense 20-car battles", at
  ~1260p; DF calls the added fluidity "palpable"
  ([DF](https://www.digitalfoundry.net/articles/digitalfoundry-2023-gran-turismos-four-new-120hz-performance-modes-tested)).
- **Blur length is frame-time bound:** the same 180° shutter smears 2× longer at 30 fps than 60 —
  why 30 fps racers need blur for anti-strobing and 120 fps racers barely do (§5).
- **Interpolation:** GT4 is a fixed 60 Hz guest. Above 60, poses must be interpolated (prev/current
  eye+target+roll, one-frame lag) or camera motion strobes at exactly the frequencies the shake/FOV
  work is adding. No published precedent; this one is arithmetic on the guest tick.

---

## 7. Audio (half the sensation, and the best-evidenced part)

- **Engine sound sets speed discrimination.** Two-interval JNDs at a 100 km/h reference: **1.93 km/h**
  with engine sound, **3.48 km/h** EV-like, **5.15 km/h** in silence (n=30). No difference at 40 km/h.
  Attributed to "RPM-related, clearly audible frequency shifts", not loudness or sharpness
  ([arXiv 2607.19023](https://arxiv.org/abs/2607.19023)).
- **Killing engine sound makes people drive faster** and control speed worse; sound is "feedback
  rather than noise". Reduced engine sound → underestimated speed → higher speed, worse control
  ([PMC review 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11446104)).
- **Boosted/artificial engine sound raises perceived sportiness** and improves curve speed control
  without changing mean speed. Production reference (Renault): 2nd harmonic **+3 dB below 2000 rpm,
  +7 dB at 2000–3500 rpm, nothing above 3500 rpm**
  ([Wiley 2021](https://onlinelibrary.wiley.com/doi/10.1155/2021/4396401),
  [TRR 2022](https://sciencedirect.com/science/article/pii/S136984782200167X)).
- **Wind and road roar** are the other cues and are systematically under-studied; tyre noise varies
  with surface, aero with weather ([PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC11446104)).
  No published dB-per-km/h curve for either. **UNVERIFIED.**
- **Implication:** RPM-linked pitch does more perceptual work than any post effect, and GT4 already
  supplies RPM and gear per car (`CAR_TACHO`). Cheapest real win available.

---

## 8. Summary table

Effect sizes are the best measured values; "none" = published null result.

| Technique | Measured effect size | Cost | Confidence |
|---|---|---|---|
| Widen FOV toward periphery | GFOV/FOV 1.00→1.22 removes a **10%** underestimate; 60° vs 135° GFOV = large bias | ~zero | **High** (4 studies) |
| Camera shake (trauma²) | No published speed effect; physicality/impact cue (3%/22%/73% at trauma .3/.6/.9) | ~zero, CPU | **High** it's cheap, **low** it sells speed |
| Motion blur (camera or per-object) | **Detected, no effect** on perceived speed, lap time, enjoyment | 0.5–2 ms half-res desktop; heavy on mobile | **High** on the null result |
| Speed-linked FOV ramp | Never measured for any title; basis is the GFOV ratio result | ~zero | **Low** on numbers, **High** on mechanism |
| Chase spring lag (2v/ω) | None measured; Criterion ships it for precision | ~zero | **High** math, **low** effect |
| Locked frame rate | Unlocked = judder + inconsistent input; locked 30 accepted | scheduling | **High** |
| 120 fps | "Palpable" fluidity, locked 120 in GT7 racing | 4× GPU or ~1260p | **Medium-High** |
| Engine pitch / RPM audio | JND **1.93 vs 5.15 km/h** at 100 km/h | CPU + samples | **High** |
| Engine sound boost (AES) | More perceived sportiness, better curve control, same mean speed | trivial | **Medium-High** |
| Radial/zoom blur | No isolated measurement found | cheaper than velocity blur | **Low** — **UNVERIFIED** |

### Reading of the evidence

1. **FOV + audio first** — both have measured perceptual effect sizes and both are nearly free.
2. **Camera lag/shake third** — they sell weight and physicality, not speed; Criterion treats them as
   handling-precision tools. Keep amplitudes small; below the trauma² threshold they are invisible.
3. **Motion blur last** — the one controlled experiment on a shipped racing game found no
   perceived-speed benefit. Ship it for strobing, and say so in the option text.
4. **Never ship a raw per-frame lerp** (Nesky's weights are quoted at 60 FPS); use dt-normalised
   smoothing or a critically damped spring.

### Not found, and not invented

No published per-game FOV-per-km/h curve. No published spring stiffness/damping values for any shipped
racing title (Criterion names the parameters, not the numbers). No published shake amplitude/frequency
table per game. No dB-per-km/h wind or road-noise curve. If a future doc needs those, they must be
measured here and A/B'd on players — not cited.
