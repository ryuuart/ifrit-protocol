#!/usr/bin/env python3
"""The cues cue_deck draws: sent over UDP, or written as a recording.

    python3 sender.py
        sends one Cue datagram every 1/rate seconds to 127.0.0.1:27025
        until interrupted, which is what makes the deck move in a window.

    python3 sender.py --record
        writes the same cues into data/cue.feed, which is what a capture
        replays, so a still of the sketch is the same picture every run.

A message is one Cue exactly as cue_deck.fbs states it, as a FlatBuffer:
the bytes are built with the modules flatc generated from that schema,
the CueDeck package beside this file. A TouchDesigner patch sends the
same bytes, built the same way, from touchdesigner/scripts/cue_deck.py.

The cue is a function of the message's own time and of nothing else, so
a recording and a live send carry the same messages at the same seconds
and neither holds any randomness.

Needs the standard library and the `flatbuffers` package, which is the
runtime every generated module imports; the CueDeck package is found
because it stands beside this file.
"""

import argparse
import colorsys
import math
import os
import socket
import struct
import sys
import time

import flatbuffers
from CueDeck import Channel, Cue, Marker
from CueDeck.Color import CreateColor

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes, both numbers little-endian.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# EDIT THESE FIRST: what the show is called, what its desk carries and
# where its bar is marked.
SHOW = "NIGHT GARDEN · ACT II"
CHANNELS = ("KEY", "FILL", "RIM", "HAZE", "BASS", "HATS")
MARKERS = (("INTRO", 0.0), ("VERSE", 4.0), ("DROP", 8.0), ("BREAK", 12.0))
TEMPO = 120.0  # beats a minute
BAR = 16.0  # the beats one pass of the ruler spans
HUE_TURN = 0.04  # how far round the wheel the cue's colour turns per second


def cue_at(seconds):
    """The cue at `seconds`, as plain values a builder writes."""
    channels = []
    for index, name in enumerate(CHANNELS):
        phase = seconds * (0.6 + 0.17 * index) + index * 1.3
        level = 0.5 + 0.42 * math.sin(phase) * math.cos(phase * 0.37)
        channels.append((name, round(min(1.0, max(0.0, level)), 3)))
    red, green, blue = colorsys.hsv_to_rgb((0.08 + seconds * HUE_TURN) % 1.0, 0.62, 1.0)
    return {
        "show": SHOW,
        "channels": channels,
        "color": (round(red, 3), round(green, 3), round(blue, 3)),
        "markers": MARKERS,
        "beat": round(seconds * TEMPO / 60.0 % BAR, 3),
    }


def encode(cue):
    """The bytes of one Cue: `cue` built into a FlatBuffer by the
    generated modules.

    A builder writes back to front, so every string and every table a
    vector holds is made before the vector, and every vector before the
    table that holds it.
    """
    builder = flatbuffers.Builder(512)
    show = builder.CreateString(cue["show"])

    channels = []
    for name, level in cue["channels"]:
        label = builder.CreateString(name)
        Channel.Start(builder)
        Channel.AddName(builder, label)
        Channel.AddLevel(builder, level)
        channels.append(Channel.End(builder))
    Cue.StartChannelsVector(builder, len(channels))
    for channel in reversed(channels):
        builder.PrependUOffsetTRelative(channel)
    channel_vector = builder.EndVector()

    markers = []
    for name, beat in cue["markers"]:
        label = builder.CreateString(name)
        Marker.Start(builder)
        Marker.AddName(builder, label)
        Marker.AddBeat(builder, beat)
        markers.append(Marker.End(builder))
    Cue.StartMarkersVector(builder, len(markers))
    for marker in reversed(markers):
        builder.PrependUOffsetTRelative(marker)
    marker_vector = builder.EndVector()

    Cue.Start(builder)
    Cue.AddShow(builder, show)
    Cue.AddChannels(builder, channel_vector)
    # A struct is written in place, inside the table that holds it, so it
    # is made between Start and End rather than before them.
    Cue.AddColor(builder, CreateColor(builder, *cue["color"]))
    Cue.AddMarkers(builder, marker_vector)
    Cue.AddBeat(builder, cue["beat"])
    builder.Finish(Cue.End(builder))
    return bytes(builder.Output())


def frame_times(seconds, rate):
    """The time of every message in a run of `seconds` at `rate` a second.

    A message stands at (i + 0.5) / rate rather than at i / rate so that
    no arrival falls on a frame boundary a reader could take a frame
    early or a frame late.
    """
    return [(index + 0.5) / rate for index in range(int(seconds * rate))]


def write_recording(path, seconds, rate):
    """Every cue over a run of `seconds`, as a recording file."""
    times = frame_times(seconds, rate)
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in times:
            payload = encode(cue_at(at))
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
    return len(times)


def send(host, port, rate):
    """One cue a tick to a port, until interrupted."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"sending cues to {host}:{port} at {rate:g}/s")
    started = time.monotonic()
    index = 0
    while True:
        at = (index + 0.5) / rate
        pause = started + at - time.monotonic()
        if pause > 0:
            time.sleep(pause)
        door.sendto(encode(cue_at(at)), (host, port))
        index += 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record", action="store_true", help="write data/cue.feed instead of sending"
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=10.0, help="cues a second")
    parser.add_argument("--host", default="127.0.0.1", help="where the cues go")
    parser.add_argument("--port", type=int, default=27025, help="the UDP port")
    arguments = parser.parse_args(argv)

    if arguments.record:
        here = os.path.dirname(os.path.abspath(__file__))
        path = os.path.join(here, "data", "cue.feed")
        written = write_recording(path, arguments.seconds, arguments.rate)
        print(f"{path}: {written} cues over {arguments.seconds:g}s")
        return 0
    try:
        send(arguments.host, arguments.port, arguments.rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
