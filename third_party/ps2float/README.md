# Integer EE arithmetic

`ps2float.c` and `ps2float.h` are from libretro/ps2, commit
`cac1c32c7d4c2ad00edd6f4ba756340cefb39dd3`, paths `pcsx2/ps2float.*`:
https://github.com/libretro/ps2

Copyright PCSX2 Dev Team, LGPL-3.0-or-later. See COPYING.LESSER and COPYING.
The files are unmodified apart from LF line endings.

MoonEngine uses this model for original Base replay decoding (GT4
`00579E00`) and native scalar physics in every live mode. The build lowers
our C float operators to these functions, preserving operand order. Original
commands depend on these result bits over thousands of steps; host rounding
mode alone does not preserve them. Native GT4R v1 playback retains its
historical host arithmetic for compatibility.

This library implements arithmetic only. It does not fetch or execute game
instructions. Generated native C and original-code test oracles are separate:
see `docs/notes/DISC_REPLAY_ARITHMETIC.md` for the scope of comparisons.
