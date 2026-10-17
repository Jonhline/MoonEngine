# THE SDK APP — design contract

**Rules, not taste.** The captain's words: *"make sure it wont be vibecoded garbage. make it corporate and
professional in terms of design."* This file is the checklist the GUI has to pass before anyone sees it.

## WHAT THIS APP IS

A **content-authoring tool**, not a marketing page and not a dashboard toy. The reference class is real
professional software: Blender, Figma, Ableton, a CAD package, JetBrains IDEs. Dense, quiet, systematic.
Information and controls, arranged so nothing has to be hunted for. It is opened every day by the same person.

## THE REJECTED LIST (any one of these = start over)

- Purple/blue hero gradients, neon glows, glassmorphism, blurred translucent chrome.
- Emoji used as UI — icons, buttons, bullets, headings. Emoji are not icons.
- Cards that lift, tilt, bounce or glow on hover. Wiggling anything.
- `transition: all`. One shadow style only, and it is not a glow.
- Random radii (4 → 12 → 32) and improvised spacing. Both are systems here, not choices.
- Fake content: placeholder "Sarah P." rows, lorem Ipsum, dead links, decorative stats.
- More than one accent colour. Colour is meaning (state, error, brand), never decoration.
- Sparklines, gauges, or charts that exist to look busy.

## SYSTEM

**Layout.** Three panes, real draggable splitters, widths remembered between launches: tools rail (left,
208 px default), viewer (centre, takes the rest), inspector (right, 288 px, collapsible with a keyboard
shortcut and a visible affordance). A log pane under the viewer, collapsible, showing the CLI's real output.

**Density.** Row height 28–32 px. Controls 24–28 px tall. Gutters 8 px, groups 16 px, sections 24 px.
A dense tool is not a cramped tool: keep the alignment rigid and the breathing room in the group spacing.

**Spacing.** Strict 4 px grid, no exceptions, no odd numbers.

**Type.** One family for UI (Inter, variable), one for numbers/paths/logs (JetBrains Mono). Ramp, used
everywhere: 11 px labels/units, 13 px body and inputs, 15 px section titles, 20 px app title. Weights 400/500/600
only — never 300, never 700+ headings.

**Colour.** Neutral dark surface (near-black, e.g. `#141414` base / `#1B1B1B` panel / `#242424` raised), text at
two levels (`#EDEDED` primary, `#9A9A9A` secondary), borders `#2E2E2E`. ONE accent used for selection, focus and
primary action. ONE error red, ONE warning amber, ONE success green — and they appear only to mean something.
Contrast of body text on its surface must clear 4.5:1.

**Radii.** Two values: 4 px (inputs, buttons, rows) and 6 px (panels, popovers). Nothing rounder.

**Motion.** 100–120 ms, ease-out, only to acknowledge a state change (hover, focus, open/close). It never moves
a control out from under the cursor and never loops. No attention-seeking animation exists in a tool.

**Icons.** One outline set (Lucide or Phosphor), 16 px, 1.5 px stroke, currentColor. Every tool action has an
icon **and** a label; icon-only buttons exist only in the viewer toolbar, with a tooltip.

**Interaction.** Keyboard-first: tab order follows the panes, `Esc` closes the inspector, `Space`/`Enter`
activates the focused action, and every destructive action asks once. Focus is always visible (`:focus-visible`,
never removed). Long operations show a determinate progress and can be cancelled. Every operation's real CLI
output lands in the log pane, verbatim — the app never paraphrases engine output.

**States.** Every control defines: rest, hover, active, focus-visible, disabled, loading. Every pane defines:
empty (an instruction, not an apology), loading, error (the CLI's own message, plus what to do). No pane is
ever a blank rectangle.

**Validation.** Fields that map to a real table show their target (`DEFAULT_PARTS +0x10`, `ENGINE +0x1a`) in a
monospace hint. A field the engine cannot honour yet is **disabled with a reason**, not silently absent and
never fake-wired.

## THE TWO VIEWS (the captain's design, 2026-10-17)

The centre of the window hosts **views**, Adobe-style: the modern one and the game one, swappable, splittable
side by side, arrangement remembered. Same document, two pairs of eyes.

**THE MODERN VIEWER — where everything is done.** three.js in the webview (already vendored). It shows the
*source* model exactly as authored: orbit, selection by clicking a part, transform gizmos. All authoring lives
here and nowhere else — assigning a mesh as `wheel_LF`, setting the forward axis, the real length, where the
tyres touch, which parts are body — because this is the view where every interactive affordance exists.

**THE GAME VIEWER — only to see what the game will draw.** Read-only, no editing, ever. It must not be a second
renderer we keep honest by hand: it uses the engine's own code.

- **First implementation: the engine renders it.** A "Render in game" action runs the engine binary headlessly
  against the package (the engine already takes a disc, the overlay and a rig, and can save a frame), and the
  app shows that frame. The picture is then, by construction, the game's own output — no reimplementation, no
  drift, and it works on day one.
- **Later: a live native viewport** for orbiting the real thing in the app, linking the same C loader and draw
  code rather than inventing a second picture of reality.

A view that cannot lie to you is worth more than a view that updates faster.

## DELIVERY AND STORAGE (the captain's rules, 2026-10-17)

- **One program file.** The Windows build produces a **single `MoonSDK.exe`** — no sidecar binaries, no
  runtime DLLs beside it, no installer script that leaves parts scattered. The Tauri app carries its own
  front-end inside the binary. (WebView2 is the one external thing: it ships with Windows 10/11 by default.)
- **All of the user's things live under `%APPDATA%\MoonSDK`** — its own root, **not** MoonEngine's. The engine
  and the SDK never write into each other's folders. Layout:

```
%APPDATA%\MoonSDK\
  settings.json        window geometry, pane widths, last used paths
  projects/            one folder per work-in-progress car or track (source files + notes)
  packages/            exported packages, ready to drop into the game's mods folder
  cache/               converted models and images, safe to delete at any time
  logs/                the CLI's own output, one file per run
```

- The SDK never writes to the disc, and never writes into the engine's mod list. Installing a package into
  the game is an explicit button that copies the finished folder into the engine's own mods folder and
  reports the exact path it used.
- Anything under `cache/` must be deletable without warning and without breaking a project — a project keeps
  its sources, not its build products.

## THE ONE QUESTION

If this app could be swapped with any other modding tool and nobody noticed, it failed. It should look like it
was made by the people who made the engine — because it was.
