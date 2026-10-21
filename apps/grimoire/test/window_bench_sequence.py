#!/usr/bin/env python3
"""A sketch reads the same rate late in a sequence as it reads alone.

The window's frame-rate lane presents the sketches it is asked for one
after another in one process. A row is only a measurement if it is the
sketch's own: whatever the selections before it left behind, and however
long the process has been running, a cheap page must read what it reads
presented alone. A process the system has demoted to its background tier
half a minute into the run, or a selection whose work is still going on
behind the next, reads as a cheap page presenting at a fraction of the
display's rate while its own work stays tiny.

So this presents a cheap page alone, then the same page again and again
with another cheap page between each showing, for long enough that the
last showing is well past the first half minute of the process, and
holds every showing of the page to the band of the page alone.

Usage (invoked by the build; the paths are all absolute):
  window_bench_sequence.py --grimoire <Grimoire binary> --work <scratch dir>
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# The page measured, and the page shown between its showings: both
# static, both far inside a frame's budget on any machine with a window.
PAGE = "alpha_ground"
BETWEEN = "bousen"

# How many times the page is shown in the sequence, each after BETWEEN.
# With the lane's warm-up and the stretch below each showing costs about
# four seconds, so the last showing of the page falls past the first
# minute of the process.
SHOWINGS = 8

# The measured stretch of each showing, in seconds.
STRETCH = "2.5"

# How far below the page alone a showing in the sequence may read: the
# lane's own band for a presented rate.
BAND = 0.10

ROW = re.compile(r"^WINDOW (\S+) (.*)$")


def rows_of(output: str) -> list[tuple[str, dict]]:
    """The WINDOW rows, in the order printed, as (stem, fields)."""
    rows = []
    for line in output.splitlines():
        found = ROW.match(line.strip())
        if not found:
            continue
        fields = dict(
            pair.split("=", 1) for pair in found.group(2).split() if "=" in pair
        )
        rows.append((found.group(1), fields))
    return rows


def present(grimoire: Path, work: Path, stems: list[str]) -> list[tuple[str, dict]]:
    command = [
        str(grimoire),
        "--state",
        str(work / "state"),
        "--window-bench",
        STRETCH,
        "--window-size",
        "900x700",
        "--window-scale",
        "2",
    ]
    for stem in stems:
        command += ["--sketch", stem]
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(
        command, capture_output=True, text=True, env=dict(os.environ), timeout=280
    )
    sys.stdout.write(result.stdout)
    rows = rows_of(result.stdout)
    if result.returncode != 0:
        sys.stderr.write(result.stderr[-4000:])
        sys.exit(f"exited {result.returncode}")
    if [stem for stem, _ in rows] != stems:
        sys.exit(f"asked for {stems}, the rows name {[stem for stem, _ in rows]}")
    return rows


def present_alone(grimoire: Path, work: Path) -> float | None:
    """The page's rate presented alone, or None where the lane could not
    present it at all."""
    command = [
        str(grimoire),
        "--state",
        str(work / "state"),
        "--window-bench",
        STRETCH,
        "--window-size",
        "900x700",
        "--window-scale",
        "2",
        "--sketch",
        PAGE,
    ]
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(
        command, capture_output=True, text=True, env=dict(os.environ), timeout=120
    )
    sys.stdout.write(result.stdout)
    rows = rows_of(result.stdout)
    if len(rows) != 1 or "fps" not in rows[0][1]:
        print(f"the window lane presented nothing here: {rows}")
        return None
    return float(rows[0][1]["fps"])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grimoire", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    shutil.rmtree(args.work, ignore_errors=True)
    args.work.mkdir(parents=True, exist_ok=True)

    # A machine where the page alone is not presented has no window to
    # measure in, which is not this lane's defect to report.
    alone = present_alone(args.grimoire, args.work)
    if alone is None:
        sys.exit(77)
    sequence = [PAGE]
    for _ in range(SHOWINGS - 1):
        sequence += [BETWEEN, PAGE]
    rows = present(args.grimoire, args.work, sequence)
    floor = alone * (1.0 - BAND)
    for showing, (stem, fields) in enumerate(row for row in rows if row[0] == PAGE):
        if "fps" not in fields:
            sys.exit(f"{stem} was not measured at showing {showing + 1}: {fields}")
        rate = float(fields["fps"])
        print(f"{stem} showing {showing + 1}: {rate:.1f} fps (alone {alone:.1f})")
        if rate < floor:
            sys.exit(
                f"{stem} read {rate:.1f} fps at showing {showing + 1} of the "
                f"sequence and {alone:.1f} alone, below the band of "
                f"{BAND:.0%}; work {fields.get('work')}"
            )


if __name__ == "__main__":
    main()
