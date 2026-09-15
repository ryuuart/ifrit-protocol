#!/usr/bin/env python3
"""The desk osc_desk listens to: OSC packets to a port, or a recording.

Two modes, one desk:

    python3 desk.py
        sends the traffic to 127.0.0.1:27070 until interrupted, and
        listens on 27071 for the /sky/state readings the sketch sends
        back, printing each one — which is the whole round trip a fader
        on a real desk makes.

    python3 desk.py --record data/desk.feed --seconds 6
        writes that same traffic to a file in the feed recording format,
        which is what the sketch's plate replays.

WHAT THE DESK SAYS. Three addresses, and the sketch answers a fourth:

    /sky/wind     one float: the fader, px a second of drift
    /sky/gust     two floats: how hard, and over how long
    /sky/palette  nine floats: three colours, red green blue each
    /sky/state    what comes back: the eased wind, one float

A PACKET IS SPELLED HERE BY HAND, because the whole point of the sketch
is that nothing between the desk and the drawing had to be installed. An
OSC message is its address as a null-terminated string padded with nulls
to a multiple of four, then the type tag string — a comma and one
character per argument — padded the same way, then the arguments
themselves. A float is four bytes, big-endian, which is the byte order
the protocol writes every number in.

The traffic is a function of its own time and of nothing else, so the
file and the live send carry the same messages at the same seconds and
neither holds any randomness. A message stands off both the fader grid
and the whole frames a reader steps in, so no arrival is taken a frame
early or a frame late.

Standard library only: this runs wherever python3 does, with nothing
installed.
"""

import argparse
import colorsys
import math
import select
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

# EDIT THESE FIRST: what the desk does, and when.
WIND_RATE = 8.0  # fader readings a second
WIND_SPAN = 34.0  # how far the fader travels either way, px a second
WIND_PERIOD = 7.0  # how long one sweep of the fader takes
# When a burst is thrown, how hard, and over how long it falls away.
GUSTS = ((1.27, 0.85, 1.6), (4.43, 0.62, 1.1))
# When the desk turns the three colours over.
PALETTES = (1.79, 4.61)


def osc_string(text):
    """One OSC string: the text, a null, and nulls up to a multiple of four."""
    raw = text.encode("ascii") + b"\0"
    return raw + b"\0" * (-len(raw) % 4)


def osc_message(address, arguments):
    """The bytes of one message: the address, the type tags, the floats."""
    tags = "," + "f" * len(arguments)
    body = b"".join(struct.pack(">f", float(value)) for value in arguments)
    return osc_string(address) + osc_string(tags) + body


def read_string(raw):
    """One OSC string and what follows it, or (None, b"")."""
    end = raw.find(b"\0")
    if end < 0:
        return None, b""
    padded = end + 4 - (end % 4)
    if padded > len(raw):
        return None, b""
    return raw[:end].decode("ascii", "replace"), raw[padded:]


def read_message(packet):
    """The address and the float arguments of one message, or None.

    Enough of a reader for what comes back down this door, which is
    floats under one address; anything else is not this conversation.
    """
    address, rest = read_string(packet)
    if address is None or not address.startswith("/"):
        return None
    tags, rest = read_string(rest)
    if tags is None or not tags.startswith(","):
        return None
    arguments = []
    for tag in tags[1:]:
        if tag != "f" or len(rest) < 4:
            return None
        arguments.append(struct.unpack(">f", rest[:4])[0])
        rest = rest[4:]
    return address, arguments


def wind_at(seconds):
    """The fader at `seconds`: one slow sweep either side of still air."""
    return round(WIND_SPAN * math.sin(2.0 * math.pi * seconds / WIND_PERIOD), 2)


def palette_at(index):
    """The `index`th turn of the three colours, as nine floats."""
    floats = []
    for step in range(3):
        hue = (0.08 + index * 0.27 + step * 0.13) % 1.0
        red, green, blue = colorsys.hsv_to_rgb(hue, 0.42, 0.92)
        floats.extend((round(red, 3), round(green, 3), round(blue, 3)))
    return floats


def traffic(seconds):
    """Every message of a run of `seconds`, as (at, address, arguments).

    The fader stands at (i + 0.5) / rate, which is half a frame off the
    grid a reader steps in; the bursts and the colour changes stand
    between fader readings, so no two messages share one moment.
    """
    messages = []
    for index in range(int(seconds * WIND_RATE)):
        at = (index + 0.5) / WIND_RATE
        messages.append((at, "/sky/wind", [wind_at(at)]))
    for at, strength, falls in GUSTS:
        if at < seconds:
            messages.append((at, "/sky/gust", [strength, falls]))
    for index, at in enumerate(PALETTES):
        if at < seconds:
            messages.append((at, "/sky/palette", palette_at(index)))
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


def drain(back, until):
    """Prints whatever reaches the return door until `until`, then waits."""
    while True:
        remaining = until - time.monotonic()
        if remaining <= 0:
            return
        ready, _, _ = select.select([back], [], [], remaining)
        if not ready:
            return
        packet, _ = back.recvfrom(65536)
        message = read_message(packet)
        if message:
            address, arguments = message
            readings = " ".join(f"{value:.2f}" for value in arguments)
            print(f"  <- {address} {readings}")


def send(host, port, listen_port, seconds):
    """The same traffic, to a port, over and over until interrupted."""
    out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    back = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    back.bind(("", listen_port))
    schedule = traffic(seconds)
    started = time.monotonic()
    cycle = 0
    while True:
        for at, address, arguments in schedule:
            drain(back, started + cycle * seconds + at)
            out.sendto(osc_message(address, arguments), (host, port))
            if address != "/sky/wind":
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
        default=6.0,
        help="how long a recording runs, and how long one live cycle takes",
    )
    parser.add_argument("--host", default="127.0.0.1", help="where to send")
    parser.add_argument("--port", type=int, default=27070, help="the port to send to")
    parser.add_argument(
        "--listen-port",
        type=int,
        default=27071,
        help="the port the sketch sends /sky/state back to",
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds)
        print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
        return 0

    print(
        f"working the desk at {arguments.host}:{arguments.port}, "
        f"listening for /sky/state on {arguments.listen_port}"
    )
    try:
        send(arguments.host, arguments.port, arguments.listen_port, arguments.seconds)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
