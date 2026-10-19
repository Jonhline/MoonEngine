"""Complete export/import through actual save-screen actions. Isolated real
career profiles and generated media; only native file choice is injected.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

exe, profile_fixture, package_fixture, disc, output = map(Path, sys.argv[1:6])
output.mkdir(parents=True, exist_ok=True)
negative = "--negative" in sys.argv[6:]
with tempfile.TemporaryDirectory(dir=output) as temporary:
    temporary = Path(temporary).resolve()
    source, destination = temporary / "source", temporary / "destination"
    common = dict(os.environ, SDL_AUDIODRIVER="dummy", MOON_WINDOW_HIDDEN="1",
                  MOON_OVERLAY=str(temporary / "no-mods"))
    setup = subprocess.run([str(profile_fixture.resolve()), str(disc.resolve())],
                           env=dict(common, MOON_PROFILE_STORAGE_FIXTURE_DIR=str(source)),
                           capture_output=True, timeout=45)
    assert setup.returncode == 0, setup.stderr.decode("utf-8", "replace")
    generated = temporary / "generated"
    setup = subprocess.run([str(package_fixture.resolve()), str(generated)],
                           capture_output=True, timeout=30)
    assert setup.returncode == 0, setup.stderr.decode("utf-8", "replace")
    library_source = next(generated.iterdir()) / "source"
    for name in ("photos", "replays", "films", "imports"):
        shutil.copytree(library_source / name, source / name, dirs_exist_ok=True)
    (source / "photos" / "Foto São Paulo.png").write_bytes((source / "photos" / "photo.png").read_bytes())
    destination.mkdir()
    # Seed one complete valid player so import reaches the normal save screen.
    for name in ("slot01.ogt4", "active.ogt4"):
        shutil.copy2(source / name, destination / name)
    config = temporary / "options.ini"
    config.write_text("preset=Classic\ninterface.save_transfer_tools=1\ngraphics.renderer=opengl\n", encoding="utf-8")
    package = temporary / "Jogador São Paulo.moonplayer"
    env = dict(common, MOON_TEST_SAVE_PACKAGE=str(package))

    def run(name, folder, keys):
        command = [str(exe.resolve()), str(disc.resolve()), "save", "--save-dir", str(folder),
                   "--config-file", str(config), "--shot", str(output / (name + ".png")),
                   "--dialog-answer", "yes", "--input", ",".join(keys)]
        result = subprocess.run(command, env=env, capture_output=True, timeout=45)
        log = (result.stdout + result.stderr).decode("utf-8", "replace")
        (output / (name + ".log")).write_text(log, encoding="utf-8")
        assert result.returncode == 0 and "unsupported project script" not in log, log
        return log

    export_log = run("export", source, ["down"] * 5 + ["accept"])
    assert "focus=Export" in export_log and package.exists(), export_log
    expected = {"manifest.ini", "profile.ogt4"}
    with zipfile.ZipFile(package) as archive:
        assert archive.read("profile.ogt4") == (source / "slot01.ogt4").read_bytes()
        for folder in ("photos", "replays", "films", "imports"):
            for path in (source / folder).rglob("*"):
                if path.is_file():
                    relative = path.relative_to(source).as_posix()
                    expected.add("library/" + relative)
                    assert archive.read("library/" + relative) == path.read_bytes(), relative
        assert set(archive.namelist()) == expected
    if negative:
        with zipfile.ZipFile(package) as archive:
            members = [(name, archive.read(name)) for name in archive.namelist()]
        with zipfile.ZipFile(package, "w") as archive:
            for name, data in members:
                archive.writestr(name, b"invalid profile" if name == "profile.ogt4" else data)
    import_log = run("import", destination, ["down"] * 5 + ["left", "accept"])
    assert "focus=Import" in import_log, import_log
    assert (destination / "slot02.ogt4").exists(), import_log
    for folder in ("photos", "replays", "films", "imports"):
        for path in (source / folder).rglob("*"):
            if path.is_file():
                relative = path.relative_to(source)
                assert (destination / relative).read_bytes() == path.read_bytes(), str(relative)
print("Complete-package UI: actual Export/Import actions transfer all generated media, lists, auxiliary files and a real career profile")
