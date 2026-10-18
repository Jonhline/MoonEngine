# THE WISHLIST — every idea, registered so it cannot drift

*This is the idea file. It came out of the recompilation era (`archive/vulcan4-recomp/docs/GOALS.md`
§G6) and is carried forward here **unchanged**, because the ideas did not retire with the method — and
because a good part of it is a set of promises made in public.*

**Where the ideas come from.** Two places, and the difference matters:

1. **The captain.** Entries marked "the captain's own" or with no provenance line are his, said in
   passing on a given date, and registered so a sentence in a chat does not become a memory.
2. **The community, through the captain's own post.** On 2026-10-01 he posted *"GT4 Recomp
   suggestions"* to **r/granturismo4** (thread `t3_1wurol8`) asking what this project should add. It
   drew **46 comments**. They were read in full, not skimmed, and the answers are recorded here **in
   the community's own words** — including the ones the captain answered *"Noted"* to in public.
   **A promise made in public is kept in writing here.**

**How to read it.** Every entry is a **goal, not a wish**, and `G6` was the phase name in the old
ledger — the numbering is kept so nobody has to relearn it. Two rules apply to all of it, without
exception:

- **Law 12 — everything here ships OFF by default.** `OFF` means the untouched 2005 behaviour,
  including the 2005 rules. A feature that ships enabled has changed somebody's game without asking.
- **Nothing here is scheduled.** Every line is gated on Stage 1 being honest first: a tyre-wear slider
  on a broken screen is a lie waiting to happen.

⚠️ **Provenance note:** the mechanism notes under each entry were written for the recompilation (they
name recompiler hooks, VU1 microcode, our own GS layer, the SpecDB tables we had opened). The engine
changed in 2026-10-13; where an entry names the old method, read it as "the equivalent seam in the
engine" — the *idea*, the fence and the honesty notes are the parts that survived. The old method's
detail is archived with the old work.

🆕 **Engine-era entries** (`G6.20`–`G6.22`, marked with 🆕 and dated 2026-10-13) were written against
the **forked engine**, with `file:line` citations into the code that exists today, so they do not need
that translation. The captain's brainstorm of that day — *"we can finally put our ideas on making the
ultimate racing sim"* — is registered there: the logger, the race engineer, and the honest race.

---


## G6 — MODS & NEW CONTENT *(the captain's plans, registered so they cannot drift)*

> **Why this phase exists:** the captain, 2026-09-30: *"God I have so much plans for this recomp."*
> Every idea below is a goal, not a wish. None of it starts until the game runs — but writing it down
> now is how we make sure the runtime we are building can actually carry it.

### ⬜ G6.0 — **NEW CARS** *(the captain's first big question: "can we add new cars?")*
- **Answer: yes — and a recomp makes it easier than modding the original.** We own the loader, so we
  are not bound to GT4's own model format: **we can add a reader for any format we choose** (GLB,
  FBX, a converted Assetto Corsa/Forza model). GT4's own car format was never publicly cracked; we
  do not have to crack it to add a car.
- **What a new car actually needs — five pieces:**
  1. a **SpecDB entry** (the game's own database): CarID, name, maker, power, weight, dimensions,
     drivetrain, gearbox, tyre sizes — copied in *shape* from a real car
  2. a **model** carrying the nodes the game expects (`body` + four wheels), because the engine
     steers, spins and animates them
  3. **physics data in GT4's own fields** — ⚠️ GT4's *real* physics runs natively here, so it will
     judge the car: good data feels authentic, lazy data drives like a trolley
  4. an **engine sound** assignment (a table entry, or borrow an existing car's sample)
  5. **shop/menu integration** if it should be buyable/winnable: price, thumbnail, event eligibility
- **Honest limits:** nothing here starts before the game runs; new car IDs must be *additive* (never
  reuse an existing ID — saves refer to them).
- **⚠️ HONESTY RULE FOR THIS GOAL:** the model path is ours to choose, but the *car itself* has to
  satisfy GT4's own systems. A pretty mesh with wrong wheelbase/tread will be positioned wrong and
  drive wrong. The data is the hard half, not the model.
- **Community weight:** "add your own car to GT4" is the thing players have wanted for twenty years
  — a bigger draw than the native port itself.
  > **The captain, 2026-09-30:** *"This is like the most requested feature EVER. Bc in the original
  > game u cannot add modded cars."* — and he is right, for a concrete technical reason:
- **THE CAPTAIN ELEVATED IT TO A RELEASE GATE, 2026-10-15:** *"Custom cars and maps should be upon release
  caine. We have to work on it as soon as possible."* → **release requirement**, not a post-launch wish.
  The reason is the one he already gave: adding a car is the *only* thing the original GT4 can never do, so
  it is the whole difference between a port and a platform. Registered as the car half of the release gate;
  the map half is `G6.33`. What "as soon as possible" honestly means is in each entry — the *survey* starts
  now, the end-to-end feature cannot exist before the game runs (G4) and before `G6.0a` names the ceilings.

### ⬜ G6.0a — **SWAP vs ADD:** find every place GT4 assumes a fixed car count
- **Why you cannot add cars to the original:** on real hardware or an emulator, a modder has only
  bytes — no code. GT4's cars live in a **binary database** (SpecDB) plus models inside `GT4.VOL`,
  and the game's menus, shop lists, prize tables and event eligibility almost certainly index cars
  through **fixed-size structures**. A new CarID has nowhere to live. So every existing GT4 car mod is
  a **SWAP** — replace an existing car's model/specs — and a swap costs you a car. **Add has been
  impossible**, not for lack of models, but because the engine's tables do not grow.
- **Why a recomp changes it:** we have the engine's own logic as editable C++, we control the model
  loader, and we have a hook layer. Growing a table and teaching a lookup to accept the new range is
  *work* — but it is work on our side of the line, not a wall.
- **THE ACTUAL BLOCKER TO FIND (this is the dish):** every place the game assumes "there are N cars".
  Search the recompiled C++ and the data structures for: fixed arrays sized by car count, index
  tables with a hardcoded upper bound, shop/prize/menu lists built once at init, model-path
  construction (`car/<maker>/<id>` schemes), and any loop bounded by a constant that matches the
  known car total. Produce a ranked list: *what breaks first when we add car N+1.*
- **DONE WHEN:** `docs/NEW-CARS.md` names those sites with addresses/file+line evidence, ranked by
  what blocks an addition first — the survey that turns "can we add cars?" into a work plan.
- **⚠️ CORRECTED BY MEASUREMENT, 2026-10-15 — "add has been impossible" is too strong, and the proof is a
  disc we already own.** Spec II v1.10 measured against retail v2.00, same tool, same box:
  `GENERIC_CAR` **722 → 744 rows** (ids now reaching **1106**, above retail's 1077), `COURSE` **110 → 114**,
  `ALLOW_ENTRY` **58 → 73**, `/crs` 221 → 223 — and its specdb directory is *renamed*
  (`GT4_US2560` → `GT4_PREMIUM_US2560`). So the scene **has** grown these tables on the original engine.
  What it cost them is the answer to "why is it rare": a **patched executable** (MIPS machine code, not data),
  a repacked container, and an xDelta diff **locked to one exact disc build** — which is why their release
  notes read *"requires this version"*. A plain SWAP needs none of that, which is why twenty years of GT4
  mods are swaps. **Our fork changes the PRICE, not the possibility:** the loader is C we own, so adding is
  a row plus a file — no MIPS patching, and a disc profile instead of a version lock.
- **STATUS:** ⬜ registers now, executes after playability (G4). It costs nothing to know where the
  ceiling is, and knowing it shapes how we design the loader **today** — which is why it is written
  down before the game runs.

### ⬜ G6.1 — **CUSTOM MUSIC** *(the captain's demand, 2026-09-30)*
- **The good news, and the same trick as cars:** we own the audio path, so we do **not** have to
  crack GT4's music format to add our own. Two layers, and they are independent:
  1. **Play the original** — decode from the user's own disc (see G4.3: the audio bank lives inside
     `GT4.VOL`; the survey found no SGDP and no ADPCM path yet).
  2. **Play custom** — our own music loader: drop files in a folder (OGG/MP3), a mapping table says
     which *game state* each file covers (main menu, race, replay, results), and a **hook point**
     intercepts the game's "play BGM id" call and plays ours instead.
- **The hook is the whole point.** Somewhere the game asks to start background music — a script call,
  a syscall, a table lookup. Custom music = intercept that request and satisfy it from our side.
  That is the same pattern as `G6.0` (own the loader) and it needs the same discipline: find the
  request site, name it, hook it.
- **Honest split:** *background music* is the easy 90% — files, a table, a hook. **Dynamic audio**
  (engine notes, tyre squeal, impacts) is a different problem: those are banks with pitch/loop
  parameters tied to car state, and they should keep coming from the disc for authenticity.
- **Capability note:** a decoder dependency will be needed (OGG/Vorbis or similar — free, GPL-
  compatible). Name it before adding it; do not install anything silently.
- **STATUS:** ⬜ post-playability (G4), same lane as the rest of the audio work (G4.3).

### ⬜ G6.2 — **THE LAP CHIME** *(the captain's own touch, 2026-09-30)*
- **The captain, 2026-09-30:** *"u know how in gt5 when u finish a lap and it shows u ur time it makes a
  small sound? i wanna add that."*
- **What it is:** the instant a completed lap's time lands on screen, one short cue sounds — a small,
  unmistakable "that lap is now history" blip. In GT4 the same moment passes **silently**.
- **Why a recomp can do this and an emulator cannot:** the lap-completion path *is our code now*. On an
  emulator the lap timer is a black box behind an interpreter; here the function that commits a lap
  time is a C++ function we can name, read, and wrap. This is the smallest possible demonstration of
  what `G10` (ownership) buys — a one-line hook on a moment the game already has.
- **Mechanism, in order:**
  1. **Audio must exist first.** A cue needs an output path; today none is wired (see G4.3 — the audio
     bank inside `GT4.VOL` is unwalked, no ADPCM path). Nothing here starts before that.
  2. **Find the moment, and prefer the *logic* site over the *draw* site.** Two candidates: the
     HUD/lap-counter update (easy to spot, but it is a *symptom*), and the race-logic site where the
     lap timer's value is committed and reset (the *cause*). Hook the cause: a drawn number can be
     redrawn, a committed lap happens once.
  3. **The sound itself — and the fence matters here.** Ship **code, not assets** (G7.0):
     - **shippable:** a cue we synthesise in our own audio code — two partials and an envelope, no
       sample file, no third-party audio, nothing to license. This is what the repository can carry.
     - **the exact GT5 chime:** that is Sony's recorded asset. It never enters the repo; it is the
       same pattern as `G6.1` — **ship the loader and the hook, the file is the user's own.** Drop it
       in a folder, a mapping entry names it, and the hook plays it.
- **Honest limits:** awaits a *running* race (G4) and a live audio path; the trigger must be the game's
  real lap event, never a reimplementation of lap detection (a fake trigger that fires on the wrong
  frame is exactly the kind of plausible lie Law 2 forbids).
- **DONE WHEN:** a driven lap completes and a cue is audible, with the trigger site named (function +
  address) and the cue synthesised on our side — plus a documented path for the captain to supply his
  own sample.
- **Why it is registered now, before the game runs:** it is the cheapest end-to-end proof of the whole
  promise of this project — *the game is yours, so the game can sound like you want it to.* It costs
  one hook and one envelope, and it is the kind of thing that makes a recomp feel like an instrument
  rather than a port.
- **STATUS:** ⬜ post-playability, same lane as G4.3/G6.1. Registry entry only — do not dispatch.

### ⬜ G6.3 — **THE MERCY SKIP** *(skip a driving-school lesson you have failed too many times — and get mocked for it, 2026-09-30)*
- **The captain, 2026-09-30:** *"the abilty to skip a lesson in a course at the drivign school if u fail
  it too much. and maybe mock the player for doing it."*
- **What it is, and why it is a genuinely good feature:** GT4's licence school and Driving Missions can
  hard-stop a player — one lesson, one gold-less wall, forever. Every other sim of the era had an
  escape hatch; GT4 has none. **This is an accessibility fix first** and a joke second: the game should
  not be able to end a player's evening.
- **Why a recomp can do it:** the lesson's result path is our C++ now. GT4 *already* has the shape of
  this feature — the retry flow — so we are not inventing a system, we are **extending one the game
  already runs**: count consecutive failures per lesson, and when the count crosses a threshold, offer a
  third option beside `RETRY` / `EXIT`.
- **Mechanism, in order:**
  1. **Find the lesson-outcome site:** where the game stores the result of a licence test / mission
     attempt (result code, the retry menu's state, the lesson id). Prefer the *logic* commit site over
     the results screen (same rule as `G6.2` — hook the cause, not the redraw).
  2. **Count failures in OUR state, not the save.** A per-lesson counter kept at runtime keeps this
     save-safe by construction. Persisting it is a *separate* decision, and it must be additive —
     never a rewrite of the game's own save fields (`G6.0a` is the same class of problem: the game's
     tables and saves are fixed-shape).
  3. **The skip must be honest to the game's own model.** Two ways, and we pick after reading the code:
     either complete the lesson in GT4's own completion fields, or unlock the next lesson without
     claiming a medal. **Never counterfeit a gold** — a faked medal would poison the licence
     progression, prize cars and event eligibility downstream. The skip says *"moved on"*, not
     *"you earned this"*.
  4. **The mock lines are ours, so they are shippable** (strings we write, in our code — no game text
     needed, no asset, `G7.0` clean). They should mock the *situation*, in the game's own dry voice —
     a licence examiner would be rude about it, not a chatbot. Punch down at the player's pride for
     skipping, never at the player for struggling.
- **Honest limits:** needs playability (G4) and the licence flow actually reachable in our runtime;
  the threshold and the wording are the captain's calls — these are his feature, so they get built with
  a knob, not baked in.
- **DONE WHEN:** after N consecutive failures on one lesson the game offers a skip, taking it advances
  the licence progression without a counterfeit medal, and the player is insulted appropriately — with
  the result-site named (function + address) so the hook is real and not a heuristic on frames.
- **Why it is registered now:** it is the smallest *logic* sibling of `G6.2` — a hook on a moment the
  game already has — and together they are the demo reel for `G10`: a sound that never existed and a
  button that never existed, both in a twenty-year-old game, neither one a hack.
- **STATUS:** ⬜ post-playability, same lane as G4/G6.1/G6.2. Registry entry only — do not dispatch.

### ⬜ G6.4 — **THE SHOWROOM PREVIEW** *(show me the car, not just its name, 2026-09-30)*
- **The captain, 2026-09-30, looking at his own used-car lot with Cr. 177,904:** *"showing a preview of the
  cars. bc the game only shows u names. i dont know every cars name by memory lol. ion think anyone does.
  u hnave to press on it. let it load then see the car only to go back. a waste of time!"*
- **What it is:** every row in a car list — **used lot, dealership, garage, tune shop** — carries a small
  thumbnail of the actual car, so a name is not the only way to know what you are looking at.
- **Why this is possible here and impossible in an emulator:** the list is drawn by code we now own, and
  the game **already knows how to load and show a car** — that is precisely what the select-and-wait does
  today. The fix is not new capability, it is **calling an existing path at a better time**. An emulator
  cannot reorder a menu it does not own; we can.
- **Mechanism, in order:**
  1. **Depends on car rendering existing at all** (`G3`/`G4`): if we cannot load and draw a car, there is
     nothing to preview. This ships *after* a car renders on its own.
  2. **Offscreen pass, not a new renderer.** Render the model into a small target (128×96 or so), then
     composite it as a textured quad in the row — the GS layer already does textured quads.
  3. **It must be async or cached — this is the whole engineering risk.** GT4's menus stream from the
     disc, and a synchronous model load per row would turn a snappy list into a slideshow. Correct shape:
     draw the row immediately, load the model in the background, and **cache the thumbnail in the user's
     own data directory** so the first visit pays and every later visit is instant.
  4. **One hook, four screens.** The used lot, the dealership, the garage and the tune shop draw their
     lists the same way — build it once and wire it to all four, not four times.
- **Fence (G7.0):** thumbnails are **generated at runtime from the user's own disc and cached locally**.
  They are rendered game content — same class as the disc — so they are **never shipped and never
  committed**. We ship the code that makes them.
- **Honest limits:** needs the car path (`G3`/`G4`) first; the thumbnail cache costs VRAM and disk (small,
  but named); if a list's row geometry is baked into the data rather than drawn, the hook goes on the
  **draw**, not on the data — the layout is not ours to rewrite.
- **DONE WHEN:** opening the used-car lot shows recognizable thumbnails of the actual cars, the list does
  not hitch while they appear, and they survive a restart (cached) — checked against the captain's own
  eyes, in the field.
- **Why it belongs in G6:** it is the same deal as the chime and the skip — a small, obvious quality of
  life the original could not have and the player should have had. Accessibility-in-the-shape-of-courtesy.
- **STATUS:** ⬜ post-playability, same lane as G4/G6.1–G6.3. Registry entry only — do not dispatch.

### ⬜ G6.5 — **THE ENGINE SWAP** *(put an engine somewhere it has no business being, 2026-09-30)*
- **The captain, 2026-09-30:** *"can u do engine swaps?"* → then, on remembering GT7's version: *"i swear there was
  a gran turismo game where u could engine swap. i literaly saw an animation of that."* → **"engine swaps is
  totally something i wanna add."** *(He was right: it is GT7, in GT Auto → Car Maintenance & Service →
  Engine Swaps, unlocked at Collector Level 50. GT4 never had it. Our version would be a first for GT4.)*
- **What it is:** take the engine out of car A and make car B run it — power, torque curve, redline,
  induction, gearing **and its own sound**, the way `G6.2` demands a real cause and not a costume.
- **The design spec is already proven, and it came free with the captain's memory of GT7:** a swapped car is
  reported to get a **new sound file and new running characteristics**, not just new numbers. That is the
  whole shape of the feature stated by the people who built it: **engine = parameter block + sound identity.**
- **What an engine actually is, in a game we own:** a **parameter block** (power/torque curve, redline,
  induction type, gear ratios, weight effect) + an **engine sound ID** + a drivetrain linkage. A swap is a
  transplant of that block between two cars and a re-pointing of the sound ID.
- **Mechanism, in order — and the hard part is not the transplant, it is the callers:**
  1. **Find every place the game asks "what engine is this?"** — tach/HUD, gearbox, engine audio, physics
     tables, AI performance estimates, the tune shop's own menus. The transplant is easy; **the call sites
     are the work**, and this is exactly the `G10` ownership bill coming due.
  2. **Start with same-family swaps** (turbo → turbo, NA → NA, similar displacement) so the physics tables
     are a reference change, not an invention. A rotary into a kei is a **later stage**, not the first dish.
  3. **Sound is part of the feature, not polish.** A swapped engine with the old engine's voice will read as
     broken, and the captain will feel it before he can name it (`TASTE-TEST` check #7).
  4. **Reversible and save-safe by construction.** Record the swap so it can be undone; never rewrite the
     game's own save fields (`G6.0a`/`G6.3` carry the same rule).
- **Fence (`G7.0`):** engine sounds come from the user's own disc, or from audio we synthesise ourselves. We
  ship the mechanism, never the content — same fence as the disc.
- **Honest limits:** needs `G10`; the torque curve is baked per car so "plausible" is the ceiling, **never
  parity with the factory engine**; a swap can break the game's own AI/event assumptions (BoP-ish effects) and
  that is a finding to log, not to hide; and this is **not** GT7's feature — it is our own, built from
  scratch, and must not be described as a port of it.
- **Why it belongs in G6:** it is the funniest possible proof that we own this game. GT4 never had swaps, and
  a Honda **TODAY** with something absurd under the bonnet is exactly the kind of thing the community will
  screenshot. It is also the natural partner of `G11`: the bridge brings new *cars* in, this changes what is
  *inside* the ones already there.
- **DONE WHEN:** two cars swap engines in-game, both still drive and **sound** recognisably like their new
  engine, the swap can be undone, and the save survives — with the engine-parameter path named (structure +
  the call sites found), not hand-waved.
- **STATUS:** ⬜ post-playability. Needs `G10` first. Registry entry only — do not dispatch.

### ⬜ G6.6 — **THE PIT WALL** *(tune the car without leaving the track, 2026-09-30)*
- **The captain, 2026-09-30:** *"would also love an option for u to tune a car mid practice race. that way u can test
  the car instantly after u change its settings"* — i.e. change LSD / suspension / gearing / tyres / aero and
  **feel it on the very next corner**, instead of: leave the track → menus → tune → reload the track → drive.
- **Why it is the most *real* feature on this list:** this is what a race engineer actually does. Real racing has
  a **pit wall**: you come in, the setup changes, you go back out and find out if it was better. GT4 makes you
  fake the pit stop by *quitting the session entirely*, which destroys the comparison — you lose the reference
  in your hands between the two laps.
- **Why it is possible here and impossible in an emulator:** the setup menu and the practice session are both
  code we own. The physics reads its parameters every frame — so the feature is **re-reading them at a defined
  moment**, not inventing a system. An emulator cannot add a menu to a session it does not control.
- **Mechanism, in order — and the safety rule is the whole design:**
  1. **Never hot-swap a live physics state.** Changing spring rates mid-corner would produce a state the game's
     own integrator has no reason to be valid in. The correct shape is a **pit stop**: car stationary, clock
     stopped, setup applied at a clean state, you resume where you are — same track, same reference.
  2. **Mark the session, not just the lap.** Live tuning invalidates times as comparisons *unless* the change
     point is recorded. Log the change so a tuned run can never masquerade as a clean one.
  3. **v1 is practice / test only.** This must **not** reach a race, a championship, or a licence test — the
     moment it does, every time in the game becomes negotiable. That is a cheat, not a quality of life fix.
     *(This is the same fence `G6.3` carries: never counterfeit progress.)*
  4. **The setup screen it opens is the game's own** — same sliders, same values, same effects. We are moving
     *when* it can be opened, not what it does.
- **Honest limits:** needs playability (`G4`) plus the tune/setup path working; applying a setup may require a
  physics re-init that is not instant (measure it — **if a "pit stop" costs five seconds, that is the number we
  print**, not a claim of "instant"); and if the game caches derived values per session, that cache has to be
  invalidated deliberately rather than hoped about.
- **DONE WHEN:** in a practice session the setup menu opens in place, a changed setting is **felt on the next
  corner**, the change is recorded, and no race or licence time can be produced with live tuning — with the
  apply-point (function + where it lands) named in the entry.
- **Why it belongs in G6:** together with `G6.4` this is the pair that makes GT4 a better **test bench** than it
  ever was a game — thumbnails so you know what you are looking at, and a pit wall so you know what you just
  changed. Both are frictions the original had no good reason to keep.
- **STATUS:** ⬜ post-playability, same lane as `G4`/`G6.1`–`G6.5`. Registry entry only — do not dispatch.

### ⬜ G6.7 — **THE RUSH** *(make the speed feel like speed, 2026-10-01)*
- **The captain, 2026-10-01:** *"also. one more request for the recomp. making gt4 make u feel like ur going fast"*
- **What it is:** GT4 at 60 fps, 4:3-era camera framing and a near-static chase cam can read *slow* even at
  300 km/h — the speedometer says fast, the eyes say gentle. THE RUSH is a set of **opt-in, renderer-side**
  changes whose only job is to close that gap: camera field of view, camera dynamics, speed-scaled shake,
  motion blur, and a wind layer in the audio. It changes **what it feels like**, never what the game *is*.
- **Why the original couldn't:** it was bound to a fixed PS2 frame budget with no post-processing headroom, a
  fixed output resolution and a camera tuned for *readability on a CRT*, not for sensation. Almost everything
  below is either a number the guest already publishes (which we can rescale) or a pass over a finished frame
  (which we own, because we supply the GS).
- **Mechanism, in order — cheapest and most honest first:**
  1. **Find the camera state as data, first.** The guest publishes an eye/target/roll triple and some FOV or
     projection constant every frame. **A dish that cannot name that address does not get to write a knob** —
     the whole feature depends on owning that one structure, and reading it is also how we prove we have not
     disturbed it.
  2. **FOV + camera placement** (the biggest single win, and it is arithmetic): widen the projection, drop the
     eye point a little and move it in. Peripheral motion is what sells speed; the difference should be proven
     with a side-by-side capture pair at the same car, same track, same speed.
  3. **Camera dynamics instead of a rigid mount:** integrate our camera toward the guest's target with a small
     spring/damper so the camera *trails under acceleration and leads under braking*. This is the single change
     that turns "a car in a frame" into "something being thrown at you" — and it is renderer-side only, so the
     guest's own state stays untouched.
  4. **Post-process on the finished frame** (`G2`'s own output stage): radial blur scaled by speed, a shallow
     speed-scaled vignette, and a light chromatic pinch at the edges. **Frame-rate independent by construction**
     — a blur whose strength is a per-second value, not a per-frame one — and it must be honest about cost:
     print the measured frame time with the pass on and off.
  5. **Shake, deliberately conservative:** speed- and kerb-scaled, amplitude-capped, and **never** in a way that
     moves the driving line more than the player's own inputs do.
  6. **Audio wind layer** *(depends on `G4.3`)*: wind and tyre roar scaled by road speed, because a large part
     of "fast" is heard, not seen. Same rule as `G6.2`: a real cause-driven layer, never a costume.
  7. **A high-refresh mode** as the cheapest trick of all: the renderer is ours, so present at 120/144 Hz with
     the guest still running at its own tick. This costs almost nothing to try and is often the single most
     convincing speed cue on a modern panel.
- **The fence (non-negotiable, and it is this project's whole identity):** every one of these is **opt-in with
  `OFF = the untouched original frame`**. A purist must be able to switch THE RUSH off and get exactly the
  picture the 2005 hardware produced — this is a *view*, not a remaster, and it is never allowed to be
  **on by default in a way the player cannot audit**. Nothing here touches physics, times, AI, saves or
  progression: `G6.6`'s rule (never counterfeit progress) applies in its strongest form, because this feature
  is pure appearance.
- **Also non-negotiable, for a different reason:** shake and blur are the two effects that make some players
  physically ill. Each is a **separate** toggle with a real `0`, and `0` must mean *zero*, not "a subtle
  amount". A preset nobody can fully turn off is a bug, not a feature.
- **Honest limits:** the FOV and camera-placement work is only possible if the camera state is a structure we
  can read (step 1 — if it turns out to be computed in VU1 microcode, this goal inherits `G3.1`'s schedule);
  wider FOV costs precision on tight circuits like Costa di Amalfi, so the default must stay the game's own;
  motion blur needs GPU headroom we have not measured yet on the target machines, and on a Titan-class card
  that is fine while on an integrated GPU it may not be — **the number gets printed either way**; camera
  trailing can make a *precise* driver slower, which is why it is a slider and not a religion; and the
  audio layer cannot be honestly built before the audio path (`G4.3`) exists at all.
- **DONE WHEN:** on a real track, at a real speed, `OFF` produces a frame indistinguishable from the untouched
  renderer while `RUSH` produces a capture pair a stranger describes as *faster* without being told which is
  which; every effect has a real zero; frame cost is measured and printed; and the camera/FOV address we
  rescale is named in the entry, not guessed at.
- **Why it belongs in G6:** `G6.4` made the game tell you **what** you are driving, `G6.6` made it tell you
  **what you changed** — this one makes it tell you **how fast you are going**. Three frictions the original
  had no good reason to keep: not knowing your car, not knowing your setup, and not feeling your speed.
- **STATUS:** ⬜ post-playability, same lane as `G4`/`G6.1`–`G6.6`. Registry entry only — do not dispatch.

### ⬜ G6.8 — **THE MARATHON** *(the races that were longer than a day, 2026-10-01)*
- **The captain, 2026-10-01:** *"what are like the longest endurance races in history? lets add more!"*
- **What it is:** an **endurance event pack** built out of races that really happened — real names, real durations,
  real distances, taken from the sport's own history rather than invented for difficulty. GT4's 24-hour events
  are the ceiling the disc shipped with; the sport's ceiling is higher, and the number already exists as a number
  in the game's own event structure.
- **The real history this draws from (verified, not remembered):**
  - **Marathon de la Route, Nürburgring 1965–1971** — the longest motor race ever held. It began at **82 hours**
    and grew every year, peaking at **96 hours / ~11,120 km in 1971** — four days, on the Nordschleife, with a
    team of drivers. This is the north star of the pack and the one the captain will actually want to attempt.
  - The **24-hour family** GT4 does not carry: **Spa 24 Hours** (1924–), **Daytona 24 Hours** (1966–), and the
    delightful outlier **25 Hours of Thunderhill** — the only race on earth that is *one hour longer than a day*.
  - Already on the disc: **Le Mans (Sarthe 24h I/II)** and **Nürburgring 24h** — those stay untouched; they are
    the reference, not the target.
  - **Bol d'Or (1922–)** is the same idea on two wheels and is deliberately **not** in scope: no bikes, no entry.
- **Mechanism, and it is deliberately small:** GT4's endurance events are **duration-defined** — the 24h events
  are a time value in the game's own event data, not a bespoke code path. So v1 is **our event table with longer
  durations** on circuits the disc already has (Nordschleife, Sarthe, Tsukuba, Motegi, Fuji), reading the same
  race loop, the same pit logic and the same tyre wear the 24h events already use. **We add a row and change a
  number.** Anything more than that is a different goal.
- **Honest limits, and they are the interesting part:**
  - **GT4 has no fuel model** — endurance strategy on the disc is *tyres only*. A real 96-hour race was a fuel
    and reliability exercise; ours inherits a simpler game and must say so instead of pretending.
  - **No driver-fatigue model, and B-spec already is the relief driver** — the honest framing is that B-spec *is*
    the multi-driver team rule, which is why this pack is B-spec-shaped.
  - **Time-of-day and weather do not change in GT4's enduros** (a documented limitation of the disc's own 24h
    events) — a 96-hour race that never sees dusk is a lie the original already tells, and we inherit it until
    it is fixed on purpose.
  - **96 hours at 3× B-spec is still 32 real hours**, so a **suspend/save** path is a *precondition* for this
    pack, not a nicety. On the emulator the captain already has save states; a native build needs the equivalent
    (same class as `G6.7`'s opt-in features), and it must be **off by default** like everything else (law 12).
  - **Spa, Daytona, Thunderhill and Bathurst are not circuits on this disc** — those events are the `G6.0`
    problem (add content), so they are named in the pack but **not promised in v1**.
- **The fence:** off by default (law 12); prize money scaled honestly rather than made absurd — a longer race may
  pay more, but it must not break the game's own economy, and no payout may ever imply a result the player did
  not actually drive (`G6.3`'s rule, in its strictest form); the event uses the game's own race loop and leaves
  the existing 24h events byte-for-byte alone.
- **DONE WHEN:** a **≥72-hour** event appears only when the pack is enabled, runs end-to-end on the game's own
  race loop with working pits and tyres, can be **suspended and resumed**, pays a sane prize, prints its real
  duration/lap/tyre numbers in the entry, and leaves a vanilla 24h event provably unchanged alongside it.
- **Why it belongs in G6:** because it is the one kind of content a recomp can extend **without inventing a
  single number** — the durations came from the real sport, the circuits are already on the disc, and the event
  structure is the game's own. It is the captain's completionism pointed at the only wall that was ever real:
  the clock.
- **STATUS:** ⬜ post-playability, same lane as `G4`/`G6.1`–`G6.7`. Registry entry only — do not dispatch.

### ⬜ G6.9 — **THE FEEL OF THE WHEEL** *(deeper force feedback, 2026-10-01 — asked for in public)*
- **Where it came from:** the captain's first public post as **u/Professional-Bar6778** —
  *"GT4 Recomp suggestions"* in r/granturismo4 (thread id `1wurol8`). A reader asked for *"more (in-depth)
  force feedback settings/support"*, and the captain answered *"Noted."* **A promise was made in public, so
  this entry exists to keep it.**
- **What it is:** GT4's rumble is computed by the **guest's own pad code** and read out by the runtime we
  supply. So this is not a new effect we invent — it is **exposing and shaping a signal the game already
  produces**, the same shape as `G6.7`'s camera work: renderer/IO-side only, the guest's state untouched.
- **Mechanism, in order:**
  1. **Measure before touching.** Log what the guest actually publishes per frame (the pad port's motor
     levels, or the buffer it fills), with a real wheel attached. **A dish that cannot name where the FFB
     numbers come from does not get to write a slider** — the same rule `G6.7` uses for the camera state.
  2. **Depth.** GT4 has a small fixed set of effects (kerb, gear, slip, off-track). The honest first win is
     per-effect depth plus a master curve, because the raw signal already exists.
  3. **A curve, not a cheat:** a low/high deadband and an overall intensity multiplier that maps the
     guest's range onto a modern wheel's. **`0` must mean zero motors, not "subtle".**
  4. **Only after that:** per-car and per-surface shaping, if the guest's data supports it.
- **🆕 Engine-era addition (2026-10-13):** there are now **two** possible sources, and they should be
  measured against each other rather than assumed. (a) Shape the guest's published rumble — the original
  signal, the safer claim. (b) **Compute the force ourselves** from the tyre model: the physics here is
  new C that we own (`phys.h:2`), so self-aligning torque and slip-based force are available as numbers,
  which is what modern wheels expect. (b) can feel better and is *ours*; (a) is what the game intended.
  Make it a visible choice, default to (a), and never let (b) lie about grip.
- **The fence:** off by default (law 12) — **a recomp that shakes your wheel without being asked is a
  worse recomp**; it never lies about grip (no vibration invented where there is no slip event, ever); and
  it must be a **real** force curve, not a cosmetic effect with a motor attached.
- **Honest limits:** depends on `G4.1` (input) existing at all, and on the guest actually publishing motor
  levels — **if GT4 computes rumble in VU1 microcode or a driver we do not yet own, this inherits
  `G3.1`'s schedule**; some wheels need a vendor SDK we would have to ship separately; and a modern direct
  -drive wheel is a genuinely different device from a PS2 DualShock, so an honest "this is what the game
  intended" is the ceiling, not "this feels like a modern sim".
- **DONE WHEN:** a connected wheel produces the game's own rumble with a real `0`, per-effect depth and a
  curve, on a real kerb and a real slip, with the guest's published values named in the entry — and with the
  feature switched off the wheel is completely silent.
- **STATUS:** ⬜ post-playability, same lane as `G4.1` then `G6`. Registry entry only — do not dispatch.

### ⬜ G6.10 — **MORE THAN SIX** *(bigger fields, 2026-10-01 — asked for in public)*
- **Where it came from:** same thread, first reply of the whole post: *"Not sure if it'll be possible but I
  would definitely love races with more than 6 cars"* — and the captain's public reply was *"Noted, i want
  that too."*
- **What it is:** GT4's grid is capped at six entries per race, in the **event data**, not in some hardcoded
  array we have to fight. So this is the same shape as `G6.8`: a **number in a table we supply**.
- **Mechanism, in order:** 1. name the field that caps the entry list (event entry record, or the AI
  opponent list the event builds) — **no knob before that is located**; 2. raise the cap and prove 8 cars
  load, render and race without touching the physics integrator; 3. **only then** consider AI skill/density,
  because more cars at the same AI level is a wall of cars, not a better race.
- **The fence:** off by default (law 12); the cap is raised in **our** event table only, never by editing the
  game's own save; a field size that must be reallocated (car array, collision slots) is a **real memory
  limit and must be measured, not assumed** — if the runtime has a fixed pool of car slots, "more than six"
  is a pool-size goal, not a menu-number goal.
- **Honest limits:** six is not arbitrary — it is what the PS2 could load, draw and shadow in one frame.
  More cars means more draw calls (our GS is single-threaded today), more AI, more audio voices, and a
  **frame-time measurement** on all three before this is claimed. A cap raise that drops the frame rate is
  not the feature; the feature is more cars **at the same frame rate**.
- **DONE WHEN:** an 8-car (and then 12) field runs a full race on our renderer with **measured** frame time
  printed and unchanged within the stated budget, and a vanilla 6-car event is provably identical alongside
  it.
- **STATUS:** ⬜ post-playability. Registry entry only — do not dispatch.

### ⬜ G6.11 — **VR** *(the wish the captain is unsure about — registered so it is not lost, not promised)*
- **Where it came from:** the same thread, asked for alongside force feedback. The captain's answer in
  public was *"Noted"* — and privately, on the same day: *"vr im not sure ill make but sure."*
- **So this entry is deliberately weaker than its siblings and says so up front: this is a WISH, not a
  commitment.** It is registered because the project already refuses to forget what the captain asked for
  (see `G6.3`, `G6.5`, `G6.6` — all of which exist because he said a sentence in passing) and because the
  one useful thing to do about an uncertain promise is to say out loud what it would cost.
- **What it would actually take, honestly:** VR is the **largest** renderer change in the registry, not a
  small one. Stereo rendering is easy. Everything VR implies is not: head tracking needs the camera state
  `G6.7` must already own; **latency** is the whole game and would need the guest's tick decoupled from the
  present; comfort (fixed-origin option, no forced camera motion) is a design decision, not a flag; and
  **it depends on `G2` (a picture) and `G3.1` (the VU1) existing at all** — there is nothing to put in a
  headset until a car and a track render.
- **The honest position, stated plainly:** if `G6.7` happens, VR is *possible*. If it does not, VR is a
  rewrite of the renderer for a game that does not render yet. **Therefore: do not let VR set the order of
  anything.** It is last in this list, after the picture, the 3D and a drivable race.
- **The fence, if it ever happens:** off by default (law 12); `OFF` must be exactly the flat renderer;
  **a motion-sickness disclaimer and a real "reduce motion" toggle are requirements, not extras**; and no
  tracked-camera assumption smuggled into the core renderer — VR must be a view mode over an unchanged
  scene, exactly like `G6.7`.
- **STATUS:** ⬜ **committed to nothing.** Registered at the captain's request so the answer exists on
  paper. Not scheduled, not promised, not to be dispatched. Revisit only when `G6.7` has proven the camera
  state can be read.

### ⬜ G6.12 — **THE POLE AND THE PIT WALL OF THE PAST** *(the race-rules wish list, 2026-10-01 — one comment, 13 items)*
- **Where it came from:** u/FantozziUgo, one hour into the thread, opening *"Dunno if these are even
  doable but just thinking out loud:"* and then listing thirteen things. Boss's public reply:
  *"all of these are planned and engine swaps are top priority out of all of these"* — so this entry
  exists to make sure that public promise is not a memory. It is a **list, not a feature**: it is
  deliberately kept as one block so nothing silently disappears.
- **The thirteen, verbatim, grouped by what they actually are:**
  - *race format* — start a race with less than full fuel; qualifying for single races; track limits
    for quali and races; contact penalties on **all** races, not only Special Conditions; timing
    sectors (F1-style) on every track
  - *setup freedom* — a clutch you must press to change gear (dogleg gets none — clutch on downshift
    only); rewindable replays
  - *camera* — a POV driver camera even with no cockpit
  - *the car* — repainting; racing mods like GT2; **engine swaps**; oil change for the Polyphony
    Formula car; mods for special vehicles (*"super softs and NOS on the Benz motorwagen"*)
  - *the world* — wet surface on **all** tracks, not just Tsukuba
  - and the one that overlaps an existing entry: **+1 to races with more cars** → this is `G6.10`
- **What is already registered here, so the public reply was honest:** engine swaps = `G6.5` (the
  captain's own, and the one he called top priority); pit-wall tuning = `G6.6`; more than six cars =
  `G6.10`; wet weather for all tracks is new; everything else is unregistered.
- **The honest part, stated now rather than in a comment three years from now:** several of these are
  **not "impossible in a recomp"** and the captain should know which before the reply goes out —
  • *low-fuel starts and qualifying* are **data and rules**, squarely in reach once the game runs.
  • *contact penalties on all races* is a **rules-table change**, same family.
  • *sectors and wet weather on every track* are **event/track data**, same family again.
  • *the clutch* is real work: it is a **new input + a change to the shift path**, not a flag.
  • *POV camera* is `G6.7`'s camera state, already scoped there.
  • *rewindable replays* is a **recording/transport layer that does not exist yet** — closest thing on
    the list to a genuinely new system, and the one most likely to be quietly dropped.
- **The fence:** all of it **off by default** (law 12). A "low fuel start" option that quietly changes
  every race would be the exact failure this project is named for. Every item is opt-in, and `OFF`
  must be the untouched 2005 behaviour including the 2005 rules.
- **STATUS:** ⬜ registered, **not scheduled**. Nothing here outranks the picture, the VU1, or a
  drivable race. It is a promise *kept in writing*, not a promise *made in public*.

### ⬜ G6.13 — **THE COMMUNITY'S MODDING FRAMEWORK** *(2026-10-01 — "some sort of framework to make it easy to mod the game")*
- **Where it came from:** u/RobinVerhulstZ. This is the comment worth answering carefully, because it
  is not a feature request for *this* project — it is a request about **who is allowed to build the
  features**, and it quietly describes what `G11` (the asset bridge) would have to be.
- **What he is actually asking for:** not "add cars", but a *supported way for anyone to add cars*.
  That is a different deliverable: a documented data format, a validation pass, and a hot-load path.
- **The dependency is the whole answer:** this cannot exist before `G6.0`/`G6.0a` (add cars, and the
  audit of every place GT4 assumes a fixed car count). A framework for a car system that cannot yet
  add a car is a framework for nothing.
- **The fence, and it is a real one:** a modding framework means third-party code touching the
  runtime. Law 1 (no game data in the repo) and law 3 (refused, not guessed) both have to extend to it.
  Anything shipped must be opt-in (law 12) and must not be able to write into the recompiler's own
  build. Say that before promising anything, not after someone ships a bad mod.
- **STATUS:** ⬜ registered, **not scheduled**, blocked on `G6.0a`. The *answer* to him can be given
  today, though — "add cars is the first thing I'm building; a framework for it comes after, and it
  ships to everyone" is true and is a better reply than a promise.

### ⬜ G6.14 — **THE LONG FRAME** *(triple-screen in career, and a physics that is not glued to the frame, 2026-10-01)*
- **Where it came from:** u/RobinVerhulstZ, same comment. Two different asks, both real.
- **Triple-screen in career mode:** the game **already has** triple-screen — but it only works in
  arcade mode, and it needs two extra PS2s for the hardware. The ask is therefore *"let the third screen
  work in career"*, i.e. drop a hardware dependency that never should have gated the mode. That is a
  narrow, honest, reachable-sounding fix **once three linked clients exist at all** — which is
  `G4`-era at the earliest.
- **Decoupling physics from framerate / stable 120 Hz with double-rate physics (GT7-style):** this is
  the most technically load-bearing request in the whole thread and the easiest to say "yes" to by
  accident. It is **not a flag.** It means a fixed-timestep accumulator the guest's own timing respects,
  which touches the scheduler, the frame pacing and every physics thread — the same machinery
  `G6.7` (THE RUSH) needs for high-refresh mode. **Do not let this be answered with a frame cap.**
- **The fence:** `OFF` must be exactly GT4's current framerate-locked behaviour. A "stable" mode that
  silently changes how physics is stepped is a change to the game, not a setting.
- **STATUS:** ⬜ registered, **not scheduled**. Both items depend on a running game and on the timing
  work in `G6.7`; neither may set the order of anything.

### ⬜ G6.15 — **THE BRAKE POINT** *(a racing line with braking zones, and a camera you can shape, 2026-10-01)*
- **Where it came from:** u/StreetScientist5652 — *"a racing line (that shows braking zone) might be
  nice. Obviously it should be toggleable. And maybe customizable camera too!"*
- **Honest scope, so nobody is surprised:** this is **not** a simple overlay. A usable racing line means
  GT4's own track geometry and corner data, **generated per track**, and a way to decide the line —
  which is a racing-line *generator* (the classic shortest-path/curvature solution), not a hand-drawn
  spline per track. That is real, bounded work, and it is genuinely useful, but it should be called
  what it is rather than described as "a toggle".
- **Braking zones are the cheaper half** and could ship first: they are derived from corner radius,
  entry speed and the car's deceleration, so they can be drawn *from physics we already compute*.
- **The camera half overlaps `G6.7`** — a customizable camera is the same camera state, so this is one
  system with two consumers, not two features.
- **The fence:** off by default (law 12); a racing line drawn on screen **must not** touch AI
  behaviour, and must never be on in the "authentic" profile.
- **STATUS:** ⬜ registered, **not scheduled**. Depends on the renderer and on track data.

### ⬜ G6.16 — **THE OPPONENT THAT DRIVES LIKE A PERSON** *(better AI + a camera-stiffness slider, 2026-10-01)*
- **Where it came from:** u/GTalaune — *"Better AI, is it even possible ? A slider to change the camera
  stiffness like we can do now by using cheats."*
- **Better AI is a research problem, not a toggle**, and it is the only item in this batch that would
  make GT4 *stop* being GT4. GT4's AI has a character. Replacing it with a modern racing-line follower
  would be a different game, so it needs the same fence as everything else (`OFF` = untouched 2005 AI)
  **and** it needs the racing line from `G6.15` before it could even be attempted honestly.
- **The camera-stiffness slider is the cheap, honest half** and is a small slice of `G6.7`: it changes
  how fast the camera reaches the car's heading, nothing more. Worth saying yes to early because it
  costs almost nothing and people demonstrably want it.
- **STATUS:** ⬜ registered, **not scheduled**. Camera stiffness = a slice of `G6.7`. AI = research,
  blocked behind `G6.15` and behind a decision about whether we want a different game.

### ⬜ G6.17 — **THE SOUNDTRACK** *(the PAL soundtrack, 2026-10-01)*
- **Where it came from:** u/musicallymotivated93 — *"The Spec II additions/tweaks and the PAL
  soundtrack"*. (The Spec II half is already covered: `G5.1`.)
- **What this actually is, so the entry is not a fantasy:** the PAL release of GT4 has its own licensed
  music that the NTSC disc does not contain. Shipping it means **shipping music we do not have the
  right to ship** — the same line law 1 draws for the game itself. So this entry exists to record the
  wish honestly and to say where it lands:
  - **Not on this project.** A recomp that ships the PAL soundtrack is a recomp that ships copyrighted
    audio it has no licence for, and the repo carries no game data by rule.
  - **What *is* ours** is `G6.1` (custom music) — if the captain's own music, or music the user
    supplies from a disc they own, can be dropped in, the PAL question answers itself without us
    distributing one byte of anyone's soundtrack.
- **The honest reply to him, if one is sent:** "not from us — that music is licensed and we don't ship
  game assets; what we're building is a custom-music slot so *you* can put a soundtrack in."
- **STATUS:** ⬜ registered as a **documented no** for the PAL audio itself, with the substitution
  (`G6.1`) named so the wish is answered rather than dismissed.

### ⬜ G6.18 — **QUAL-OF-LIFE: the boring half of the wish-list** *(r/granturismo4, 2026-10-01)*
- **Where it came from:** the 46-comment thread *"GT4 Recomp suggestions"* (r/granturismo4,
  t3_1wurol8) — read in full, not skimmed. **That thread is the captain's own post** (u/Professional-Bar6778
  is Boss), opened to collect the community's wishes for *this* recomp. So this entry is the
  community's answers to our own question, in their words, and it is the spec's source of truth.
- **Why one entry, not forty:** law 12 makes every one of these **opt-in**, and that is a *per-feature
  engineering cost*, not a formality. Forty sliders means forty opt-outs, forty defaults, forty things
  to test against the untouched original. So they are registered as ONE block with the shared shape
  named once, and split out only when one of them is actually built.
- **What was asked for, grouped by what it costs us:**
  - **Difficulty / accessibility:** tyre wear rate slider ("15 laps on mediums"), lower AI difficulty,
    penalties-for-contact toggle, oil-change intervals.
  - **Race structure:** qualifying for single races, rolling-start/low-fuel starts, **more than 6 cars**,
    track limits for quali+races, F1-style sector timing, wet weather on all tracks.
  - **Camera:** fully custom camera with **saveable presets**, camera-stiffness slider, POV driver cam.
  - **Racing line:** one commenter noted the data probably already exists — the red "suggested gear"
    brake flash in A/B licence tests. **Read the disc, don't reimplement physics.**
  - **Cars:** paint with **custom hex + liveries**, colour options, car repaints, engine swaps
    (the thread's top engine-swap ask: super-softs and NOS on the Benz), more colours/time options.
  - **Sound:** engine-swap audio, clutch-kick audio, gearbox whine.
  - **Presentation:** custom aspect ratios with **no UI stretch** (48:9, 16:3), higher refresh, photo-mode
    PNG export, screenshot export, more cars in the grid.
  - **Framework:** a real modding framework so users can add cars/tracks without recompiling us.
- **The one that is not ours to refuse:** *"ability to ADD cars instead of replacing existing cars"* —
  that is `G6.2`/`G6.1`'s supply side and it is the thread's most-upvoted idea. Already the project
  thesis. This recompile makes it possible for the first time in GT4's history; the previous
  generations could only swap what shipped. The captain's own plan, stated in the same thread: give
  the ability to ADD cars rather than replace them, and add more brands.
- **What we will NOT promise:** *"I don't know whether it's possible"* was the other recomp author's
  honest answer to VR/interior imports. Same here: those are registered, not promised.
- **STATUS:** ⬜ registered, **not scheduled**. Every line is gated on Stage 1 (a picture) — a tyre-wear
  slider on a black screen is a lie waiting to happen. Law 12 applies to all of it without exception.

### ⬜ G6.19 — **TRIPLE SCREEN, and the misconception that hid it** *(from the same thread, 2026-10-01)*
- **Where it came from:** u/After_Inside_7231, replying in the captain's own thread — *"triple screen
  support in career mode? the game already has it but it only works in arcade mode, unlike GT5/GT6."*
- **The correction that matters, and it is ours to state plainly:** on PC this is **three windows on one
  machine**, not three consoles and not three TVs. The PS2 version needed two extra consoles because that
  was the hardware; ours is one PC, so the console-count objection doesn't exist here. What genuinely
  remains is **career mode**, which GT4 does not offer even on real hardware.
- **The fallback the thread itself proposed, which is cheaper and probably the better first move:**
  **custom aspect ratios (48:9, 16:3) with no UI stretch** — geometrically a triple screen on one display,
  and testable by one person instead of three. It is registered under `G6.18` and this entry is the
  career-mode half.
- **STATUS:** ⬜ registered, **not scheduled**. Blocked on Stage 1, same as `G6.18`.

### ⬜ G6.19a — **NO STRETCH AT ANY WINDOW SIZE** *(the captain, 2026-10-16)*
- **The ask, verbatim:** *"we can't just stretch the game to fit every screen. what if i play on a 1440p
  screen? or a 720p screen? we have to adapt the game to it."* Raised while resizing the studio launcher
  and watching the room, the car and the dock all squash.
- **The engine already owns the mechanism and the screens bypass it:** the virtual screen and its
  letterbox live in `src/platform/sdl/video_gl.c:59-62` (`view_w/view_h`, `scale_x/scale_y/off_x/off_y`,
  `ui_panel_x/y/w/h`), and the `aspect_ratio` video option already exists (`launcher_config.c:129`).
  What is missing is that it be the **only** path: one mapper from window size to a uniform scale and a
  centred viewport, used by menus, race and studio alike, plus integer factors where the window allows.
- **Our own UI is exempt from the virtual screen:** the launcher dock and its cursor are drawn in
  framebuffer pixels at design size, centred, so they never resize with the picture.
- **STATUS:** ⬜ registered, **not scheduled** — the captain's call: *"lets maybe do it later"*. Close
  cousin of `G6.18` (custom aspect ratios with no UI stretch) and worth doing in the same pass.

### 🆕 G6.20 — **THE LOGGER** *(telemetry, 2026-10-13 — first day of the engine era, the captain's brainstorm)*

- **Where it came from:** the captain, 2026-10-13: *"we can finally put our ideas on making the ultimate
  racing sim."* Registered as the first new idea of the engine era, and it is possible for exactly one
  reason: **the physics here is not the original's machine code.** It is new C, "rewritten from the
  original code (CORE.GT4, US v2.00)" (`src/core/physics/phys.h:2`), running a fixed 1/60 step with all
  its state in RAM we own.
- **What it is:** a per-tick trace of everything the simulation knows — speed, throttle/brake/steering
  as the physics sees them, per-wheel slip, load, travel, tyre temperature and wear, engine rpm and
  torque, gear, lateral and longitudinal G, race position. Written as an ordinary file: **CSV first**,
  then whatever sim racers actually read (MoTeC/i2) once the fields have proven stable.
- **Why only this fork can do it to GT4:** on hardware and on an emulator that state lives inside the
  guest's RAM in a format nobody documented, and a mod can only poke at it. We can *read* it because we
  **compute** it. It is the cheapest feature on this list with the largest effect on how the project is
  perceived: it is what makes people call something a simulator instead of a port.
- **Mechanism, in order:** 1. name the fields and the structure each lives in (`car.h` `CAR_*`,
  `WHEEL_*`, `PARAMS_*` — the per-tick order is fixed, `step.c:67-127`); 2. a logging hook that runs
  **outside the tick**, because the step must not change: the physics sets round-toward-zero + FTZ/DAZ
  every frame inside `fegetenv`/`fesetenv` (`race_run.c:753-779`), so a logger that perturbs timing or
  rounding is measuring something else; 3. a file format and a reader before any UI; 4. only then
  overlays, dashboards, comparisons.
- **The fence — and this one is a hard gate, not a nicety:** off by default (law 12), and **a logging
  run must produce a bit-identical state digest to a non-logging run of the same race.** If the trace
  changes the race, the trace is worthless. The test suite already has a state digest
  (`tests/physics/state_digest.h`) to prove it with.
- **Honest limits:** this is telemetry of **our model of GT4's physics**, not of GT4's own internals —
  and parts of that model are labelled approximate (`race_sound.c:192`, the `A:` adapters in the photo
  and audio code). A trace that shows the model being wrong is a finding, not a bug in the logger.
  Also: sampling rate must be the simulation rate (1/60), never the frame rate, or the data lies under
  a frame drop.
- **STATUS:** ⬜ registered, **not scheduled** — but named by the captain on 2026-10-13 as the first
  thing to build once the baseline (a race we can actually drive) is honest.

### 🆕 G6.21 — **THE RACE ENGINEER** *(the log, talking to you, 2026-10-13)*

- **Where it came from:** the same brainstorm — the idea that GT4 never had someone in your ear, and
  that `G6.20` is the thing that makes it possible.
- **What it is:** not a spotter that reads gaps off the HUD. An engineer that reads **the trace** and
  tells you what the lap actually did: which corner is costing you, where you brake early, where you
  get on the throttle before the car is ready, what the tyre is doing while you do it. Voice and/or a
  panel; the voice is the last part to build, not the first.
- **Why it is not a gimmick:** it turns the port into a **coach**, and it is the one feature here that
  cannot be copied from a mod, an emulator or the original, because it needs both the physics *and* the
  log. The data always existed on the PS2; it was simply invisible.
- **Mechanism, in order:** 1. a lap **is** a trace — segment it using the track's own sectors and line
  (`course.c` already reads sectors and the AI line); 2. compare two laps — yours against your best, or
  against a ghost — and rank the differences **by time cost**, not by distance travelled or by visual
  drama; 3. say **one** thing at a time, because the failure mode of every coaching tool ever shipped is
  saying five; 4. only then the voice.
- **The fence:** off by default; it may never invent a number that is not in the trace; and it has to be
  honest about what the model cannot know — an engineer that sounds confident about an approximate
  model is worse than silence.
- **Honest limits:** "brake later" needs a reference — a real racing line per track, which is `G6.15`'s
  job, so this depends on it. Voice needs either host-side TTS or recorded lines (never licensed audio).
  And it must not become a cheat: it reads **your** telemetry, never a rival's.
- **STATUS:** ⬜ registered, **not scheduled**. Depends on `G6.20`, and on `G6.15` for the reference lap.

### 🆕 G6.22 — **THE HONEST RACE** *(online, verified by re-simulation, 2026-10-13)*

- **Where it came from:** the same brainstorm. Upstream has already sketched this in Portuguese in
  `docs/EDITIONS.md` ("multiplayer competitivo", as a long-term Enhanced idea): the server runs the same
  physics headless and refuses a lap the replay cannot reproduce. **This entry is the mechanism on our
  side**, and it is the concrete verification that makes their sketch possible.
- **What it is:** the server does not trust the client. Given a race result it takes the player's
  **command tape** and re-simulates the race itself.
- **Why the mechanism already exists in this tree:** a replay is the player's 0x20-byte command per step,
  not a video, and the rivals are re-derived from grid-slot seeds (`replay_file.h:3-7`); a replay is
  **already re-simulated from the race start** (`replay_file.h:3-6`). The simulation is deterministic by
  construction — fixed 1/60 step (`car.c:20-21`, dt bits `0x3c888888` `race_run.c:756`), round-toward-zero
  with FTZ/DAZ per frame (`phys.h:10-15`), an LCG seeded to a constant (`race_run.c:147`), and a fixed
  subsystem and car iteration order (`step.c:65-127`). A doctored file can therefore only change the
  commands — which the server can re-run and reject.
- **What must be true first, and is measurable today:** **bit-identical results across CPU, compiler and
  OS.** That is a determinism test suite (state digest per step, run across builds and machines), not a
  hope — and it is the honest first rung. Divergence must be *measured and then driven to zero*; nothing
  networked starts before that.
- **The fence:** the offline career stays local and moddable; the multiplayer profile lives server-side
  (upstream's own design); only verified content — hashes of the car and course data — enters a ranked
  race; and everything here is opt-in, with the single-player game untouched.
- **THE PRODUCT TARGET, stated plainly (the captain, 2026-10-13): *"This is literally full on online racing."***
  Yes. Real people, real time, racing each other — public lobbies, matchmaking, ranked if it earns its
  place. The command-tape re-simulation above is **the referee, not the gameplay**: it is what makes a
  result trustworthy and a cheater impossible. The *racing* is the part below, and the ladder is the
  **engineering order**, not the ambition's ceiling.
- **How a live race actually works (this is the whole thing, and it is small):** every machine runs the
  same deterministic sim at the same 60 Hz, and each client sends its **input for the next tick** —
  throttle, brake, steering, gear, all of it, which is ~20 bytes. Six cars is roughly **1.2 KB/s per
  player**. Bandwidth is not the problem and never was. **Latency is the problem**, and it is solved in
  one of two ways, both proven elsewhere:
  - **delay-based lockstep** — everyone waits for the slowest input. Simple, honest, and it feels like
    driving with a heavy wheel; fine to 60-80 ms.
  - **rollback** — the local player's car responds instantly, and when a remote input arrives late the
    sim **rewinds and re-runs** those ticks. Fighting games do this at 60 Hz on far less deterministic
    sims than GT4's. This is the right answer for a racing game and it is the hard part.
  Neither is exotic; the choice is a measurement (how many ticks must we roll back at 150 ms?) and that
  measurement needs the determinism rung below to exist first.
- **The other half of "full online" is the boring half:** a lobby — create a room, see it, join it, pick
  a car, load the same course, start. That is our own small self-hostable service, free to run, with
  direct-IP join as the escape hatch so a group of friends never depends on our uptime.
- **How the server is hosted (asked 2026-10-13), and why this one is cheap:** the race itself needs no
  server — clients simulate locally and exchange inputs. What needs hosting is small:
  - **the lobby**: a room list, joining, and the car/course handshake. A tiny service with a database.
  - **the relay**: because two players behind NAT cannot always reach each other, one small process
    forwards their inputs. **The bandwidth is the joke here** — ~1.2 KB/s per player, so a six-car room
    is about **7 KB/s, ~25 MB an hour of racing**. One modest VPS could relay thousands of rooms; a home
    box could relay a friends' league forever.
  - **the referee**: re-simulating a reported race is real CPU work, but it happens *after* the race and
    is not latency-bound — it can run anywhere, including on the players' own machines with random audits
    for ranked play.
  **Three tiers, and the lowest one has no server at all:** (1) **LAN / direct IP** — just type an
  address, no infrastructure, always available, never dependent on us; (2) **self-hosted** — the server
  is the same small binary anyone can run, and a friends' league runs on one person's PC; for reaching it
  from outside, the answer to NAT is a free personal tunnel (Tailscale/WireGuard-class) or a port
  forward, not a paid service; (3) **public ladder** — one small always-on box for the room list and
  relays, the only thing we would ever pay for, and even that stays replaceable because anyone can host
  it. Anything with a paywall or an account requirement is out on principle: **no login, no cloud, no
  problem.**
- **Honest limits:** latency is unsolved — lockstep versus rollback is an open question and none of it is
  free; NAT and self-hosting are a real burden for a project with no servers; and the honest ladder is
  **LAN first**, then self-hosted, then public, if ever.
- **The other path, and why we are not taking it first (the captain asked, 2026-10-13):** the Spec II /
  Online-Beta disc *does* contain the original online mode — `NET/`, `IRX/` and `DNAS310.IMG` are all
  there — so reviving it is conceivable. It means two unknown jobs at once: **DNAS authentication**
  (Sony's dead server-side check, spoofable client-side — the GT Sport 1.69 precedent proves the flag is
  liftable) and **Polyphony's own race protocol**, which nobody has documented for this beta. That is
  reverse-engineering a protocol we have never seen, to get netcode from 2005 that a lockstep design
  beats anyway. **Decision: not the first path.** Keep it as the fallback that lets *emulator players*
  join, since it is the only way a real PS2 can be in the same race.
- **STATUS:** ⬜ registered, **not scheduled**. First rung: the determinism test.

### 🆕 G6.23 — **ENHANCED TEXTURES** *(release requirement, 2026-10-13)*

- **The captain, 2026-10-13:** *"Before release I want 3 things to be in Vulcan4. Enhanced textures. Native spec
  ii support and everything in the easy roadmap."* → **release gate**, not a wish.
- **What it is:** the engine loads **texture replacements from a folder the user owns**, instead of the
  disc's own Tex1 image, wherever one exists. The user brings the pack; we ship only the loader — the same
  fence as mods (LAW 1).
- **The design decision that makes this cheap:** match the community's convention instead of inventing one,
  so their existing packs work unchanged. **MEASURED 2026-10-13 from the captain's own pack**
  (`/mnt/ssd/gt4-tex/SCUS-97436-specII-HD-textures.7z`, 590 MB → 4,517 files / 1.7 GB, HD HUD & UI):
  - layout is `<serial>/replacements/<category>/…` with 50 category folders (`bootup`, `events`,
    `keyboard`, `options`, `replay`, …);
  - **every file is `.ktx`** — BC-compressed, not PNG. A PNG-only loader would load *nothing* from this
    pack. (Other community packs do ship `.png`; both must work.)
  - the name is `<texture hash>-<clut hash>-r<register word>-<gs offset>`, e.g.
    `1b191015984049cd-f2da2b43af133664-rb4000000b40000-00006214.ktx` — 3,860 files carry four segments,
    647 carry three, and a handful two or five. The `r…` segment is the **packed TEX0/register word**
    (`b4000000b40000` = width/height/format), not the literal `-r360x270` form other packs use.
- **So the real work, honestly:** (a) implement the GS **texture hash + CLUT hash** over the same source
  data PCSX2 hashes, and (b) read **KTX** (at minimum the uncompressed and the common BC formats) as well
  as PNG. Both are contained — `src/core/tex1.c` already has the decoded image and the CLUT in hand — but
  this is a *moderate* job, not the "free load-time substitution" it looked like before the measurement.
- **What the code gives us:** every decoded texture passes through `tex1_decode` into `tex1_image`
  (`tex1.h:14-25`), and the backend uploads plain RGBA (`render3d.c:233-250`) — so the substitution point
  needs no renderer change at all.
- **The fence:** replacement textures are **the user's files** — never bundled, never redistributed, and
  never in the repository. Off by default (LAW 12): without a pack the engine draws exactly the disc.
- **Honest limits:** the packs are AI-upscaled, not authored, so this is not "GT4 in 4K" — it is a different
  image with more pixels, and purists will hate it. It also costs VRAM (the known packs want ~4 GB at 5×)
  and the pack is *large*; the loader must fail soft, per texture, never all-or-nothing.
- **STATUS:** ⬜ **release requirement**, not scheduled. First step is the measurement: does our hash match
  theirs?

### 🆕 G6.24 — **NATIVE SPEC II SUPPORT** *(release requirement, 2026-10-13)*

- **The captain, 2026-10-13:** same sentence as `G6.23`. Spec II is the mod he actually plays, so "it runs
  on my disc" is the bar.
- **What Spec II is, precisely:** an xDelta patch over the **GT4 Online Public Beta NTSC (SCUS-97436,
  CRC `4CE521F2`)** — a *different executable and a different disc* from the retail v2.00
  (`SCUS-97328`, CRC `77E61C8A`) the engine is coded for today. It adds cars (from the Korean and MX-5 demo
  discs), events, a race-length multiplier, prize changes, and an Event Synthesizer.
- **Why this is real work and not a config flag — three hard-coded assumptions in the tree:**
  1. the VOL index is read at a **fixed page for NTSC-U** (`vol.c:15`, `0x2159`, with an explicit refusal
     message for anything else) — a different disc must be *measured*, not assumed;
  2. career constants are read out of `CORE.GT4` at **hard-coded addresses, each verified against the
     original instruction bytes** (`career_prices_disc.c:24-28` and the rest of `career_*_disc.c`) — a
     patched executable will fail those verifications, so the constants need a per-disc profile or a
     documented fallback;
  3. the launcher accepts any `SCUS-97328` release (`launcher_config.c:73-76`) — it needs to know about the
     beta's id too.
- **MEASURED 2026-10-13, and the first assumption is already confirmed — with a twist.** The captain's own
  Spec II build was downloaded (`Gran Turismo 4 (Spec II v1.10).7z` → 3,683,418,112-byte ISO), verified by
  size, extracted and hashed: **sha256 `6a3484b9672c10a92154ecee6faa5bbcfd4ed8cd538d8283e429c3fe6e1704b2`**,
  sha1 `7fecc1e634fcd63794b45d110f55c79e07e7a30d`. The engine refuses it, honestly:
  *"unsupported disc image"* (`tools/verify_iso.py`, `config/supported_discs.json` has no beta entry).
  What the disc actually is:
  - it boots **`SCUS_974.36`** (6,520,944 bytes) — **24× the retail executable's 273,020 bytes**. This is
    not the retail stub with a patch applied; it is a different build.
  - its root is **not** the retail root: `SYSTEM.CNF`, `SCUS_974.36`, `EPSON/`, `IRX/`, `NET/`,
    `DNAS310.IMG` (Sony's online authentication — it is an online beta) and `GT4.VOL` (3,435,528,192
    bytes, *larger* than retail's). **There is no `CORE.GT4` and no `IOPRP300.IMG` at all.**
  - **the twist: the RoFS index is at page 0, not page `0x2159`.** Read directly from the image, the VOL's
    first 4 bytes are `ad90b9ac` — the little-endian `RoFS` magic — while page `0x2159` (where the retail
    disc keeps it) holds noise. Retail's layout is *decoy index at page 0, real index at `0x2159`*; this
    disc is simply the other way round. **So the fix is narrower than feared: the index page becomes a
    per-disc value (0 or `0x2159`), and the magic check already validates the choice.**
- **The consequence that is still open, and it is the real work:** every career constant the port reads is
  addressed *inside `CORE.GT4`* (`career_*_disc.c`), and on this disc that file does not exist as a file.
  The constants have to be found — inside `SCUS_974.36`, inside `GT4.VOL`, or in a differently-shaped
  equivalent — before any of the career can work on it. That is a hunt, and it starts with a VOL index
  reader that takes the page as a parameter.
- **What the mod actually ships, verified on the author's own page (2026-10-13):** *"Spec II is based on the
  NTSC version of Gran Turismo 4 Online Public Beta and is **distributed as an xDelta patch requiring this
  version**."* So the user supplies the beta disc and applies a diff — and because it is a diff, **the newly
  added content is inside the patch**: the added cars (Mini Cooper 1.3i '98, Mazda MX-5 '05 from the MX-5
  demo, the Korea-exclusive Hyundai Click Type-R '04 / Tiburon Turbulence '99 / Tuscani CCS '03, Lexus
  IS200 GT-1 Race Car '04, restored Vauxhalls and Altezzas), **Circuit Valencia from Tourist Trophy**, and
  *"enhanced engine sounds for 400+ cars with 130+ new recordings"*. The beta is the **platform**; the added
  content was ported from other discs and travels in the patch.
- **Does any of that have to enter our repository? No — and this is the important consequence.** The assets
  land in the **user's patched disc**, and the engine reads the disc. We ship code that understands a disc,
  exactly as we do for retail v2.00. Nothing here changes LAW 1: the fence is on *us redistributing game
  content*, and we never do — not the ISO, not the diff, not one texture from it.
- **The honest constraint that remains:** supporting Spec II means supporting *that disc family* — the user
  must own the beta and apply the patch, the same way the vanilla path requires them to own retail. That is a
  requirement on the user's shelf, not an asset in our tree.
- **The route that removes the constraint entirely** is already upstream's plan (`docs/MODS.md`): a mod
  format plus an **assets-only importer**, so those cars can come from *any* disc the user already owns
  (Korean GT4, the MX-5 demo, Tourist Trophy for Valencia) instead of requiring the pre-patched image. Spec
  II is proof that the scene can add cars by shipping a diff to a disc; the point of this fork is to make it
  a **format** — no disc patch, no redistributed assets.
- **"A toggleable Spec II mode" — answered precisely, because the word hides two different things.**
  - **What CANNOT be toggled:** Spec II's content. It is not a set of runtime options — it is a *different
    executable and a different disc* with added bytes (cars, Circuit Valencia, 130+ engine recordings). No
    menu switch can conjure content that is not on the disc you launched. A "Spec II toggle" over a retail
    disc would be a lie with a checkbox.
  - **Therefore the design is: one engine, N disc profiles.** The engine identifies the disc (serial +
    hash) and selects a profile: the VOL index page, the constant source, the executable ids, the data
    offsets. Vanilla v2.00 is profile #1, the online-beta family (Spec II included) is profile #2. You get
    whatever is on the disc you pointed it at — which is exactly why the repository stays clean.
  - **What IS toggleable**, and it is the useful half: **the profile**, auto-detected with an explicit
    override for the day detection is wrong (`--disc-profile=auto|retail|spec2`). Plus every engine-level
    option we add, which stays off by default (LAW 12). That is also the honest version of the
    Halo-MCC-style "change to original" button the captain promised publicly: for *our* additions it is a
    switch; for *Spec II's content* it is the other disc.
  - **This generalises, which is why it is the right shape:** PAL, NTSC-J and the Korean release are the
    same problem, and upstream's planned mod loading is a profile **plus an overlay**. Nothing here is
    Spec-II-specific except the profile data.
  - **The work, in order:** 1. hashes into `config/supported_discs.json` (the patched ISO is already
    hashed; the unpatched beta still needs one); 2. parameterise the VOL index page — measured to be **0**
    here versus **0x2159** on retail, and the existing magic check already validates the choice; 3. **find
    the career constants** (the long pole — `CORE.GT4` does not exist on this disc); 4. teach the launcher
    the beta's id; 5. run it and show a picture.
- **The fence:** the mod is the user's; nothing from the patch or its added content is ever committed, and
  the image lives on the SSD outside the repository. Vanilla v2.00 must keep working untouched — a second
  profile is added, never a replacement.
- **MEASURED 2026-10-13, second pass — the container now opens; the next wall is the SCRIPT VERSION.** After
  two format fixes in `src/core/vol.c` (index page found by scan; `pairs = count >> 1`), the engine reads the
  Spec II volume: index at page `0x22b7`, **27,242 entries**, and it reads `/projects/boot/boot.adc` out of it.
  What that file then says is the wall:
  - retail's `boot.adc` begins **`ADCH005`** (104,613 bytes) — the version the port's VM implements;
  - the Spec II / online-beta disc's begins **`ADCH007`** (126,438 bytes) with the internal path
    `projects/gt4o_us/boot/_tmp_boot.ad` — a **GT4 Online** build with a newer scripting version;
  - `adhoc_code.c:144` rejects anything that is not version 5 (`atoi(data + 4) != 5`), so the failure is
    honest and exact: *"not an ADCH005 script"*. Whether 007 differs only in the version digit or in its
    frame/opcode layout is **the next measurement** — and the reference is already on this box
    (`archive/vulcan4-recomp/docs/REFERENCES-OPENADHOC.md`: OpenAdhoc re-creates GT4's scripts from source;
    GTAdhocToolchain covers the family's later versions).
  - The run's other two messages are *expected*, not new: *"career: original calendar, price or oil constants
    unavailable for this disc"* and *"loading: original assets unavailable"* — this disc has no `CORE.GT4`.
- **Tool added for exactly this work:** `tools/vol_file.py` — list/read/extract inside `GT4.VOL` on either
  disc, with the index-page scan and the corrected page rule. It produced every number above, and it is what
  the constant hunt needs next.
- **STATUS:** ⬜ **release requirement**, not scheduled. The image is in hand (`/mnt/ssd/gt4/spec2/`) and
  the disc is measured; the open work is the constant hunt above.

### 🆕 G6.25 — **THE OVERLAY** *(apply a patch without making a second ISO, 2026-10-13)*

- **Where it came from:** the captain, 2026-10-13: *"Why not add the ability to let u make the spec ii iso?"*
  — and the answer is that we can do better than making one.
- **The idea:** instead of patching a 3.7 GB image and writing a second 3.7 GB image, **serve the patched
  bytes at read time**: the user keeps **one** disc on disk plus the patch file, and the engine applies the
  diff on the way in. The user points at the beta + the xdelta, and Spec II simply appears — no duplicate
  image, no patching step to forget, and the original disc is never modified.
- **Why this is architecturally cheap for *this* engine:** every byte the port reads — the VOL container,
  `CORE.GT4`, movies, everything — funnels through **one function**, `plat_file_read_at`
  (`platform.h:26-32`). An overlay is a layer at that single point, not a change scattered across the
  readers.
- **And it is the keystone of the whole content plan, not a Spec II side-feature.** The same mechanism is
  what upstream's mod format needs: a base disc plus overlays. Spec II becomes **overlay #1** (a foreign
  patch we merely apply), an added car becomes **overlay #2** (our own format), and the community's packs
  become the same thing again. One mechanism, three use cases.
- **The honest cost:** an xdelta patch is a *sequential* stream, and we need **random access** — so the
  first job is to index the patch into changed ranges once, then serve reads from that map (a patch with no
  index means re-running the delta for every read, which is not a real option). That indexing is real work,
  and it belongs with the mod framework (`G6.13`), not before the baseline.
- **The fallback, if the overlay proves slow to build:** a plain "apply the patch, write a new image" tool —
  which is xdelta3 with a UI and a progress bar, nothing more. Honest, one-time, and it does not generalise
  into mods the way the overlay does.
- **DECIDED 2026-10-13 — two entry paths, both supported, and they are not equal in cost:**
  - **Path B — "bring your own patched disc."** The user arrives with the beta + Spec II already applied
    (exactly the image now on the SSD). This costs us **only the disc profile**: identification, the VOL
    index page, the constant source. **No patch code at all.** It is also the path that must work first,
    because it is the one we can test today.
  - **Path A — "bring the base disc and the mod, we apply it."** Two flavours, cheapest first:
    **(A1) apply and write a new image** — one-time, explicit, needs ~3.7 GB of free space, and it is
    nothing more than the applier plus a progress bar; **(A2) the live overlay** (`G6.25`) — no second
    image, no waiting, and it is the mechanism the mod system ultimately needs.
  - **In both flavours, the applier validates before it applies:** the patch names its source, so a wrong
    or different beta revision must be **refused**, not half-applied. That check is the difference between
    a tool and a foot-gun.
  - **✅ Licence verified 2026-10-13, and it is the good answer:** modern **xdelta3 is Apache-2.0**
    (`jmacd/xdelta`, release3_1_apl onward — the *original* GPL line was moved to a separate repository,
    `jmacd/xdelta-gpl`). Apache-2.0 is GPL-3.0-compatible, our project is GPL-3.0, so the applier can be a
    **linked library** rather than a reimplementation of VCDIFF — it exposes a zlib-style API
    (`xdelta3.h`), and the format is RFC 3284.
  - **What both paths share, and it is most of the work anyway:** disc identification, per-profile
    parameters, namespaced saves, and an honest report to the user of which profile they are running and
    what that profile supports.
- **The fence:** we ship the **applier**, never the patch and never the result. The user brings both the
  disc and the mod; nothing derived from either enters the repository.
- **STATUS:** ⬜ registered, **not scheduled**. Depends on the mod framework's shape being decided first.

### ⬜ G7.0 — **DISTRIBUTION: code only. Assets come from the user's disc.** *(the captain's rule)*
- **The captain, 2026-09-30:** *"We don't publish anything. Just the code. People have to bring their
  own iso file of the game — that's where we take assets."*
- **This is already how the project is built, and it is now a stated policy, not a habit:**
  - the repository ships **no game data** — no ISO, no `GT4.VOL`, no extracted textures, models,
    audio or the recompiled C++ (which is a derivative of the game)
  - the runtime requires **the user's own legally-obtained disc image** — that is the *only* input
  - **mods are distributed as code and/or files the modder has the right to share** — never as game
    content
  - `.gitignore` blocks the extensions, and every dish brief repeats the rule; it is enforced by
    review and by the goal gates
- **Precedent we already set:** the same model as the emulator scene, and the same reasoning used on
  the captain's other projects — **ship the engine, never the assets.** Without the user's disc the
  program has nothing to run, exactly like an emulator without a ROM.
- **Why this is a feature, not just caution:** it means the project can be public, discussed and
  contributed to openly, with no takedown risk — and the community already understands this model.
- **STATUS:** ✅ **in force now.** Verify on every release that a fresh clone plus a user's disc is
  the whole setup (goal `G0.5` — clean room — is the standing proof).

### 🆕 G6.26 — **CUSTOM RACE MODES** *(the captain's own ideas, 2026-10-13)*

- **What was asked for, in the captain's words:** *"I also wanna add custom races. Like elimination
  mode"* — race formats the shipped game does not have, chosen by the player per event.
- **Elimination mode:** at a set interval, the last-placed car is out; last one running wins. This is
  **not** a new physics or AI feature — it is a *race-format rule layer*: a timer, a position read, and
  a "remove/retire car N" action, all of which the engine already has in pieces (`race_game.c` owns
  position and the field; the pit/retire path already exists). The real work is the **HUD and the
  announcement** so the player can see who is out and why, and a rule table so the format is data, not
  a hard-coded mode.
- **Family it belongs to:** this is the same shelf as the F1-style sectors, qualifying for single races
  and low-fuel starts in `G6.12`/`G6.18` — **race-format data, not engine surgery.** Build the rule
  layer once and elimination, reverse-grid, one-make and "no-contact" are all the same mechanism.
- **The fence:** stays faithful to `LAW 12` — the shipped races behave exactly as they always have, and
  every custom format is an option the player turns on.
- **STATUS:** ⬜ registered, not scheduled. Cheapest real feature on this list after the credits screen.

### 🆕 G6.27 — **ONE SPRING** *(the Uma-career mode: one car, one run, one story — named by the captain,
2026-10-15)*

- **What was asked for, in the captain's words:** *"a Uma career inspired mode where u race one car and
  upgrade it"* — Gran Turismo as a **single-car career** instead of a garage of dozens: you pick one
  car, and the whole arc is that car, upgraded and raced, with the story of *that car* being the point.
- **Why this is the right shape and not a gimmick:** GT Mode's own strength is the ladder, but its
  weakness is that cars are disposable — you win, sell, move on. Uma Musume's career is the opposite
  structure: **one character, one long arc, incremental growth, and the identity is the investment.**
  The captain plays both games, so this is his own design language pointed at GT4.
- **What it actually needs — and most of it is already in the disc:** (1) a career container that is
  *not* the garage (a save-scoped one-car context), (2) the upgrade path surfaced as progression rather
  than a parts list — the tuning shop, mileage, oil and break-in already model wear, so **an upgraded
  car is a real, simulated object, not a stat block**, (3) events filtered to the car's class so the
  ladder is *about the car*, and (4) a progress view that reads like a career, not a spreadsheet.
  The engine already reads the career tables from the disc for the real GT Mode (`career.c`), so this is
  a **new front-end over existing data**, not a new simulation.
- **The honest risk, stated now:** it must not become "GT Mode with fewer cars". The mode only earns its
  place if the *feel* of investment is real — if the upgrade has a cost in character as well as credits.
- **Depends on:** `G6.20` (the logger) to make progression visible, and the career-constant work already
  open for `G6.24` (the Spec II disc moved those constants, and the same hunt serves both).
- **⭐ WHAT THE MODE IS (the defining rule, in the captain's own correction:** *"The Uma career game mode where u
  choose one car for an entire run."***): one car, one run, never sold — the whole arc is that car, and the
  identity of the run *is* the car. It is the clean opposite of GT Mode's economy, which is built on selling and
  replacing. **THE MODE IS NAMED: *ONE SPRING*** — see the naming block below for why it fits.
- **⭐ NO NAMED SUBSYSTEM — IT IS JUST *THE CHOICE*, MADE ON THE FLY (captain, 2026-10-15:** *"Bookmaker? We
  don't need that. It's a choice in the game u choose on the fly."***):** there is no odds layer with its own
  name. Per race you pick how hard you want it, and the purse follows that choice — the same way you pick a
  tyre or an event. Written plainly so nobody invents a system around a mechanic that is a decision.** The
  loop:** choose the difficulty for this race → enter → win pays → upgrade → choose again. The failure state is
  still absolute (a lost race ends the run), which is what makes the choice matter without needing a name.
- **THE DESIGN CONSTRAINT THAT KEEPS IT HONEST — "harder" must be something GT4 can express.** A difficulty
  slider that just moves an AI number is a lie the player sees through. The honest lever list from this
  game's own systems: **stronger/limit-tuned AI field** (`G6.16`), **longer race** (lap multiplier, already
  proven by Spec II), **starting position** (rolling/low-fuel starts are already a community ask, `G6.12`),
  **tyre-wear and fuel-pressure multipliers**, **ballast/horsepower limits**, **part and tyre-class
  restrictions**, **weather and time of day**, and **single-attempt rules** (no retry, licence-style). Those
  are the dials; the *purse* is derived from them, not invented beside them.
- **THE STAKE IS THE POINT (and the one new economy idea):** a wager needs something to lose. Entry costs
  either credits or part of the car's value, and a car that cannot finish (worn out, over-tuned, wrong tyres)
  is a real loss the player chose — the same reason the captain's own *"if ur car can't make it ur cooked"* is
  the right phrase and not a joke.
- **THE REPLAYABILITY THIS BUYS, in plain terms:** the same event at three odds is three different evenings,
  the same car at three build levels is three different cars, and a run that fails early is a *story*, which is
  exactly the Uma structure — and the reason the captain says he would play it himself. That enthusiasm is the
  acceptance criterion, not a nice-to-have.
- **STATUS:** ⬜ registered, not scheduled — a Stage 2+ dish. Named here so it is not lost.
- **⭐ THE RUN RULES, 2026-10-15 (captain, verbatim:** *"Yeah u can use ur cars and saves it just dosnt change
  ur save but u can keep the car u make in the career thingy… That's the car u win. But u gotta work for it.
  One lost race and u die"***):**
  1. **YOUR GARAGE AND SAVES ARE READABLE.** A Keeper run can start from cars you already own — the island
     reads your save rather than living in a sealed box.
  2. **THE RUN CANNOT TOUCH THAT SAVE.** All run state (the car, its build, the ladder, the money) lives in the
     island's own scope, so a career save cannot be corrupted by a run — which is also what makes `LAW 12`
     honest: your game is untouched unless you ask for something.
  3. **ONE LOST RACE ENDS THE RUN.** No retries, no rewinds, one attempt per race. This is what makes the
     **difficulty choice** mean something: you are not wagering credits, you are wagering *the run*.
  4. **AND THE CAR IS THE TROPHY — the thing you win by surviving.** You finish a run, and the car you
     built (with its own history) is yours to keep.
- **⚠️ THE ONE HONEST WRINKLE IN "IT DOESN'T CHANGE YOUR SAVE", and the rule that resolves it:** keeping the
  won car means writing it into the normal garage — a single, deliberate **victory hand-off**, and the only
  time a run ever writes to the player's own save. So: it happens **once, at the moment of victory, only if a
  garage slot is free** (the garage holds 1000, `career_original.h:11`), the vanilla save-shape is respected
  exactly (the format accepts only `0x3a070` / `0x3a160`, `save_ps2.c:43`), and a backup of the save is taken
  before the write. Never automatic, never silent, never mid-run.
- - **⭐ THE NAME IS DECIDED, 2026-10-15: *ONE SPRING*** — the captain, after three batches: *"Sure one spring
  sounds good."* **Why it earns the name:** Uma Musume's whole theme is that **you only get one youth** — a
  career you can lose and never get back — and this mode is the racing version of exactly that: one car, one
  run, one loss ends it. Short, ours (`LAW 16`), and it does not describe a *feature*, it describes a *stakes*.
  **Struck drafts, never to be resurrected:** `SKIN IN THE GAME`, `THE ODDS LADDER`, `THE KEEPER`,
  `THE GAUNTLET`, `ONE SHOT`, `CLEAN RUN`, `CLASSIC YEAR`, `ONE RUN`, `THE RAISING`, `DEBUT`,
  `THE LONG SEASON`.
- **THE HUB IS STILL UNNAMED.** `THE STABLE` is the standing candidate for **THE ISLAND** (`G6.39`) — the house
  your cars live in — but that is a separate decision and the island keeps its working label until it is made.
- **⭐ IT IS *VULCAN EXTENDED CONTENT*, NOT A MOD (captain, 2026-10-15:** *"it's not a mod it's a mode. It's
  part of the game. I'd call those the Vulcan extended content"***).** The one-car mode ships **inside** Vulcan 4
  — a mode you select in the game, not a package anyone installs. The vocabulary and the fence are written up in
  `AGENTS.md`, *The three kinds of thing*: vanilla (the disc), **Vulcan extended content** (ours, built in),
  mods (the user's cars and tracks). `LAW 12` still applies: vanilla remains the default path; the mode is a
  *choice*, not a new default.

### 🆕 G6.39 — **THE ISLAND** *(the hub where Vulcan extended content lives, inside career mode)*

- **The captain, 2026-10-15, verbatim:** *"U dont have to enable them if u dont wanna. But they're gonna be an
  island inside the career mode that u have a ton of stuff u can play for fun."* → this entry exists so every
  other extended-content entry has an address: **the island is the hub**, and the modes hang off it.
- **WHAT IT IS:** a place you enter from career mode — its own menu world with a pile of playable things — as
  opposed to a settings toggle or a separate executable. You go there when you want it; **the vanilla career
  sits untouched on the other side**, so `LAW 12` is honoured by *choosing an activity* rather than flipping a
  switch. Nothing about the shipped game's own path changes.
- **WHAT LIVES ON IT (the current residents, all already registered):** **ONE SPRING** with its on-the-fly
  difficulty choice (`G6.27`), **CUSTOM RACE MODES** (`G6.26` — elimination and friends),
  and it is the natural front door for the fun-first experiments that do not belong in a career save. Other
  candidates already on this list: the **LOBBY** (`G6.30`) if it ships solo-first, and the **SIMULATION BENCH**
  (`G6.36`) in its *player-facing* form ("race yourself, compare builds").
- **WHY A PLACE AND NOT A MENU ITEM — the design reason:** a career save is *one continuous story with
  consequences*; these modes are deliberately not that. Putting them behind a settings toggle would imply they
  change the game, and putting them in the career flow would contaminate a save's identity. An island keeps
  both promises: **the story stays clean, and the playground is always open.**
- **WHAT IT NEEDS TO BE REAL:** an entry point in the career menu (the engine already runs the game's own
  career menu projects, so this is a new surface over existing UI machinery), its own save scope so an island
  run cannot corrupt a career save (`G6.27` needs the same thing), and the Keeper's own container.
- **THE NAME:** *the island* is the captain's word for the shape, and it is written here as the entry name. If
  it ever needs a proper title on screen, the candidate on the table is **THE WORKS** — garage-culture for the
  full treatment — but that is a naming decision for when there is something to look at, not now.
- **STATUS:** ⬜ registered, **not scheduled** — a Stage 2+ dish, and the natural home for the modes above.

### 🆕 G6.28 — **BROWSER BUILD** *(the captain's ask, 2026-10-13)*

- **What was asked for:** *"I want browser support"* — Vulcan 4 running in a browser tab, the way
  `ps2-web-recomp` puts statically-recompiled PS2 games on the web in WebAssembly.
- **Why it is not a fantasy here, which is worth writing down:** the engine is **portable C** — plain C,
  SDL2 and OpenGL, no Windows API left inside the core after the Linux bring-up. A PS2-era workload is
  also the *good* case for WASM: the guest expects a 294 MHz EE, not a modern CPU. So the honest position
  is "hard, not absurd", and the parts that are hard are named below rather than waved away.
- **The two real blockers, in order:**
  1. **The disc.** A browser cannot read a 4.4 GB DVD image from a path. It needs the
     **File System Access API** (the user picks their own ISO once, we get a handle) and *streamed*
     reads — a full load into memory or IndexedDB is not viable at that size. This is the same law as
     everywhere else: **the user brings their own disc**, the page never ships one.
  2. **The graphics backend.** The engine's renderer is OpenGL; a browser gives WebGL2 or **WebGPU**.
     WebGPU is the right target and — this is the useful part — **it wants the same seam `G8` builds for
     Vulkan.** One renderer abstraction, three backends: Vulkan on desktop, Metal on macOS, WebGPU in
     the browser. So the RHI work is not a detour for this feature, it is its prerequisite.
- **Also needed, and cheaper than it sounds:** SDL2 already has an Emscripten backend, audio has a Web
  Audio path, and the Adhoc script VM is plain C that will compile as-is. The recompiler's output is C,
  not JIT — which is exactly why a WASM build is even on the table.
- **The honest verdict:** a Stage 3+ moon-shot, and it should not be started before the RHI seam exists.
  But it is the *same* work as Vulkan, not extra work — and if it lands, Vulcan 4 becomes the only way to
  play a native port of GT4 on a phone, a Mac and a Chromebook from one codebase.
- **STATUS:** ⬜ registered, not scheduled. Prerequisite: `G8` (the renderer seam).

### 🆕 G6.29 — **ACCOUNTS AND CLOUD SAVES** *(the captain's ask, 2026-10-13)*

- **What was asked for:** *"account support for saving"* — a save that follows you between machines
  instead of living on one PC.
- **The shape this project can actually ship, and the line it must not cross:** saves are **the player's
  own data**, not game content, so syncing them is entirely legal — but the service must never touch game
  assets. The account stores **our save format and nothing else**.
- **Design rules, decided now so the feature cannot drift:**
  - **local-first.** Saves always work with no account and no network. The account is a *sync*, never a
    gate. No login, no cloud, no problem.
  - **off by default** (`LAW 12`): the shipped game behaves exactly as it always has until the player
    turns it on.
  - **self-hostable, and free.** The captain does not use paywalled services, so the server must be a
    small thing anyone can run — the same duty the project already has for the applier. A hosted instance
    is a convenience, not a requirement.
  - **per-disc-profile namespacing** from day one, because retail and Spec II saves must never collide
    (`G6.24` already requires this).
- **What it really buys:** the captain plays on several machines; the Odin, the desktop and the Mac each
  having their own half-finished career is the current reality. This is the feature that fixes it.
- **STATUS:** ⬜ registered, not scheduled. Depends on the save-format work under `G6.24`.

### 🆕 G6.30 — **THE LOBBY AND THE AVATAR** *(interactive social hub + custom racing avatars — the captain's own idea, 2026-10-13)*

- **What was asked for, in the captain's words:** *"custom racing avatars and the lobby will be
  interactable… like Xenoverse 2 and Ace Combat"* — a **place** you stand in with other players, as a
  character that is yours, before the race starts.
- **The measurement that makes this a real feature and not a daydream (taken 2026-10-13, from our own
  disc):** GT4 **already ships character models**. The volume root has a `character/` directory on both
  discs, and retail contains `driver.mdl`, `driver2..4.mdl`, `driver_mtr.mdl`, `driver_nike.mdl`,
  `pitman00/01/03.mdl`, `workman00.mdl`, `classic1/2.mdl` — **drivers, pit crew and workers, in the same
  model family the engine already loads for cars.** So an avatar is not a new rendering problem: it is
  the car pipeline pointed at a different node.
- **Why it shares its foundation with the thing this project exists for:** the hard part of both
  `G6.0a` (add a car) and this entry is the **same component — a mesh importer**. Build it once and it
  serves: cars, avatars, pit crews, lobby props. The fence is unchanged and already stated: we ship the
  **converter**, the user brings their own models, and nothing from a disc or a mod enters this repo.
- **The lobby itself is cheaper than it sounds, because the engine is already a 3D world renderer:** a
  lobby is a *scene* — a small course-like space with a camera and a few animated models — and we already
  draw course geometry, cars, lighting, HUD and scripts. The interactivity is UI over a scene: walk,
  emote, look at a car, see who is online, then the host starts the race. GT4's own menu scripts (the
  Adhoc VM) already handle menus over rendered backgrounds, so the plumbing exists.
- **The avatar over the wire is the *same* netcode, and that is the good news:** a moving character in a
  lobby is *also* just position + animation state broadcast at a low rate — the input-exchange layer
  built for racing carries it with a bigger margin. There is no second multiplayer system to write.
- **Honest cost, and the order it has to happen in:** this is **Stage 3+**, and it depends on things that
  do not exist yet — the mesh importer (`G6.0a`), the renderer seam (`G8`), and the online layer
  (`G6.22`). It is also the feature most likely to grow without bound (an avatar creator has no natural
  end), so the first shippable version must be small: **stand in a lobby as one of the game's own driver
  models, see other players, walk around, start a race.** Custom models come after that works.
- **Why it belongs in this project at all:** it is the difference between a port and a place. The
  captain's actual ask, in one line: he does not want to play GT4 alone in 2026 — he wants to be *in* it
  with other people.
- **STATUS:** ⬜ registered, not scheduled — the biggest dream on this list, and the one that most depends
  on everything above it. Written down so it survives contact with a long project.

### 🆕 G6.31 — **PROFILE PICTURES** *(the cheap slice of the lobby dream — the captain's own idea, 2026-10-13)*

- **Where it came from, in the captain's words:** *"GT5 has racer customization but it's only for profile
  pics. We could add profile pics as well."* He is right about GT5, and the observation matters: the
  feature Gran Turismo *did* ship for racer identity was **a picture**, not a body. So this is the slice
  of `G6.30` that needs **zero 3D work, zero netcode and zero new formats** — and it is available years
  before anybody walks around a lobby.
- **The decode is already in this tree:** `third_party/stb/stb_image.h` is present, so PNG/JPG reading
  exists today. A profile picture is therefore: let the player pick an image, scale it once at import,
  store it in the profile, draw it where identity is shown.
- **Where it shows, in order of how cheap each is:** (1) our own launcher / profile screen, (2) the
  GT Mode home area and the save-slot view, (3) the **online lobby** (`G6.22`) where a picture next to a
  name is exactly what a lobby needs, (4) optionally the race HUD or a replay nameplate — that last one
  is *not* cheap to make look studio-made, so it stays optional.
- **The fence, unchanged:** we ship the **loader**, never the pictures. No image from a disc, a mod or
  anywhere else enters this repository — the player brings their own file, exactly like the disc and the
  mods. And a picture syncs with the profile under `G6.29`, which is the other half of why this feature
  is worth having: identities that follow you between machines.
- **An honest correction to my own first guess, kept because it is instructive:** I expected GT4's
  `icon/` directory to hold avatar art. It does not — measured 2026-10-13, it holds **Photo Mode
  overlay frames** (`GT4_film_16_to_9_.ico`, `GT4_Rep_to_license.ico`, and friends). So this really is a
  new UI surface, not a slot the disc left open, and it should be built as one.
- **`LAW 12`:** off by default. A player who wants the authentic GT4 look sees the authentic GT4 look.
- **STATUS:** ⬜ registered, **the most shippable thing on this list after the credits screen** — small,
  self-contained, no dependencies beyond a profile surface to hang it on.

### 🆕 G6.32 — **THE READABILITY PASS** *(the captain's ask, 2026-10-14: "I soon want ponytail to scan the entire Vulcan 4 project engine (opengt4) and make it readable — but not now")*

- **What it is:** make the whole engine readable and editable by a stranger — which is the exact bar the
  GTPlanet thread set for GT4 reverse-engineering work (Dave A, post 12 of 13: *"the code needs to be
  written and structured in a way that other people (not AI) can read, understand and edit it"*). Read
  `LESSONS-FROM-UPSTREAM.md` first: **this is credibility work, not cosmetics.**
- **The tool:** **Ponytail** (`github.com/DietrichGebert/ponytail`, 163,200★, MIT) — the "lazy senior dev"
  skill. Its rules are installed on Cortex as a skill
  (`~/.hermes/skills/software-development/ponytail/SKILL.md`, sha256 byte-verified against upstream). The
  **plugin** half (two Node lifecycle hooks + six slash-commands) is **blocked by Hermes' own plugin
  scanner** on this box ("community source + dangerous verdict", 136 findings, all false positives on
  docs/tests) and was **deliberately not forced** — we want the ruleset, not the hooks. To fetch when
  this starts: `ponytail-review`, `ponytail-audit`, `ponytail-debt` from the same repo.
- **Ponytail's grain points the other way — so use it correctly:** it is *"the smallest change that fully
  solves the task"* and *"deletion beats addition"*. A wholesale rewrite of 78,458 lines is the opposite
  of that, and it is precisely the move that would cost us the only gate we have. So the shape is
  **audit → ranked findings → surgical passes**:
  1. audit the tree and rank what actually hurts a reader (not what merely looks unfamiliar);
  2. fix the two known tells — `ASSIGN_OLD` (**6** sites) gets Nenkai's disassembler cited at each one,
     and the **8** "not rewritten yet / run the original" comment lines, across **6** files under
     `src/core/physics` (plus one related "uses the original clock" line in `src/core/ui_motion.c`),
     get re-worded as *oracle bridges*;
  3. then the biggest offenders, **one file per pass**: `src/core/menu.c` (6,405 lines),
     `src/app/race_game.c` (3,586), `src/app/main.c` (2,642), `src/core/ui.c` (2,480).
- **The measured baseline — we are not inventing a problem:** 78,458 lines of C/H under `src`+`include`;
  mean line length **59** characters; nothing minified; the biggest files carry only 3–8% comment lines.
  The real defect is **density, not formatting**: unnamed struct offsets, magic numbers with no PS2
  address beside them, and long single-purpose functions.
- **THE FENCE — behaviour never changes, and the gate decides:** every pass keeps the **byte-identical
  reference frame** (`md5 68d8eb83dc75b6fe6e03cc2372ca186d`) and the disc-gated tests green (66 register
  without a disc, 304 with one — 288 pass / 16 fail as of 2026-10-13, and those 16 stay honest). A
  readability change that moves one pixel is a regression, not progress. Nothing gets rewritten that the
  oracle has not already vouched for (LAW 13), and every pass is committed as the captain.
- **Why it earns a slot:** this is the one entry that converts the community's suspicion into their
  cooperation — readable code is what makes strangers able to contribute, which is what makes every other
  entry on this list survivable without us.
- **STATUS:** ⬜ registered, **NOT SCHEDULED** — the captain's words: *"but not now."* The two tells in
  step 2 are cheap enough to land with the next commit that already touches those files.

### 🆕 G6.33 — **CUSTOM MAPS** *(new circuits, not just new cars — release requirement, 2026-10-15)*

- **The captain, 2026-10-15:** *"Custom cars and maps should be upon release caine. We have to work on it as
  soon as possible."* → the **map half of the release gate**; the car half is `G6.0`. A platform that can
  only grow its garage is half a platform.
- **Why it is the same fight as cars, and harder:** GT4 has no "install a track" slot either. The course
  list, the event tables, licence tests and the arcade menus all index a **fixed set of courses**, so a new
  circuit needs an *additive course id* plus everything the game's own course data carries — not just
  geometry. `G6.0a`'s discipline (find every place the game assumes N) is the same job with `N = courses`.
- **What a new map actually needs — five pieces, and the tree already reads most of them:**
  1. a **course id** that is additive, never reused — saves and event tables refer to them;
  2. **geometry** — RNW4 scenery + road surface (`course.c` reads the sectors; the RNW4/`COURSE_DATA +BC
     GDGT` placements are parsed in `course_gadget.c` at `00395B60`);
  3. **the AI line** — GT4's opponents drive a recorded path and `course.c` already reads it; without one
     the field is scenery;
  4. **collision and surface rules** — walls, kerbs, off-track grip: the part that decides whether a map is
     drivable or a picture;
  5. **the UI metadata** — name string, minimap, thumbnail, event eligibility, and a start/finish +
     pregrid layout (`course_pregrid.c`).
- **The cheap route first, and it has measured precedent:** port the **game's own** course format from a disc
  the user already owns. Spec II ships **Circuit Valencia, lifted from Tourist Trophy**, inside its xDelta
  patch (`G6.24`) — proof that GT4-family course data is portable. Our version is an **assets-only importer**
  (the shape of `G6.13`'s framework + `G6.25`'s overlay): the user points at a disc they own, we import it,
  nothing redistributes (`LAW 1`, `G7.0`).
- **What can start before the game is playable — the honest reading of "as soon as possible":**
  1. `tools/vol_file.py` already lists and extracts inside `GT4.VOL` on either disc — **list the course tree
     and count entries per course** (this is the survey that produced every number in `G6.24`);
  2. **do not re-derive the format:** upstream credits *"Nenkai's PDTools (MIT), against which the model and
     course formats were cross-checked"* (`UPSTREAM-README.md:130`) — cite it at each site;
  3. run the **fixed-course-count survey** exactly like `G6.0a`, ranked by what breaks first when course
     N+1 appears. **DONE WHEN:** a ranked list with file+line evidence.
- **Honest limits:** geometry is the *easy* half. A course with no AI line, no collision and no id is a
  screenshot, not a track, and one that does not satisfy GT4's own camera/checkpoint/event code cannot even
  be selected. Expect the first playable custom map to be **a re-imported existing course**, not an authored
  Blender track — say so out loud before anyone expects otherwise.
- **The fence:** we ship the loader and the importer; the user brings the disc. No course data, ever, in the
  repository (`G7.0`) — same fence as `G6.23`'s textures and `G6.24`'s Spec II.
- **STATUS:** ⬜ **release requirement**, not scheduled. Blocked on G4 (a running game) for the end-to-end
  feature; the surveys in this entry and in `G6.0a` can start immediately and cost no disc data.

### 🆕 G6.34 — **THE MOD MANAGER, AND MOD PACKAGES AS A SECOND VOL** *(the captain's own design, 2026-10-15)*

- **The captain, 2026-10-15, verbatim:** *"I say we add a mod manager built in. Once u add a car we make
  ANOTHER Vol file with the custom cars and tracks so the game reads from 2 sources. Works!"* → registered
  as the **shipping shape** of `G6.0`/`G6.33`: a built-in manager plus one container per mod.
- **Why the second-VOL idea is right, and better than the loose folder first sketched in the surveys:**
  the RoFS volume reader already exists (`vol.c`), so a mod package is one file we can open with code we
  already have — add/remove is a file operation, not a directory merge, and one mod = one artifact a user
  can share without touching the disc image.
- **THE CORRECTION THAT MATTERS — *files shadow, tables merge*.** Assets overlay cleanly: a mod VOL that
  carries `/car/menu/<newcar>` or `/crs/<newtrack>` simply wins over the disc for that path, because every
  read is path-based through `gt4_vol_read` (`vol.c:340`).
  **Index tables are different.** `GENERIC_CAR.dbt`, `GENERIC_CAR.idi`, `COURSE.dbt`, `COURSE.idi` are each
  read as **one whole file** (`car_model.c:152-153`, `menu.c:2580-2581`) — so *shadowing* a table replaces
  it and the disc's 722 cars disappear. A mod table must therefore be **merged** (our reader appends the
  mod's rows to the disc's, honouring the sort order the bisect requires — `specdb.c:41-48`), or the mod
  ships a **delta** instead of a whole table. Precedent: Spec II itself is an xDelta **diff** of the disc,
  not a replacement (`G6.24`).
- **What the manager therefore OWNS, not just displays:**
  1. **load order and precedence** — with more than one mod, "which one wins" must be a visible, ordered list,
     never filesystem chance;
  2. **id allocation** — two mods both claiming car id 1078 is a real collision. The manager is the only
     thing that can see all mods at once, so it assigns/validates ids;
  3. **removal must never renumber** — a save points at ids, so taking mod A out cannot shift mod B's cars;
  4. **budget meters, told honestly** — the profile holds **128 course keys with 110 already used**
     (`career_home.c:62,64`), so the manager shows *"18 track slots left"* rather than letting the 19th be
     dropped silently, exactly like an event's 60-slot car list (`career_admission.c:28-42`);
  5. **a manifest per package** (name, author, version, what it adds, its ids) — which is also what makes
     the collision checks and the "credit the original author" rule possible.
- **THE FENCE (unchanged, and the manager makes it enforceable):** we ship the manager and the readers, the
  user brings the cars and tracks. Nothing from a mod ever enters the repository (`LAW 1`, `G7.0`), and the
  credits are carried in the manifest rather than lost in a folder.
- **OFF BY DEFAULT (`LAW 12`):** zero mods loaded must mean the disc, byte-identical — the reference frame
  (`md5 68d8eb83dc75b6fe6e03cc2372ca186d`) may not move.
- **THE ROUTE DECISION, 2026-10-15 — the captain weighed two designs and the second one wins:**
  *(1) inject the mods into the disc's own `GT4.VOL` every run*, or *(2) build a second VOL and tell the
  engine to read the original **and** the mod container.* **Route 2 is the architecture.** Route 1 costs a
  full 5.3 GB image rewrite per edit, cannot stack (two mods edit the same bytes, last one wins) and destroys
  the pristine disc; route 2 needs no VOL writer at all — the same reader, one more file, and load order is
  the manager's list. **Route 1's one genuine advantage is distribution:** a patched ISO runs in PCSX2 and
  on real hardware, which is exactly how Spec II ships (a diff, `G6.24`). **Therefore:** author in route 2,
  and offer **export → patched image** later, so a mod can also be shared with people not running this
  engine. One authoring format, both worlds. (This is the same reflex as `G6.25` — never make a second ISO
  to run one.)
- **WRITTEN DOWN IN PLAIN LANGUAGE:** `docs/vulcan4/MODS-DESIGN.md` — the two-source picture, the
  shadow/merge rule, the three steps and the honest limits, in the captain's own words and at his reading
  level. That file is the one to hand to anyone who asks how mods work; this entry stays the ledger record.
- **Dependency:** it cannot be built before the overlay seam exists (`NEW-CARS.md` C1 / `NEW-MAPS.md` M1) —
  the manager is the *face* of that seam, and it absorbs `G6.13`'s community-modding-framework wish.
- **STATUS:** ⬜ registered, **not scheduled**. First honest dish is still C1/M1: one overlay in
  `gt4_vol_read`, read-only and off by default, proven with an existing path. The manager comes after a
  single mod loads — never before.

### 🆕 G6.35 — **THE SDK** *(a program for making cars and tracks, not a wiki of hex offsets)*

- **The captain, 2026-10-15, verbatim:** *"Once we finish those things. I want to build an sdk to make it
  easier for people to mod the game. I'm talking a full on program. That will allow u easy work on tracks
  and cars to make the game work almost natural with them."* — this is the entry that absorbs `G6.13`
  (the community's *"some sort of framework to make it easy to mod the game"*) and gives `G11` (the Assetto
  bridge) its front door.
- **THE DESIGN DECISION THAT MAKES IT POSSIBLE — and it is not "make people write GT4's format":** the game's
  model file is a **command program**, not a mesh (a 256-opcode interpreter, `model.c:158`, whose callbacks
  set wheel and steering attachments at `model.c:306`). Asking a modder to hand-author that is asking them to
  be us. So: **the SDK defines its own authoring format — a plain mesh plus plain parameters — and the
  compiler emits what the engine eats** (or the engine reads the simple form natively through the new loader
  seam). Authors use **Blender** for geometry; we do not build a 3D editor, we build the **bridge**.
- **WHAT THE PROGRAM ACTUALLY IS — five stages, and stage 2 is the product:**
  1. **Import** — glTF/GLB first; Assetto Corsa `.kn5` + `.ini` + `ui_car.json` as the flagship path
     (`G11`), because that library is thousands of cars deep and already modern.
  2. **VALIDATE — the real sell.** The game will judge the car whether we like it or not: a wrong wheelbase
     or tread is *positioned and driven wrong*, a mesh without attachment tags renders with **dead wheels**,
     and lazy physics data "drives like a trolley" (`WISHLIST.md`, `G6.0` item 3). A tool that says **why**
     an import will look or behave wrong is worth more than a converter that silently succeeds. Same for
     tracks: a course with no id, no collision or no AI line is a screenshot.
  3. **Convert** — mesh + textures (the GS swizzle/CLUT path the renderer already understands) + parameters
     mapped into the game's own fields.
  4. **Preview** — show it before the game does, on the engine we already have (the native showroom and the
     course preview exist today: `docs/CAR_CATALOG.md`, `docs/COURSE_CATALOG.md`).
  5. **Package + test** — write the mod VOL (`G6.34`), fill the manifest (name, author, version, ids used),
     and run the acceptance check: does the game list it, load it, and drive it?
- **RULES THE SDK SHIPS UNDER (no exceptions):**
  - **Tauri, never Electron** (the captain's hard rule) — and it is a *local* app: **no accounts, no upload,
    no telemetry.** It reads the user's own disc and their own models, exactly like the frozen-in-stone
    precedent one console down (GT2Recomp converts *your* disc locally in 30-60 minutes and never phones
    home).
  - **The budget meter is part of the UI:** `ALLOW_ENTRY` lists are 60 slots, the profile holds 128 course
    keys with **110 already used** (`career_home.c:62,64`) — the tool shows the remaining slots instead of
    letting a mod quietly fall off the end.
  - **Credit in the manifest, never lost in a folder** — the community's authors are the reason the AC
    library exists at all.
  - **Off by default (`LAW 12`):** a game with no mods installed is the disc, byte-identical.
- **THE FENCE:** the SDK ships **tools**; the user brings **discs and models**. No game asset, no mod asset,
  no converted car ever enters the repository (`LAW 1`, `G7.0`).
- **HONEST HARD PARTS (do not promise past these):** emitting the engine's mesh form (or teaching the engine
  the simple one) is unbuilt; texture swizzle/CLUT writing is unbuilt; car parameters are an **approximation
  across two different tyre models** — never claim parity with an AC car's handling; and for tracks the
  **AI line is still unlocated**, which decides whether a custom map is a race or a time trial.
- **THE TWO ENGINES INSIDE THE SDK (added 2026-10-15, after the format work landed):**
  1. **A compiler** — car mesh (+ textures + parameters) → the game's CAR4/MDLS booklet. Mechanical and
     deterministic: **once it exists, this IS the easy path**, because nothing has to be judged.
  2. **A generator** — track geometry → the `.ad` racing lines. **Not** a conversion: a line has to be
     *drivable*, so this is a quality problem (racing line, braking points, overtaking), not a format problem.
     The good news, measured: the game **computes** height, pitch, roll and surface at load (they are 0 on
     disc), so we author only the line and the four 0x80 blocks — less than feared.
     **Two ways in, and manual comes first (captain, 2026-10-15):** *"Ai lines can be manually made or we can
     make some sort of algorithm that might do it automatically."*
     **— refined the same day into TWO AUTHORING MODES, and the second one is the good one:**
     - **Mode A — DRAW IT.** Mouse-draw the path/race boundaries over the track. Best for quick fixes (drag an
       apex, smooth a corner) and for the lane variants, since you are not driving to place a point.
     - **Mode B — DRIVE IT (the captain's idea, with Satisfactory as the precedent):** *"U choose any car u
       want. And literally drive on the map and the drawer will follow ur car and that way u will draw the
       line."* The app **records** your lap and writes it as the `.ad`.
       **The captain's clarification, 2026-10-15 — and it is the default:** *"The driving part is only for
       recording the line. Not the acceleration or breaks. Unless u want it to."* So:
       - **Default: record the PATH ONLY.** The line is what you drove; braking, acceleration and steering
         stay the game's business, which is also why one line serves a Kei car and an F1 car alike.
       - **Optional: also record the PACE.** The format carries a speed per point, so a recorded lap *can*
         contribute its speeds — a "target pace" mode, useful when you deliberately want the field to run at
         your reference speed.
       - **And the captain's insight about difficulty, worth building around:** *"A skilled driver can make a
         hard ai and make a map challenging"* — a fast, precise hand-drawn/driven line is the difficulty knob.
         **The SDK must therefore make a good line easy to author, not flatten everyone to the same mediocre
         one.** That is a feature, not a side effect.
     - **Scale, so nobody is surprised:** a course needs a *family* of lines, not one — line 4 = pit lane +
       start grid, 6 = rolling start, 1/2 = left/right lanes, 5 = per-race cleared.
  Both live behind one UI with Blender supplying geometry, and the app's own preview uses the engine's existing
  course/car preview (`docs/COURSE_CATALOG.md`) so nobody has to launch a race to see the result.
- **SEQUENCING — the SDK is last on purpose:** it needs (a) the loader seam (`NEW-CARS.md` C1 / `NEW-MAPS.md`
  M1), (b) the mesh contract (`docs/vulcan4/research/format-mesh-contract.md`), and (c) exactly one mod that
  loads end to end. **No UI gets built before one mod loads** — a beautiful tool over an unproven format is
  how this kind of project dies.
- **STATUS:** ⬜ registered, **not scheduled**.

### 🆕 G6.36 — **THE SIMULATION BENCH** *(drive it headless, and let the game judge the mod)*

- **The captain, 2026-10-15, verbatim:** *"Can we simulate the driving experience? Like an ai simulation for
  example we can calculate how much an ai needs to break to make a turn like this or if he will oversteer or
  understeer? Or break too much?"* → **yes, and the reason is structural: the physics is OURS.** This port runs
  GT4's own driving model in C (`src/core/physics/`), so the simulator is the game minus the menus and the
  renderer — not a re-implementation that would only *approximate* GT4.
- **What it turns from opinion into measurement:**
  1. **AI lines** (`G6.33`/`G6.35`): drive a generated line with the real physics and report the truth — apex
     missed by X metres, braking started Y metres early, oversteer/understeer per corner. Then iterate the line
     until the lap stabilises. **The generator gets the game itself as its oracle** (LAW 13).
  2. **Car validation**: the same test corner, driven before and after an import, answers the SDK's real
     question — *"does this thing understeer at 120 km/h, and does it brake too late?"* — instead of only
     checking that the numbers are well-formed.
  3. **It is the same machinery as three entries already on this list:** `G6.20` (the logger supplies the
     numbers), `G6.22` (an online result verified by re-simulation), and `G6.16` (an opponent that drives like
     a person — which requires measuring what a person would do).
- **The report is the product:** lap time, speed trace, braking points, slip angles, and a per-corner verdict —
  deterministic and repeatable, so two versions of the same car are **comparable**. A repeatable lap is the
  gate; a pretty lap is not.
- **HONEST DEPENDENCIES — two, and they are real:** (a) the car must **drive** first (`G4` playability; today
  the engine boots and reads the disc, it does not race); (b) the step must be **deterministic** — same input,
  same lap — which is a design requirement on the scheduler, not a bonus. **And one rule for the code:** the
  physics must never depend on the renderer, or a headless bench is impossible by construction.
- **THE FENCE / `LAW 12`:** it is a tool, off by default, and it changes nothing about how the game plays. The
  bench measures the game; it does not replace it.
- **THE LADDER:** 1. one existing car, one existing course, one repeatable lap from a fixed input trace
  (proves determinism); 2. the same lap with a modded car, and a diff of the two; 3. per-corner verdicts;
  4. drive a *generated* line and grade it.
- **STATUS:** ⬜ registered, **not scheduled** — and it cannot be scheduled before the car drives (G4).

### 🆕 G6.37 — **THE LAUNCHER: the front door** *(one place for the disc, the mods, the options — wearing the game's own clothes)*

- **The captain, 2026-10-15, verbatim:** *"For the entire Vulcan4 itself not regarding the game we will have a
  small launcher where everything is. Choosing iso. Mod browser. All in 1 launcher before u launch the game.
  Cool right? And we keep the design of the launcher as close as possible to the games aesthetics."*
- **What exists TODAY — and the honest gap.** There is already a launcher (`src/app/launcher.c`, with
  `launcher_config.c` for the disc/saves paths, `launcher_icons.c`, `launcher_photos.c`), and it validates the
  disc properly (`launcher_validate_disc` reads `SYSTEM.CNF` and tells the user when a file is not a PS2
  image). But its header states the opposite of this goal, deliberately:
  *"No disc font, image, menu script, audio bank or logo is loaded here. Layout and system font are P: new PC
  choices, not claimed as PS2 fidelity"* (`launcher.c:1-3`) — and it is **Windows-only GDI**. So "match the
  game's aesthetic" is a real direction change, not a tweak.
- **MIMIC THE LOOK — DO NOT READ IT OFF THE DISC (captain's correction, 2026-10-15:** *"Dosnt need to wear the
  gt4s art. We can mimic it."*)** Three reasons it is the better call, and one of them is a bug we would have
  shipped: (1) **the old plan was circular** — a launcher that styles itself from the disc cannot draw itself
  before a disc is chosen, and picking the disc is exactly what it is for; (2) **zero asset dependency** — no
  fonts, textures or logos read from the user's disc, so nothing about the front door depends on a 5 GB file
  being present, valid, or the right region; (3) `LAW 1`/`G7.0` stay trivially satisfied: we ship our own
  drawn UI, and the palette we match is a colour reference the launcher file has quoted since its first
  version (`/projects/option/option.mproject` ScreenRoot: bg `171,171,178`, popup focus `184,132,46`).
  **The line we do not cross:** we mimic the *feel* — flat panels, that grey-and-amber focus language, dense
  type — but we do **not** ship or copy the GT wordmark/logo. The launcher carries **VULCAN 4**'s own mark in
  the same visual language. Mimicry is a style choice; the logo is Sony's property.
- **IT IS A WINDOW, NOT A FULL-SCREEN FRONTEND — the captain's clarification, 2026-10-15:** *"Not a full screen
  launcher. It's a window. It's like. U know Skyrim on pc? Bethesda games in general. They got that launcher
  thing?"* → **the Bethesda pre-game launcher is the model**: a small window that appears before the game
  starts, with `Play` / `Options` / **`Data Files`** / `Exit`. Their "Data Files" is our **mod list** — the
  precedent proves the shape works and that players already understand it. Mouse-first desktop app (no
  full-screen takeover), keyboard-navigable, gamepad optional.
- **What lives in it (the Bethesda layout, wearing GT4's clothes):**
  1. **Disc** — pick the ISO, with the existing validation and a clear message when it is the wrong release;
  2. **Mods** — the manager from `G6.34` (their "Data Files"): tick on/off, order, budget meters, manifest
     credits;
  3. **Options** — everything `LAW 12` requires to be opt-in, in one place, all off by default;
  4. **Saves / profiles** — the path handling and `save.c` work already exist;
  5. **START.**
- **Rules it ships under:** local only (no accounts, no upload), off by default, and **cross-platform** — the
  current Windows-only GDI path must not be the design centre, since the project builds and runs on Linux.
  The engine's own SDL UI is the natural home; this is not a web-app job.
- **Dependency:** it hosts `G6.34` and therefore arrives with it; the disc picker and options exist already, so
  the first real work is the mods page and the asset-driven styling.
- **THE ANDROID TWIN — the captain, 2026-10-15:** *"on android something similar but better controls lol"* →
  the same window, built touch-first:
  - **One codebase, two shells.** The engine's UI is **SDL**, and SDL runs on Android, so this is the same
    launcher with a touch layout — not a second app to maintain.
  - **Touch means bigger targets:** a large Play button, a scrollable mod list with real toggles (not 12-pixel
    checkboxes), no hover states, no small hit areas — the Android build is not the desktop window scaled down.
  - **Storage, handled honestly:** the disc is a ~5 GB file on shared storage, so the launcher asks for it once
    (scoped storage / SAF) and keeps **read-only** access — which the engine's reader already is by
    construction (`vol.c`). No write permissions, no copying the disc into app storage.
  - **Why Android is not a side quest:** the captain already plays GT4 Spec II on an Android handheld, so this
    is the platform where a mod browser earns its keep — pick the disc, tick mods, play, with the same files
    that work on desktop.
  - **Honest sequencing:** the launcher can exist and be useful **before** the game itself runs on Android —
    the two halves are independent, and nothing here promises the renderer works on a phone yet.
- **HOW IT GETS BUILT — the decision and the ladder (2026-10-15):**
  **The stack decision: the engine's own SDL window, not a web UI.** Reasons, in order of weight: the launcher
  must (a) **read the user's disc** to wear the game's art, (b) **configure the mod overlay**, which is
  engine-side state, (c) run on **desktop and Android from one codebase**, and (d) it already exists in C
  (`src/app/launcher.c`). A web wrapper would add a second toolchain *and* an IPC boundary to reach the very
  things the launcher exists to touch — and it still could not read the game's own UI scripts. This is also why
  the Tauri rule (never Electron) does not apply here: that rule governs *web apps*, and this is the inside of
  a C engine.
  **The ladder — each rung is verifiable on its own:**
  1. **Make today's launcher portable.** The current one is Windows-only GDI (`launcher.c:1-3,16-27`). Move the
     window and drawing to **SDL** (already a dependency) and verify the disc/saves/options screen opens and
     works on **Linux**, not just Windows. No new features on this rung — it is a port, and it is the gate.
  2. **Add the Mods page.** Read the mod folder / container manifests, list them, tick on/off, set order, show
     the budget meters, and write the list into the launcher's own config (`launcher_config.c` already owns
     the disc/saves paths — the mod list joins them).
  3. **Mimic the look, in our own drawing.** Restyle the window to echo GT4's UI language — the flat panels,
     the grey/amber focus colours the file already quotes, that dense typography — using **our own primitives
     and our own mark**. No disc assets, so the launcher is fully functional with no disc, wrong disc, or disc
     unplugged.
  4. **The Android shell.** The same binary via SDL's Android entry, plus the touch layout and the one-time
     disc permission from the twin above.
  5. **Start the game** — the launcher hands the disc path plus the mod list to the engine and exits.
- **AND THE LINE BETWEEN THE TWO APPS (so they do not become one blob):** the **launcher is the front door**
  (small, fast, always present), the **SDK is the workshop** (`G6.35`: import, validate, convert, package). They
  share code as a library — the disc reader, the mod format, the converter — but they are not the same window,
  and the launcher must never grow a 3D modelling tool.
- **STATUS:** ⬜ registered, **not scheduled**.

### 🆕 G6.38 — **THE RENDER PRESETS** *(better graphics, Minecraft-shader shaped)*

- **The captain, 2026-10-15:** *"Think we can add better graphics? Like Minecraft shaders?"* → yes, and the
  shape is genuinely the same: a **preset** plus optional packs, not a different game.
- **Why we are further along than it sounds — measured, not hopeful:** the GS (the PS2's graphics chip) cannot
  be recompiled, so **the renderer was always going to be ours**; it is OpenGL 3.3 core with an offscreen
  framebuffer and our own shaders that reproduce the GS's behaviour (alpha as a coefficient, destination-alpha
  test, texture addressing, per-pixel spherical environment mapping for cars — `docs/RENDERER.md`). An
  **upscaler already ships**: FSR1 (`third_party/fsr1`, MIT, embedded at build time), and shadow shaders
  already exist for the studio/showroom.
- **Three tiers, cheapest first:**
  1. **Clean original, sharpened** — resolution, widescreen, FSR upscaling, better filtering/AA. Half built.
  2. **Post-processing** — colour grading, bloom, tonemapping, motion blur, real depth of field. **Photo mode
     is the showpiece** (`G6.30`-adjacent: the game already has the UI for framing a shot).
  3. **Re-lighting, the big one** — real shadow maps instead of the game's faked shadow geometry, environment
     reflections instead of spherical mapping. Touches what the game bakes into its own data, so it is last.
- **THE HONEST CEILING:** shaders polish; they cannot invent detail. GT4's textures are 2005-era, so the
  resolution half belongs to **user-supplied HD texture packs** (`G6.23`) — the renderer can only present them
  well.
- **`LAW 12` GOVERNS THE WHOLE ENTRY:** every effect ships as a **preset, off by default**, with the untouched
  original as the default state — the same shape GT2Recomp ships one console down (*"Out of the box everything
  is off"*). A purist must be able to launch this and see the game they remember.
- **THE UI IS A SEPARATE, EASIER CASE — and half of it is already solved (measured 2026-10-15).** The captain:
  *"We can try to upscale the menu things… it's not like they're that low quality."* Two cases, not one:
  1. **Text is already resolution-independent.** `src/core/glyph_sdf.c` converts the game's glyph atlases into
     **distance fields** (Mitchell-Netravali edge tracing) and the shader reconstructs them — so menus do not
     blur at 4K. Nothing to fix.
  2. **UI *art* is the target, and it is replacement-friendly:** panels, buttons, logos, maps and HUD plates
     ship as **`.gpb` image packages** the game's own scripts load (`loadGpb` → `ui_load_package`,
     `menu.c:766`). Discrete images upscale well, and a community **HD HUD & UI pack** already exists for
     exactly this — taken as the user's own content through the `G6.23` loader, nothing shipped.
  **Honest caveat:** AI upscaling is weakest on **logos and fine line art** (it smears fine geometry), which is
  why that pack's author hand-fixed the text. Expect *much better*, not perfect.
  **Sequencing consequence:** UI-art upscaling belongs in tier 1 beside AA — small, visible, and it raises no
  authenticity question because the files are the user's.
- **THE NO-SLOP GATE — the captain's own condition, 2026-10-15:** *"Think we can make the game look fabulous?
  Without. U know. Slopifying it?"* Answered with a rule rather than a mood, because the difference between
  *better* and *slop* is measurable:
  - **TWO CLASSES, and the class decides the default.** **Accuracy upgrades** — mipmaps and anisotropic
    filtering (road shimmer), MSAA/SSAA, **vertex-snap removal (PGXP-style; GT2Recomp ships exactly this, and
    GT4 wobbles)**, z-stability, frame pacing — make the image *more correct*, never different, and may ship as
    quality options. **Restyle upgrades** — real shadow maps, environment reflections, bloom, grading, motion
    blur/DOF — add a look, so `LAW 12` puts them behind a preset, off by default.
  - **THE GATE, and it is checkable:** every candidate frame is compared **side by side against PCSX2**, the
    oracle we already treat as ground truth. (1) The frame's palette/hue deviation from the reference must stay
    inside a stated threshold. (2) **Any glow, contrast or light the reference does not have is a reject.**
    That kills bloom-everywhere, fake-HDR oversaturation, sharpening halos, chromatic aberration, film grain
    and smeared fake-AO **by rule instead of by argument**.
  - **The floor:** with everything off, the reference frame (`md5 68d8eb83dc75b6fe6e03cc2372ca186d`) does not
    move a byte.
- **WRITTEN DOWN PROPERLY:** `docs/vulcan4/GRAPHICS.md` — the full plan (the accuracy/restyle rule, what exists
  already, the three tiers, the no-slop gate with its reject list, the honest ceiling and the sequencing), on
  the captain's instruction *"Write this shi down."* This entry stays the ledger record; the document is what
  anyone reads before touching the renderer.
- **STATUS:** ⬜ registered, **not scheduled** — tier 1 is mostly existing work, tier 3 is a project of its own.

### 🆕 G6.40 — **THE DEBUG OVERLAY** *(on-screen performance, the captain's ask, 2026-10-18)*

- **What was asked for, in the captain's words:** *"debug overlay. Showing performance for testing."*
- **What it is:** a toggleable on-screen readout for testing — frames per second and frame time (including the
  worst frame, not just the average), the simulation tick's own cost, draw submissions and triangles, GPU/VRAM
  pressure, the renderer in use (`opengl` / `vulkan`), the platform and the device name. It is the *screen*
  half of `G6.20` (THE LOGGER), which writes to a file: same information, different consumer — his eyes, on a
  handheld in a race, not a spreadsheet afterwards.
- **The fence is G6.20's fence, verbatim:** off by default (law 12), and it must not perturb the tick. The
  physics sets round-toward-zero + FTZ/DAZ every frame inside `fegetenv`/`fesetenv` (`race_run.c:753-779`), so an
  overlay that changes arithmetic ordering is measuring something else. **Gate:** with the overlay hidden, the
  reference frame is byte-identical (`md5 68d8eb83dc75b6fe6e03cc2372ca186d`), and with it shown the frame cost is
  printed so it can be subtracted.
- **Why it matters now, not later:** the Android port and the Switch port both live or die on numbers nobody can
  currently see on the device. Every "it feels slow" argument ends the moment the overlay can be switched on.
- **STATUS:** ⬜ registered, **not scheduled.**

### 🆕 G6.41 — **THE MOVIES WITHOUT THE SPARE PROGRAM** *(kill the 161 MB ffmpeg.exe, the captain's ask, 2026-10-18)*

- **What was asked for, in the captain's words:** *"Finding an alternative to ffmpeg.exe. It's literally 13x~
  times bigger than our exe. We need to figure out a different way to display the videos."* — measured and true:
  the BtbN static `ffmpeg.exe` is **168,964,608 B** against a **13,319,343 B** `MoonEngine.exe`, i.e. **12.7×**.
- **What the movies actually are:** the menu's MPEG-2 program-stream pictures the engine plays by **spawning an
  external program** (`src/platform/sdl/movie.c`, Windows `CreateProcessW`). Two consequences: the install ships a
  second huge binary, and on any platform that cannot spawn a program (Android) the movies have no route at all.
- **The route already exists in this tree — on the Switch.** `cmake/SwitchTargets.cmake` drops `movie.c` for
  `src/platform/switch/movie_ffmpeg.c` + `movie_decode.c`, links **libavformat/libavcodec/libavutil** through
  pkg-config and sets `MOON_SWITCH_MOVIES=1`. So the work is not "find a decoder", it is **make the linked-av
  route the route on every platform** and retire the spawned binary.
- **Options, cheapest first:** (1) link libav on Windows as well (a handful of DLLs beside the exe, or a static
  build) and delete the search-order machinery from `movie.c`; (2) ship a **minimal** ffmpeg build — mpegps demuxer
  + mpeg2video decoder only, no filters, no CLI — which is a few megabytes instead of 161; (3) a small standalone
  MPEG-2 decoder library, if it can be proven frame-identical against the current output.
- **Gate:** the same menu frame decoded by the new path must be **byte-identical** to the frame the spawned
  ffmpeg produces today (the comparison harness from the ffmpeg work already exists), and the log must lose
  `movie: using ffmpeg from <path>` because there is no path any more. One file delivered to the player is the
  point; a 161 MB download dressed as a dependency is not.
- **STATUS:** ⬜ registered, **not scheduled.**

## 📱 ANDROID-ONLY

### A1 — **TOUCH THAT DRIVES** *(the captain's ask, 2026-10-18)*

- **What was asked for, in the captain's words:** *"Full touch screen buttons support for controlling the car
  but. Not just emulating a controllers button. I want full on native touch controls buttons (unless controller
  emulation is easier)."*
- **Answer to the parenthesis, measured:** controller **emulation is not needed and is not what is there**. The
  engine has one input abstraction — the pad state — and `src/platform/android/mobile_controls.c` is native touch
  code that fills it: `moon_mobile_contact(phase, id, x, y)` tracks each finger independently,
  `moon_mobile_bounds()` is the control geometry, and `moon_mobile_pad()` publishes the result as the pad state
  the race already reads. Same path as a physical pad, so **no physics fork** (law 3) and no emulated device.
  Presets live in Options → Screen Settings → Controls (`mobile_options.c`, *Classic* keeps pad-only behaviour).
- **What is still missing, and it is measured:** (1) the overlay has not been validated on real hardware — that
  needs the Odin 2, and it is the only judge; (2) **the launcher takes no touch at all**: SDL delivers
  `SDL_FINGER` events with touch-mouse synthesis disabled, while the launcher reads mouse events only (reported by
  the launcher-on-Android job, 2026-10-18, with no second input stack invented). So a phone with no controller can
  currently pick a disc but not press a dock button.
- **The design law for this entry:** touch is a first-class input source feeding the engine's own pad state — never
  a simulated DualShock, never a parallel control path, never a second copy of the layout logic.
- **STATUS:** ⬜ registered — the driving half exists, the device proof and the launcher half do not.
