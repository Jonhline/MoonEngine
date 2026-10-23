"""Real native writer -> editor -> native reader, including a deliberate negative."""
import pathlib
import json
import subprocess
import sys
import tempfile

bridge, editor = sys.argv[1:3]
negative = '--negative' in sys.argv[3:]
with tempfile.TemporaryDirectory(prefix='moon-editor-') as folder:
    source = pathlib.Path(folder) / 'source.ogt4'
    target = pathlib.Path(folder) / 'copy.ogt4'
    subprocess.run([bridge, 'create', str(source)], check=True)
    subprocess.run([editor, '--roundtrip', str(source), str(target)], check=True)
    assert source.read_bytes() == target.read_bytes(), 'No-op must preserve the complete native envelope'
    typed = json.loads(subprocess.check_output([editor, '--inspect', str(source)], encoding='utf8'))
    for key, node in typed['value']:
        if key == 'money':
            node['value'] = '12345678901234567'
    edit_json = pathlib.Path(folder) / 'edit.json'
    with edit_json.open('w', encoding='utf8', newline='') as output:
        json.dump(typed, output)
    edited = pathlib.Path(folder) / 'edited.ogt4'
    subprocess.run([editor, '--apply-json', str(edit_json), str(edited)], check=True)
    subprocess.run([bridge, 'verify', str(edited), '--edited'], check=True)
    result = subprocess.run([bridge, 'verify', str(target)] + (['--negative'] if negative else []))
    sys.exit(result.returncode)
