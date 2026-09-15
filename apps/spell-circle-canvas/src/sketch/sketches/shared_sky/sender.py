#!/usr/bin/env python3
"""The sky shared_sky draws: handed over in memory, or written as a recording.

Two modes, one sky:

    python3 sender.py
        makes the shared memory region named `shared_sky` and writes one
        JSON message into it 30 times a second until interrupted, which
        is what makes the sketch move in a window. Start this BEFORE the
        window: a reader maps what is there when it opens, so a region
        made afterwards is one that reader never sees.

    python3 sender.py --record data/sky.feed --seconds 6 --rate 4
        writes those same messages to a file in the feed recording
        format, which is what the sketch's plate replays.

A message is the sky as compact JSON, with no schema over it:

    {"bands": [{"height": h, "speed": s, "wobble": w}, ...],
     "wind": {"x": x, "y": y},
     "palette": [{"r": r, "g": g, "b": b, "a": a}, ...]}

THE REGION'S LAYOUT, which is the whole of what the two ends share. A
region opens with sixty-four bytes, every number little-endian:

    offset  0  16 bytes  b"sigil-shared-1" and zeros after it
    offset 16   4 bytes  the payload bytes the region was made to hold
    offset 20   4 bytes  spare
    offset 24   8 bytes  the count of the messages written into it
    offset 32   8 bytes  the size of the message standing now
    offset 40   8 bytes  when it was written, in nanoseconds
    offset 48  16 bytes  spare, so a payload begins at sixty-four

and the payload follows. THE COUNT IS THE PROTOCOL: it is raised to an
odd number before the payload is touched and to the next even one once
the message stands whole, so a reader that copies the payload between
two reads of the same even count has a whole message, and one that does
not looks again. Nothing locks and nothing waits: a reader cannot hold
this program up, and this program cannot make a reader wait.

The sky is a function of the message's own time and of nothing else, so
the file and the live writing carry the same messages at the same
seconds and neither holds any randomness.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import colorsys
import json
import math
import signal
import struct
import sys
import threading
import time
from multiprocessing import shared_memory

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes. Both numbers are written little-endian, which is the
# byte order of every machine that reads one of these.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# The region: what it is called, how much payload it is made to hold, and
# where each field of its header stands. A message of this sky is a few
# hundred bytes, and the room above that costs nothing until it is used.
REGION_NAME = "shared_sky"
REGION_MAGIC = b"sigil-shared-1"
REGION_HEADER = 64
REGION_CAPACITY = 1 << 16
CAPACITY_AT = 16
SEQUENCE_AT = 24
SIZE_AT = 32
WRITTEN_AT = 40
QUAD = struct.Struct("<Q")
QUAD_32 = struct.Struct("<I")

# EDIT THESE FIRST: the shape of the sky.
BAND_COUNT = 5
PALETTE_COUNT = 3
BAND_HEIGHT = (90.0, 190.0)  # the range a band's thickness breathes over
BAND_SPEED = 26.0  # how fast a band drifts on its own, px per second
BAND_WOBBLE = (10.0, 32.0)  # how far a band breathes up and down
WIND_SPEED = 30.0  # the drift every band shares, px per second
HUE_TURN = 0.035  # how far round the wheel the palette turns per second

# The two counts have to reach a reader AROUND the payload and never
# after it, and a plain store in this language is ordered against another
# by nothing at all. Taking and releasing a lock is the one thing here
# that is, so one stands between the count and the payload it brackets.
ORDER = threading.Lock()


def barrier():
    """Everything written before this reaches a reader before what follows."""
    with ORDER:
        pass


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
                # can see.
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
    """The time of every message in a run of `seconds` at `rate` a second.

    A message stands at (i + 0.5) / rate rather than at i / rate so that
    no arrival falls on a whole second, where a reader stepping in whole
    frames could take it a frame early or a frame late.
    """
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


def make_region(name, capacity):
    """The region, made fresh under `name` and opened with its header.

    Whatever stood under the name is taken away first: a region is sized
    as it is made, and the header is written before the first message so
    that a reader mapping it finds the layout and no message rather than
    a message of no bytes.
    """
    total = REGION_HEADER + capacity
    try:
        region = shared_memory.SharedMemory(
            name=name, create=True, size=total, track=False
        )
    except FileExistsError:
        stale = shared_memory.SharedMemory(name=name, track=False)
        stale.close()
        stale.unlink()
        region = shared_memory.SharedMemory(
            name=name, create=True, size=total, track=False
        )
    region.buf[:REGION_HEADER] = bytes(REGION_HEADER)
    region.buf[: len(REGION_MAGIC)] = REGION_MAGIC
    QUAD_32.pack_into(region.buf, CAPACITY_AT, capacity)
    return region


def put(region, payload, capacity):
    """One message into the region, by the count that brackets it."""
    if len(payload) > capacity:
        return False
    standing = QUAD.unpack_from(region.buf, SEQUENCE_AT)[0]
    # Odd: a reader that finds an odd count is looking at a message
    # nobody could read whole, and looks again rather than copying half
    # of one.
    QUAD.pack_into(region.buf, SEQUENCE_AT, standing + 1)
    barrier()
    QUAD.pack_into(region.buf, SIZE_AT, len(payload))
    QUAD.pack_into(region.buf, WRITTEN_AT, time.time_ns())
    region.buf[REGION_HEADER : REGION_HEADER + len(payload)] = payload
    barrier()
    # Even again, and the message stands whole.
    QUAD.pack_into(region.buf, SEQUENCE_AT, standing + 2)
    return True


def stop(*_):
    """Ends the run the way an interrupt does, so the region is taken back.

    A name outlives the program that made it, so a writer killed rather
    than interrupted would leave a region standing with nobody writing
    to it, which the next reader would map and read one stale message
    out of.
    """
    raise SystemExit(0)


def hand_over(name, capacity, rate):
    """The same messages, into the region, until interrupted."""
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, stop)
    region = make_region(name, capacity)
    print(f"the region {name} stands · writing the sky at {rate:g}/s")
    try:
        started = time.monotonic()
        index = 0
        while True:
            at = (index + 0.5) / rate
            pause = started + at - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            put(region, message_at(at), capacity)
            index += 1
    finally:
        region.close()
        region.unlink()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write a recording at PATH instead of writing to the region",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=30.0, help="messages a second")
    parser.add_argument("--name", default=REGION_NAME, help="the region to write")
    parser.add_argument(
        "--capacity",
        type=int,
        default=REGION_CAPACITY,
        help="the payload bytes the region is made to hold",
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds, arguments.rate)
        print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
        return 0

    try:
        hand_over(arguments.name, arguments.capacity, arguments.rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
