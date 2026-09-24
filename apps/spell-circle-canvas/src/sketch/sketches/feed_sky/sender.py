#!/usr/bin/env python3
"""The skies feed_sky draws: sent through a door, or written as recordings.

One sky, three doors, each door's sky tinted from a palette of its own so
the three panels read apart:

    python3 sender.py
        sends one JSON datagram every 1/rate seconds to 127.0.0.1:27020
        until interrupted, which is what makes the UDP panel move in a
        window.

    python3 sender.py --door shared
        makes the shared memory region named `feed_sky` and writes one
        JSON message into it every 1/rate seconds until interrupted,
        which is what makes the SHARED MEMORY panel move. EITHER END MAY
        START FIRST: a reader holds the region's name rather than the
        memory behind it, so a region made after the window opened is one
        it reads, and a sender stopped and started again is read as the
        region it made afresh.

    python3 sender.py --record
        writes every door's messages to its recording under data/ —
        udp.feed, quic.feed and shared.feed — which is what a capture
        replays. `--door` narrows it to one.

THE QUIC DOOR HAS NO LIVE SENDER HERE. Speaking quic:// needs a QUIC
stack and Python ships none in its standard library, so this script
writes that door's recording only. The live other side is Seer, which
registers every transport this tree carries and opens
`quic://127.0.0.1:27100?insecure=1` as a wire like any other;
`insecure=1` is what reaches the self-signed pair beside the sketch.

A message is the sky exactly as feed_sky.fbs states it, in that schema's
own JSON form:

    {"bands": [{"height": h, "speed": s, "wobble": w}, ...],
     "wind": {"x": x, "y": y},
     "palette": [{"r": r, "g": g, "b": b, "a": a}, ...]}

The UDP panel converts every arrival through that schema and counts what
does not fit; the other two read the same JSON with no schema over it.

The sky is a function of the message's own time and of nothing else, so
a recording and a live send carry the same messages at the same seconds
and neither holds any randomness.

THE REGION'S LAYOUT, which is the whole of what the two ends of the
shared memory door share. A region opens with sixty-four bytes, every
number little-endian:

    offset  0  16 bytes  b"sigil-shared-1" and zeros after it
    offset 16   4 bytes  the payload bytes the region was made to hold
    offset 20   4 bytes  spare
    offset 24   8 bytes  the count of the messages written into it
    offset 32   8 bytes  the size of the message standing now
    offset 40   8 bytes  when it was written, in nanoseconds
    offset 48  16 bytes  spare, so a payload begins at sixty-four

and the payload follows. THE COUNT IS THE PROTOCOL: it is raised to an
odd number before the payload is touched and to the next even one once
the message stands whole, so a reader that copies the payload between two
reads of the same even count has a whole message, and one that does not
looks again. Nothing locks and nothing waits.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import colorsys
import json
import math
import os
import signal
import socket
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

# EDIT THESE FIRST: the shape of the sky.
BAND_COUNT = 5
PALETTE_COUNT = 3
BAND_HEIGHT = (90.0, 190.0)  # the range a band's thickness breathes over
BAND_SPEED = 26.0  # how fast a band drifts on its own, px per second
BAND_WOBBLE = (10.0, 32.0)  # how far a band breathes up and down
WIND_SPEED = 30.0  # the drift every band shares, px per second
HUE_TURN = 0.035  # how far round the wheel the palette turns per second

# EACH DOOR'S PALETTE: the hue its first colour starts at, how far round
# the wheel each next colour stands, how saturated they all are, and how
# opaque the first one is — each next is five hundredths clearer.
DOORS = {
    "udp": {"hue": 0.52, "step": 0.11, "saturation": 0.55, "alpha": 0.58},
    "quic": {"hue": 0.58, "step": 0.09, "saturation": 0.5, "alpha": 0.6},
    "shared": {"hue": 0.97, "step": 0.06, "saturation": 0.5, "alpha": 0.58},
}

# The region: what it is called, how much payload it is made to hold, and
# where each field of its header stands. A message of this sky is a few
# hundred bytes, and the room above that costs nothing until it is used.
REGION_NAME = "feed_sky"
REGION_MAGIC = b"sigil-shared-1"
REGION_HEADER = 64
REGION_CAPACITY = 1 << 16
CAPACITY_AT = 16
SEQUENCE_AT = 24
SIZE_AT = 32
WRITTEN_AT = 40
QUAD = struct.Struct("<Q")
QUAD_32 = struct.Struct("<I")

# The two counts have to reach a reader AROUND the payload and never
# after it, and a plain store in this language is ordered against another
# by nothing at all. Taking and releasing a lock is the one thing here
# that is, so one stands between the count and the payload it brackets.
ORDER = threading.Lock()


def sky_at(seconds, door):
    """The sky at `seconds`, in `door`'s palette, as the message the sketch reads."""
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
    tint = DOORS[door]
    palette = []
    for index in range(PALETTE_COUNT):
        hue = (tint["hue"] + index * tint["step"] + seconds * HUE_TURN) % 1.0
        red, green, blue = colorsys.hsv_to_rgb(hue, tint["saturation"], 1.0)
        palette.append(
            {
                "r": round(red, 2),
                "g": round(green, 2),
                "b": round(blue, 2),
                "a": round(tint["alpha"] - index * 0.05, 2),
            }
        )
    return {"bands": bands, "wind": wind, "palette": palette}


def message_at(seconds, door):
    """The bytes of one message: the sky, as compact JSON."""
    return json.dumps(sky_at(seconds, door), separators=(",", ":")).encode("utf-8")


def frame_times(seconds, rate):
    """The time of every message in a run of `seconds` at `rate` a second.

    A message stands at (i + 0.5) / rate rather than at i / rate so that
    no arrival falls on a whole second, where a reader stepping in whole
    frames could take it a frame early or a frame late.
    """
    count = int(seconds * rate)
    return [(index + 0.5) / rate for index in range(count)]


def write_recording(path, door, seconds, rate):
    """Every message of `door` over a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in frame_times(seconds, rate):
            payload = message_at(at, door)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def paced(rate):
    """The time of each next message, reached by sleeping until it is due."""
    started = time.monotonic()
    index = 0
    while True:
        at = (index + 0.5) / rate
        pause = started + at - time.monotonic()
        if pause > 0:
            time.sleep(pause)
        yield at
        index += 1


def send(host, port, rate):
    """The UDP door's messages, to a port, until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"sending the sky to {host}:{port} at {rate:g}/s")
    for at in paced(rate):
        door.sendto(message_at(at, "udp"), (host, port))


def barrier():
    """Everything written before this reaches a reader before what follows."""
    with ORDER:
        pass


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
    than interrupted would leave a region standing with nobody writing to
    it, which the next reader would map and read one stale message out of.
    """
    raise SystemExit(0)


def hand_over(name, capacity, rate):
    """The shared door's messages, into the region, until interrupted."""
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, stop)
    region = make_region(name, capacity)
    print(f"the region {name} stands · writing the sky at {rate:g}/s")
    try:
        for at in paced(rate):
            put(region, message_at(at, "shared"), capacity)
    finally:
        region.close()
        region.unlink()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--door",
        choices=sorted(DOORS),
        help="which door to send through, or to record (every door when recording)",
    )
    parser.add_argument(
        "--record",
        action="store_true",
        help="write the recordings under data/ instead of sending",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=4.0, help="messages a second")
    parser.add_argument("--host", default="127.0.0.1", help="where UDP sends to")
    parser.add_argument("--port", type=int, default=27020, help="the UDP port")
    parser.add_argument("--name", default=REGION_NAME, help="the region to write")
    parser.add_argument(
        "--capacity",
        type=int,
        default=REGION_CAPACITY,
        help="the payload bytes the region is made to hold",
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        here = os.path.dirname(os.path.abspath(__file__))
        for door in [arguments.door] if arguments.door else sorted(DOORS):
            path = os.path.join(here, "data", f"{door}.feed")
            written = write_recording(path, door, arguments.seconds, arguments.rate)
            print(f"{path}: {written} messages over {arguments.seconds:g}s")
        return 0

    try:
        if arguments.door == "quic":
            print(
                "the QUIC door has no live sender here: open "
                "quic://127.0.0.1:27100?insecure=1 in Seer",
                file=sys.stderr,
            )
            return 2
        if arguments.door == "shared":
            hand_over(arguments.name, arguments.capacity, arguments.rate)
        else:
            send(arguments.host, arguments.port, arguments.rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
