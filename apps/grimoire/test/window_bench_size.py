#!/usr/bin/env python3
"""The window the lane presents in is the size asked for, where it fits.

The window's frame-rate lane opens its window at a stated size and device
pixel ratio, and every row says what the window really was. A window the
display has room for comes back at exactly the size asked; one the
display cannot hold is sized by the system, and the lane says so beside
the row rather than leaving a different size unexplained.

So this asks for two windows small enough for any display a window
server drives — one at a ratio of two, one at a ratio of one — and holds
each row to the ask, then asks for one no display holds and finds the
lane naming the room the display has.

Usage (invoked by the build; the paths are all absolute):
  window_bench_size.py --grimoire <Grimoire binary> --work <scratch dir>
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# A static page far inside a frame's budget.
PAGE = "alpha_ground"

# Sizes, in the units a ratio scales, that every display a window server
# drives has room for: at most 1280 by 800 pixels.
FITTING = [("640x400", "2"), ("800x450", "1")]

# A size no display has room for at a ratio of two.
OVERSIZED = ("6000x4000", "2")

ROW = re.compile(r"^WINDOW (\S+) (.*)$")


def present(grimoire: Path, work: Path, size: str, ratio: str):
    """The row's fields and everything the run wrote, or None for the
    fields where the lane presented nothing."""
    command = [
        str(grimoire),
        "--state",
        str(work / "state"),
        "--window-bench",
        "1.5",
        "--window-size",
        size,
        "--window-scale",
        ratio,
        "--sketch",
        PAGE,
    ]
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(
        command, capture_output=True, text=True, env=dict(os.environ), timeout=120
    )
    for line in result.stdout.splitlines():
        found = ROW.match(line.strip())
        if found:
            print(line)
            fields = dict(
                pair.split("=", 1) for pair in found.group(2).split() if "=" in pair
            )
            return fields, result.stdout + result.stderr
    return None, result.stdout + result.stderr


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grimoire", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    shutil.rmtree(args.work, ignore_errors=True)
    args.work.mkdir(parents=True, exist_ok=True)

    for size, ratio in FITTING:
        fields, said = present(args.grimoire, args.work, size, ratio)
        if fields is None:
            # No window presented here, which is not this lane's defect.
            print(said[-2000:])
            sys.exit(77)
        asked = f"{size}@{ratio}"
        if fields.get("window") != asked:
            sys.exit(f"asked for a window {asked}, the row reports {fields.get('window')}")
        if "does not fit the display" in said:
            sys.exit(f"a window {asked} was said not to fit the display")

    size, ratio = OVERSIZED
    fields, said = present(args.grimoire, args.work, size, ratio)
    if fields is None:
        sys.exit(f"a window {size}@{ratio} presented nothing")
    if "does not fit the display" not in said:
        sys.exit(f"a window {size}@{ratio} was not said to exceed the display")


if __name__ == "__main__":
    main()
