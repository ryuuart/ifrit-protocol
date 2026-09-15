#!/usr/bin/env python3
"""The sky feed_sky draws: sent to a port, or written as a recording.

Two modes, one sky:

    python3 sender.py
        sends one JSON datagram every 1/rate seconds to 127.0.0.1:27020
        until interrupted, which is what makes the sketch move in a
        window.

    python3 sender.py --record data/sky.feed --seconds 6 --rate 4
        writes those same messages to a file in the feed recording
        format, which is what the sketch's plate replays.

A message is the sky exactly as feed_sky.fbs states it, in that
schema's own JSON form:

    {"bands": [{"height": h, "speed": s, "wobble": w}, ...],
     "wind": {"x": x, "y": y},
     "palette": [{"r": r, "g": g, "b": b, "a": a}, ...]}

A struct is an object there, wind and colour alike, and a field the
schema does not declare is not a message at all: the reader converts
every arrival through the schema and counts what does not fit.

The sky is a function of the message's own time and of nothing else, so
the file and the live send carry the same messages at the same seconds
and neither holds any randomness. A message stands at (i + 0.5) / rate
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
import socket
import struct
import sys
import time

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
                # can see, and every message travels in one datagram.
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
        hue = (0.52 + index * 0.11 + seconds * HUE_TURN) % 1.0
        red, green, blue = colorsys.hsv_to_rgb(hue, 0.55, 1.0)
        palette.append(
            {
                "r": round(red, 2),
                "g": round(green, 2),
                "b": round(blue, 2),
                "a": round(0.58 - index * 0.05, 2),
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


def send(host, port, rate):
    """The same messages, to a port, until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    started = time.monotonic()
    index = 0
    while True:
        at = (index + 0.5) / rate
        pause = started + at - time.monotonic()
        if pause > 0:
            time.sleep(pause)
        door.sendto(message_at(at), (host, port))
        index += 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write a recording at PATH instead of sending to the port",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=4.0, help="messages a second")
    parser.add_argument("--host", default="127.0.0.1", help="where to send")
    parser.add_argument("--port", type=int, default=27020, help="the port to send to")
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds, arguments.rate)
        print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
        return 0

    print(f"sending the sky to {arguments.host}:{arguments.port} at {arguments.rate:g}/s")
    try:
        send(arguments.host, arguments.port, arguments.rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
