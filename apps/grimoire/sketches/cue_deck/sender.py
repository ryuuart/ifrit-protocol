#!/usr/bin/env python3
"""The cues cue_deck draws: sent over UDP in one of three wires, or recorded.

    python3 sender.py
        sends one cue every 1/rate seconds as JSON text to 127.0.0.1:27025
        until interrupted, which is what makes the deck move in a window.

    python3 sender.py --wire osc
        sends the same cues as OSC messages to port 27026, one value each.

    python3 sender.py --wire flatbuffers
        sends the same cues as FlatBuffers to port 27027, built with the
        modules flatc generated from cue_deck.fbs.

    python3 sender.py --record
        writes the JSON cues into data/cue.feed, which is what a capture
        replays, so a still of the sketch is the same picture every run.
        With --wire, the recording holds that wire instead and is written
        beside it: data/cue-osc.feed or data/cue-flatbuffers.feed.

ONE CUE, THREE WIRES. cue_at() makes the cue as plain values in the shape
the sketch reads: the show's name, a name and a level per channel, a
colour, a name and a beat per marker, and the beat the show stands on.
The JSON wire is that shape as text. The OSC wire is the same values one
message each, under the address that names them. The FlatBuffer wire is
that shape built into a Cue as cue_deck.fbs states it.

The cue is a function of the message's own time and of nothing else, so
a recording and a live send carry the same messages at the same seconds
and neither holds any randomness.

The JSON and OSC wires need the standard library alone. The FlatBuffer
wire also needs the `flatbuffers` package, which is the runtime every
generated module imports; the CueDeck package is found because it stands
beside this file.
"""

import argparse
import colorsys
import json
import math
import os
import socket
import struct
import sys
import time

# The line every feed recording opens with, and the frame one arrival is
# written as: the time as a double, the length as a 32-bit unsigned, then
# that many bytes, both numbers little-endian.
HEADER = b"sigil-feed-recording 1\n"
FRAME = struct.Struct("<dI")

# The port each wire's door listens on, and the recording each one writes.
PORTS = {"json": 27025, "osc": 27026, "flatbuffers": 27027}
RECORDINGS = {
    "json": "cue.feed",
    "osc": "cue-osc.feed",
    "flatbuffers": "cue-flatbuffers.feed",
}

# EDIT THESE FIRST: what the show is called, what its desk carries and
# where its bar is marked.
SHOW = "NIGHT GARDEN · ACT II"
CHANNELS = ("KEY", "FILL", "RIM", "HAZE", "BASS", "HATS")
MARKERS = (("INTRO", 0.0), ("VERSE", 4.0), ("DROP", 8.0), ("BREAK", 12.0))
TEMPO = 120.0  # beats a minute
BAR = 16.0  # the beats one pass of the ruler spans
HUE_TURN = 0.04  # how far round the wheel the cue's colour turns per second


def cue_at(seconds):
    """The cue at `seconds`, in the shape the sketch reads."""
    channels = []
    for index, name in enumerate(CHANNELS):
        phase = seconds * (0.6 + 0.17 * index) + index * 1.3
        level = 0.5 + 0.42 * math.sin(phase) * math.cos(phase * 0.37)
        channels.append({"name": name, "level": round(min(1.0, max(0.0, level)), 3)})
    red, green, blue = colorsys.hsv_to_rgb((0.08 + seconds * HUE_TURN) % 1.0, 0.62, 1.0)
    return {
        "show": SHOW,
        "channels": channels,
        "color": {"r": round(red, 3), "g": round(green, 3), "b": round(blue, 3)},
        "markers": [{"name": name, "beat": beat} for name, beat in MARKERS],
        "beat": round(seconds * TEMPO / 60.0 % BAR, 3),
    }


# ── the JSON wire ──────────────────────────────────────────────────────


def json_datagrams(cue):
    """The JSON wire: the whole cue as JSON text, one datagram."""
    return [json.dumps(cue).encode()]


# ── the OSC wire ───────────────────────────────────────────────────────


def osc_string(text):
    """One OSC string: the text as UTF-8, a null, and nulls up to a
    multiple of four."""
    raw = text.encode() + b"\0"
    return raw + b"\0" * (-len(raw) % 4)


def osc_message(address, arguments):
    """The bytes of one OSC message: the address, a type tag per argument
    (`s` for a string, `f` for a 32-bit float), then the arguments, every
    number big-endian."""
    tags = "," + "".join("s" if isinstance(value, str) else "f" for value in arguments)
    body = b"".join(
        osc_string(value) if isinstance(value, str) else struct.pack(">f", value)
        for value in arguments
    )
    return osc_string(address) + osc_string(tags) + body


def osc_datagrams(cue):
    """The OSC wire: one message a value, each its own datagram. A level
    and a colour channel are one float each, so a CHOP's channels can be
    sent as they stand; a marker is its name and its beat."""
    messages = [osc_message("/cue/show", [cue["show"]])]
    for channel in cue["channels"]:
        messages.append(
            osc_message(f"/cue/level/{channel['name']}", [channel["level"]])
        )
    for name, value in cue["color"].items():
        messages.append(osc_message(f"/cue/color/{name}", [value]))
    for marker in cue["markers"]:
        messages.append(osc_message("/cue/marker", [marker["name"], marker["beat"]]))
    messages.append(osc_message("/cue/beat", [cue["beat"]]))
    return messages


# ── the FlatBuffer wire ────────────────────────────────────────────────


def encode(cue):
    """The bytes of one Cue: `cue` built into a FlatBuffer by the
    generated modules.

    A builder writes back to front, so every string and every table a
    vector holds is made before the vector, and every vector before the
    table that holds it.
    """
    import flatbuffers
    from CueDeck import Channel, Cue, Marker
    from CueDeck.Color import CreateColor

    builder = flatbuffers.Builder(512)
    show = builder.CreateString(cue["show"])

    channels = []
    for channel in cue["channels"]:
        label = builder.CreateString(channel["name"])
        Channel.Start(builder)
        Channel.AddName(builder, label)
        Channel.AddLevel(builder, channel["level"])
        channels.append(Channel.End(builder))
    Cue.StartChannelsVector(builder, len(channels))
    for channel in reversed(channels):
        builder.PrependUOffsetTRelative(channel)
    channel_vector = builder.EndVector()

    markers = []
    for marker in cue["markers"]:
        label = builder.CreateString(marker["name"])
        Marker.Start(builder)
        Marker.AddName(builder, label)
        Marker.AddBeat(builder, marker["beat"])
        markers.append(Marker.End(builder))
    Cue.StartMarkersVector(builder, len(markers))
    for marker in reversed(markers):
        builder.PrependUOffsetTRelative(marker)
    marker_vector = builder.EndVector()

    color = cue["color"]
    Cue.Start(builder)
    Cue.AddShow(builder, show)
    Cue.AddChannels(builder, channel_vector)
    # A struct is written in place, inside the table that holds it, so it
    # is made between Start and End rather than before them.
    Cue.AddColor(builder, CreateColor(builder, color["r"], color["g"], color["b"]))
    Cue.AddMarkers(builder, marker_vector)
    Cue.AddBeat(builder, cue["beat"])
    builder.Finish(Cue.End(builder))
    return bytes(builder.Output())


def flatbuffer_datagrams(cue):
    """The FlatBuffer wire: the whole cue as one Cue, one datagram."""
    return [encode(cue)]


WIRES = {
    "json": json_datagrams,
    "osc": osc_datagrams,
    "flatbuffers": flatbuffer_datagrams,
}


# ── sending and recording ──────────────────────────────────────────────


def frame_times(seconds, rate):
    """The time of every cue in a run of `seconds` at `rate` a second.

    A cue stands at (i + 0.5) / rate rather than at i / rate so that no
    arrival falls on a frame boundary a reader could take a frame early
    or a frame late.
    """
    return [(index + 0.5) / rate for index in range(int(seconds * rate))]


def write_recording(path, wire, seconds, rate):
    """Every cue over a run of `seconds` on `wire`, as a recording file:
    each datagram one arrival, every datagram of a cue at that cue's time."""
    datagrams = WIRES[wire]
    times = frame_times(seconds, rate)
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at in times:
            for payload in datagrams(cue_at(at)):
                recording.write(FRAME.pack(at, len(payload)))
                recording.write(payload)
    return len(times)


def send(wire, host, port, rate):
    """One cue a tick on `wire` to a port, until interrupted."""
    datagrams = WIRES[wire]
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    print(f"sending {wire} cues to {host}:{port} at {rate:g}/s")
    started = time.monotonic()
    index = 0
    while True:
        at = (index + 0.5) / rate
        pause = started + at - time.monotonic()
        if pause > 0:
            time.sleep(pause)
        for payload in datagrams(cue_at(at)):
            door.sendto(payload, (host, port))
        index += 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--wire",
        choices=tuple(WIRES),
        default="json",
        help="what the cues are spelled as",
    )
    parser.add_argument(
        "--record",
        action="store_true",
        help="write the wire's recording under data/ instead of sending",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument("--rate", type=float, default=10.0, help="cues a second")
    parser.add_argument("--host", default="127.0.0.1", help="where the cues go")
    parser.add_argument(
        "--port", type=int, help="the UDP port; the wire's own by default"
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        here = os.path.dirname(os.path.abspath(__file__))
        path = os.path.join(here, "data", RECORDINGS[arguments.wire])
        written = write_recording(
            path, arguments.wire, arguments.seconds, arguments.rate
        )
        print(f"{path}: {written} cues over {arguments.seconds:g}s")
        return 0
    port = arguments.port or PORTS[arguments.wire]
    try:
        send(arguments.wire, arguments.host, port, arguments.rate)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
