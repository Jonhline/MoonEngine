"""Read-only retail GT4 regulation audit from a user-owned PCSX2 .p2s.
Game memory and screenshots stay local; this script contains no game assets.
Usage: python tools/regulation_savestate_audit.py state.p2s [--negative]
"""
import hashlib
import json
import struct
import sys
import zipfile
from pathlib import Path

path = Path(sys.argv[1])
with zipfile.ZipFile(path) as state:
    ram = state.read("eeMemory.bin")
    spr = state.read("Scratchpad.bin")
assert len(ram) == 32 * 1024 * 1024
u32 = lambda at: struct.unpack_from("<I", ram, at)[0]
f32 = lambda value: struct.unpack("<f", struct.pack("<f", value))[0]

def instances(vtable):
    at = 0
    needle = struct.pack("<I", vtable)
    while True:
        at = ram.find(needle, at)
        if at < 0:
            return
        if at % 4 == 0 and at >= 4:
            yield at - 4
        at += 4

def absolute(widget):
    x = y = 0.0
    seen = set()
    while widget:
        assert widget not in seen and widget + 0x58 < len(ram)
        seen.add(widget)
        rect = struct.unpack_from("<4f", ram, widget + 0x34)
        x = f32(x + rect[0])
        y = f32(y + rect[1])
        widget = u32(widget + 0x54)
    return x, y

# 00215FE0 reads the framebuffer dimensions; 004A2DF8 halves an interlaced Y.
width, field_height = struct.unpack_from("<II", spr, 0x2264)
assert (width, field_height) == (640, 224), (width, field_height)
rows = []
for box in instances(0x66AF28):
    parent = u32(box + 0x54)
    if not parent or u32(parent + 4) != 0x66E898:
        continue
    x, y, w, h = struct.unpack_from("<4f", ram, box + 0x34)
    if x != 0 or w != 100 or h != 55:
        continue
    global_x, global_y = absolute(box)
    left = int(global_x)
    top = int(f32(f32(global_y * f32(448 / 480)) * .5))
    height = int(f32(f32(h * f32(448 / 480)) * .5))
    rows.append({"object": hex(box), "logical_y": global_y,
                 "scissor": [left, left + int(w) - 1, top, top + height - 1]})
rows.sort(key=lambda row: row["logical_y"])
assert len(rows) == 4, rows

# Find actual SCISSOR1 writes preceding the dark regulation ColorFace quads.
packets = set()
needle = struct.pack("<I", 0x33241203)
at = 0
while True:
    at = ram.find(needle, at)
    if at < 0:
        break
    if at % 4 == 0:
        for q in range(max(0, (at - 160 + 15) // 16 * 16), at, 16):
            value, reg = struct.unpack_from("<QQ", ram, q)
            if reg == 0x40:
                packets.add(tuple((value >> shift) & 2047 for shift in (0, 16, 32, 48)))
    at += 4
if "--negative" in sys.argv:
    rows[0]["scissor"][3] -= 1
for row in rows:
    assert tuple(row["scissor"]) in packets, row
print(json.dumps({"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                  "rows": rows, "result": "original object bounds agree with recorded GS scissors"}, indent=2))
