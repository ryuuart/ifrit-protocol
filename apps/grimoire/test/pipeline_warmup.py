#!/usr/bin/env python3
"""A sketch opened twice does not build its device programs twice.

A backend builds one device program per distinct draw and the thread
that recorded the draw waits for it, so a sketch whose root wears a
chain of runtime-shader stages pays one program per stage on its first
frame. Which programs a sketch's draws needed is written down as it
goes, under the name the host knows the sketch by, and read back the
next time the sketch is selected: the set is replayed on a worker before
the sketch opens, so its first frame finds every program standing and
builds none.

Nothing in a still can say so — `--frame` opens no window and a headless
still draws through another context — so this opens the real window
twice over one store and reads the tally each run closes with.

The first run starts on an empty store: it has nothing to replay, stands
up what it can of the stock stages, builds the rest as the draws ask for
them, and writes down the ones the sketch's draws wanted. The second run
replays that set before the sketch opens, and the sketch's first frame
must build no program at all — and the run must write back a set no
larger than the one it read, because a replayed program is reported as
built again and a run writing back everything it recorded would grow the
file at every launch.

A SKETCH WITH RUNTIME EFFECTS OF ITS OWN, named with `--own-effects`, is
opened once more before the two that are measured. Its effects are
compiled as it sets up, after the window's device context was made, and
a context can carry a key only for a program over an effect it was told
about when it was made; so the open before writes those effects down, the
first measured open declares them before its context exists and records
their programs, and the second must build none.

Usage (invoked by the build; the paths are all absolute):
  pipeline_warmup.py --grimoire <Grimoire binary> \\
      --sketch <name or file> [--own-effects <name or file>] \\
      --work <scratch dir>
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

TALLY = re.compile(
    r"\[grimoire\] pipelines: (\d+) built for a draw, (\d+) built ahead "
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
            f"{self.built_ahead} built ahead, "
            f"{self.found} found ({self.found_standing} standing ahead), "
            f"{self.replayed} replayed, "
            f"{self.written} of {self.recorded} written down"
        )


def run(command: list[str]) -> tuple[str, str]:
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(command, capture_output=True, text=True)
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        sys.exit(f"exited {result.returncode}")
    return result.stdout, result.stderr


FIRST_FRAME = re.compile(
    r"\[grimoire\] pipelines: the first frame of (\S+) built (\d+) programs "
    r"for its draws"
)


def names(reported: str, sketch: str) -> bool:
    """Whether the name a line carries is @p sketch: a registry key, or
    the path of a file the window was pointed at."""
    return reported == sketch or (
        sketch.endswith((".cpp", ".py")) and Path(reported) == Path(sketch).resolve()
    )


def first_frame_of(reported: str, sketch: str, what: str) -> int:
    """How many programs the first frame of @p sketch built."""
    for match in FIRST_FRAME.finditer(reported):
        if names(match.group(1), sketch):
            built = int(match.group(2))
            print(f"{what}: its first frame built {built} programs", flush=True)
            return built
    sys.exit(f"{what}: no first-frame count was printed for {sketch}")


def tally_of(reported: str, what: str) -> Tally:
    match = TALLY.search(reported)
    if not match:
        sys.exit(f"{what}: no pipeline tally was printed")
    tally = Tally(match)
    print(f"{what}: {tally}", flush=True)
    return tally


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grimoire", type=Path, required=True)
    parser.add_argument("--sketch", action="append", default=[])
    parser.add_argument("--own-effects", action="append", default=[])
    parser.add_argument("--work", type=Path, required=True)
    arguments = parser.parse_args()
    if not arguments.sketch and not arguments.own_effects:
        sys.exit("name a sketch to open with --sketch or --own-effects")
    shutil.rmtree(arguments.work, ignore_errors=True)
    for number, sketch in enumerate(arguments.sketch):
        measure(arguments.grimoire, sketch, arguments.work / f"sketch-{number}")
    for number, sketch in enumerate(arguments.own_effects):
        measure(
            arguments.grimoire,
            sketch,
            arguments.work / f"own-effects-{number}",
            writesEffectsFirst=True,
        )


def measure(
    grimoire: Path, sketch: str, work: Path, writesEffectsFirst: bool = False
) -> None:
    """Opens @p sketch twice over one store and holds the second open to
    building no program in its first frame."""
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    # Both opens keep their state under one root: the second replays what
    # the first recorded there, and neither reads or leaves anything
    # outside it — settings and recents included.
    state = work / "state"
    store = state / "pipelines"

    # A registry key is selected by name; a file is opened by its path.
    opening = (
        [str(Path(sketch).resolve())]
        if sketch.endswith((".cpp", ".py"))
        else ["--sketch", sketch]
    )
    shot = [
        str(grimoire),
        "--state",
        str(state),
        *opening,
        "--shot",
        str(work / "window.png"),
    ]

    # A FILE IS BUILT FIRST, by a still over the same state root: the build
    # it leaves is what both windows open on, so neither spends its run
    # compiling and the first open's tally is the sketch's own draws.
    if sketch.endswith((".cpp", ".py")):
        run(
            [
                str(grimoire),
                *opening,
                "--state",
                str(state),
                "--frame",
                str(work / "still.png"),
            ]
        )

    # ITS OWN EFFECTS WRITTEN DOWN FIRST, by an open of its own: they are
    # declared from the next launch on, so the first measured open is the
    # one that records the programs over them.
    if writesEffectsFirst:
        _, written_down = run(shot)
        if "renderer: Graphite GPU" in written_down and not re.search(
            r"\d+ runtime effects no declaration named written down",
            written_down,
        ):
            sys.exit(
                f"opening {sketch} wrote down none of its own runtime "
                "effects for the next launch to declare"
            )

    _, first = run(shot)
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
    first_bytes = written[0].stat().st_size
    print(f"recorded {written[0].name}, {first_bytes} bytes")

    _, second = run(shot)
    if not re.search(r"recorded programs stood up before \S+ opened", second):
        sys.exit("the second open replayed nothing it had recorded")
    again = tally_of(second, "the second open")
    if again.replayed == 0:
        sys.exit("the second open rebuilt none of the recorded programs")
    if again.found_standing == 0:
        sys.exit(
            "the second open's draws found no program that had been stood "
            "up ahead of them — the replay reached nothing they wanted"
        )
    # THE SKETCH'S FIRST FRAME BUILDS NOTHING: every program its draws
    # wanted, the stack's chain inlined into them among them, was stood up
    # from the set before the sketch opened.
    first_built = first_frame_of(first, sketch, "the first open")
    if first_built == 0:
        sys.exit(
            "the first open's first frame built no program, so the second "
            "cannot show one it no longer had to build"
        )
    if first_frame_of(second, sketch, "the second open") != 0:
        sys.exit(
            "the second open's first frame still built programs its "
            "recorded set should have stood up before it opened"
        )

    # THE SET IS WHAT THE DRAWS WANTED, NOT EVERYTHING THAT WAS BUILT. A
    # replayed program is reported as built again, so a run that wrote
    # back everything it recorded would write back its own replay and the
    # file would grow at every launch — and every key in it is a program
    # the next launch stands up before its canvas draws.
    rewritten = sorted(store.glob("*.keys"))
    if len(rewritten) != 1:
        sys.exit(f"expected one recorded set under {store}, found {rewritten}")
    second_bytes = rewritten[0].stat().st_size
    if second_bytes > first_bytes:
        sys.exit(
            f"the recorded set grew from {first_bytes} to {second_bytes} "
            f"bytes over a second open of the same sketch — a launch is "
            f"writing back more than its draws wanted"
        )
    print(
        f"the recorded set held at {second_bytes} bytes against the first "
        f"open's {first_bytes}",
        flush=True,
    )


if __name__ == "__main__":
    main()
