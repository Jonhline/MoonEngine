"""Native mission attempts. Setups and losing attempts never count as wins."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def drive(args, number):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GT4_MISSION_")}
    env.update(GT4_MISSION_ONE=str(number), GT4_MISSION_RUN="1", GT4_MISSION_STEER="1",
               GT4_MISSION_REQUIRE_WIN="1", GT4_MISSION_FRAMES="86400")
    if args.line is not None:
        env["GT4_MISSION_LINE"] = str(args.line)
    if args.passing_gap is not None:
        env["GT4_MISSION_PASSING_GAP"] = str(args.passing_gap)
    if args.draft:
        env["GT4_MISSION_DRAFT"] = "1"
    if args.lateral_target is not None:
        env["GT4_MISSION_LATERAL_TARGET"] = str(args.lateral_target)
    if args.look_factor is not None:
        env["GT4_MISSION_LOOK_FACTOR"] = str(args.look_factor)
    if args.predictive:
        env["GT4_MISSION_PREDICTIVE"] = "1"
    if args.gain is not None:
        env["GT4_MISSION_GAIN"] = str(args.gain)
    if args.lane is not None:
        env["GT4_MISSION_LANE"] = str(args.lane)
    if args.controller_kind:
        env["GT4_MISSION_CONTROLLER_KIND"] = str(args.controller_kind)
    if args.takeover:
        env["GT4_MISSION_TAKEOVER"] = "1"
    path = args.out / f"mission-{number:02}.log"
    with path.open("wb") as output:
        status = subprocess.run([str(args.exe), args.iso], env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=600).returncode
    text = path.read_text(encoding="utf-8", errors="replace")
    outcome = re.search(r"physical outcome (\w+) finished=(\d+) rank=(\d+) time=(\d+) state=(-?\d+)", text)
    won = status == 0 and outcome is not None and outcome.group(2, 3, 5) == ("1", "1", "2")
    print("WIN" if won else "UNPROVED", number, outcome.groups() if outcome else status, flush=True)
    return dict(number=number, won=won, status=status, log=str(path),
                inputs={k: v for k, v in env.items() if k.startswith("GT4_MISSION_")},
                outcome=outcome.groups() if outcome else None,
                log_sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso")
    parser.add_argument("--exe", type=Path, default=Path("build/Release/gtmode_mission_disc.exe"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--line", type=int, choices=range(7), help="Authored course line observed by the pad driver")
    parser.add_argument("--passing-gap", type=float, help="Overtake decision distance of the external driver")
    parser.add_argument("--draft", action="store_true", help="Geometry-only drafting/passing input policy")
    parser.add_argument("--lateral-target", type=float, help="Desired acceleration of the test pad driver")
    parser.add_argument("--look-factor", type=float, help="Lookahead of the test pad driver")
    parser.add_argument("--predictive", action="store_true", help="Preview/braking policy of the external pad driver")
    parser.add_argument("--gain", type=float, help="Test-driver steering input gain")
    parser.add_argument("--mission", type=int, action="append", choices=range(1, 35))
    parser.add_argument("--lane", type=float, help="Test-driver lateral target in metres")
    parser.add_argument("--controller-kind", type=int, choices=(1, 2))
    parser.add_argument("--takeover", action="store_true", help="Native AI is an explicit test controller")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    assert not args.out.exists()
    args.out.mkdir(parents=True)
    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for record in pool.map(lambda n: drive(args, n), (args.mission or range(1, 35))):
            records.append(record)
            (args.out / "results.json").write_text(json.dumps(dict(
                executable_sha256=hashlib.sha256(args.exe.read_bytes()).hexdigest(),
                scope="native finish/rank only; prize/save cycle requires separate proof",
                records=records), indent=2) + "\n", encoding="utf-8")
    assert all(r["won"] for r in records), "some native victories remain unproved"


if __name__ == "__main__":
    main()
