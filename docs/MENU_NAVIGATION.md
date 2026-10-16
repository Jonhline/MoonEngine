# Menu navigation

Implementation: `src/core/menu.c and src/platform/sdl/pad_sdl.c`.

Original Adhoc scripts choose destinations and handle key events. The native bridge maintains pages, focus, widget state and deferred callbacks. Input bubbles through widgets and page handlers; project changes occur after script callbacks. Host input is translated to the key symbols expected by the scripts.

## Evidence and limitations

The [development record](notes/MENU_NAVIGATION.md) retains addresses, experiments,
test results and outstanding limitations. Older entries describe the state
at their recorded date and may be superseded. Consult [Fidelity](FIDELITY.md)
for per-item grades; implementation coverage is not full-game certification.

## Car and course favorites

Every menu project exposes the original MFavorite interface on `main.game.favorite_car` and `favorite_course`: `clear`, `has`, `append`, `remove`, `buildLabelList`, `getSize`, `isFull`, and the original no-op `dump`. Each list holds 32 distinct 64-bit SpecDB keys in insertion order. Removing a key compacts the list and fills its former last slot with UINT64_MAX. Labels resolve through GENERIC_CAR, TUNED_CAR, then COURSE; the returned label list preserves insertion order.

The PC profile keeps `max`, `count`, and 32 `code`/`table` entries, also used by PS2 save import. Native methods are rebound after profile restoration and missing lists are initialized. Successful mutations mark the PC profile dirty so ordinary exit autosave captures the lists even when no option was changed; duplicate additions leave the profile clean. The favorites oracle compares the original constructor, list operations and serialized payload byte for byte. The disc menu test exercises the authored course toggle, Arcade list construction and save reload; both tests have deliberate failing variants.

## Arcade race return and shoulder buttons

Leaving the Arcade quick menu reloads the page saved in `major_menu_page` by the original `GameRoot` scripts. Normal selection returns to `CategoryRoot` (track type); favorites-only selection returns to `CourseRoot`. `ArcadeProject.onLoad` uses this page when `menuRef` retains a selected game mode, otherwise it opens `TopRoot`. The application reloads the Arcade project and copies shared game data plus its transient selection stack into the fresh VM. The outgoing hidden cursor, widgets, actors and showroom script locals are discarded. It then uses this authored destination instead of the previous car page. `arcade_return` compares both destinations against the disc's selection and reload bytecode, with a negative case that restores the wrong car destination.

L1 and R1 produce distinct key symbols `0xFFC8` and `0xFFD2` from `D_006192E4`. They are offered to the authored key handlers before the native root fallback. The original fallback `func_00235680` does not move focus for these two keys, and `quick-arcade` supplies no handler for them: they do not jump to the first or last pre-race icon. `pregrid_shoulders_oracle` executes that original fallback, checks the disc key table, and checks unchanged focus for each available icon before and after completion. Its negative case injects a directional move. Other pages can define their own shoulder-button actions; this proof is scoped to the generic fallback and Arcade pre-race.

`arcade_return` also covers the hidden-cursor return, preserved mode stack, cleared showroom locals, and authored CategoryRoot → CourseRoot → MakerRoot → CarRoot navigation with a populated car list. Dedicated negative cases hide the new cursor and empty that list. This is a state/navigation integration check; full PS2 transition imagery remains outside it.

## Arcade specification line and rebuilt list selection

`CarRoot.set_car_info` (arcade.ad L4116..4125) writes drivetrain, catalog power/RPM and torque/RPM into its ToolTipFace. The focus adapter preserves that scripted line when an item has no translated tooltip. Selection changes invoke the original `focus_callback(context, current, previous)` contract from `002B29C0 +0x9CC..0xA58`; Arcade registers `set_car` through this callback at L4077. This keeps the displayed car and specification line synchronized.

`setItemCount` clamps the current selection to the new list bounds, following `002B4A00 +0x2D0 -> 002B3AF8 +0x38..0x84`. An empty list has index -1; refilling selects zero. The authored car-page teardown empties its list, so opening another maker starts with its first car. Lists that retain their contents and explicit script-selected indices keep their original selection behavior; pre-race focus is not globally reset.

`arcade_return` exercises the first specification line, subsequent car changes, and changing makers after selecting several cars. It compares each automatically updated line with the original script's explicit `set_car_info` output. Negative cases corrupt the line and restore a stale third-car index. This checks integration and state at zero tolerance, not complete PS2 text rasterization or every catalog formatter.

## Race pause confirmation

The PC race pause accepts Cross/A and Circle/B for the focused movie button,
for either player. This requested input adaptation is separate from the
regional menu key mapping. Result screens and pit controls retain their
existing Circle behavior. pause_buttons uses virtual SDL controllers and
the actual pause poller; its wrong-button negative must fail.

## Arcade course-card movies

The original Arcade initialization calls `MMovieFace.loadIpic("course")`
(`arcade.ad` L30). `/mpeg/main/course.ipic` supplies named still pictures
while a new movie is prepared. `001FD528 +0x3C..0x68` loads the archive;
`002771C8 +0x0C..0x20` and `00277228 +0x38..0x70` define its count,
32-byte entries, names and picture offsets.

The PC movie worker decodes these pictures once, outside the render thread.
Most are 640x448; category and Akasaka are 640x512 and are decoded separately
before scaling to the menu viewport. The selected picture is usable immediately,
so course navigation no longer waits for PSS decoding or the video blend.
A single GPU texture is updated from the cached pictures. The compressed
archive and decoded frames remain local runtime data from the supplied disc.

`movie_ipic` compares every batched picture with an independent decode and
runs the actual MovieFace draw bridge through 112 selection changes while
no PSS decoder is available. Removing the archive is the deliberate negative.
This proves the readiness/selection path, not the entire PS2 movie raster or
a frame-time bound on every machine. Cold UI resources can still load separately.
