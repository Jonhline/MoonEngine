"""Inventory real-disc UI roots and diagnostics; screenshots remain local.

Direct root opening does not supply every authored caller's arguments. This
is a discovery report, not a claim that every listed error is a production
navigation failure or that a screenshot proves original visual fidelity.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def diagnostic_lines(log):
    return [line for line in log.splitlines() if re.search(
        r"script:|undefined|call of nil|onInitialize.*error|unsupported|not implemented|error:|audit timeout",
        line, re.I)]


def active_roots(log):
    return re.findall(r"UI_TOUR requested=\S+ active=(\S+)", log)


def project_in_scope(name):
    return name not in {"network", "online", "message", "eyetoy", "print", "event"}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("disc", type=Path)
    p.add_argument("--exe", type=Path, default=Path("build/Release/ui_tour.exe"))
    p.add_argument("--vol-exe", type=Path, default=Path("build/Release/gt4vol.exe"))
    p.add_argument("--projects", nargs="+", default=["gtmode", "option", "arcade"])
    p.add_argument("--output", type=Path, default=Path("extracted/ui_tour"))
    p.add_argument("--all-projects", action="store_true", help="Discover project folders from the disc")
    p.add_argument("--inventory-only", action="store_true", help="List authored roots without opening them")
    p.add_argument("--limit", type=int, help="Limit roots for a focused reproduction")
    args = p.parse_args()
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", MOON_WINDOW_HIDDEN="1", MOON_GRAPHICS_API="opengl",
               MOON_OVERLAY=str(Path("build/autosave-no-mods").resolve()))
    if args.all_projects:
        listing = subprocess.run([str(args.vol_exe.resolve()), str(args.disc.resolve()), "ls", "/projects"],
                                 capture_output=True, text=True, timeout=30, check=True)
        args.projects = re.findall(r"^\s+([\w-]+)/\s*$", listing.stdout, re.MULTILINE)
    args.projects = [name for name in args.projects if project_in_scope(name)]
    args.output.mkdir(parents=True, exist_ok=True)
    config = args.output.resolve() / "options.ini"
    config.write_text("preset=Classic\n", encoding="utf-8", newline="")
    records = []
    for project in args.projects:
        folder = args.output / project
        folder.mkdir(exist_ok=True)
        description = folder / "project.mproject"
        extracted = subprocess.run([str(args.vol_exe.resolve()), str(args.disc.resolve()),
                                    "cat", f"/projects/{project}/{project}.mproject", str(description)],
                                   capture_output=True, text=True, timeout=30)
        if extracted.returncode:
            records.append({"project": project, "extraction_error": extracted.stderr})
            continue
        roots = re.findall(r'RootWindow\s*\{\s*name\s+string\s*\{\s*"([^"]+)"',
                           description.read_text(encoding="utf-8", errors="replace"))
        if args.inventory_only:
            records.append({"project": project, "roots": roots,
                            "widget_counts": {name: len(re.findall(r"\b" + name + r"\s*\{", description.read_text(encoding="utf-8", errors="replace")))
                                              for name in sorted(set(re.findall(r"\b(\w+(?:Face|Box|Window))\s*\{", description.read_text(encoding="utf-8", errors="replace"))))},
                            "boundary": "authored inventory; not rendered or validated"})
            (args.output / "inventory.json").write_text(json.dumps(records, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="")
            print(f"{project}: {len(roots)} authored roots", flush=True)
            continue
        for root in roots:
            if re.search(r"eyetoy|network|online|print", root, re.I):
                continue
            if args.limit is not None and len(records) >= args.limit:
                break
            shot = folder / (root + ".png")
            command = [str(args.exe.resolve()), str(args.disc.resolve()), project, root,
                       "--save-dir", str((folder / "isolated-saves").resolve()),
                       "--config-file", str(config), "--dialog-answer", "no", "--shot-delay", "0.2", "--shot", str(shot)]
            try:
                result = subprocess.run(command, env=env, capture_output=True, text=True,
                                        encoding="utf-8", errors="replace", timeout=45)
                log = result.stdout + result.stderr
                code = result.returncode
            except subprocess.TimeoutExpired as error:
                def decode(value):
                    return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else value or ""
                log = decode(error.stdout) + decode(error.stderr) + "\naudit timeout\n"
                code = None
            (folder / (root + ".log")).write_text(log, encoding="utf-8", newline="")
            diagnostics = diagnostic_lines(log)
            records.append({"project": project, "root": root, "exit_code": code,
                            "screenshot": str(shot) if shot.exists() else None,
                            "diagnostics": diagnostics,
                            "actual": active_roots(log),
                            "boundary": "direct root; caller-specific arguments not supplied"})
            (args.output / "report.json").write_text(json.dumps(records, indent=2, ensure_ascii=False) + "\n",
                                                     encoding="utf-8", newline="")
            print(f"{project}/{root}: exit={code}, shot={shot.exists()}, diagnostics={len(diagnostics)}", flush=True)
    if not args.inventory_only:
        rows = ["# Local UI discovery tour", "",
                "Direct root opening does not supply caller arguments; warnings require reproduction through the authored route.", "",
                "| Screen | Active after initialize | Exit | Warnings | Requested-tree capture |", "|---|---|---|---|---|"]
        for item in records:
            if "root" not in item:
                continue
            warnings = "<br>".join(item["diagnostics"]).replace("|", "\\|") or "none"
            shot = Path(item["screenshot"]).resolve() if item["screenshot"] else None
            capture = f"[capture]({shot.as_uri()})" if shot else "missing"
            active = ", ".join(item.get("actual", [])) or "not recorded"
            rows.append(f"| {item['project']}::{item['root']} | {active} | {item['exit_code']} | {warnings} | {capture} |")
        (args.output / "report.md").write_text("\n".join(rows) + "\n", encoding="utf-8", newline="")
    report = args.output / ("inventory.json" if args.inventory_only else "report.json")
    print(f"Discovery report: {report}", flush=True)


if __name__ == "__main__":
    main()
