# WORKING WITH JONHLINE — the collaboration plan (measured 2026-10-15)

The captain: *"we will from now on be collaborators of this project… everything vulcan4 related that we have
right now should stay but once we finally do actual pushes we should push here… we dont know how are we gonna
make sure we wont work on top of others work or merge or clash… just dont commit or push to this repo yet"*

**The repo:** `github.com/Jonhline/MoonEngine` (**renamed by him from `.../OpenGT4` on 2026-10-15 18:50Z** —
same repository, same commits) — **private**, **GPL-3.0**, default branch `main`, HEAD `c1ca1290`, one tag
`v0.2.0-alpha`, pushed 2026-10-15 18:11Z. Nothing has been pushed to it and nothing will be without the
captain's word.

**2026-10-16 — THE NAME IS SETTLED, AND IT CAME FROM HIM.** The rename is the **repository name** only: HEAD is
still `c1ca1290` and no new commit landed with it, and his pushed tree carries no `moon` string anywhere (checked
with `git grep -i moon` on the fetched branch). So the name question this document left open is answered — the
joint project is **Moon Engine**, and *our* tree already carries it on every surface (window titles, launcher
strings, log lines, user-data root, `moonengine` binary) in commit `c49bfc4`. The first PR that touches
`CMakeLists.txt` or `src/app/` also carries the name onto his side.
Our git remote points at the new URL and was renamed with it: **the remote is `moonengine` now** (it was
`opengt4`), so an older command like `git fetch opengt4` reads as `git fetch moonengine`.

## WHAT THE TWO LINES ACTUALLY LOOK LIKE (measured, not guessed)

| | |
|---|---|
| merge base (our last shared commit) | `736124f` — 2026-10-12, *"GT Mode: races use the transmission chosen in the settings"* |
| he is ahead by | **1 commit** — `c1ca129` *"Update to the current development state"* |
| we are ahead by | **606 commits** |
| that one commit is | **763 files, +151,210 / −63,014** — his whole working tree in a single commit |
| files BOTH sides touched | CMakeLists.txt, README.md, .gitignore, `src/app/{launcher,main,race_game,video_settings}.c`, `src/core/{vol,menu,career_original}.c`, `src/platform/sdl/video_gl.c`, `tests/physics/race_setup.c`, `tests/ui/outside_menu.c`, `docs/MODELS.md` |

**He is ahead on the launcher, by a lot.** His `src/app/` already contains `launcher_audio.c`,
`launcher_fsr.c`, **`launcher_skin.c`** (a theming system) and **`launcher_update.c`** (an updater), plus
`tools/publish_update.py`, `tools/configure_update.py` and an Inno Setup installer script. So our dock work is
**not** "our launcher vs his" — it is a piece that should plug into *his* skin/framework.

## THE MODEL: HIS REPO IS THE TRUNK, OURS IS THE WORKSHOP

1. **We never push to `main`.** Every piece of ours arrives as a **branch + pull request** — small, reviewable,
   and reviewed by him. That is the whole answer to "how do we not work on top of each other".
2. **Our 606 commits do not go across.** Most of them are our internal material (W-numbered missions,
   `docs/vulcan4/`, `.auto/`, the AGENTS.md laws). A collaboration ships *features*, not a history dump — and
   his own line is already a single squashed commit, so a 606-commit history would be unmergeable noise.
3. **Rebase onto his `main` before starting anything**, and keep branches short-lived (days). A branch that
   lives a week is a conflict.
4. **Local:** `git branch jonhline-baseline c1ca129` is created (local only). That branch is *the shared truth*
   to cut our patches against; do not develop on it, do not push it.

## THE RULES TO AGREE WITH HIM (this is the part to send him)

- **Branch per feature, PR into `main`, never a direct push to `main`.**
- **Area ownership**, so the person who knows a file reviews changes to it. Proposed split, for him to accept
  or change: **he keeps the recomp core, the emulation and his launcher framework**; **we own the UI/dock, the
  mods system, the docs** — as PRs.
- **No wholesale reformatting and no mass renames.** One reformat touches every file and guarantees conflicts
  forever. (We did exactly that once, in our own repo, and it is why our tip no longer shares a base.)
- **A `CODEOWNERS` file plus CI on PRs**: the test suite runs on every PR, so a change that moves the
  reference frame or breaks the disc-gated tests is caught before it lands, not after.
- **Small commits, plain messages.** His single "Update to the current development state" is exactly what makes
  this hard to reconcile; both of us should stop doing that.

## WHAT THIS MEANS FOR VULCAN 4 (the honest version)

- **The engine work merges with his line.** The launcher we built, the mods system and the docs are the parts
  we bring.
- **The VULCAN 4 name is settled (2026-10-16): the shared project is MOON ENGINE and the old name is retired**
  — the shared name won, and it was his rename that made it official (see the status block at the top). What is
  still ours to do: `branding/` is a whole VULCAN identity (wordmark, letterforms, marks) and needs a Moon Engine
  one.
- **First technical step, when he says go:** replay our launcher delta on top of `jonhline-baseline` and open
  it as a small PR, so a real change lands in his repo and the workflow gets tested with something harmless.

## THE FIRST PR IS PUSHED AND MERGED (2026-10-16) — **PR #13**, and the PR model is dead

**Standing rule from the captain, 2026-10-16:** *"pull request? we dont neeed em. me and jonhline are
literaly teammates. we work together"* → and then, sharper: *"just no need for a pull request just push
when i tell u."* — so: **push straight to `main`, and only when the captain says push.** No branches-for-
review, no PR ceremony, and no unpushed-until-told limbo either: when he says go, it goes to `main`
directly. Commits stay small and plain so the other side can read them as they land. (What still stands:
we never push on our own initiative, our 606-commit internal history never crosses, and the GPL notice
stays.)

**PR #13 landed at `2026-10-16T05:33:25Z`** (merged from our own account, `20333874`) — his `main` now
carries both commits: `32bbf82` (the Linux bring-up) and `37e06cb` (the Linux build docs). Verified on
his tree, not on ours: `main` = `20333874`, 264 `OPENGT4_CORE_STACK` / `OPENGT4_SDL_STACK` uses and
`src/platform/common/msvc_compat.h` present. The redundant `linux-support` branch was deleted from his
repo afterwards.

Boss: *"can u commit the linux support that u built?"* — done as a real contribution commit rather
than a patch: branch **`linux-port`**, `32bbf82`, **parented on his `c1ca129`**, 4 files
(`CMakeLists.txt`, new `src/platform/common/msvc_compat.h`, `tests/ui/outside_menu.c`,
`tests/physics/race_setup.c`), plus a second commit `37e06cb` adding the Linux section of
`docs/BUILDING_AND_TESTING.md`. Pushed as branch **`linux-support`** and opened as
**https://github.com/Jonhline/MoonEngine/pull/13** (OPEN, MERGEABLE, 5 files, +358/−262). The
ball is in his court; nothing of ours touches `main`.

Verified on Linux/GCC in a throwaway worktree (`/mnt/ssd/moon-baseline`, build tree
`/mnt/ssd/moon-baseline-build`): build `EXIT=0` for every target, **102/102 ctest pass**, and his
engine opens the captain's GT4 v2.00 image and draws the arcade menu. The ready-to-paste PR body,
the item-by-item "what his tree already had" measurement and the repeatable procedure are in
`docs/vulcan4/PR-LINUX-SUPPORT.md`.

This is the workflow test the plan asked for: one small branch, one reviewer-visible diff, no
history dump, nothing on `main`.

## FETCHED 2026-10-16 — NO COMMITS, BUT A WORKFLOW APPEARED

Boss: *"make sure to fetch or reclone bc he made some commits."* Measured, not assumed:

- **`git fetch moonengine` brought nothing.** His `main` is still **`c1ca129`** (pushed 2026-10-15
  18:11Z; `pushed_at` unchanged), **one branch**, one tag. `gh api .../branches` → `main` only. So there
  is nothing to rebase and our `linux-port` commit still parents cleanly on his tip. No fork exists
  (`forks: 0`), and `gh repo list Jonhline` shows one repository.
- What *did* happen: **he turned the repo into an issue-driven workflow** (repo events 2026-10-15
  20:17–21:37Z are `IssuesEvent`s, not pushes). 12 issues exist; the closed ones (#1, #2, #3, #5, #6,
  #10) are his own integration checks — *"Verify Moon Dev deployment and private issue workflow"*,
  *"Temporary coordination validation task"* — and every open issue carries a machine-readable
  tracking block: `Area`, **`Responsible Discord user ID`**, `Work status`, `Approval`, `Evidence`,
  the commands `/scope` and `/build-info`, and the footer *"Approval and completion are separate. No
  push is authorized by this issue."* Each body ends with `<!-- moon-discord:<guild>:<channel> -->`
  — guild `1560371674434641952`, one Discord channel per issue.

| # | area | responsible Discord ID | who |
|---|---|---|---|
| 7 custom-car workflow | SDK | 1202732535482814534 | **the captain** |
| 8 custom-track workflow | SDK | 1202732535482814534 | **the captain** |
| 9 original-game blockers / fidelity evidence | Fidelity | 430414457152339978 | him |
| 11 Vulkan implementation | Rendering | 430414457152339978 | him |
| 12 New Launcher | Launcher | 430414457152339978 | him |

- **He says Vulkan is already running** (comment on #11, 2026-10-15 21:37Z): *"Already playable, but
  still experimental… on my GTX, it struggled a little to maintain 60 FPS."* That work is **not on
  GitHub** — his tree has not moved. So "he made commits" most likely means that work sitting on his
  machine. Nothing for us to pull.

**What it means:** our side is the **SDK area** — exactly the captain's goal (a car that was never on
the disc). Watch the tracker before claiming an area; his issues are the coordination surface now, and
they forbid pushes on their own authority, so a PR still needs his review.
