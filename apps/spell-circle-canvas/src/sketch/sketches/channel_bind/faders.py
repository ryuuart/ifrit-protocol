#!/usr/bin/env python3
"""The desk channel_bind listens to: three faders, to a port or to a file.

Two modes, one desk:

    python3 faders.py
        sends the three faders to 127.0.0.1:27080 until interrupted,
        which is what a window watches.

    python3 faders.py --record data/faders.feed --seconds 8
        writes that same traffic to a file in the feed recording format,
        which is what the sketch's plate replays.

WHAT THE DESK SAYS. Three addresses, one float each, every one of them a
reading on the same 0..127 travel a control surface sends:

    /fader/1   the blade's height   a slow sweep, all the way and back
    /fader/2   the wheel's turn     a climb that drops back to the start
    /fader/3   the plumb's sway     a faster sweep, out of step with the
                                    first

THREE RHYTHMS, and they are the point: each fader moves at its own rate
and over its own period, so nothing in the picture is in step with
anything else and each property is visibly answering its own wire. The
sketch registers no handler at all — a channel per address, a binding
chain per property — so what arrives here is what moves there, with
nothing in between.

NOTHING COMES BACK. This desk speaks and does not listen: the sketch
answers no message, so there is no socket to drain and no reply to
print.

A PACKET IS SPELLED HERE BY HAND, because the whole point of the sketch
is that nothing between the desk and the drawing had to be installed. An
OSC message is its address as a null-terminated string padded with nulls
to a multiple of four, then the type tag string — a comma and one
character per argument — padded the same way, then the arguments
themselves. A float is four bytes, big-endian, which is the byte order
the protocol writes every number in.

The traffic is a function of its own time and of nothing else, so the
file and the live send carry the same messages at the same seconds and
neither holds any randomness. Each fader stands off both the others'
grids and the whole frames a reader steps in, so no arrival is taken a
frame early or a frame late and no two of them share one moment.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
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

# EDIT THESE FIRST: what a fader reads at the top of its travel, and the
# three faders themselves — the address, the shape, how long one pass of
# that shape takes, where in the pass the run starts, how many readings a
# second go out, and where in the gap between two of them the reading
# stands.
FULL = 127.0
FADERS = (
    ("/fader/1", "sweep", 5.5, 0.00, 10.0, 0.13),
    ("/fader/2", "climb", 9.0, 0.00, 6.0, 0.41),
    ("/fader/3", "sweep", 3.1, 0.69, 15.0, 0.77),
)


def osc_string(text):
    """One OSC string: the text, a null, and nulls up to a multiple of four."""
    raw = text.encode("ascii") + b"\0"
    return raw + b"\0" * (-len(raw) % 4)


def osc_message(address, arguments):
    """The bytes of one message: the address, the type tags, the floats."""
    tags = "," + "f" * len(arguments)
    body = b"".join(struct.pack(">f", float(value)) for value in arguments)
    return osc_string(address) + osc_string(tags) + body


def sweep(seconds, period, phase):
    """A fader running its whole travel and back, once per `period`."""
    turn = 2.0 * math.pi * (seconds / period + phase)
    return round(FULL * 0.5 * (1.0 + math.sin(turn)), 2)


def climb(seconds, period, phase):
    """A fader climbing its whole travel and dropping back to nothing."""
    return round(FULL * ((seconds / period + phase) % 1.0), 2)


SHAPES = {"sweep": sweep, "climb": climb}


def traffic(seconds):
    """Every message of a run of `seconds`, as (at, address, arguments).

    A fader's nth reading stands at (n + offset) / rate, which is inside
    the gap between two frames a reader steps in and, the three offsets
    and the three rates being what they are, never at the same moment as
    another fader's.
    """
    messages = []
    for address, shape, period, phase, rate, offset in FADERS:
        for index in range(int(seconds * rate)):
            at = (index + offset) / rate
            messages.append((at, address, [SHAPES[shape](at, period, phase)]))
    messages.sort(key=lambda message: message[0])
    return messages


def write_recording(path, seconds):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, address, arguments in traffic(seconds):
            payload = osc_message(address, arguments)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def send(host, port, seconds):
    """The same traffic, to a port, over and over until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    schedule = traffic(seconds)
    started = time.monotonic()
    cycle = 0
    spoken = 0.0
    while True:
        for at, address, arguments in schedule:
            due = started + cycle * seconds + at
            remaining = due - time.monotonic()
            if remaining > 0:
                time.sleep(remaining)
            door.sendto(osc_message(address, arguments), (host, port))
            # One line a second, so a run says what it is sending without
            # a line per reading.
            if due - spoken >= 1.0:
                spoken = due
                readings = " ".join(f"{value:.2f}" for value in arguments)
                print(f"-> {address} {readings}")
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
    parser.add_argument(
        "--port", type=int, default=27080, help="the port to send to"
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds)
        print(
            f"{arguments.record}: {written} messages over {arguments.seconds:g}s"
        )
        return 0

    print(f"working the desk at {arguments.host}:{arguments.port}")
    try:
        send(arguments.host, arguments.port, arguments.seconds)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
