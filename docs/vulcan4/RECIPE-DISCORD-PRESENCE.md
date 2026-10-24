# RECIPE — Discord Rich Presence (tier 1: presence only, no SDK, no helper)

**Goal:** Moon Engine shows what you're playing on Discord — course, field size, speed, mode — using
Discord's local IPC. No SDK, no extra DLL, no child process, nothing shipped but our own code.
**Status:** recipe only. Off by default. Nothing fires until the captain says go.

---

## 0. What is authorised, and what is not

- ✅ Rich Presence over IPC (`SET_ACTIVITY`), handshake with a client id.
- ❌ No Discord Social SDK, no OAuth account linking, no friends list, no DMs, no voice, no joins.
  Those are a separate project and they need a helper process for GPL reasons (see ARMSX2's pattern).
- ❌ Never block, never stall, never retry-storm. If Discord is absent this feature does not exist.

**The only external input needed:** a Discord **Application ID** (Developer Portal → New Application).
The captain creates it; the id lives in our config as a string, never hard-coded in a commit message.

---

## 1. Protocol — exactly this, verified against Discord's own docs (2026-10-22)

**Transport**

| Platform | Path |
|---|---|
| Windows | `\\?\pipe\discord-ipc-{n}` |
| Linux/macOS | `${XDG_RUNTIME_DIR}/discord-ipc-{n}` → `$TMPDIR` → `$TMP` → `$TEMP` → `/tmp` (first that exists) |

`n` = 0..9, first that connects. Unix = `AF_UNIX` stream socket; Windows = `CreateFileA` on the pipe.

**Frame format:** little-endian `uint32 opcode`, `uint32 length`, then `length` bytes of JSON.

**Opcodes:** `0` HANDSHAKE, `1` FRAME, `2` CLOSE, `3` PING, `4` PONG.

**Handshake (before anything else):**
```json
{"v":1,"client_id":"<APPLICATION_ID>"}
```
Discord answers with a FRAME carrying `READY`. Presence may be set after that.

> Discord's RPC page says "you must be authenticated" — that applies to privileged commands
> (`GET_GUILDS`, voice, etc.). `SET_ACTIVITY` for your own client id works from the handshake alone;
> that is how the whole open-source RPC ecosystem works (pypresence, discord-rpc).

**Presence:**
```json
{"cmd":"SET_ACTIVITY","nonce":"<uuid-v4>","args":{"pid":<pid>,"activity":{
  "details":"Tsukuba · 5 cars",
  "state":"Lap 3/5 · 312 km/h",
  "timestamps":{"start":<unix-seconds>},
  "assets":{"large_image":"moonengine","large_text":"Moon Engine"},
  "instance":false}}}
```
Clearing on exit: `{"cmd":"SET_ACTIVITY","args":{"pid":<pid>,"activity":null}}`.

**Limits that shape the code:** Discord throttles presence updates (~5 per 20 s). So:
- build the string, **compare with the last sent string**, send only on change;
- hard floor of **15 s** between sends, plus a 60 s keepalive if nothing changed;
- drop to a **1 s tick**, never per frame.

---

## 2. Files and seam

**New:** `src/app/discord_presence.c` + `src/app/discord_presence.h` (platform code lives in the `.c`
behind `#ifdef _WIN32`, so the engine stays one binary and one build target).

```c
void  discord_presence_configure(int enabled, const char *app_id); /* startup, saved settings */
void  discord_presence_tick(const gen_presence *p);                /* once per second, cheap */
void  discord_presence_shutdown(void);                             /* clear + close */
```

```c
typedef struct {
    const char *course;   /* "Tsukuba"                         */
    const char *mode;     /* "Arcade" / "GT Mode" / "Time Trial" */
    const char *car;      /* player's car, already a label      */
    int   cars;           /* field size                          */
    int   lap, laps;      /* 0 when unknown                      */
    int   speed_kmh;      /* from CAR_SPEED (car.h:75) * 3.6     */
    int   paused;
} gen_presence;
```

- **Call site:** the main loop, guarded by a 1 s timer next to the existing per-second work in
  `main.c` — never inside `race_game_draw`, never in the physics step.
- **Data:** the accessors already exist (`gt4_race_run_cars`, course name, HUD speed). Do not reach
  into guest memory from this module; it takes the struct above and nothing else.
- **Config:** two store keys — `integrations.discord_presence` (`0`/`1`, default `0`) and
  `integrations.discord_app_id` (string). Both read through the existing options store.

## 3. Failure rules (these are the acceptance tests' other half)

1. No socket on any of the ten paths → `disabled_until = now + 60 s`, one `stderr` line at most,
   no error dialog, no frame cost.
2. Socket closes mid-run (Discord quit) → close cleanly, same 60 s backoff, game unaffected.
3. Every write is non-blocking with a tiny timeout; a full socket buffer must drop the update, not wait.
4. JSON: escape `"` `\` and control chars in any string we take from the disc; cap the whole payload
   at 2 KB.
5. Flag off → the module's tick is not called at all. Zero syscalls.

## 4. What a chef must deliver

- The module + the config keys + the 1 s call site.
- A **unit test for the encoder** that does not need Discord: create a `socketpair`/temporary
  listening socket, run `discord_presence_tick` against it, and assert the bytes are
  `opcode=1, length=N, JSON` with the right fields and a matching length.
- A **negative test**: no socket present → tick returns in < 1 ms and sends nothing.
- Build every target; run `ctest -R` on what it touched.
- Commits authored `Or Golan <or024662@gmail.com>`. **Never push.**

## 5. Captain's checklist (what he must do, once)

1. https://discord.com/developers/applications → **New Application** → name it **Moon Engine**.
2. Copy the **Application ID** → paste it to Caine (or type it into the options screen once it exists).
3. Optional: Rich Presence → Art Assets → upload one square image, key it `moonengine`.
4. Enable the feature in settings (off by default), start a race, look at the profile card.

## 6. Done when

- With Discord running: the card reads *Moon Engine* with course + field size + speed, and updates as
  the race changes without spamming.
- With Discord closed: the game runs identically; one log line; nothing retried in a loop.
- With the flag off: no socket is ever opened.
- Frame time unchanged (print the frame-time delta on/off, like the other opt-ins).
