# ⚖️ LESSONS FROM UPSTREAM — why OpenGT4 died, and the credibility rules this fork keeps

*Written 2026-10-14, the day the upstream project vanished. The captain's instruction: **"we can't fall
like him."** This document is the reason behind LAWS 2, 3 and 4 — read it before making any public
statement, any release, any post.*

## 1. What actually killed the project

Not the AI. **The distance between the claim and the artifact.**

Upstream's README said *"No emulation."* The tree contained `src/ee/` (EE memory, threads, syscalls,
FPU, MMI) and called guest functions by address from the shipped physics sources. Both things were
true — and together they read as a lie, because the sentence described the *product* while the reader
reasoned about the *method*.

The thread's four questions, in order of damage:

| What was asked | Why it landed |
|---|---|
| *"Why does a native port need `src/ee`? Get a non-AI explanation."* | The claim and the tree disagreed. Even the honest answer ("it's just for testing") understated how much the design leans on the original. |
| *"This code is minified to the point of unreadability — magic numbers, unnamed offsets."* | pez2k is friendly and offered help; he still bounced off it. **The code was not addressable by other engineers, so the project could not have contributors.** |
| *"`ASSIGN_OLD` is Nenkai's name from his disassembler."* | A borrowed identifier presented as original. Cheap to fix, and the one thing nobody forgave. |
| *"What proportion did you write yourself, and what's your understanding of GT4's internals?"* | There was no good answer. **This is the credibility death — not the AI use.** |

Outcome: *"I've decided to take OpenGT4 out of the public eye and keep it as a personal hobby
project."* Repository now 404. A native GT4 died of an unanswerable question.

## 2. The rules that follow (binding — these are LAW 2/3/4 made specific)

1. **The claim must equal the artifact.** Never state anything a reader cannot reproduce with a command
   against a build they can make themselves. If the number is not from a run, it is not said.
2. **State the method in one honest sentence, before anyone has to dig.** Ours:
   > *"VULCAN 4 is a native C re-implementation of Gran Turismo 4: it decodes the user's own disc. The
   > original's PS2 code is not emulated in the product — it runs only in the test harness, as the
   > correctness oracle and as the fallback for whatever has not been rewritten yet."*
   That sentence is why `src/ee` and `gt4_orig_call` exist here too, and it removes the entire attack.
3. **A stranger must be able to read this tree.** No minified one-liners carrying three operations, no
   magic number without its meaning, no struct offset without a name, and every behavioural claim cites
   the PS2 address it came from (`0x00349418`, `conductor+0x10140`). **That citation habit is the
   product's differentiator** — it is what turns "AI wrote it" into "measured, and here is where."
4. **Attribute every borrowed name, tool and finding.** Nenkai's disassembler, GT Tools, PS2Recomp,
   pez2k's and TheAdmiester's observations, upstream OpenGT4 itself (`NOTICE`, *"Based on OpenGT4"*).
   Attribution costs one line and buys the only currency that matters here.
5. **A named human must be able to explain the internals out loud.** The captain's own touches are the
   answer to *"what proportion is yours?"*. No feature ships that the captain cannot describe the
   mechanism of.
6. **Never answer criticism by retreating.** Upstream's ending was silence. Ours is a measured reply:
   reproduce the claim, show the number, fix the gap, publish the fix.
7. **Publish only from a state that survives the hostile-reader checklist (§3).** Green on the checklist
   is the precondition for a public post, not a follow-up task.

## 3. The hostile-reader checklist — run before ANY public release or announcement

Every line is a check that was actually used against upstream. Each must be answerable with a command
and a number, on a build the reader can make:

| # | Check | Command / evidence | Today |
|---|---|---|---|
| 1 | Does the shipped binary contain guest code? | `nm opengt4 \| grep -c " T func_"` → **0** | ✅ 0 |
| 2 | Is the emulation really harness-only? | shipped `opengt4` 5.7 MB with **zero** `ee_*` symbols vs `physics_run` 25.1 MB | ✅ measured |
| 3 | Is `gt4_orig_call` NULL in the product? | `nm opengt4 \| grep -w gt4_orig_call` → `.bss` (zero-initialised) | ✅ |
| 4 | Any unattributed borrowed identifier? | `grep -rn "ASSIGN_OLD" src` → **6** sites, unattributed | ⬜ **owed** |
| 5 | Is any line unreadable? | mean line length **59** chars, `src/core/menu.c` largest at 6,405 lines | ✅ |
| 6 | Comment lines that admit un-rewritten paths? | 8 x "not rewritten yet / run the original" — must read as *oracle bridges*, not unfinished porting | ⬜ owed |
| 7 | Can a stranger build it? | `cmake -S . -B /mnt/ssd/... && cmake --build` → `EXIT=0`; 66 tests without a disc, 304 with one (288 pass / 16 fail, 2026-10-13) | ✅ with the 16 honest failures stated |
| 8 | Is the licence obligation visible to users? | *"Based on OpenGT4"* in docs ✅ — in the **UI still owed** (`grep -rn "Based on OpenGT4" src` = 0) | ⬜ owed |
| 9 | Is the fidelity gate a measurement, not a vibe? | reference frame byte-identical, `md5 912bdc63bddc791afece574c7475eac8` (GL) / `531acb651f5c8552f3540e9a57fdf08f` (Vulkan) | ✅ |
| 10 | Can the captain state §2.2's sentence unaided? | rehearse it once, out loud | ⬜ |

**Two of these are code chores and they are small:** attribute `ASSIGN_OLD` (6 sites — cite Nenkai's
disassembler; do **not** blind-rename, the name encodes a real version distinction), and re-word the 8
fallback comments so they read as oracle bridges. Both land with the next commit that touches those
files, not in a hurry.

## 4. What we say in public, printed once so it cannot drift

> VULCAN 4 is a native C re-implementation of Gran Turismo 4, forked from OpenGT4 with the author's
> permission. Code only — you bring your own disc. It is not an emulator: the game's own logic and
> physics are re-implemented in C, with the original PS2 functions used in the **test harness** as the
> oracle that every rewrite is measured against, and as the fallback for anything not yet rewritten.
> Every behaviour we claim cites the PS2 address it was verified at. What is not verified says so.

If a sentence cannot sit next to that paragraph, it does not get published.
