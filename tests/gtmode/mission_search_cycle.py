"""Generate pad commands; require fresh physical win and original payout/save."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso")
    parser.add_argument("search", type=Path)
    parser.add_argument("cycle", type=Path)
    parser.add_argument("mission")
    parser.add_argument("--negative", action="store_true")
    args = parser.parse_args()
    env = {k: v for k, v in os.environ.items() if not k.startswith("GT4_MISSION_")}
    with tempfile.TemporaryDirectory(prefix="moonengine-mission-commands-") as directory:
        tape = Path(directory) / "commands.bin"
        subprocess.run([str(args.search), args.iso, args.mission, str(tape)], env=env,
                       check=True, timeout=1800)
        env["GT4_MISSION_COMMANDS"] = str(tape)
        if args.negative:
            env["GT4_MISSION_COMMAND_NEGATIVE"] = "1"
        result = subprocess.run([str(args.cycle), args.iso, args.mission], env=env, timeout=600)
        if args.negative:
            assert result.returncode != 0, "neutral commands unexpectedly passed physical success checks"
            print("PASS negative: neutral commands do not grant original success or payout")
        else:
            assert result.returncode == 0, "physical mission/prize/save cycle failed"


if __name__ == "__main__":
    main()
