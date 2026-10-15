# The original game, its improvements and future editions

This document describes the project plan, not a list of completed features.
The faithful reproduction of GT4 (called "Classic" in older notes) is the
current baseline. Improvements, mods and the Tourist Trophy extension are
later stages, built on the same native implementation.

## One game, improved by default (decided 13 October 2026)

There is no separate Classic or Enhanced mode to choose. The game first
reproduces the original; improvements (modern rendering, keyboard and mouse,
new interface items, mods) are then added to the same game and are enabled
by default, each with its own option. The PS2 appearance (image filters,
hard-edged shadows and similar) and the original behaviour of every improved
system remain available as options. An "Original" preset switches all of
them back to the PS2 game at once.

The core stays shared: physics, career rules, disc reading and Adhoc scripts.
Options are consulted only at presentation, input, mod loading and
configuration boundaries, never scattered through game logic. Fidelity is
assessed with the Original preset and no mods.

## PC integration

The game needs PC storage, profile creation, display settings, control
mapping and an exit action. These replace console-specific services and are
recorded as deliberate differences (P). New interfaces should reuse the
original visual language: fonts, frames, cursor behavior, sounds and transitions.

Any new distributed artwork must be original, rather than copied or derived
from the disc. Branded content belongs in separate mods, not the base project.
New cars, courses and other gameplay content are planned as optional mods,
not as changes to the base game.

## Presentation

The Original preset deliberately targets the PS2 appearance. The plan uses
optional filters over a shared renderer: a 640x448 internal image, dithering and
simulated interlacing. Filter parameters require evidence from the original
video setup; visual similarity alone is insufficient. Such filters can earn
C with a declared image tolerance, rather than bit-exact E. Console loading
delays and frame-rate drops are not intended to be reproduced.

The improvements are planned to add modern lighting, shadows and
reflections, higher resolutions and ultrawide displays, enabled by default.
For example, softer car shadows could be the default while the Original
preset retains the hard-edged appearance. These are plans, not renderer guarantees.

The original 16:9 mode belongs to the Original preset too. Its HUD alignment path uses
D_006184F0 and func_003AFD18 (96-unit offsets for alignment bits 4 and 8).
It should follow the original projection and placement logic. Additional
aspect ratios, such as 21:9, are planned improvements.

## Gameplay, versions and storage

Vehicle physics and race rules remain shared and checked against the original.
Visual changes must not silently change lap times or simulation results.
If a system such as rival driving gains an improved version, the validated
original should remain available in a separate module. The Original preset
uses it; an option exposes the version choice. Replays, ghosts
and records would need to identify the selected version because they are
re-simulated. The vehicle physics is not intended to fork this way.

Profiles and settings work with any options. Mod-altered profiles must be
identified so they cannot be confused with unmodified data. Mods are
optional; see [the mod design](MODS.md).

## Tourist Trophy

A later stage would reuse this architecture for Tourist Trophy. Motorcycle
physics must be implemented and validated against that game's executable;
GT4 car comparisons are not evidence for two-wheel dynamics.

## Competitive multiplayer

A longer-term plan is server-validated racing. A headless simulation
would replay player inputs and reject results it cannot reproduce. Offline
career storage would remain local; online garage, credits and rewards would
be server-controlled. Verified car/course data and cross-platform arithmetic
determinism would be prerequisites, not assumptions.

The current design preserves input-driven physics and a headless test path,
with presentation outside simulation. That supports future verification but
does not mean competitive networking is implemented.
