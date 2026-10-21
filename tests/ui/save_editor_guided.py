"""Real authored profile -> guided frontend -> native license/vehicle getters."""
from pathlib import Path
import subprocess
import sys
import tempfile

bridge, helper, editor, disc, node, captures = sys.argv[1:7]
negative = '--negative' in sys.argv[7:]
with tempfile.TemporaryDirectory(prefix='moon-guided-') as directory:
    folder = Path(directory)
    source, edited = folder / 'source.ogt4', folder / 'edited.ogt4'
    catalog, typed = folder / 'catalog.json', folder / 'edited.json'
    subprocess.run([bridge, disc, 'create', str(source)], check=True)
    with catalog.open('wb') as output:
        subprocess.run([helper, disc], stdout=output, check=True)
    subprocess.run([node, 'tests/ui/save_editor_friendly.mjs', editor,
                    str(source), str(catalog), captures + '.png', str(typed)], check=True)
    subprocess.run([editor, '--apply-json', str(typed), str(edited)], check=True)
    subprocess.run([bridge, disc, 'verify', str(edited)], check=True)
    if negative:
        result = subprocess.run([bridge, disc, 'verify', str(edited), '--negative'])
        sys.exit(1 if result.returncode == 1 else 0)
