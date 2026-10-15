#!/usr/bin/env python3
"""A window held at a moment shows the scene a still of that moment shows.

`--at` in a window run steps each presented sketch to that moment, as a
still is prepared, and then draws it again on every frame with its clock
standing. A probe written here moves a red bar across a blue canvas at a
known speed, so where the bar stands says what time the pixels show. The
still of `--frame --at` is the control; the shot of the real window must
put the bar at the same place across the canvas, and both runs must say
the time they drew.

Usage (invoked by the build; the paths are all absolute):
  held_moment.py --grimoire <Grimoire binary> --work <scratch dir>
"""

import argparse
import os
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from window_orientation import read_png, run

CANVAS = (400, 100)
SPEED = 200  # canvas units a second
MOMENT = 0.5
RED = (255, 0, 0)
BLUE = (0, 0, 255)

PROBE = f"""from sigil.sketch import sketch


@sketch(size={CANVAS!r}, background="#0000ff", captureAt=0)
class Held:
    def draw(self, pen, ctx):
        pen.background(0, 0, 255)
        pen.noStroke()
        pen.fill(255, 0, 0)
        pen.rect(ctx.elapsed * {SPEED}, 0, 8, {CANVAS[1]})
"""


def columns(picture, colour) -> list[int]:
    """Every column holding at least one pixel of @p colour."""
    width, height, channels, pixels = picture
    found = set()
    for row in range(height):
        base = row * width * channels
        for column in range(width):
            at = base + column * channels
            if tuple(pixels[at : at + 3]) == colour:
                found.add(column)
    return sorted(found)


def bar_across(picture, what: str) -> float:
    """Where the bar's left edge stands, as a fraction of the canvas."""
    blue = columns(picture, BLUE)
    red = columns(picture, RED)
    if not blue or not red:
        sys.exit(f"{what}: found no canvas or no bar — the probe did not render")
    left, right = blue[0], blue[-1]
    across = (red[0] - left) / (right - left + 1)
    print(f"{what}: the bar stands {across:.4f} across the canvas", flush=True)
    return across


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grimoire", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()

    work = args.work
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True, exist_ok=True)
    probe = work / "held_probe.py"
    probe.write_text(PROBE)
    environment = dict(os.environ)
    state = str(work / "state")
    expected = SPEED * MOMENT / CANVAS[0]

    still = work / "still.png"
    said = run(
        [
            str(args.grimoire),
            "--state",
            state,
            str(probe),
            "--frame",
            str(still),
            "--at",
            str(MOMENT),
        ],
        environment,
    )
    if f"t={MOMENT:g}s asked for" not in said:
        sys.exit(f"the still did not say it drew t={MOMENT:g}s")
    if abs(bar_across(read_png(still), "the still") - expected) > 1e-9:
        sys.exit("the still does not show the moment it was asked for")

    shot = work / "window.png"
    said = run(
        [
            str(args.grimoire),
            "--state",
            state,
            str(probe),
            "--shot",
            str(shot),
            "--at",
            str(MOMENT),
        ],
        environment,
    )
    if f"the canvas is held at t={MOMENT:.9g}s" not in said:
        sys.exit(f"the window did not say it held t={MOMENT:g}s")
    # The window fits the canvas into its own pixels, so a scaled edge
    # may fall a pixel either way of the still's.
    if abs(bar_across(read_png(shot), "the held window") - expected) > 0.01:
        sys.exit("the window does not show the moment it was held at")


if __name__ == "__main__":
    main()
