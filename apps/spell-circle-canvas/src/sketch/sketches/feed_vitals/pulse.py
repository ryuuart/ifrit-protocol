#!/usr/bin/env python3
"""The pulse feed_vitals meters: short binary messages at an uneven rhythm.

Two modes, one rhythm:

    python3 pulse.py
        sends to 127.0.0.1:27021 until interrupted, which is what gives
        the sketch a live signal to meter in a window.

    python3 pulse.py --record data/pulse.feed --seconds 8
        writes those same messages to a file in the feed recording
        format, which is what the sketch's plate replays.

A message is sixteen bytes — a four-byte mark, the count of messages
before it, and two floats — and the sketch never decodes it: it shows
the bytes as hex pairs, because what that sketch is about is the feed
and not the meaning of what crosses it.

The rhythm is a cycle of gaps, bursts and silences in turn, so the strip
chart has a shape rather than a comb. It repeats until the run is over,
and it holds no randomness: the file and the live send carry the same
messages at the same seconds.

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

# EDIT THESE FIRST: the rhythm, and what one message holds.
FIRST = 0.07  # when the first message stands, seconds
RHYTHM = (
    0.037, 0.037, 0.037, 0.037, 0.037,  # a burst
    0.61,                                # a silence
    0.046, 0.046, 0.046,                 # a shorter burst
    0.93,                                # a longer silence
    0.029, 0.029, 0.029, 0.029, 0.029, 0.029,  # the fastest burst
    0.44,
    0.13, 0.13, 0.13,                    # a steady stretch
    0.71,
)
MESSAGE = struct.Struct("<4sIff")
MARK = b"PULS"


def message_at(count, seconds):
    """The bytes of one message: the mark, the count, the phase, the level."""
    phase = math.fmod(seconds, 1.0)
    level = 0.5 + 0.5 * math.sin(seconds * 1.7)
    return MESSAGE.pack(MARK, count, phase, level)


def frame_times(seconds):
    """The time of every message in a run of `seconds`, on the rhythm."""
    times = []
    at = FIRST
    step = 0
    while at < seconds:
        times.append(at)
        at += RHYTHM[step % len(RHYTHM)]
        step += 1
    return times


def write_recording(path, seconds):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in frame_times(seconds):
            payload = message_at(written, at)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def send(host, port):
    """The same messages, to a port, on the same rhythm, until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    started = time.monotonic()
    at = FIRST
    count = 0
    while True:
        pause = started + at - time.monotonic()
        if pause > 0:
            time.sleep(pause)
        door.sendto(message_at(count, at), (host, port))
        at += RHYTHM[count % len(RHYTHM)]
        count += 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write a recording at PATH instead of sending to the port",
    )
    parser.add_argument(
        "--seconds", type=float, default=8.0, help="how long a recording runs"
    )
    parser.add_argument("--host", default="127.0.0.1", help="where to send")
    parser.add_argument("--port", type=int, default=27021, help="the port to send to")
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds)
        print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
        return 0

    print(f"sending the pulse to {arguments.host}:{arguments.port}")
    try:
        send(arguments.host, arguments.port)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
