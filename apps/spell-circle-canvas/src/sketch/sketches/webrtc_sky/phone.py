#!/usr/bin/env python3
"""The phone webrtc_sky's page would be, written down instead of spoken.

    python3 phone.py
        writes data/phone.feed beside this script: the messages a hand
        on that page sends, at the seconds it would send them, as a feed
        recording — which is what the sketch's plate replays instead of
        opening a port.

RECORDING ONLY, and that is the transport's doing. Its websocket sibling
ships a stand-in phone that really connects, because a websocket client
is a hundred lines of socket and masking. Speaking THIS door means
answering an offer with a session description, gathering candidate
addresses, agreeing on encryption and opening a stream over it — a
WebRTC stack, which the Python standard library does not ship and which
nothing here is allowed to install. So a live phone for this sketch is a
browser on the page beside this file, and what stands in for one when
nobody is holding a phone is the recording below.

The messages are the two things a hand can do, and are exactly what that
page sends on its channel:

    {"kind": "Palette", "colors": [[r, g, b, a], ...]}
    {"kind": "Gust", "strength": s, "seconds": t}

The schedule is a function of the run's own time and holds no
randomness, so the file carries the same messages at the same seconds
however often it is written, and a still of the plate is a still of the
window.
"""

import argparse
import json
import pathlib
import struct
import sys

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes. Both numbers are written little-endian, which is the
# byte order of every machine that reads one of these.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# EDIT THESE FIRST: what this phone does, and when.
PALETTES = [
    [[0.44, 0.60, 0.90, 0.82], [0.54, 0.50, 0.86, 0.82], [0.38, 0.72, 0.84, 0.82]],
    [[0.95, 0.62, 0.35, 0.82], [0.92, 0.42, 0.42, 0.82], [0.99, 0.80, 0.45, 0.82]],
    [[0.40, 0.85, 0.68, 0.82], [0.30, 0.72, 0.80, 0.82], [0.70, 0.90, 0.55, 0.82]],
]
PALETTE_FIRST = 0.6  # when the first palette turn lands
GUST_FIRST = 2.0  # when the first gust lands
GUST_STRENGTH = 140.0  # how hard a gust blows, in px a second
GUST_FALL = 1.6  # how long it takes to die away

# Where the recording is written when nothing else is asked for: beside
# this script, where the sketch mounts it.
RECORDING = pathlib.Path(__file__).parent / "data" / "phone.feed"


def moments(palette_every, gust_every):
    """Every message this phone sends, in order, for as long as it is
    asked for: a palette turn every `palette_every` seconds from the
    first one, and a gust every `gust_every` from the first of those.

    The turns start at the SECOND palette, because the sketch opens on
    the first one and a turn nobody can see is not a turn.
    """
    next_palette, next_gust, turn = PALETTE_FIRST, GUST_FIRST, 1
    while True:
        if next_palette <= next_gust:
            yield next_palette, {
                "kind": "Palette",
                "colors": PALETTES[turn % len(PALETTES)],
            }
            turn += 1
            next_palette += palette_every
        else:
            yield next_gust, {
                "kind": "Gust",
                "strength": GUST_STRENGTH,
                "seconds": GUST_FALL,
            }
            next_gust += gust_every


def payload_of(message):
    """The bytes of one message: compact JSON, as it goes on the wire."""
    return json.dumps(message, separators=(",", ":")).encode("utf-8")


def write_recording(path, seconds, palette_every, gust_every):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, message in moments(palette_every, gust_every):
            if at >= seconds:
                break
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
        type=pathlib.Path,
        default=RECORDING,
        help="where the recording is written",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument(
        "--palette-every", type=float, default=2.0, help="seconds between turns"
    )
    parser.add_argument(
        "--gust-every", type=float, default=2.0, help="seconds between gusts"
    )
    arguments = parser.parse_args(argv)

    written = write_recording(
        arguments.record,
        arguments.seconds,
        arguments.palette_every,
        arguments.gust_every,
    )
    print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
