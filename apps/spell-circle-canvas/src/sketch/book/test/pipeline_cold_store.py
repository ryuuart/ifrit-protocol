#!/usr/bin/env python3
"""A batch lane fills a store that stands empty, and never replaces one.

A headless sweep on the device draws the programs an open window draws,
with nobody waiting on any of them, so what it built is worth writing
down for a machine that has nothing written down yet — that is what
makes the FIRST interactive open of a machine the cheap one rather than
the second.

It must not do more than that. A run that drew a whole selection knows
less about what the next launch will open than a window run that drew
one sketch, so a store that already answers for this declaration has to
keep its answer, whatever a later batch run recorded.

So: one sweep over an empty store, which must leave exactly one set
holding everything its draws wanted; then a sweep of a DIFFERENT sketch
over that same store, which must record its own programs and write none
of them, leaving the first set byte for byte. A machine with no device
reports so and passes, having measured nothing.

Usage (invoked by the build; the paths are all absolute):
  pipeline_cold_store.py --sketchbook <Sketchbook binary> \\
      --first <name> --second <name> --work <scratch dir>
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

TALLY = re.compile(
    r"\[sketchbook\] pipelines: (\d+) built for a draw, (\d+) built ahead "
    r"of one, (\d+) found standing \((\d+) of them stood up ahead\), "
    r"(\d+) replayed, (\d+) of (\d+) keys written down"
)


class Tally:
    """What one run reported it spent on programs."""

    def __init__(self, match: re.Match):
        (
            self.built_for_a_draw,
            self.built_ahead,
            self.found,
            self.found_standing,
            self.replayed,
            self.written,
            self.recorded,
        ) = (int(group) for group in match.groups())

    def __str__(self) -> str:
        return (
            f"{self.built_for_a_draw} built for a draw, "
            f"{self.replayed} replayed, "
            f"{self.written} of {self.recorded} written down"
        )


def sweep(sketchbook: Path, sketch: str, out: Path, environment: dict) -> str:
    command = [
        str(sketchbook),
        "--headless",
        str(out),
        "--gpu",
        "--sketch",
        sketch,
    ]
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(
        command, capture_output=True, text=True, env=environment
    )
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if "no device runtime" in result.stderr:
        print("no device on this machine — nothing to measure")
        sys.exit(0)
    if result.returncode != 0:
        sys.exit(f"exited {result.returncode}")
    return result.stderr


def tally_of(reported: str, what: str) -> Tally:
    match = TALLY.search(reported)
    if not match:
        sys.exit(f"{what}: no pipeline tally was printed")
    tally = Tally(match)
    print(f"{what}: {tally}", flush=True)
    return tally


def digest(file: Path) -> str:
    return hashlib.sha256(file.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, required=True)
    parser.add_argument("--first", required=True)
    parser.add_argument("--second", required=True)
    parser.add_argument("--work", type=Path, required=True)
    arguments = parser.parse_args()

    work = arguments.work
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    store = work / "pipelines"
    out = work / "plates"
    out.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment["SIGIL_SKETCH_CACHE"] = str(work / "cache")
    environment["SIGIL_SKETCHBOOK_PIPELINES"] = str(store)
    environment["SIGIL_SKETCHBOOK_THUMBNAILS"] = str(work / "thumbnails")

    cold = tally_of(
        sweep(arguments.sketchbook, arguments.first, out, environment),
        "the cold store",
    )
    if cold.recorded == 0:
        sys.exit(
            "the sweep recorded no program at all, so it cannot have "
            "written one down"
        )
    if cold.written != cold.recorded:
        sys.exit(
            f"the sweep wrote {cold.written} of the {cold.recorded} keys it "
            "recorded, where a cold store takes all of them"
        )
    written = sorted(store.glob("*.keys"))
    if len(written) != 1:
        sys.exit(f"expected one recorded set under {store}, found {written}")
    seeded = digest(written[0])
    print(f"seeded {written[0].name}, {written[0].stat().st_size} bytes")

    # A SECOND BATCH RUN LEAVES IT ALONE. It records its own programs —
    # otherwise this would pass on a lane that stopped recording at all —
    # and writes none of them.
    warm = tally_of(
        sweep(arguments.sketchbook, arguments.second, out, environment),
        "the warm store",
    )
    if warm.recorded == 0:
        sys.exit("the second sweep recorded nothing, so it proves nothing")
    if warm.written != 0:
        sys.exit(
            f"the second sweep wrote {warm.written} keys over a store that "
            "already answered"
        )
    again = sorted(store.glob("*.keys"))
    if [one.name for one in again] != [one.name for one in written]:
        sys.exit(f"the store's sets changed: {written} became {again}")
    if digest(again[0]) != seeded:
        sys.exit(f"{again[0].name} was rewritten by the second sweep")
    print(f"{again[0].name} stands as the first sweep left it", flush=True)


if __name__ == "__main__":
    main()
