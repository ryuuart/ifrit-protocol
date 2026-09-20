#!/usr/bin/env python3
"""A sketch opened twice does not build its device programs twice.

A backend builds one device program per distinct draw and the thread
that recorded the draw waits for it, so a sketch whose root wears a
chain of runtime-shader stages pays one program per stage on its first
frame. Which programs a run needed is written down as it goes, under the
platform cache location, and read back at the next launch: the set is
replayed on a worker before the first frame, so the frame finds the
programs standing instead of building them.

Nothing in a still can say so — `--frame` opens no window and a headless
still draws through another context — so this opens the real window
twice over one store and reads the tally each run closes with.

The first run starts on an empty store: it has nothing to replay, stands
up what it can of the stock stages, builds the rest as the draws ask for
them, and writes the set. The second run replays that set, and its draws
must find programs standing that the first run had to build.

Usage (invoked by the build; the paths are all absolute):
  pipeline_warmup.py --sketchbook <Sketchbook binary> --sketch <name> \\
      --work <scratch dir>
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

TALLY = re.compile(
    r"\[sketchbook\] pipelines: (\d+) built for a draw, (\d+) built ahead "
    r"of one, (\d+) found standing \((\d+) of them stood up ahead\), "
    r"(\d+) replayed, (\d+) keys recorded"
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
            self.recorded,
        ) = (int(group) for group in match.groups())

    def __str__(self) -> str:
        return (
            f"{self.built_for_a_draw} built for a draw, "
            f"{self.built_ahead} built ahead, "
            f"{self.found} found ({self.found_standing} standing ahead), "
            f"{self.replayed} replayed, {self.recorded} recorded"
        )


def run(command: list[str], environment: dict) -> tuple[str, str]:
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(
        command, capture_output=True, text=True, env=environment
    )
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        sys.exit(f"exited {result.returncode}")
    return result.stdout, result.stderr


def tally_of(reported: str, what: str) -> Tally:
    match = TALLY.search(reported)
    if not match:
        sys.exit(f"{what}: no pipeline tally was printed")
    tally = Tally(match)
    print(f"{what}: {tally}", flush=True)
    return tally


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, required=True)
    parser.add_argument("--sketch", required=True)
    parser.add_argument("--work", type=Path, required=True)
    arguments = parser.parse_args()

    work = arguments.work
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    store = work / "pipelines"
    environment = dict(os.environ)
    environment["SIGIL_SKETCH_CACHE"] = str(work / "cache")
    environment["SIGIL_SKETCHBOOK_PIPELINES"] = str(store)
    environment["SIGIL_SKETCHBOOK_THUMBNAILS"] = str(work / "thumbnails")

    shot = [
        str(arguments.sketchbook),
        "--sketch",
        arguments.sketch,
        "--shot",
        str(work / "window.png"),
    ]

    _, first = run(shot, environment)
    if "renderer: Graphite GPU" not in first:
        # No device behind the window, so no device program was ever
        # built and there is nothing here to measure.
        print("no Graphite renderer on this machine — nothing to measure")
        sys.exit(0)
    opening = tally_of(first, "the first open")
    if opening.built_for_a_draw == 0:
        sys.exit(
            "the first open built no program for a draw, so the second "
            "open cannot show one it did not have to build"
        )
    written = sorted(store.glob("*.keys"))
    if len(written) != 1:
        sys.exit(f"expected one recorded set under {store}, found {written}")
    print(f"recorded {written[0].name}, {written[0].stat().st_size} bytes")

    _, second = run(shot, environment)
    if "stood up before the first frame" not in second:
        sys.exit("the second open replayed nothing it had recorded")
    again = tally_of(second, "the second open")
    if again.replayed == 0:
        sys.exit("the second open rebuilt none of the recorded programs")
    if again.found_standing == 0:
        sys.exit(
            "the second open's draws found no program that had been stood "
            "up ahead of them — the replay reached nothing they wanted"
        )
    if again.built_for_a_draw >= opening.built_for_a_draw:
        sys.exit(
            f"the second open still built {again.built_for_a_draw} programs "
            f"for a draw against the first open's "
            f"{opening.built_for_a_draw} — the recorded set removed nothing"
        )
    print(
        f"the second open built {again.built_for_a_draw} programs for a draw "
        f"where the first built {opening.built_for_a_draw}",
        flush=True,
    )


if __name__ == "__main__":
    main()
