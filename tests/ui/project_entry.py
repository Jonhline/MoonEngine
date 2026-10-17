"""Read each retail project onLoad; compare its panIn request to native entry."""
import pathlib, subprocess, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / 'tools'))
import adhoc_dis as ad
from vol_file import Vol
exe, disc = sys.argv[1:3]
vol = Vol(disc)
projects = ('arcade gtmode boot option setting event quick quick-arcade quick-tt '
            'quick-event quick-championship labomode quick-license photo_shoot '
            'quick-mission logger slide quick-freerun quick-practice quick-mt photo_save').split()
args = [exe, disc]
for project in projects:
    raw, _ = vol.read_file(f'/projects/{project}/{project}.adc')
    script = ad.load(raw)
    entries = []
    for line, instruction, operands in script.ins:
        if instruction != 'FUNCTION_DEFINE' or operands[0] != 'onLoad':
            continue
        body = next((x for x in operands if isinstance(x, ad.Frame)), None)
        terms = [x for _, _, operands in body.ins for x in operands if isinstance(x, str)]
        if 'createRenderContext' in terms:
            entries.append('panIn' in terms)
    assert len(entries) == 1, (project, entries)
    args += [project, str(int(entries[0]))]
subprocess.run(args, check=True)
