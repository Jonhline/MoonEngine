"""Fresh physical series runs sharing one isolated profile.

Ownership and controller recipes are explicit test fixtures. This is not
proof of a new career earning every car, licence or admission requirement.
Old receipts select inputs only; every result is produced again.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs(record, runs, overrides):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GT4_SERIES_")}
    strategy = next(r["strategy"] for r in runs if r["identity"] == record["identity"])
    if record["series"] in overrides:
        strategy = dict(strategy or {})
        strategy.update(overrides[record["series"]])
    env["GT4_SERIES_PIT_WEAR"] = "1"
    parts = overrides.get(record["series"], {}).get("parts", record["parts"])
    if parts not in ("", "-"):
        env["GT4_SERIES_PARTS"] = parts
    if strategy:
        for name in ("controller_kind", "pace", "pit_band", "round_controllers"):
            if name in strategy:
                env["GT4_SERIES_" + name.upper()] = str(strategy[name])
        for name in ("aids", "overtake", "max_aero"):
            if strategy.get(name):
                env["GT4_SERIES_" + name.upper()] = "1"
        if strategy.get("fuel_only_pits"):
            env.pop("GT4_SERIES_PIT_WEAR", None)
        if strategy.get("round_fixtures"):
            path = Path(strategy["round_fixtures"])
            assert digest(path) == strategy["round_fixtures_sha256"]
            rounds = json.loads(path.read_text(encoding="utf-8"))
            env["GT4_SERIES_ROUND_CARS"] = ",".join(r["car"] for r in rounds)
            env["GT4_SERIES_ROUND_PARTS"] = "|".join(r["parts"] for r in rounds)
    return env, "gtmode_series_pace.exe" if strategy else "gtmode_series_playable.exe"


def run_series(args, record, source, prior, previous):
    env, binary = inputs(record, source["runs"], args.overrides)
    env["GT4_SERIES_PROFILE"] = str((args.out / "profile").resolve())
    env["GT4_SERIES_PRIOR_RACES"] = ",".join(prior)
    if prior and args.negative in ("record", "continuity"):
        env["GT4_SERIES_" + args.negative.upper() + "_NEGATIVE"] = "1"
    exe = args.bin / binary
    if (args.out / "profile").exists():
        shutil.copytree(args.out / "profile", args.out / "checkpoints" / record["series"])
    log_path = args.out / (record["series"] + ".log")
    with log_path.open("wb") as output:
        status = subprocess.run([str(exe), args.iso, "--physical", record["series"],
                                 record["car"], "1"], env=env, stdout=output,
                                stderr=subprocess.STDOUT, timeout=7200).returncode
    log = log_path.read_text(encoding="utf-8", errors="replace")
    assert status == 0, f"physical run failed: {log_path} ({status})"
    assert "script:" not in log, f"original menu script failed: {log_path}"
    pattern = r"continuity (before|after) cars=(\d+) prizes=(\d+) money=(\d+)"
    states = {name: tuple(map(int, values)) for name, *values in re.findall(pattern, log)}
    assert set(states) == {"before", "after"}, "missing continuity checkpoints"
    if previous is not None:
        assert states["before"] == previous, "profile reset between series"
    stages = [dict(race=r, frames=int(f), rank=int(k), award=int(a), grand=int(g), pits=int(p))
              for r, f, k, a, g, p in re.findall(
                  r"physical (\w+) car=\w+ frames=(\d+) rank=(\d+) award=(\d+) grand=(\d+) pit_stops=(\d+)", log)]
    assert len(stages) == record["stages"]
    prize_count = re.search(r"series original_prize_cars=(\d+)", log)
    assert prize_count and states["after"][1] == states["before"][1] + int(prize_count.group(1))
    assert states["after"][2] == states["before"][2] + sum(s["award"] + s["grand"] for s in stages)
    assert f"continuity checked previous_records={len(prior)}" in log
    assert "PASS complete physical series and authored car-prize delivery/save reload" in log
    return dict(series=record["series"], before=states["before"], after=states["after"],
                stages=stages, log=str(log_path), log_sha256=digest(log_path),
                binary_sha256=digest(exe), inputs={k: v for k, v in env.items()
                                               if k.startswith("GT4_SERIES_")})


def finish_profile(args, stages, previous):
    env = dict(os.environ)
    if args.negative == "course":
        env["GT4_CAREER_COURSE_NEGATIVE"] = "1"
    if args.negative == "reverse":
        env["GT4_CAREER_REVERSE_NEGATIVE"] = "1"
    log = args.out / "profile.log"
    with log.open("wb") as output:
        run = subprocess.run([str(args.bin / "gtmode_career_profile.exe"), args.iso,
                              str(args.out / "profile"), str(stages), str(previous[1]),
                              str(previous[2])], env=env, stdout=output,
                             stderr=subprocess.STDOUT, timeout=120)
    assert run.returncode == 0, f"profile/calendar/course validation failed: {log}"
    return dict(log=str(log), log_sha256=digest(log),
                save_sha256=digest(args.out / "profile" / "slot01.ogt4"))


def checkpoint_start(args, order):
    result = dict(scope="continuous profile; fixture ownership; native finishes", records=[])
    if not args.checkpoint:
        return result, [], None, order
    meta = json.loads(args.checkpoint.read_text(encoding="utf-8"))
    source = json.loads(Path(meta["results"]).read_text(encoding="utf-8"))
    records = source["records"][:meta["records"]]
    assert [r["series"] for r in records] == order[:len(records)]
    profile = Path(meta["profile"])
    assert digest(profile / "slot01.ogt4") == meta["save_sha256"]
    for record in records:
        assert digest(Path(record["log"])) == record["log_sha256"]
    shutil.copytree(profile, args.out / "profile")
    result.update(records=records, checkpoint=meta)
    prior = [stage["race"] for r in records for stage in r["stages"]]
    # An interrupted championship may have a genuine paid attempt in this
    # checkpoint. Keep that history; never erase it to fit catalog totals.
    result["extra_attempts"] = meta.get("extra_attempts", [])
    for attempt in result["extra_attempts"]:
        assert digest(Path(attempt["source_log"])) == attempt["source_log_sha256"]
    return result, prior, tuple(meta["before"]), order[len(records):]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso")
    parser.add_argument("--input-overrides", type=Path, help="Explicit external driver-input recipes; never race results")
    parser.add_argument("--checkpoint", type=Path, help="Audited local save and receipt checkpoint")
    parser.add_argument("--recipes", type=Path, default=Path("build/career-finish/receipts-144.json"))
    parser.add_argument("--bin", type=Path, default=Path("build/Release"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--negative", choices=("record", "continuity", "course", "reverse"))
    parser.add_argument("--series", action="append")
    parser.add_argument("--late-series", action="append", default=[], help="Test-player event order only")
    args = parser.parse_args()
    args.overrides = json.loads(args.input_overrides.read_text(encoding="utf-8")) if args.input_overrides else {}
    allowed = {"controller_kind", "pace", "pit_band", "round_controllers", "aids", "overtake", "max_aero",
               "fuel_only_pits", "parts", "round_fixtures", "round_fixtures_sha256"}
    assert isinstance(args.overrides, dict), "driver overrides must be a series mapping"
    for strategy in args.overrides.values():
        assert isinstance(strategy, dict) and strategy.keys() <= allowed, "unknown driver input"
    if args.out is None:
        import time
        args.out = Path(f"build/career-continuous-{time.time_ns()}-{os.getpid()}")
    assert not args.out.exists(), "use a new isolated directory; never overwrite a profile"
    if args.smoke:
        source = dict(runs=[dict(identity="smoke", strategy=None)], records=[
            dict(series="am_sunday_0000", car="corvette_z06_00", parts="-", stages=5, identity="smoke"),
            dict(series="am_mr_0000", car="zonda_c12s_gmv_02", parts="-", stages=5, identity="smoke")])
    else:
        source = json.loads(args.recipes.read_text(encoding="utf-8"))
    catalog = {r["series"]: r for r in source["records"]}
    assert len(catalog) == (2 if args.smoke else 144)
    assert args.overrides.keys() <= catalog.keys(), "unknown series in driver overrides"
    order = args.series or ["am_sunday_0000", "am_mr_0000"] + [
        k for k in sorted(catalog) if k not in ("am_sunday_0000", "am_mr_0000")]
    assert len(order) == len(set(order)) and set(order) <= catalog.keys()
    assert set(args.late_series) <= set(order) and len(set(args.late_series)) == len(args.late_series)
    order = [s for s in order if s not in args.late_series] + args.late_series
    args.out.mkdir(parents=True)
    result, prior, previous, remaining = checkpoint_start(args, order)
    for series in remaining:
        record = run_series(args, catalog[series], source, prior, previous)
        result["records"].append(record)
        prior.extend(s["race"] for s in record["stages"])
        previous = record["after"]
        (args.out / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(f"PASS {len(result['records'])}/{len(order)} {series}: {len(prior)} stages, {previous[1]} prizes", flush=True)
    result["profile"] = finish_profile(args, len(prior) + len(result.get("extra_attempts", [])), previous)
    (args.out / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print("PASS fresh physical series on one profile; previous bodies/records, credit and prize continuity")


if __name__ == "__main__":
    main()
