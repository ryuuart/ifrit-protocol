#!/usr/bin/env python3
"""The lighting desk artnet_lights listens to: Art-Net to a port, or a recording.

Two modes, one desk:

    python3 console.py
        sends a universe to 127.0.0.1:6454 until interrupted and prints
        the universe the sketch sends back to 6455, which is the whole
        round trip a desk makes with a fixture that answers.

    python3 console.py --record data/desk.feed --seconds 6
        writes that same traffic to a file in the feed recording format,
        which is what the sketch's plate replays.

WHAT THE DESK SENDS. One universe, eight dimmers of it:

    1 2 3   the colour wash, red green blue
    4       the wind, still air at the middle of the fader's travel
    5       the strobe rate, nothing at all being a lamp simply on
    6 7 8   a chase on somebody else's fixture, which the sketch shows
            and is not patched to

WHAT COMES BACK, on a universe of its own: nine dimmers, three lamps of
red, green and blue, carrying the three brightest colours standing on
the canvas. It arrives on the port this script binds, so a desk lit from
the scene needs no second address told to anybody.

A PACKET IS SPELLED HERE BY HAND, because the whole point of the sketch
is that nothing between the desk and the drawing had to be installed. A
packet is the eight bytes "Art-Net" and a null, then the two-byte code
that says what it is written LOW byte first — the one number on this
wire written that way — then the protocol version, high byte first as
every other number is. A universe of dimmers carries a sequence, a
physical input, the two halves of its port address with the
sub-universe first, the count of dimmers, and then the levels: one byte
each, 0 to 255. The count is even, because the wire counts its dimmers
in pairs.

The traffic is a function of its own time and of nothing else, so the
file and the live send carry the same levels at the same seconds and
neither holds any randomness. A packet stands half a frame off the grid
a reader steps in, so no arrival is taken a frame early or a frame late.
The strobe is shut through the first seconds of a cycle, so what a plate
takes is the wash and the wind with the lamp simply on.

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

# The name every packet opens with, the code for a universe of dimmers,
# and the version this speaks.
NAME = b"Art-Net\0"
OPCODE_DMX = 0x5000
PROTOCOL = 14

# EDIT THESE FIRST: what the desk does, and when.
RATE = 12.0  # packets a second
UNIVERSE = 0  # the universe the sketch's fixture is lit on
WASH_PERIOD = 5.3  # how long the colour takes to come round
WIND_PERIOD = 7.0  # how long one sweep of the wind fader takes
STROBE_AT = 3.0  # when the strobe fader leaves the bottom
STROBE_PERIOD = 4.0  # how long it then takes to run to the top
CHASE_RATE = 2.5  # lamps a second on somebody else's fixture

# What comes back: three lamps of red, green and blue.
LAMPS = 3
LAMP_CHANNELS = 3


def art_dmx(universe, sequence, levels):
    """The bytes of one universe of dimmers."""
    held = [max(0, min(255, round(level))) for level in levels]
    # The wire counts its dimmers in pairs, so an odd list goes out with
    # one more at nothing behind it.
    if len(held) % 2:
        held.append(0)
    return (
        NAME
        + struct.pack("<H", OPCODE_DMX)
        + struct.pack(">H", PROTOCOL)
        + bytes((sequence & 0xFF, 0, universe & 0xFF, (universe >> 8) & 0x7F))
        + struct.pack(">H", len(held))
        + bytes(held)
    )


def read_dmx(packet):
    """The universe and the levels of one packet, or None.

    Enough of a reader for what comes back through this door, which is
    one universe of dimmers; anything else is not this conversation.
    """
    if len(packet) < 18 or packet[:8] != NAME:
        return None
    if struct.unpack_from("<H", packet, 8)[0] != OPCODE_DMX:
        return None
    count = struct.unpack_from(">H", packet, 16)[0]
    if count > len(packet) - 18:
        return None
    universe = ((packet[15] & 0x7F) << 8) | packet[14]
    return universe, list(packet[18 : 18 + count])


def wash_at(seconds):
    """The colour wash at `seconds`: one turn round the hues."""
    hue = (seconds / WASH_PERIOD) % 1.0
    red, green, blue = colorsys.hsv_to_rgb(hue, 0.55, 1.0)
    return [round(red * 255), round(green * 255), round(blue * 255)]


def wind_at(seconds):
    """The wind fader at `seconds`: one slow sweep either side of still
    air, which is the middle of a dimmer's travel and not the bottom."""
    return round(127.5 + 127.5 * math.sin(2.0 * math.pi * seconds / WIND_PERIOD))


def strobe_at(seconds):
    """The strobe fader at `seconds`: shut, and then run up.

    A plate is taken while it is still shut, so what the plate holds is
    the wash and the wind with the lamp simply on.
    """
    if seconds < STROBE_AT:
        return 0
    return round(255 * min(1.0, (seconds - STROBE_AT) / STROBE_PERIOD))


def chase_at(seconds):
    """Three dimmers on somebody else's fixture: one lit at a time,
    running round. The sketch is not patched to them and shows them
    anyway, which is what a universe looks like from inside one lamp."""
    lit = int(seconds * CHASE_RATE) % 3
    return [255 if lamp == lit else 0 for lamp in range(3)]


def universe_at(seconds):
    """Every dimmer the desk writes at `seconds`."""
    return wash_at(seconds) + [wind_at(seconds), strobe_at(seconds)] + chase_at(seconds)


def traffic(seconds):
    """Every packet of a run of `seconds`, as (at, sequence, levels)."""
    packets = []
    for index in range(int(seconds * RATE)):
        at = (index + 0.5) / RATE
        # The sequence counts the packets of one universe as they are
        # sent, so a receiver tells a datagram that arrived late from
        # one that arrived twice. It comes round to one: 0 is what a
        # sender that does not count writes.
        packets.append((at, index % 255 + 1, universe_at(at)))
    return packets


def write_recording(path, seconds):
    """Every packet of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, sequence, levels in traffic(seconds):
            payload = art_dmx(UNIVERSE, sequence, levels)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def drain(lamps, until):
    """Prints the universe the sketch has sent back, until `until`."""
    while True:
        remaining = until - time.monotonic()
        if remaining <= 0:
            return
        ready, _, _ = select.select([lamps], [], [], remaining)
        if not ready:
            return
        packet, _ = lamps.recvfrom(65536)
        read = read_dmx(packet)
        if not read:
            continue
        universe, levels = read
        if len(levels) < LAMPS * LAMP_CHANNELS:
            continue
        lit = "  ".join(
            "({:3d},{:3d},{:3d})".format(*levels[at : at + LAMP_CHANNELS])
            for at in range(0, LAMPS * LAMP_CHANNELS, LAMP_CHANNELS)
        )
        print(f"  <- universe {universe}  {lit}")


def send(host, port, back_port, seconds):
    """The same traffic, to a port, over and over until interrupted."""
    desk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # The way back is a port of its own: what the sketch sends is
    # addressed to a listener rather than answered to whoever wrote, a
    # lighting wire carrying one direction per universe.
    lamps = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    lamps.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    lamps.bind(("", back_port))
    schedule = traffic(seconds)
    started = time.monotonic()
    cycle = 0
    spoken = -1
    while True:
        for at, sequence, levels in schedule:
            drain(lamps, started + cycle * seconds + at)
            desk.sendto(art_dmx(UNIVERSE, sequence, levels), (host, port))
            # One line a second: a desk writes its universe many times a
            # second, and a page of identical numbers says nothing a
            # line of them does not.
            if int(at) != spoken:
                spoken = int(at)
                print(
                    f"-> universe {UNIVERSE}  "
                    f"wash {levels[0]:3d} {levels[1]:3d} {levels[2]:3d}  "
                    f"wind {levels[3]:3d}  strobe {levels[4]:3d}"
                )
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
    parser.add_argument("--port", type=int, default=6454, help="the port to send to")
    parser.add_argument(
        "--back-port",
        type=int,
        default=6455,
        help="the port the sketch's own universe arrives on",
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(arguments.record, arguments.seconds)
        print(f"{arguments.record}: {written} packets over {arguments.seconds:g}s")
        return 0

    print(
        f"working the desk at {arguments.host}:{arguments.port}, "
        f"printing the universe it answers with on {arguments.back_port}"
    )
    try:
        send(arguments.host, arguments.port, arguments.back_port, arguments.seconds)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
