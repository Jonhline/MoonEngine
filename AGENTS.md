# Moon Engine — the evidence law

Applies to every dish in this project. Read it before you report anything.

1. **The real path, not a probe.** A screenshot from `race_inspect_probe` or any
   offline harness proves nothing about what the captain sees. Evidence must
   come from the same entry path he uses (launcher -> GT Mode -> the screen).
2. **Pixels or it didn't happen.** For any visual change, attach a crop of the
   live screen and the measured difference (changed-pixel count + bounding box)
   against the same crop before the change. A log line saying `visible=1`,
   `attached`, `drawn 32` is NOT evidence.
3. **State what you did not verify.** One line, explicit. "Untested: the page
   under 16:9" beats a silent claim.
4. **A failed gate is a result, not a failure.** Report the measurement and stop
   at the wall. Never dress a gate failure up as success, and never say "should
   work".
5. **Cuts both ways.** If you claim something is impossible, show the measurement
   that proves it (file:line, struct field, hash) — no vibes.

## Launcher law — ONE launcher: the studio room

The captain's front door is the **studio room** (3D car + dock START / OPTIONS /
MOD), reached by opening `MoonEngine` with no arguments (`launcher.c` returns 2 →
`main.c` front-end). That is the only launcher.

`--launcher-v2` (`src/app/launcher_ui.c`, the plain "your own disc - no game
files" icon dock) is the **wrong** one — it is on its way out. Never:
- add a feature to `launcher_ui.c` or to that dock,
- make the plain dock the default,
- leave a second launcher or a second update path alive.

Anything the captain must be able to reach belongs in the studio room.

