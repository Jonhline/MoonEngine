"""Discovery-tool scope and VM diagnostic preservation, without launching the game."""
import importlib.util
from pathlib import Path
import sys
spec = importlib.util.spec_from_file_location("ui_tour", Path(__file__).with_name("run_ui_audit.py"))
tour = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tour)
for name in ("network", "online", "message", "eyetoy", "print", "event"):
    assert not tour.project_in_scope(name), name
for name in ("gtmode", "option", "logger", "labomode", "photo_travel"):
    assert tour.project_in_scope(name), name
log = "ordinary load\nscript: frame:1: undefined 'field' in widget\ncall of nil\nonInitialize error: missing argument\naudit timeout\nsaved shot.png\n"
expected = ["script: frame:1: undefined 'field' in widget", "call of nil", "onInitialize error: missing argument", "audit timeout"]
actual = tour.diagnostic_lines(log)
if "--negative" in sys.argv:
    actual = actual[1:]  # A discovery report must not silently drop VM warnings.
assert actual == expected, (actual, expected)
assert tour.diagnostic_lines("ordinary load\nsaved shot.png") == []

# An opened fallback screen cannot be represented as the requested screen.
assert tour.active_roots("UI_TOUR requested=logger/LoggerRoot active=logger/LoggerMenu") == ["logger/LoggerMenu"]
assert tour.active_roots("saved screenshot.png") == []
