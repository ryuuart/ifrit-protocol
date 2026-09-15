#!/usr/bin/env python3
"""The messages feed_events reads: JSON to a port, or a recording.

Two modes, one sequence:

    python3 events.py
        sends the sequence to 127.0.0.1:27072 until interrupted, which
        is what makes the sketch move in a window.

    python3 events.py --record data/events.feed --seconds 8
        writes that same sequence to a file in the feed recording
        format, which is what the sketch's plate replays.

WHAT A MESSAGE IS. One JSON record, and its `kind` is what says which
reader it is for:

    {"kind": "Wind", "value": 24}
        STATE: what the wind is now, and it stays that until another
        one says otherwise.
    {"kind": "Gust", "strength": 0.8, "seconds": 1.5}
        AN EVENT: it happened, it is over, and what is left of it is
        the motion it started.
    {"kind": "Palette", "colors": [[r, g, b, a], ...]}
        STATE: the colours the bands are tinted from.
    {"kind": "Thunder", ...}
        A kind this reader has none for. It is sent deliberately: what
        a scene cannot use is a fact about the wire, and the sketch
        counts it rather than dropping it in silence.

The sequence is a function of its own time and of nothing else, so the
file and the live send carry the same messages at the same seconds and
neither holds any randomness. A message stands off both the state grid
and the whole frames a reader steps in, so no arrival is taken a frame
early or a frame late.

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

# EDIT THESE FIRST: what arrives, and when.
WIND_RATE = 4.0  # readings a second
WIND_SPAN = 38.0  # how far the wind swings either way, px a second
WIND_PERIOD = 9.0  # how long one swing takes
# When a gust happens, how hard, and over how long it crosses.
GUSTS = ((0.83, 0.55, 0.9), (2.37, 0.92, 1.4), (5.11, 0.7, 1.2), (6.43, 0.48, 0.8))
# When the colours turn over.
PALETTES = (1.29, 4.57)
# When something nobody here reads arrives, and what it calls itself.
STRANGERS = ((1.91, "Thunder"), (3.53, "Aurora"))


def wind_at(seconds):
    """The wind at `seconds`: one slow swing either side of still air."""
    return round(WIND_SPAN * math.sin(2.0 * math.pi * seconds / WIND_PERIOD), 2)


def palette_at(index):
    """The `index`th turn of the colours: three tints, each r, g, b, a."""
    colors = []
    for step in range(3):
        hue = (0.55 + index * 0.31 + step * 0.09) % 1.0
        red, green, blue = colorsys.hsv_to_rgb(hue, 0.42, 0.92)
        colors.append(
            [round(red, 3), round(green, 3), round(blue, 3), round(0.9 - step * 0.06, 3)]
        )
    return colors


def sequence(seconds):
    """Every message of a run of `seconds`, as (at, message).

    A state reading stands at (i + 0.5) / rate, which is half a frame
    off the grid a reader steps in; every event stands between two
    readings, so no two messages share one moment.
    """
    messages = []
    for index in range(int(seconds * WIND_RATE)):
        at = (index + 0.5) / WIND_RATE
        messages.append((at, {"kind": "Wind", "value": wind_at(at)}))
    for at, strength, crossing in GUSTS:
        if at < seconds:
            messages.append(
                (at, {"kind": "Gust", "strength": strength, "seconds": crossing})
            )
    for index, at in enumerate(PALETTES):
        if at < seconds:
            messages.append((at, {"kind": "Palette", "colors": palette_at(index)}))
    for at, kind in STRANGERS:
        if at < seconds:
            messages.append((at, {"kind": kind, "note": "nobody here reads this"}))
    messages.sort(key=lambda message: message[0])
    return messages


def payload_of(message):
    """The bytes of one message: the record, as compact JSON."""
    return json.dumps(message, separators=(",", ":")).encode("utf-8")


def write_recording(path, seconds):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, message in sequence(seconds):
            payload = payload_of(message)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def send(host, port, seconds):
    """The same sequence, to a port, over and over until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    schedule = sequence(seconds)
    started = time.monotonic()
    cycle = 0
    while True:
        for at, message in schedule:
            pause = started + cycle * seconds + at - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            door.sendto(payload_of(message), (host, port))
            if message["kind"] != "Wind":
                print(f"-> {message['kind']}")
        cycle += 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write a recording at PATH instead of sending to the port",
    )
    parser.add_argument(
        "--seconds",
        type=float,
        default=8.0,
        help="how long a recording runs, and how long one live cycle takes",
    )
    parser.add_argument("--host", default="127.0.0.1", help="where to send")
    parser.add_argument("--port", type=int, default=27072, help="the port to send to")
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds)
        print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
        return 0

    print(f"sending the sequence to {arguments.host}:{arguments.port}")
    try:
        send(arguments.host, arguments.port, arguments.seconds)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
