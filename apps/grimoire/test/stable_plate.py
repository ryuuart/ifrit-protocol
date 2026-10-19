#!/usr/bin/env python3
"""A sketch photographed twice on the device is the same picture twice.

A headless capture is taken at the moment the sketch declared under a
client's clock, so two captures of one sketch by one binary are one
picture, byte for byte. A program that reads its layer past the bounds
it declared, a clock read from the wall, a hash seeded by an address or
an unordered container walked in its own order each make the second
capture differ from the first; this renders the sketch twice, each in a
process of its own over a state root of its own, and compares the plates
byte for byte, naming where they differ when they do. A machine with no
device reports so and passes, having measured nothing.

Usage (invoked by the build; the paths are all absolute):
  stable_plate.py --grimoire <Grimoire binary> --sketch <stem> \\
      --work <scratch dir>
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def render(grimoire: Path, sketch: str, work: Path, run: str) -> Path:
    out = work / run
    command = [
        str(grimoire),
        "--state",
        str(work / f"state-{run}"),
        "--headless",
        str(out),
        "--gpu",
        "--sketch",
        sketch,
    ]
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(command, capture_output=True, text=True)
    if "no device runtime" in result.stderr or "no Vulkan" in result.stderr:
        print("no device on this machine — nothing to compare")
        sys.exit(0)
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        sys.exit(f"exited {result.returncode}")
    plates = sorted(out.glob("plate_*.png"))
    if len(plates) != 1:
        sys.exit(f"expected one plate under {out}, found {plates}")
    return plates[0]


def where(first: bytes, second: bytes) -> str:
    """The first byte the two files disagree at, as an offset."""
    for offset, (one, other) in enumerate(zip(first, second)):
        if one != other:
            return f"first differing byte at offset {offset}"
    return f"one file is {len(first)} bytes and the other {len(second)}"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grimoire", type=Path, required=True)
    parser.add_argument("--sketch", required=True)
    parser.add_argument("--work", type=Path, required=True)
    arguments = parser.parse_args()

    work = arguments.work
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    first = render(arguments.grimoire, arguments.sketch, work, "first")
    second = render(arguments.grimoire, arguments.sketch, work, "second")
    first_bytes, second_bytes = first.read_bytes(), second.read_bytes()
    if first_bytes != second_bytes:
        sys.exit(
            f"two device captures of {arguments.sketch} differ: "
            f"{where(first_bytes, second_bytes)} ({first} against {second})"
        )
    print(f"two device captures of {arguments.sketch} are one picture")


if __name__ == "__main__":
    main()
