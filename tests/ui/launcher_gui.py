"""Check the shipped executable cannot allocate a startup console."""
import struct
import sys
from pathlib import Path

image = Path(sys.argv[1]).read_bytes()
assert image[:2] == b"MZ", "DOS header missing"
pe = struct.unpack_from("<I", image, 0x3C)[0]
assert image[pe:pe + 4] == b"PE\0\0", "PE header missing"
# IMAGE_OPTIONAL_HEADER32/64.Subsystem is +68 in both formats.
subsystem = struct.unpack_from("<H", image, pe + 24 + 68)[0]
assert subsystem == 2, f"Expected Windows GUI subsystem, got {subsystem}"
print("PASS launcher executable uses Windows GUI subsystem")
