"""Audit every Options row's text boundary and real closed-panel pixels on both APIs."""
from pathlib import Path
import os
import re
import subprocess
import sys
from png_diff import read_png

exe, disc, output = (Path(p).resolve() for p in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
negative = '--negative' in sys.argv[4:]
for api in ('opengl', 'vulkan'):
    cfg = output / (api + '.ini')
    cfg.write_text(f'disc={disc.as_posix()}\napi={api}\nsaves={output.as_posix()}/saves\n'
                   'car=gt40_race_69\nscene=laguna\n', encoding='utf-8', newline='')
    Path(str(cfg) + '.video.ini').write_text(
        f'preset=Enhanced\ngraphics.renderer={api}\nlauncher.guide_seen=1\n',
        encoding='utf-8', newline='')
    def capture(mode, **flags):
        shot = output / f'{api}-{mode}.png'
        env = dict(os.environ, SDL_AUDIODRIVER='dummy', MOON_WINDOW_HIDDEN='1',
                   MOONSTUDIO_MUTE='1', MOONSTUDIO_CINE_AT='0', MOONSTUDIO_CLIP_FRAME='0',
                   MOONSTUDIO_FRONT_SHOT=str(shot), **flags)
        run = subprocess.run([str(exe), '--launcher', '--launcher-config', str(cfg)],
                             env=env, capture_output=True, timeout=180)
        log = (run.stdout + run.stderr).decode('utf-8', 'replace')
        (output / f'{api}-{mode}.log').write_text(log, encoding='utf-8', newline='')
        assert run.returncode == 0, (api, mode, run.returncode, log[-1800:])
        return shot, log
    flags = {'MOONSTUDIO_PANEL_INK_AUDIT':'1'}
    if negative:
        flags['MOONSTUDIO_PANEL_INK_NEGATIVE']='1'
    _, log = capture('all-rows', **flags)
    rows = re.findall(r'STUDIO_PANEL_INK (\d+) (\d+) (-?\d+)', log)
    total = re.search(r'STUDIO_PANEL_INK_TOTAL (\d+)', log)
    assert total and len(rows) == int(total[1]) and len(rows) >= 50, (api, len(rows))
    assert {int(t) for t, _, _ in rows} == set(range(6))
    baseline, _ = capture('baseline')
    w, h, a = read_png(baseline)
    for label, tab in [('quality', '5'), ('interface', '2')]:
        shot, _ = capture(label + '-closed', MOONSTUDIO_OPTIONS=tab,
                          MOONSTUDIO_OPTIONS_ROW='0', MOONSTUDIO_OPTIONS_CLOSE_AT='1')
        bw, bh, b = read_png(shot)
        assert (w,h) == (bw,bh)
        delta = max(abs(a[y][x][c]-b[y][x][c])
                    for y in range(int(h*.20), int(h*.72))
                    for x in range(int(w*.50), int(w*.95)) for c in range(3))
        assert delta == 0, (api, label, 'closed panel text residue', delta)
    print(api, len(rows), 'option rows audited; closed panels match baseline', flush=True)
