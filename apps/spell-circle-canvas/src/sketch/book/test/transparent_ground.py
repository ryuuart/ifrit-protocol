#!/usr/bin/env python3
"""A sketch grounded in nothing is drawn on nothing, and is that shape.

A canvas is realized at the size the sketch declared and cleared to the
ground the sketch declared — the ground's ALPHA with it. Nothing else
here says so: every other sketch grounds itself in an opaque colour, so
a clear that dropped the alpha, or a surface allocated without one,
would draw exactly the same picture. This renders the one sketch that
declares `{0, 0, 0, 0}` and reads the pixels back.

What it asserts is what a publication carries, at the level a window is
not needed to reach it: the extent is the declared canvas to a whole
multiple, a pixel the sketch never drew on carries no colour and no
coverage, a pixel it drew reads the ink it named, and a pixel at the
edge of a mark carries partial coverage — so the ground is transparent
rather than merely black. The multiple is the one thing left free,
because a plate is photographed larger than its canvas on purpose.

It goes through the SWEEP rather than a still of a file, so nothing here
depends on a guest being compiled: the sketch is the registry's own, and
what is read is the plate the sweep writes for it.

Usage (invoked by the build; the paths are all absolute):
  transparent_ground.py --sketchbook <Sketchbook binary> \\
      --sketch <stem> --work <scratch dir> [--gpu]
"""

import argparse
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

# What the sketch declares, stated here rather than parsed out of it: the
# point of the case is that the file and the picture agree, so a sketch
# edited away from this fails loudly. A plate is photographed at a whole
# multiple of the canvas, which is the one thing left free.
CANVAS = (640, 360)
INK = (245, 214, 110)
# The middle of the filled dot, in canvas units.
DRAWN = (320, 180)


def pixels(png: Path) -> tuple[int, int, bytearray]:
    """An 8-bit RGBA PNG as width, height and unfiltered RGBA bytes."""
    data = png.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{png} is not a PNG")
    idat = b""
    offset = 8
    header = None
    while offset < len(data):
        (length,) = struct.unpack(">I", data[offset : offset + 4])
        chunk = data[offset + 4 : offset + 8]
        body = data[offset + 8 : offset + 8 + length]
        if chunk == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif chunk == b"IDAT":
            idat += body
        elif chunk == b"IEND":
            break
        offset += 12 + length
    width, height, depth, colour, _, _, interlace = header
    if (depth, colour, interlace) != (8, 6, 0):
        sys.exit(f"{png}: expected a non-interlaced 8-bit RGBA PNG, got {header}")
    raw = zlib.decompress(idat)
    stride = width * 4
    out = bytearray(stride * height)
    above = bytearray(stride)
    read = 0
    for y in range(height):
        kind = raw[read]
        read += 1
        line = bytearray(raw[read : read + stride])
        read += stride
        if kind == 1:
            for i in range(4, stride):
                line[i] = (line[i] + line[i - 4]) & 0xFF
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + above[i]) & 0xFF
        elif kind == 3:
            for i in range(stride):
                left = line[i - 4] if i >= 4 else 0
                line[i] = (line[i] + ((left + above[i]) >> 1)) & 0xFF
        elif kind == 4:
            for i in range(stride):
                left = line[i - 4] if i >= 4 else 0
                up = above[i]
                corner = above[i - 4] if i >= 4 else 0
                guess = left + up - corner
                distances = (abs(guess - left), abs(guess - up), abs(guess - corner))
                nearest = (left, up, corner)[distances.index(min(distances))]
                line[i] = (line[i] + nearest) & 0xFF
        out[y * stride : (y + 1) * stride] = line
        above = line
    return width, height, out


def at(image: tuple[int, int, bytearray], x: int, y: int) -> tuple[int, int, int, int]:
    width, _, data = image
    start = (y * width + x) * 4
    return tuple(data[start : start + 4])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, required=True)
    parser.add_argument("--sketch", required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--gpu", action="store_true")
    args = parser.parse_args()

    plates = args.work / "plates"
    shutil.rmtree(plates, ignore_errors=True)
    plates.mkdir(parents=True, exist_ok=True)
    command = [str(args.sketchbook), "--headless", str(plates),
               "--sketch", args.sketch, "--kind", "canvas"]
    if args.gpu:
        command.append("--gpu")
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(command, capture_output=True, text=True)
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        sys.exit(f"exited {result.returncode}")
    out = plates / f"plate_{args.sketch}.png"
    if not out.exists():
        sys.exit(f"the sweep wrote no {out}")

    image = pixels(out)
    width, height, _ = image
    over = width // CANVAS[0]
    if over < 1 or (width, height) != (CANVAS[0] * over, CANVAS[1] * over):
        sys.exit(
            f"the plate is {width}x{height}, which is not a whole multiple "
            f"of the {CANVAS[0]}x{CANVAS[1]} canvas the sketch declares — a "
            "canvas is realized at the shape it was declared at, which is "
            "the shape a subscriber receives"
        )

    for corner in ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1)):
        ground = at(image, *corner)
        if ground != (0, 0, 0, 0):
            sys.exit(
                f"the ground at {corner} reads {ground} where the sketch "
                "declares a colour with no colour and no coverage — the "
                "ground's alpha was dropped somewhere between the "
                "declaration and the surface"
            )

    drawn = at(image, DRAWN[0] * over, DRAWN[1] * over)
    if drawn[3] != 255 or drawn[:3] != INK:
        sys.exit(
            f"the mark at {DRAWN} reads {drawn} where the sketch draws "
            f"{INK} opaque — the ground is transparent but the picture is not"
        )

    # A MARK HAS EDGES. Somewhere on the mark's own row there is a pixel
    # it covers in part, and only a surface that carries alpha can hold
    # one: with the ground dropped to opaque black every pixel there
    # would read 255.
    row = DRAWN[1] * over
    partial = [x for x in range(width) if 0 < at(image, x, row)[3] < 255]
    if not partial:
        sys.exit(
            f"no pixel on row {row} is partly covered — a mark drawn on a "
            "transparent ground has edges that carry their own coverage"
        )
    print(f"{width}x{height} at {over}x, ground {at(image, 0, 0)}, mark "
          f"{drawn}, {len(partial)} partly covered pixels on row {row}",
          flush=True)


if __name__ == "__main__":
    main()
