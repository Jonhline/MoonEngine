# THE MOD MANIFEST — one file, read by three things

**Status: implemented 2026-10-16.** The format the engine, the launcher's mod manager and the SDK all agree
on. It lives at the root of the mod folder (or of the unpacked zip) and is called **`mod.txt`**.

## WHY IT EXISTS

The engine can merge a mod's *tables* by itself (`src/core/mod_merge.c`) — a row is a row. It cannot know
what a mod **added**: the Arcade car list is authored script data (`InitData.car_carlist` in
`/projects/boot/US_carlist.adc`), the maker of a car comes from the script's own `CarData` arrays, and a
course is a label in `CourseData`. None of those can be reached by merging bytes, so the mod says what it
adds and the engine appends it (`src/core/mod_manifest.c`, used by `menu.c`). The same file is what the
manager shows and what the SDK writes — one source of truth, no second list to keep in sync.

## FORMAT

Plain text, one directive per line, `#` starts a comment. The first word is the directive, the second its
value; directives that take options carry them as `key=value` after the value.

```
# MOON ENGINE mod manifest
name        Moon's first car
version     1.0
author      Or Golan
description A 1996 hatchback that never shipped in this game. 500 credits.
image       card.png
price       500
car         moon_ka_added maker=ford
course      moon_first_added_track
source      ka_01
```

| directive | meaning | read by |
|---|---|---|
| `name` | what the manager shows as the title | manager, SDK |
| `version` | free text, for the manager's card | manager |
| `author` | who made it — credits travel with the mod (`G7.0`) | manager |
| `description` | one paragraph for the card | manager |
| `image` | picture file beside the manifest, shown on the card | manager |
| `price` | what the added car costs, in credits | SDK (writes it into the car's own row) |
| `car <label> [maker=…]` | **an added car.** The maker files it in the dealership | engine, manager |
| `course <label>` | **an added course** | engine, manager |
| `source <label>` | what it was built from; provenance, never a dependency | manager, SDK |

Anything unknown is ignored on purpose (`LAW 2`: a manifest is written by a tool and read by a person who may
add notes).

## WHAT THE ENGINE DOES WITH IT

- `car <label>` — the label joins the **Arcade car list** when that list is built, resolved through the same
  merged `GENERIC_CAR.idi` the disc's own labels use. If the label cannot resolve, it is skipped, silently —
  a mod that names a car it did not add must not break the game's own list.
- `maker=…` — `GetManufacturer` answers with it for a car the disc's catalog never had, so the car can be
  filed under a brand instead of erroring ("missing manufacturer for <code>").
- Everything else in the file is the manager's and the SDK's business; the engine does not need it.

## WHERE THE PRICE LIVES (not in the manifest)

The manifest's `price` is an **authoring input**: the SDK writes the number into the car's own
`GENERIC_CAR.dbt` row, because that is where the game reads it — the base price is a **float at row +8**
(`menu.c` `GetPrice`, and `00441128`'s input in `career_prices.c:24`). Nothing at runtime reads `price=`; the
row is the truth. That is deliberate: a mod that ships without a manifest still has a correct price.

## WHERE MODS LIVE (the captain's call, 2026-10-16)

**In the user's own Moon Engine folder, not beside the program.** Everything the engine needs from the user
is already there — saves, `launcher.ini`, `video.ini`, the studio rig — so mods join them:

| | |
|---|---|
| the mods | `<prefs>/mods/<mod name>/` (`%APPDATA%\MoonEngine\MoonEngine\mods\` on Windows, `~/.local/share/MoonEngine/MoonEngine/mods/` on Linux) |
| which are on | `<prefs>/mods.ini` — one mod folder name per line, `#` comments, written by the manager |
| how the engine learns | `moon_mod_roots()` (`mod_manifest.c`): `OPENGT4_OVERLAY` when the launcher hands it over, else the tick list. One decision, one place |
| a portable install | a `mods` folder **beside the exe** is still read (the user's folder wins a name clash), so nothing that worked yesterday stops working |

The program's own folder can now stay read-only: the engine writes nothing outside `<prefs>`.

## RULES THAT BIND IT

- **No game data** in the manifest or the mod (`LAW 1`) — it names things, it never carries the disc's bytes.
- **Additive ids only**: `max + 1`, never a reuse (a reuse silently re-points a car an existing save names).
- **Off by default** (`LAW 12`): with `OPENGT4_OVERLAY` unset nothing in this file is read at all.
- The mod folder is the unit: delete the folder (or untick it) and the game is exactly the disc again.
