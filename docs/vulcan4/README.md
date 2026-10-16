# `docs/vulcan4/` — our side of the documentation

Everything in this directory is **ours** (the VULCAN 4 fork). Everything else under `docs/` — and every
other document in this repository that we did not write — belongs to **upstream OpenGT4**.

The split is deliberate, and it exists for one reason: **upstream sends updates.** When we merge their
work, their files must have stayed exactly where they were. So upstream's fifty-odd documents are never
moved or renamed, and our handful lives here, together, unmistakably ours.

**The engine's own docs are not second-class.** Two of them are binding on us:

- **`../EDITIONS.md`** (Portuguese) — Classic and Enhanced are two modes of one executable; anything
  added must look like the studio made it; mods belong to Enhanced only; the fidelity ledger is measured
  in Classic mode.
- **`../MODS.md`** (Portuguese) — the planned mod format and the assets-only importer. Registered, not in
  progress.

Read those before proposing a feature. `../INDEX.md` lists every document with its language and what it
is; it marks the normative ones.

## What is in here

| File | What it is |
|---|---|
| `OPENGT4-ENGINE.md` | **The fork record.** What OpenGT4 is; the licence position (GPL-3.0 + the section-7 notices); the ten MSVC-isms the Linux port cost; how the delta patch is generated and regenerated; how to update from upstream now that the engine *is* this repository. |
| `SESSION-LOG.md` | **The dated log.** What was done each session, what was measured, what it proved, what is still open — with the commands that reproduce it. The first entry (2026-10-13) covers the fork landing, the Linux bring-up, the test suite going from 304 to 500, and the Spec II container opening. |
| `WISHLIST.md` | **The idea file.** Every idea registered so it cannot drift, and every promise made in public — the captain's, and the community's answers to his r/granturismo4 post. Two rules cover all of it: off by default, and nothing is scheduled before the game is honest. |
| `UPSTREAM-README.md` | Upstream's own README, kept for provenance. Superseded by the root `README.md`, which describes the fork; this one describes what we forked. |

## Where the truth lives

- **Product rules:** upstream's `EDITIONS.md` + `MODS.md`, plus our `AGENTS.md` (the laws) and the root
  `README.md` (status and roadmap).
- **What the engine actually does:** the code. When a document and the code disagree, the code is right
  and the document is a bug — `OPENGT4-ENGINE.md` carries two places where that already happened
  (the "renderer behind an interface" claim, and a symbol tally that was off by one).
- **What the disc contains:** `../analysis/` and the format documents.
- **The retired static-recompilation era:** `archive/vulcan4-recomp/`, with its own index.
