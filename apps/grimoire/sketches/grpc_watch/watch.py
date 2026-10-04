#!/usr/bin/env python3
"""The caller grpc_watch answers — written down, since it cannot be spoken.

    python3 watch.py --record data/watch.feed
        writes the messages a caller sends, at the seconds it sends them,
        as a feed recording. That file is what the sketch's plate
        replays, so a still of the capture is a still of a scene a caller
        drove.

THERE IS NO LIVE MODE HERE. gRPC is not in the Python standard library,
and a script that needs something installed before it runs is a script
that does not run. The live other side is Seer, which opens the sketch's
own method as a caller and needs nothing written here:

    Seer grpc://127.0.0.1:27090/Sky/Watch

THE MESSAGES ARE THE TWO THINGS A HAND CAN DO, and are exactly what a
caller writes on the stream:

    {"kind": "Palette", "colors": [[r, g, b, a], ...]}
    {"kind": "Gust", "strength": s, "seconds": t}

A recording holds the messages and not who sent them, so what this writes
is the caller's half of the conversation and nothing of the sky that goes
back the other way.

The schedule is a fixed list of seconds and holds no randomness, so the
file is the same file every time it is written and a still of the plate
is the still it was yesterday.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import json
import struct
import sys

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes. Both numbers are written little-endian, which is the
# byte order of every machine that reads one of these.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# EDIT THESE FIRST: what the caller does, and when.
#
# The sketch captures at 2.5 seconds, so both turns land before that and
# the gust is half a second old there — which is the frame that shows
# both things a caller can do at once.
PALETTES = [
    [[0.95, 0.62, 0.35, 0.84], [0.92, 0.42, 0.42, 0.84], [0.99, 0.80, 0.45, 0.84]],
    [[0.40, 0.85, 0.68, 0.84], [0.30, 0.72, 0.80, 0.84], [0.70, 0.90, 0.55, 0.84]],
]
PALETTE_AT = [0.6, 1.6]  # when each palette turn lands
GUST_AT = 2.0  # when the gust lands
GUST_STRENGTH = 150.0  # how hard it blows, in px a second
GUST_FALL = 1.6  # how long it takes to die away


def moments():
    """Every message a caller sends, in order, as (second, message).

    Two palette turns and a gust. The turns come from the SECOND palette
    on, because the sketch opens on the first one and a turn nobody can
    see is not a turn.
    """
    written = [
        (at, {"kind": "Palette", "colors": colours})
        for at, colours in zip(PALETTE_AT, PALETTES)
    ]
    written.append(
        (GUST_AT, {"kind": "Gust", "strength": GUST_STRENGTH, "seconds": GUST_FALL})
    )
    return sorted(written, key=lambda moment: moment[0])


def payload_of(message):
    """The bytes of one message: compact JSON, as it goes on the wire."""
    return json.dumps(message, separators=(",", ":")).encode("utf-8")


def write_recording(path):
    """Every message a caller sends, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, message in moments():
            payload = payload_of(message)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write the caller's messages at PATH as a feed recording",
    )
    arguments = parser.parse_args(argv)

    if not arguments.record:
        print("nothing is spoken from here: gRPC is not in this library.")
        print("the live caller is Seer, on grpc://127.0.0.1:27090/Sky/Watch")
        print("the recording is written with --record data/watch.feed")
        return 0

    written = write_recording(arguments.record)
    print(f"{arguments.record}: {written} messages a caller sends")
    return 0


if __name__ == "__main__":
    sys.exit(main())
