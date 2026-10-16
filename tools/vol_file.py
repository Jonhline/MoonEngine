#!/usr/bin/env python3
"""Read files out of GT4.VOL (the game's own container) from the user's own disc.

Usage:
  python tools/vol_file.py <disc.iso|GT4.VOL> ls [DIR]
  python tools/vol_file.py <disc.iso|GT4.VOL> cat FILE [OFFSET] [BYTES]
  python tools/vol_file.py <disc.iso|GT4.VOL> extract FILE OUT_PATH

Reads only. Nothing is uploaded, and nothing from the disc is ever committed.

Format notes (docs/VOL_FORMAT.md; the two measured differences below are why this
script exists):
  * the index page is NOT at a fixed page. Retail v2.00 (SCUS-97328) keeps it at
    page 0x2159; the Spec II / online-beta disc (SCUS-97436) keeps it at 0x22b7.
    Page 0 carries the RoFS magic on both discs and is not the index, so the
    discriminator is the declared page size (0x800), not the magic.
  * a leaf page's u16 count field has a FLAG in its low bit: retail always clears
    it, Spec II always sets it. The entry pair count is count >> 1.
"""
import struct
import sys
import zlib
from pathlib import Path

SECTOR = 2048
PAGE = 0x800
MAGIC = 0xACB990AD
OFFSET_KEY = 0x14AC327A
TOC_XOR = 0x55
DATA_MAGIC = 0xFFF7EEC5
KNOWN_TOC_PAGES = (0x2159,)
SCAN_LIMIT = 128 * 1024 * 1024
sys.path.insert(0, str(Path(__file__).resolve().parent))
from iso9660 import Iso  # noqa: E402


def le16(b, o=0):
    return struct.unpack_from("<H", b, o)[0]


def le32(b, o=0):
    return struct.unpack_from("<I", b, o)[0]


class Vol:
    def __init__(self, path):
        self.path = path
        p = Path(path)
        if p.suffix.lower() == ".iso":
            iso = Iso(path)
            e = next((x for x in iso.listdir(iso.root) if x.name.upper() in ("GT4.VOL", "GT4L1.VOL")), None)
            if e is None:
                raise SystemExit("no GT4.VOL in the ISO root")
            self.base = e.lba * SECTOR
            self.size = e.size
        else:
            self.base, self.size = 0, p.stat().st_size
        self.f = open(path, "rb")
        self.toc_page = self._find_toc()
        self.entries = self._parse()

    # ---- raw access -------------------------------------------------------
    def _read(self, off, n):
        self.f.seek(self.base + off)
        return self.f.read(n)

    def _find_toc(self):
        for page in KNOWN_TOC_PAGES:
            h = self._read(page * PAGE, 0x40)
            if len(h) >= 0x14 and le32(h) == MAGIC and le16(h, 0x10) == PAGE:
                return page
        CH = 1 << 20
        for off in range(0, SCAN_LIMIT, CH):
            buf = self._read(off, CH)
            if len(buf) < CH:
                break
            for p_ in range(0, len(buf) - 0x14, PAGE):
                if le32(buf, p_) == MAGIC and le16(buf, p_ + 0x10) == PAGE:
                    return (off + p_) // PAGE
        raise SystemExit("no GT4 RoFS index page found (not a supported GT4 volume?)")

    # ---- index ------------------------------------------------------------
    def _parse(self):
        toc = self.toc_page * PAGE
        h = self._read(toc, 0x40)
        toc_pages = le16(h, 0x12)
        header_pages = le32(h, 0x0C)
        self.data = toc + header_pages * PAGE
        packed = self._read(toc + 0x40, 4 * (toc_pages + 1))
        offs = [le32(packed, 4 * i) ^ ((i * OFFSET_KEY + OFFSET_KEY) & 0xFFFFFFFF)
                for i in range(toc_pages + 1)]
        out = []
        for i in range(toc_pages):
            raw = self._read(toc + offs[i], offs[i + 1] - offs[i])
            page = zlib.decompressobj(-15).decompress(raw, PAGE)
            page = bytes(b ^ TOC_XOR for b in page[:PAGE])
            if struct.unpack_from("<h", page, 0)[0]:
                continue                       # index page: only speeds lookups
            pairs = le16(page, 2) >> 1
            for k in range(pairs):
                dv = le32(page, PAGE - 4 * (2 * k + 1))
                dk = le32(page, PAGE - 4 * (2 * k + 2))
                voff, vlen = dv & 0xFFFF, dv >> 16
                koff, klen = dk & 0xFFFF, dk >> 16
                if voff + vlen > PAGE or koff + klen > PAGE or klen < 4:
                    continue
                key, val = page[koff:koff + klen], page[voff:voff + vlen]
                parent = struct.unpack_from(">I", key, 0)[0]
                name = key[4:].split(b"\x00")[0].decode("ascii", "replace")
                t = val[0]
                e = {"parent": parent, "name": name, "type": t}
                if t == 0 and vlen >= 5:
                    e["node"] = le32(val, 1)
                elif t == 1 and vlen >= 13:
                    e["page"], e["size"] = le32(val, 1), le32(val, 9)
                elif t == 2 and vlen >= 17:
                    e["page"], e["csize"], e["size"] = le32(val, 1), le32(val, 9), le32(val, 13)
                else:
                    continue
                out.append(e)
        return out

    # ---- lookups ----------------------------------------------------------
    def children(self, node):
        return [e for e in self.entries if e["parent"] == node and e["name"] not in (".", "..")]

    def lookup(self, path):
        """Return the node id of a directory (all components must be directories)."""
        node = 1
        for part in [p for p in path.replace("\\", "/").split("/") if p]:
            hit = next((e for e in self.entries if e["parent"] == node and e["name"] == part), None)
            if hit is None:
                raise SystemExit(f"not found in the volume: {path} (at '{part}')")
            if hit["type"] != 0:
                raise SystemExit(f"'{part}' is not a directory")
            node = hit["node"]
        return node

    def find(self, path):
        """Return the entry for a file path (the last component may be a file)."""
        parts = [p for p in path.replace("\\", "/").split("/") if p]
        if not parts:
            raise SystemExit("empty path")
        node = self.lookup("/".join(parts[:-1]))
        hit = next((e for e in self.entries if e["parent"] == node and e["name"] == parts[-1]), None)
        if hit is None:
            raise SystemExit(f"not found in the volume: {path}")
        return hit

    def read_file(self, path):
        e = self.find(path)
        if e["type"] == 0:
            raise SystemExit(f"'{path}' is a directory")
        if e["type"] == 1:
            data = self._read(self.data + e["page"] * PAGE, e["size"])
        else:
            raw = self._read(self.data + e["page"] * PAGE, e["csize"])
            if first_word(raw) != DATA_MAGIC:
                raise SystemExit("compressed entry without DATA_MAGIC")
            want = (-struct.unpack_from("<i", raw, 4)[0]) & 0xFFFFFFFF
            data = zlib.decompressobj(-15).decompress(raw[8:], want)
        return data, e


def first_word(b):
    return le32(b) if len(b) >= 4 else 0


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    what, cmd, args = sys.argv[1], sys.argv[2], sys.argv[3:]
    v = Vol(what)
    print(f"# {what}: index at page {v.toc_page:#x}, {len(v.entries)} entries, "
          f"data at {v.data:#x}", file=sys.stderr)
    if cmd == "ls":
        node = v.lookup(args[0]) if args else 1
        for e in sorted(v.children(node), key=lambda x: x["name"]):
            kind = {0: "dir ", 1: "file", 2: "comp"}[e["type"]]
            size = e.get("size", 0)
            print(f"{kind} {size:>12} {e['name']}")
        return 0
    if cmd == "cat":
        data, e = v.read_file(args[0])
        off = int(args[1], 0) if len(args) > 1 else 0
        n = int(args[2], 0) if len(args) > 2 else len(data)
        print(f"# {args[0]}: type {e['type']}, {len(data)} bytes", file=sys.stderr)
        sys.stdout.buffer.write(data[off:off + n])
        return 0
    if cmd == "extract":
        data, e = v.read_file(args[0])
        Path(args[1]).write_bytes(data)
        print(f"wrote {args[1]} ({len(data)} bytes, type {e['type']})", file=sys.stderr)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
