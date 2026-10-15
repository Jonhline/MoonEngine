"""Check that the Switch application graphics units use native deko3d.

SDL's packaged archive can retain a dormant EGL driver; checking final symbol
presence alone would misidentify that as the active application renderer.
"""
import argparse
import os
from pathlib import Path
import re
import struct
import subprocess
import sys


def verify(build, nm, negative=False, window_negative=False, diagnostics=False,
           diagnostics_negative=False):
    elf = (build / 'opengt4.elf').read_bytes()
    if elf[:4] != b'\x7fELF' or struct.unpack_from('<H', elf, 18)[0] != 183:
        raise ValueError('application is not an AArch64 ELF')
    ninja = (build / 'build.ninja').read_text(encoding='utf-8')
    units = re.findall(r'^build (CMakeFiles/opengt4_platform_sdl.dir/[^:]+):', ninja, re.M)
    for name in ('deko_device', 'deko_video', 'deko_render'):
        if not any('/switch/' + name + '.o' in unit for unit in units):
            raise ValueError('missing native compilation unit: ' + name)
    for unit in units:
        if re.search(r'/sdl/(video_gl|render3d|gl|switch_render)\.o$', unit):
            raise ValueError('legacy application GL unit linked: ' + unit)
    library = build / 'libopengt4_platform_sdl.a'
    undefined = subprocess.check_output([str(nm), '-u', str(library)], text=True)
    if negative:
        undefined += '\n U SDL_GL_CreateContext\n'
    if window_negative:
        undefined += '\n U SDL_CreateWindow\n'
    if re.search(r'\b(?:gl[A-Z]\w*|egl[A-Z]\w*|SDL_GL_\w+)\b', undefined):
        raise ValueError('application platform archive still calls GL/EGL')
    if re.search(r'\bSDL_(?:CreateWindow\w*|DestroyWindow)\b', undefined):
        raise ValueError('SDK SDL windows implicitly own an EGL surface')
    symbols = subprocess.check_output([str(nm), '-g', str(build / 'opengt4.elf')], text=True)
    for symbol in ('dkQueuePresentImage', 'dkCmdBufDraw', 'nd_init'):
        if symbol not in symbols:
            raise ValueError('native command/presentation code missing: ' + symbol)
    if '__wrap_nvGpuChannelKickoff' not in symbols or '--wrap=nvGpuChannelKickoff' not in ninja:
        raise ValueError('raw driver rejection diagnostic was not linked')
    if diagnostics:
        fault_text = elf if not diagnostics_negative else b''
        for marker in (b'GPU page fault', b'GPU method error', b'GPU timeout',
                       b'Switch GPU fault context:', b'draw outside vertex buffer'):
            if marker not in fault_text:
                raise ValueError('native GPU diagnostics missing: ' + marker.decode())
        if 'WarningMsg' not in symbols or not re.search(r'(?:-ldeko3dd\b|libdeko3dd\.a)', ninja):
            raise ValueError('SDK diagnostic archive was not linked')
    for stage in ('ui.vert', 'ui.frag', 'mesh.vert', 'mesh.frag', 'mix.frag'):
        code = (build / 'generated' / (stage + '.dksh')).read_bytes()
        if len(code) < 24 or code[:4] != b'DKSH':
            raise ValueError('bad compiled shader: ' + stage)
        _, _, control, size, _, count = struct.unpack_from('<6I', code)
        if not count or control + size > len(code):
            raise ValueError('truncated shader: ' + stage)
    nro = (build / 'opengt4.nro').read_bytes()
    if nro[16:20] != b'NRO0':
        raise ValueError('NRO packaging missing')
    print('Switch native backend: AArch64/NRO, 5 DKSH modules, deko draw/present; no application GL calls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--nm', type=Path)
    parser.add_argument('--negative', action='store_true')
    parser.add_argument('--window-negative', action='store_true')
    parser.add_argument('--require-diagnostics', action='store_true')
    parser.add_argument('--diagnostics-negative', action='store_true')
    args = parser.parse_args()
    sdk = Path(os.environ.get('DEVKITPRO', 'C:/devkitPro' if os.name == 'nt' else '/opt/devkitpro'))
    if os.name == 'nt' and not sdk.exists():
        sdk = Path('C:/devkitPro')
    nm = args.nm or sdk / 'devkitA64/bin' / ('aarch64-none-elf-nm.exe' if os.name == 'nt' else 'aarch64-none-elf-nm')
    try:
        verify(args.build, nm, args.negative, args.window_negative,
               args.require_diagnostics, args.diagnostics_negative)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('Switch backend check failed:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
