# MOON ENGINE — DISPATCH QUEUE

The captain's own words, one block per thing he wants dispatched. **Nothing here is fired until he says go.**
Status: `OPEN` (not sent) · `PROMPTED <job>` · `LANDED <commit>` · `BLOCKED <why>`.

Measured facts under each entry come from me reading the code, never from a chef's summary. His verbatim sentence is
quoted so this file stays his voice, not my paraphrase.

---

## Q1 — PERF: the A34's 5 fps (2026-10-19) — `OPEN`

His words:
> *"how the fuck are we getting 15 fps everywhere. Even on the starting cutscene."*
> *"And rendering a car gives me like 6 fps on the showroom. I can't imagine a race lol"*
> *"This has to be a mediatek issue. This cpu and gpu are most def capable of rendering 10k tris."*
> *"Sure. Anything we can do to optimize?"*

**Measured on his device** (G6.40 overlay screenshot, Peugeot 307 showroom):

| field | value |
|---|---|
| fps / frame time | **5.2 / 193.92 ms** (worst 243.98 ms) |
| cpu / rss | 43% / 1112.4 MB |
| gpu / kernel load | 71% / 69% |
| draws / tris / cull | **102 / 11,798 / 0** |
| renderer / api | Vulkan / vulkan |
| size / platform | **2267×1080** / android Mali-G68 MC4 |
| late frames | 1028 |

**Verdict:** geometry is irrelevant (his point, upheld); the frame is pixel/bandwidth bound at full panel resolution.

**Measured in the tree, with Arm's Mali guide behind it:**
1. No render-scale exists on Android — `video_scene_quality()` is `#if !defined(__SWITCH__) && !defined(__ANDROID__)`
   (`video_gl.c:936-944`) and the FSR `upscale` path is refused since `81b6543`; `video_begin_scene()` then takes
   `ratios[0] = 1` → **scene at full output size**.
2. The entire render path runs in `VK_IMAGE_LAYOUT_GENERAL`: attachments (`vk_pipeline.cpp:231-234`), sampled
   textures (`:435`), offscreen image (`vk_context.cpp:185`); the swapchain is blitted from `UNDEFINED`
   (`vk_context.cpp:623`). Arm: GENERAL/UNDEFINED are *unsafe* layouts — **transaction elimination and AFBC are
   lost**, so the frame round-trips external memory every pass.
3. The swapchain is `VK_IMAGE_USAGE_TRANSFER_DST_BIT` only (`:553`) and receives a **full-extent blit every frame**
   (`:637`) — 2.45 Mpx of pure bandwidth, and TRANSFER_DST itself can disable AFBC.

**Proposed order** (cheapest + safest first):
1. **Render-scale knob for Android** — render 0.5–0.7× offscreen, let the existing blit upscale. Also acts as the
   *proof* of the bandwidth diagnosis: fps should scale with pixel count.
2. **Layouts** — `COLOR_ATTACHMENT_OPTIMAL` / `SHADER_READ_ONLY_OPTIMAL` / real swapchain layouts, so Mali can use
   AFBC and transaction elimination.
3. **Draw into the swapchain** — give it `COLOR_ATTACHMENT_BIT` and delete the full-res blit.
4. **Merge the 3D and UI render passes** (Arm: two passes = two DRAM round trips).
5. Side note, not fps: **RSS 1112 MB** is heavy for a 5.3 GB phone.

---

## Q2 — LAUNCHER ALWAYS (2026-10-19) — `OPEN`

His words:
> *"The launcher was gone. For some reason."*
> *"What. The launcher is supposed to be there all the time man. Lemme clear data"*

**Measured:** a picked disc is persisted (`DiscActivity.java:105`, persistable grant at `:91`), so `DiscActivity`
opens it and jumps to `GameActivity` (`:66-69`, `:112`) and `GameActivity.getArguments()` hands it to native — where
`launcher_bootstrap` bails at `launcher.c:21` (`argc>1 && argv[1] != "--launcher"`) and returns *run the game*.
Proof line: `launcher: game (arguments 4)` instead of `launcher: studio front-end (arguments 6)`. The APK is not
missing the launcher (`libmain.so` still carries `studio front-end`).

**Want:** the room on **every** launch; a remembered disc may only *pre-fill* it, never bypass it.

---

## Q3 — BUG: the settings' green EXIT lands on the wrong screen (2026-10-19) — `OPEN`

His words:
> *"When I go to home and then settings and I finish and press on the green exit button it puts me back in the main
> menu. Where u choose arcade or gran turismo mode. This is likely due to the options setting also being in the main
> menu. Not just the gt mode home. This might be a global fix not just Android."*

**Measured:** not yet reproduced in the tree. His own diagnosis is plausible: one shared Options screen serves both the
GT-mode home and the title screen, so *finish* returns to a single hard-coded destination instead of the screen it was
opened from. He is right that it is probably **not** Android-specific — if it is a fixed destination, desktop walks the
same road.

**Want:** *exit* returns to **the screen that opened the options**, on every platform.

---

## Q4 — FEATURE: the Android driving controls are unusable (2026-10-19) — `OPEN`

His words:
> *"I've seen the Android controllers for driving the car. Terrible. First of all I want the buttons circles. The
> steering button. I don't even know how to use it. It's just a big square. For now I want 2 steering buttons when u
> choose digital on the setting. I've seen there's settings for Android."*

**Measured:** `mobile_controls.c:11-15` — nine controls, **all plain rectangles**; the steering box is
`{.06f,.53f,.26f,.36f}` = a 26 % × 36 % slab, which is exactly the "big square" he means. The digital/analog choice
already exists (`moon_mobile_contact:92` branches on `s->options.steering`; the option is `controls.touch_steering` in
`mobile_options.c:12`), so **digital mode is the only half that has to change**: two round buttons, left and right.

**Want:** round buttons for everything, and in **digital** steering mode two separate steering buttons. Drawing stays
tasteful; hit boxes may be larger than the art.

**Also measured (his follow-up question, "is there a system that hides touch when a controller is there?"):** yes, and
it works — `android_host.c:409-420` sets `s->controller = (plat_sdl_pad(0) != NULL)`, and that flag gates **both** input
(`mobile_controls.c:27`) and drawing (`android_host.c:440`). On the Odin the built-in pad is connected, so the controls
are hidden by design — that is why he never saw them. The driving set is drawn only while `driving == 2` and **not
paused** (`main.c:5727-5729`), so races-not-paused is the only state that shows them. **The gap:** the flag keys on
*connected*, not on *recent pad input* — a paired-but-idle Bluetooth pad on the A34 would hide the touch controls.

---

## Q5 — ISSUE: buttons sometimes don't register (2026-10-19) — `OPEN`

His words:
> *"sometimes the buttons don't press. Especially on the settings. Dunno what causes it."*

**Measured:** not yet reproduced. **Lead to check, not a conclusion:** the touch menus were built on the shared
preview / second-tap-confirm behaviour (`docs/INPUT.md` phase 3), so a "dead" press may be a *first* press waiting for
a second. The other candidates are hit-testing against a rectangle the art overhangs, and the bottom-right boxes
sitting under the gesture bar (the lowest box ends at y = .87 of the screen).

---

## Q6 — FEATURE: touch-friendly settings (2026-10-19) — `OPEN`

His words:
> *"making the settings ui more touch screen friendly without breaking its artstyle ofc."*

**Measured:** the engine's own settings screen is drawn by the shared UI layer; no Android-specific sizing/padding
exists for it today.

**Want:** bigger rows, one control per row, and no art change — the GT4 look survives, the targets get honest.

---

## Q7 — FEATURE: the exit button on splash and cutscene (2026-10-19) — `OPEN`

His words:
> *"the exit button I know it vanishes eventually after some time and it dosnt show up at all in a race which is good.
> Also add that it also vanishes and don't appear at all on the splash screen and the cutscene at the start of the
> game."*

**Measured:** the touch UI is drawn from one place — `moon_android_overlay(ui)` (`main.c:5729`, defined in
`android_host.c:438`) — so the lifetime rule has a single home to fix.

**Want:** no exit button on the splash screen and never during the opening cutscene; keep today's behaviour in races.

---

## Q8 — A34 TEST RESULTS AFTER "CLEAR DATA" — `AWAITING HIS RUN`

To capture when he retries: does the launcher/wizard come back, does the disc pick work once, and does the game
boot — plus `Download/MoonEngine/moonengine.log` if anything dies.

---

## LATER — DISCORD INTEGRATION (his ask, 2026-10-19) — `NOT MEASURED, NO DISH`

His words:
> *"Oh right. I also want discord integration soon."*

**CONFIRMED 2026-10-19: Rich Presence.** (The table below is kept so the other readings stay documented, but only the
first row is the ask.)

**How we would build it (research, not measured in our tree yet):** Discord's own docs say Rich Presence normally needs the
Game/Social SDK, but the local IPC route is documented and dependency-free: a socket (`$XDG_RUNTIME_DIR/discord-ipc-0`,
fallback `/tmp/discord-ipc-0`; Windows named pipe `\\.\pipe\\discord-ipc-0`) carrying frames of
`uint32 opcode + uint32 length + JSON`, with `SET_ACTIVITY` = opcode 1. That is a few hundred lines and **no SDK, no
extra DLL, no child process** — which matches his ONE-artifact rule.

**Needs from the captain when we start:** a Discord **Application ID** from the Developer Portal (I cannot create it
for him) and one small art asset uploaded to that application if he wants a picture on the card.

**Reality check:** the desktop Discord client must be running, so this is **PC-only forever** — it can never be
"global" in his sense, and Android gets nothing. It also ships **off by default**; if Discord is absent the game must
stay silent (no error, no stall, no retry loop).

Older note — not defined yet, kept for context; the candidates were:

| reading | what it would be | platform reality (his own scope convention) |
|---|---|---|
| **Rich Presence** | his Discord profile shows *Playing Moon Engine — Gran Turismo 4, Tsukuba* | **PC-only.** Android has no Rich Presence equivalent, so this can never be "global" |
| **A Moon Engine bot** | build/commit/APK posts into a Discord channel, mirroring the dev log | any platform, runs off Cortex |
| **From the game, into Discord** | screenshots / lap reports / session summaries posted by the engine | needs a token in the app — decide where the secret lives |
| **Community side** | the repo already runs an issue↔Discord bridge convention (Jonhline's tracker) | coordination, not code in the game |

Whatever it is, it will be its own dish and it ships **off by default** like everything we add.

## Q9 — LAUNCHER SKIPPED, AGAIN (his words, 2026-10-19 pm) — `OPEN, same cause as Q2`

> *"Launcher skipped again. Launcher must always be present at the start. It's literally a launcher that's where u
> start the game."*

**Measured:** nothing was ever dispatched for this. Q2 is still `OPEN`, so this is **the same known defect, not a
regression** — a saved disc is persisted (`DiscActivity.java:105`) and `launcher.c:21` bails on `argc > 1`. Fix = Q2.

---

## Q10 — TWO PRESSES TO SELECT (his words, 2026-10-19 pm) — `OPEN`

> *"I have to press on things twice to actually select. For example I press on course select I gotta press twice on the
> same button."*

**Measured — and it is not a lost tap.** The engine ships this as its own documented behaviour:
`video_settings.c:661` — *"Touch Menus: first tap previews, second tap confirms; drag lists and the GT Mode map."* The
first tap previews, the second commits. **The decision we owe him:** which screens keep the double tap (where the
preview is genuinely useful) and which should commit on one tap (a course-select row looks like a one-tap job).

---

## Q11 — STILL BAD WITH THE RENDER SCALE (his words, 2026-10-19 pm) — `OPEN, awaiting his screenshot`

> *"Runs terribly. Even with the mediatek fix. Please research why. Maybe it's a mediatek issue that must be fixed.
> Standby. I'll send u screenshot and then u research. If u need to add extra data for u in the debug overlay please
> add."*

**Two things to establish from his screenshot, in this order:**
1. **Did the knob get moved?** `graphics.android_render_scale` defaults to **0 = full**, so if he never set it the
   frame is unchanged by design — level 0 is a literal no-op (`if (android_scene_scale > 0)`).
2. **Even turned on, the knob is bounded.** It shrinks **only the 3D scene target** (`video_gl.c`, `#ifdef __ANDROID__`).
   The 2D/UI layer and the full-extent present blit stay at 2267×1080 — so the layout / AFBC / swapchain work is where
   the rest of the win has to come from. He should not expect this one knob to fix the frame.

**Extra overlay fields worth adding (he authorised this):** the actual **render-target size** beside the output size
(so a screenshot proves whether the scale took), **blit/present milliseconds** separately from the scene, and whether
**vsync/present mode** is on. Today's line shows fps/ms/worst/cpu/rss/gpu/load/draws/tris/cull/state/tex/size/platform —
none of which reveals the two numbers that decide this argument.

---

## MY SUGGESTIONS (mine, not his — nothing here is fired or promised)

Offered because he asked "what are u suggesting we add?". Each one exists to either *make a complaint reproducible*
or *close a door he has already walked into once*.

1. **S1 — A touch test page, and a pressed state on every control.** A panel in the Android settings that lights up
   each control as it is hit, plus a visible pressed state and a haptic tick. Turns Q5 ("sometimes they don't press")
   from a mystery into a number he can read off his own phone. Instrument first, exactly like G6.40 did for fps.
2. **S2 — Hit boxes bigger than the art** (~1.3×), neighbours allowed to overlap slightly. A finger is bigger than an
   eye; the drawn circles stay small and pretty.
3. **S3 — Safe-area inset for the gesture bar.** The lowest control box ends at y = .87 of the screen, which is where
   One UI puts the swipe bar. Inset the whole control layer off the real insets.
4. **S4 — Fewer, bigger targets.** The six cycle / gear buttons want to be one swipe or one rocker, not six dots.
5. **S5 — Per-device defaults.** `controls.touch_layout` already exists; a phone should open with a different default
   set than a handheld whose pad is connected.
6. **S6 — Show the pad status on screen.** One line: `Pad connected — touch hidden`. Today's confusion ("why don't I
   see the controls on my Odin?") cost a round trip; that line kills the whole class of it.
7. **S7 — Pause → Back to the studio.** Once the game is running there is no path back to the launcher room. This ties
   straight into Q2: the disc both skips the room *and* strands him in the game.
8. **S8 — One rule table for the exit button.** States (splash, movie, GT home, race, pause) → allowed / not allowed,
   in a single function, so Q7 becomes a table edit instead of a patch hunt.
9. **S9 — Cap the log.** `moonengine.log` is never truncated, and the Android side copies the whole file into
   `Download/MoonEngine/` every 3 seconds. A rotating cap keeps a long session from grinding storage in the background.
10. **S10 — `controller` should mean *recent pad input*, not *pad connected*.** A paired-but-idle Bluetooth pad must
    not silently delete a phone's only way to drive.

---

## INTERACTION RULES FOR EVERY DISPATCH

- **SCOPE WORDS (the captain's convention, 2026-10-19):** *"global feature" / "add this globally"* = build it for
  **every platform** (PC, Android, and every platform we add later) — never just the one he happens to be testing on.
  Otherwise say the platform out loud (*"Android-only"*) and every recipe states its scope in one line, so a dish can
  never quietly ship as Android-only when he asked for global, or the reverse.
- **BRANCH ORDER (the captain, 2026-10-19):** *"We both push first to main then make an experimental branch."*
  So: land the safe work on `main` and push it **first**, then cut the experiment from that **updated** `main` —
  never from a stale base. A branch is short-lived: rebase onto `main` and merge, or delete it.
- A dish is fired only on his go-ahead. Never restart a service or mutate state without his OK.
- **No full `ctest` suite** — it is a release gate only; a dish builds every target and runs `ctest -R` on what it touched.
- Commits author **`Or Golan <or024662@gmail.com>`**; **never push** — he gates every push.
- Never write his state files: `launcher.ini`, `video.ini`, `mods.ini`, `saves/`, `studio-rig.txt`.
- He is PC-less: the deliverable is an APK on the share + the log he can attach.
