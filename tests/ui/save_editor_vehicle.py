"""Real native garage factory and advanced UI, with temporary profiles only."""
from pathlib import Path
import subprocess
import sys
import tempfile

bridge, catalog_helper, editor, vehicle_helper, disc, node, capture = sys.argv[1:8]
with tempfile.TemporaryDirectory(prefix='moon-vehicle-ui-') as directory:
    folder = Path(directory)
    source, catalog = folder / 'source.ogt4', folder / 'catalog.json'
    subprocess.run([bridge, disc, 'create', str(source)], check=True)
    with catalog.open('wb') as output:
        subprocess.run([catalog_helper, disc], stdout=output, check=True)
    result = subprocess.run([node, 'tests/ui/save_editor_vehicle.mjs', editor,
                             vehicle_helper, disc, str(source), str(catalog),
                             capture, *sys.argv[8:]])
    sys.exit(result.returncode)
