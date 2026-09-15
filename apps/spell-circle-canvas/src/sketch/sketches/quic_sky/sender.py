#!/usr/bin/env python3
"""The sky quic_sky draws, written as the recording a capture replays.

    python3 sender.py --record data/sky.feed --seconds 6 --rate 4

THIS SCRIPT ONLY WRITES THE FILE. Its datagram sibling beside feed_sky
is both ends of that door — it sends to the port as well — and this one
cannot be: speaking a quic:// door needs a QUIC stack, and Python ships
none in its standard library. The live other side is Seer, which
registers every transport this tree carries and so opens

    quic://127.0.0.1:27100?insecure=1

as a wire like any other; the sketch listens on that port with the
certificate and key beside it, and `insecure=1` is what reaches a
self-signed pair.

A message is the sky as plain JSON, which is how the sketch reads it:

    {"bands": [{"height": h, "speed": s, "wobble": w}, ...],
     "wind": {"x": x, "y": y},
     "palette": [{"r": r, "g": g, "b": b, "a": a}, ...]}

The sky is a function of the message's own time and of nothing else, so
the file and a live send carry the same messages at the same seconds and
neither holds any randomness. A message stands at (i + 0.5) / rate
rather than at i / rate so that no arrival falls on a whole second,
where a reader stepping in whole frames could take it a frame early or
a frame late.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import colorsys
import json
import math
import struct
import sys

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes. Both numbers are written little-endian, which is the
# byte order of every machine that reads one of these.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# EDIT THESE FIRST: the shape of the sky.
BAND_COUNT = 5
PALETTE_COUNT = 3
BAND_HEIGHT = (90.0, 190.0)  # the range a band's thickness breathes over
BAND_SPEED = 26.0  # how fast a band drifts on its own, px per second
BAND_WOBBLE = (10.0, 32.0)  # how far a band breathes up and down
WIND_SPEED = 30.0  # the drift every band shares, px per second
HUE_TURN = 0.035  # how far round the wheel the palette turns per second


def sky_at(seconds):
    """The sky at `seconds`, as the message the sketch reads."""
    bands = []
    for index in range(BAND_COUNT):
        phase = seconds * 0.31 + index * 1.17
        low, high = BAND_HEIGHT
        floor, ceiling = BAND_WOBBLE
        bands.append(
            {
                # Whole pixels: a fraction of one is not a thickness anybody
                # can see, and a smaller message is a smaller stream.
                "height": round(low + (high - low) * (0.5 + 0.5 * math.sin(phase))),
                # Alternate bands run against the wind, so the sky shears.
                "speed": round(
                    BAND_SPEED * math.sin(phase * 0.7 + 0.4) * (1 if index % 2 else -1),
                    1,
                ),
                "wobble": round(
                    floor + (ceiling - floor) * (0.5 + 0.5 * math.cos(phase * 1.3))
                ),
            }
        )
    wind = {"x": round(WIND_SPEED * math.sin(seconds * 0.17), 1), "y": 0}
    palette = []
    for index in range(PALETTE_COUNT):
        hue = (0.58 + index * 0.09 + seconds * HUE_TURN) % 1.0
        red, green, blue = colorsys.hsv_to_rgb(hue, 0.5, 1.0)
        palette.append(
            {
                "r": round(red, 2),
                "g": round(green, 2),
                "b": round(blue, 2),
                "a": round(0.6 - index * 0.05, 2),
            }
        )
    return {"bands": bands, "wind": wind, "palette": palette}


def message_at(seconds):
    """The bytes of one message: the sky, as compact JSON."""
    return json.dumps(sky_at(seconds), separators=(",", ":")).encode("utf-8")


def frame_times(seconds, rate):
    """The time of every message in a run of `seconds` at `rate` a second."""
    count = int(seconds * rate)
    return [(index + 0.5) / rate for index in range(count)]


def write_recording(path, seconds, rate):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in frame_times(seconds, rate):
            payload = message_at(at)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        default="data/sky.feed",
        help="where to write the recording",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=4.0, help="messages a second")
    arguments = parser.parse_args(argv)

    written = write_recording(arguments.record, arguments.seconds, arguments.rate)
    print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
