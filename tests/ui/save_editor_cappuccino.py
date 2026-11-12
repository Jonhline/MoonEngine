"""Native menu restore after editor power/mass changes on both Cappuccinos."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

bridge, helper, iso = sys.argv[1:4]
with tempfile.TemporaryDirectory(prefix="moon-cappuccino-") as folder:
    base = Path(folder)
    stock = str(base / "stock.ogt4")
    subprocess.run([bridge, iso, "create", stock], check=True)
    for label in ["cappuccino_91", "cappuccino_95"]:
        added = str(base / (label + ".ogt4"))
        selected = str(base / (label + "-selected.ogt4"))
        tuned = str(base / (label + "-tuned.ogt4"))
        for args in [[helper, iso, stock, added, "add", label, "0"],
                     [helper, iso, added, selected, "select", "1"],
                     [helper, iso, selected, tuned, "tune", "1", "160", "7"]]:
            subprocess.run(args, check=True, stdout=subprocess.DEVNULL)
        result = subprocess.run([bridge, iso, "verify-cappuccino", tuned, *sys.argv[4:]])
        if result.returncode:
            sys.exit(result.returncode)
print("PASS Cappuccino 1991/1995: saved power/mass, actual menu restore, current-car body and native UI getters")
