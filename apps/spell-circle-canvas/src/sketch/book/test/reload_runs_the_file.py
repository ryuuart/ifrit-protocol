#!/usr/bin/env python3
"""A hot-loaded sketch draws its OWN body, not the host's copy of it.

Every sketch in the registry is compiled into the host as well as into
the dylib a live edit produces, so the two images carry the same symbols
for the same sketch. Nothing about the dlopen reports which of them ran:
the host says it built, the picture comes out, and a picture drawn by the
host's copy looks exactly like a correct one until someone edits the file
and nothing moves.

So this drives the difference. A copy of a registry sketch is written
with its ground colour replaced by one the original does not use, taken
through `--frame`, and the corner pixel of the result must be that
colour. The control is the same sketch drawn from the registry, whose
corner must NOT be it: without that, a sketch whose ground already
happened to be the fixture colour would pass while proving nothing.

Usage (invoked by the build; the paths are all absolute):
  reload_runs_the_file.py --sketchbook <Sketchbook binary> \\
      --source <crossing_rule.cpp> --work <scratch dir>
"""

import argparse
import re
import struct
import subprocess
import sys
import zlib
from pathlib import Path

# The colour the fixture copy grounds itself in: fully saturated red,
# which no sketch in the registry uses as a ground and which survives the
# encoder exactly.
FIXTURE_RGBA = (255, 0, 0, 255)
FIXTURE_LITERAL = "{1, 0, 0, 1}"

# The constant the sketch's `ctx.background` names and its sheet grounds
# with, matched by its declaration so a fixture that stops substituting
# fails loudly here rather than silently rendering the original.
GROUND_DECLARATION = re.compile(r"(SkColor4f\s+kGround)\{[^}]*\}")


def corner_pixel(png: Path) -> tuple[int, int, int, int]:
    """The top-left pixel of an 8-bit RGBA PNG.

    Read without a decoder: the first pixel of the first scanline has no
    pixel to its left and no row above it, so every filter type reduces
    to the stored bytes there and the filter byte can be skipped.
    """
    data = png.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{png} is not a PNG")
    idat = b""
    offset = 8
    depth = colour_type = None
    while offset < len(data):
        (length,) = struct.unpack(">I", data[offset : offset + 4])
        chunk = data[offset + 4 : offset + 8]
        body = data[offset + 8 : offset + 8 + length]
        if chunk == b"IHDR":
            _, _, depth, colour_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", body
            )
            if (depth, colour_type, interlace) != (8, 6, 0):
                sys.exit(
                    f"{png}: expected a non-interlaced 8-bit RGBA PNG, got "
                    f"depth {depth}, colour type {colour_type}, interlace "
                    f"{interlace}"
                )
        elif chunk == b"IDAT":
            idat += body
        elif chunk == b"IEND":
            break
        offset += 12 + length
    raw = zlib.decompress(idat)
    return tuple(raw[1:5])


def run(command: list[str]) -> None:
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(command, capture_output=True, text=True)
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        sys.exit(f"exited {result.returncode}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()

    work = args.work
    plates = work / "plates"
    plates.mkdir(parents=True, exist_ok=True)

    # THE FIXTURE COPY. The registry's own file is never touched: the
    # copy stands in a directory of its own, which is also the shape a
    # sketch outside this checkout has.
    original = args.source.read_text()
    fixture_text, substitutions = GROUND_DECLARATION.subn(
        r"\1" + FIXTURE_LITERAL, original
    )
    if substitutions != 1:
        sys.exit(
            f"{args.source}: expected one kGround declaration to ground the "
            f"fixture with, found {substitutions} — the fixture needs a "
            "sketch whose whole canvas is one named colour"
        )
    fixture = work / args.source.name
    fixture.write_text(fixture_text)

    # THE RELOADED PICTURE: compiled from the copy, dlopened, drawn.
    reloaded = work / "reloaded.png"
    run([str(args.sketchbook), str(fixture), "--frame", str(reloaded)])
    drawn = corner_pixel(reloaded)
    if drawn != FIXTURE_RGBA:
        sys.exit(
            f"the hot-loaded sketch drew {drawn} where the file on disk says "
            f"{FIXTURE_RGBA} — the host's own copy of this sketch ran instead "
            "of the one that was just compiled"
        )

    # THE CONTROL: the same sketch out of the registry, which the fixture
    # cannot have touched.
    run(
        [
            str(args.sketchbook),
            "--headless",
            str(plates),
            "--sketch",
            args.source.stem,
            "--kind",
            "canvas",
        ]
    )
    compiled_in = plates / f"plate_{args.source.stem}.png"
    if not compiled_in.exists():
        sys.exit(f"the sweep wrote no {compiled_in}")
    host_drawn = corner_pixel(compiled_in)
    if host_drawn == FIXTURE_RGBA:
        sys.exit(
            f"the registry's own {args.source.stem} draws {host_drawn}, which "
            "is the fixture's colour — the case above proves nothing while "
            "that is true"
        )
    print(f"reloaded {drawn}, compiled in {host_drawn}", flush=True)


if __name__ == "__main__":
    main()
