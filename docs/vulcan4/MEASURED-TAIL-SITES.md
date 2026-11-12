# MEASURED TAIL SITES — CONDUCTOR FIELDS AT OR ABOVE 0xCA58

Measured 2026-10-25 from the user's `extracted/core.bin`
(`0x00100000`–`0x006D5DFC`, SHA-1 `38cc7f0d7841d1b75e76ad3678efd813386f65ce`).
Decode: rabbitizer R5900. Register trace: last write before the site, inside the
containing `glabel`. Race-time RAM: the 32 MiB `*.state` captures under
`/mnt/ssd/moon-merge-build/` with `vtable at cond+0x10140 = 0x00679CE8` and
`car count at cond+0xF8A0 = 6`.

Full-image scan for `addiu` (opcode 9) and `addi` (opcode 8) whose 16-bit
immediate equals one of `0xCA58, 0xCAB8, 0xCBD8, 0xCBE0, 0xCCC8, 0xDA50,
0xF818, 0xF830, 0xF858, 0xF860, 0xF898, 0xF8A0`: **38 sites**.

## 1. THE 38 SITES

| addr | word | disassembly | verdict | trace |
|---|---|---|---|---|
| `0x14FDF0` | `0x24C6CA58` | `addiu $a2,$a2,-0x35A8` | ABSOLUTE | `lui $a2,0x0015` at `0x14FDEC`; result `0x0014CA58` |
| `0x2D0CB0` | `0x2442CA58` | `addiu $v0,$v0,-0x35A8` | ABSOLUTE | `lui $v0,0x006A` at `0x2D0CA8`; result `0x0069CA58` |
| `0x2D43B8` | `0x24A5CA58` | `addiu $a1,$a1,-0x35A8` | ABSOLUTE | `lui $a1,0x006A` at `0x2D43A4`; result `0x0069CA58` |
| `0x2D1BF0` | `0x2471CAB8` | `addiu $s1,$v1,-0x3548` | ABSOLUTE | `lui $v1,0x006A` at `0x2D1BE8`; result `0x0069CAB8` |
| `0x21D780` | `0x24E7CBD8` | `addiu $a3,$a3,-0x3428` | ABSOLUTE | `lui $a3,0x0022` at `0x21D76C`; result `0x0021CBD8` |
| `0x2D4AC0` | `0x2450CBD8` | `addiu $s0,$v0,-0x3428` | ABSOLUTE | `lui $v0,0x006A` at `0x2D4AB4`; result `0x0069CBD8` |
| `0x57E6AC` | `0x2444CBD8` | `addiu $a0,$v0,-0x3428` | ABSOLUTE | `lui $v0,0x006D` at `0x57E6A4`; result `0x006CCBD8` |
| `0x57E79C` | `0x2508CBD8` | `addiu $t0,$t0,-0x3428` | ABSOLUTE | `lui $t0,0x006D` at `0x57E798`; result `0x006CCBD8` |
| `0x57ECF4` | `0x244DCBD8` | `addiu $t5,$v0,-0x3428` | ABSOLUTE | `lui $v0,0x006D` at `0x57ECEC`; result `0x006CCBD8` |
| `0x108494` | `0x24E5CBE0` | `addiu $a1,$a3,-0x3420` | ABSOLUTE | `lui $a3,0x0069` at `0x108488`; result `0x0068CBE0` |
| `0x108DDC` | `0x2450CCC8` | `addiu $s0,$v0,-0x3338` | ABSOLUTE | `lui $v0,0x0069` at `0x108DD4`; result `0x0068CCC8` |
| `0x2D7058` | `0x2471CCC8` | `addiu $s1,$v1,-0x3338` | ABSOLUTE | `lui $v1,0x006A` at `0x2D7050`; result `0x0069CCC8` |
| `0x16F488` | `0x24C6DA50` | `addiu $a2,$a2,-0x25B0` | ABSOLUTE | `lui $a2,0x0017` at `0x16F478`; result `0x0016DA50` |
| `0x2DE6FC` | `0x24C6DA50` | `addiu $a2,$a2,-0x25B0` | ABSOLUTE | `lui $a2,0x002E` at `0x2DE6F4`; result `0x002DDA50` |
| `0x2FFD14` | `0x2484DA50` | `addiu $a0,$a0,-0x25B0` | ABSOLUTE | `lui $a0,0x006A` at `0x2FFD0C`; result `0x0069DA50` |
| `0x5C2120` | `0x24C6DA50` | `addiu $a2,$a2,-0x25B0` | ABSOLUTE | `lui $a2,0x0089` at `0x5C2118`; result `0x0088DA50` |
| `0x5C2904` | `0x2450DA50` | `addiu $s0,$v0,-0x25B0` | ABSOLUTE | `lui $v0,0x0089` at `0x5C28FC`; result `0x0088DA50` |
| `0x614CE8` | `0x24C6F818` | `addiu $a2,$a2,-0x7E8` | ABSOLUTE | `lui $a2,0x006D` at `0x614CE0`; result `0x006CF818` |
| `0x14F274` | `0x2451F830` | `addiu $s1,$v0,-0x7D0` | ABSOLUTE | `lui $v0,0x0069` at `0x14F26C`; result `0x0068F830` |
| `0x1CC220` | `0x24C6F830` | `addiu $a2,$a2,-0x7D0` | UNKNOWN | value from `lhu $a2,0x6($v1)` at `0x1CC208`, `$v1=$a0+0x10` at `0x1CC1CC`; not a conductor base |
| `0x26BAE0` | `0x24A5F830` | `addiu $a1,$a1,-0x7D0` | ABSOLUTE | `lui $a1,0x005E` at `0x26BAD8`; result `0x005DF830` |
| `0x35C0B8` | `0x27BDF830` | `addiu $sp,$sp,-0x7D0` | UNKNOWN | `$sp` frame size; not a conductor base |
| `0x3B2708` | `0x27BDF830` | `addiu $sp,$sp,-0x7D0` | UNKNOWN | `$sp` frame size; not a conductor base |
| `0x5495E4` | `0x2624F830` | `addiu $a0,$s1,-0x7D0` | UNKNOWN | `lh $s1,0($s0)` at `0x5493AC`, `$s0=$a0` at `0x5493A0`; loaded value, not a conductor base |
| `0x5F6480` | `0x24C6F830` | `addiu $a2,$a2,-0x7D0` | ABSOLUTE | `lui $a2,0x0089` at `0x5F6478`; result `0x0088F830` |
| `0x5FDD3C` | `0x2450F830` | `addiu $s0,$v0,-0x7D0` | ABSOLUTE | `lui $v0,0x0089` at `0x5FDD34`; result `0x0088F830` |
| `0x5FE938` | `0x24C6F830` | `addiu $a2,$a2,-0x7D0` | ABSOLUTE | `lui $a2,0x0089` at `0x5FE930`; result `0x0088F830` |
| `0x5FEBC8` | `0x24C6F830` | `addiu $a2,$a2,-0x7D0` | ABSOLUTE | `lui $a2,0x0089` at `0x5FEBC0`; result `0x0088F830` |
| `0x144C58` | `0x24C6F858` | `addiu $a2,$a2,-0x7A8` | ABSOLUTE | `lui $a2,0x0014` at `0x144C54`; result `0x0013F858` |
| `0x14F46C` | `0x2451F860` | `addiu $s1,$v0,-0x7A0` | ABSOLUTE | `lui $v0,0x0069` at `0x14F464`; result `0x0068F860` |
| `0x1B99C4` | `0x2463F860` | `addiu $v1,$v1,-0x7A0` | ABSOLUTE | `lui $v1,0x0066` at `0x1B99BC`; result `0x0065F860` |
| `0x1B99E8` | `0x2442F860` | `addiu $v0,$v0,-0x7A0` | ABSOLUTE | `lui $v0,0x0066` at `0x1B99DC`; result `0x0065F860` |
| `0x5FDF34` | `0x2450F860` | `addiu $s0,$v0,-0x7A0` | ABSOLUTE | `lui $v0,0x0089` at `0x5FDF2C`; result `0x0088F860` |
| `0x1BAA24` | `0x24C6F898` | `addiu $a2,$a2,-0x768` | ABSOLUTE | `lui $a2,0x005D` at `0x1BAA1C`; result `0x005CF898` |
| `0x14F664` | `0x2451F8A0` | `addiu $s1,$v0,-0x760` | ABSOLUTE | `lui $v0,0x0069` at `0x14F65C`; result `0x0068F8A0` |
| `0x38FC20` | `0x2463F8A0` | `addiu $v1,$v1,-0x760` | ABSOLUTE | `lui $v1,0x0039` at `0x38FC18`; result `0x0038F8A0` |
| `0x5FE054` | `0x2450F8A0` | `addiu $s0,$v0,-0x760` | ABSOLUTE | `lui $v0,0x0089` at `0x5FE04C`; result `0x0088F8A0` |
| `0x615BC0` | `0x24A5F8A0` | `addiu $a1,$a1,-0x760` | ABSOLUTE | `lui $a1,0x006D` at `0x615BB8`; result `0x006CF8A0` |

Counts: **34 ABSOLUTE, 4 UNKNOWN, 0 COND+imm.** None of the 38 addresses forms a
conductor-relative base. The three `lui 0x0015` / `lui 0x006A` rows are the
`0xCA58` decoys; they must not be patched.

## 2. THE CONDUCTOR'S ALLOCATION

Native engine: `race_run.c:334` allocates one 32 MiB block
(`RAM_SIZE = 0x02000000`, `calloc`) and `gt4_phys_mem` points at it. The
conductor is the fixed address `0x01CF8ED0` (`race_run.c:35`). There is no
separate per-conductor allocation in the native path.

Race-time capture (`vulkan-race-reference.state`, 32 MiB):

- base `0x01CF8ED0`
- last used field `0x00679CE8` at `cond+0x10140` (session vtable), so the used
  tail ends at `cond+0x10144`
- first non-zero byte above the tail: `0x01E32000` = `cond+0x139130`
- bytes `cond+0x10144 .. cond+0x122E3` (0x21A0 bytes): **all zero**
- bytes `cond+0x10144 .. cond+0x13912F`: all zero (`0x128FEC` bytes)
- `0x27` bytes immediately below the base (`cond-0x27 .. cond-0x01`): all zero;
  last non-zero below is `cond-0x28` (`u32 = 1`), so no readable size header
  sits at the base in this capture

The same window (`0x21A0` bytes above `cond+0x10144`) is zero in all 12
`race-reference.state` / `resident.state` captures checked. Measured used size:
`0x10144` bytes. Allocation size beyond that: **UNKNOWN** (no header in the
capture); free measured to `cond+0x139130`.

## 3. THE FREE-FREE CASE — 0xCA58 ARRAY

The array is reached by two accessors; both add the literal `0xCA58`:

- `0x353EB8`: `lbu $v1,0x544($a0)` (player), `lw $v0,0x4($a0)` (session),
  `sll $v1,$v1,4`, `addu $v0,$v0,$v1`, `ori $at,0,0xCA58` at `0x353EC8`,
  `addu $v0,$at,$v0` at `0x353ED0` (delay slot). Result:
  `session + player*16 + 0xCA58`.
- `0x5F4F40`: `sll $a1,$a1,4`, `addu $v0,$a0,$a1`,
  `ori $at,0,0xCA58` at `0x5F4F48`, `addu $v0,$at,$v0` at `0x5F4F50`
  (delay slot). Result: `conductor + player*16 + 0xCA58`.

No site derives the array base from the last state entry
(`cond + 0x6698 + 6*0x10A0`) or from any other already-shifted register. The
constant is always `ori ...,0xCA58`. The array does **not** move for free when
the states move; both `ori` immediates need `0xCA58 -> 0xEBF8`.

In the race capture the array is six entries of `0x10` bytes
(`cond+0xCA58 .. cond+0xCAB7`), first word of each `0x00800000`; the next
array starts at `cond+0xCAB8`.

The only other `0xCA58` uses in the image are absolute:
`0x14FDF0`, `0x2D0CB0`, `0x2D43B8` (section 1) and `0x2F19AC`
(`lui $v0,0x0084`; `sw $v1,-0x35A8($v0)` = `0x0083CA58`).

## 4. VERDICT

**0 of the 38 `addiu`/`addi` sites need the `+0x21A0` shift.**

The shift is carried elsewhere:

- `61` `ori ...,<field>` literals (2 proven for `0xCA58`: `0x353EC8`,
  `0x5F4F48`; 36 have an `a0`–`a3` base directly; 25 are load/computed).
- `157` `lui 0x1` + `addu` load/store sites with a field offset
  (`112` base directly `a0`, `9` `a1`–`a3`, `36` load/computed/`UNKNOWN`).
- `3` genuine `addiu ...,0x6698` car-base sites
  (`0x1572B4`, `0x34BF54`, `0x34C1AC`) for `0x6698 -> 0x7798`.
- `5` `addiu ...,0x10A0` sites; the stride is unchanged, so the shift does
  not change them.

`0xF818 + 0x21A0 = 0x119B8`, `0xF8A0 + 0x21A0 = 0x11A40`: these cannot be
written into a 16-bit `ori` literal. The shifted tail is only addressable as a
positive offset from `conductor + 0x10000` (`0xF818 -> 0x19B8`,
`0xF8A0 -> 0x1A40`).

Tier 1 (7 cars) is therefore a **rewrite**, not a guarded six-site patch:
218 candidate `ori`/load-store sites (157 with a direct argument base), 2
proven `0xCA58` accessors, 3 car-base `addiu` sites, and at least one accessor
shape (`ori 0xF818` / `ori 0xF8A0`) that cannot be re-encoded in place.
