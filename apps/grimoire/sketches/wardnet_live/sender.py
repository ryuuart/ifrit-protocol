#!/usr/bin/env python3
"""The show wardnet_live draws: sent over UDP or a WebSocket, or recorded.

    python3 sender.py
        plays the show as JSON datagrams to 127.0.0.1:27030, over and
        over until interrupted, which is what makes the console move in
        a window.

    python3 sender.py --door ws
        plays the same show over the WebSocket at
        ws://127.0.0.1:27031/wardnet, through a SigilIO hub of its own.

    python3 sender.py --record
        writes one pass of the show into data/wardnet.feed, which is what
        a capture replays, and data/silence.feed, a recording with
        nothing in it, which a capture replays at the other door.

ONE MESSAGE, ONE THING THAT CHANGED. Every message is a JSON object
whose "kind" names what it changes: a row added to the scrollback, the
alert, the load or one card. The show is a list of them at the seconds
they land, so a recording and a live send carry the same messages at the
same seconds and neither holds any randomness.

The UDP door and the recording need the standard library alone. The
WebSocket door needs the sigil package, whose SigilIO hub is the client.
"""

import argparse
import json
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

UDP_PORT = 27030
WEBSOCKET_URI = "ws://127.0.0.1:27031/wardnet"


# ── the four kinds of message ──────────────────────────────────────────


def row(tag, body):
    """A row added to the scrollback: TRACE, INFO, WARN or BREACH."""
    return {"kind": "row", "tag": tag, "body": body}


def alert(text, level):
    """The alert chip and the marquee: clear, warn or breach."""
    return {"kind": "alert", "text": text, "level": level}


def meter(load):
    """The load, from zero to one."""
    return {"kind": "meter", "load": load}


def card(title, value, note, level="clear"):
    """A card, found by its title: a title sent again is that card again."""
    return {
        "kind": "card",
        "title": title,
        "value": value,
        "note": note,
        "level": level,
    }


# EDIT THIS FIRST: the show, as (second, message) in the order they land.
SHOW = [
    (0.3, card("WARDS ANSWERING", "12 / 12", "heartbeat 1.0 s")),
    (0.5, row("TRACE", "handshake node/07 ← relay/02 · 14 ms")),
    (0.6, alert("PERIMETER QUIET", "clear")),
    (0.8, meter(0.34)),
    (1.0, row("INFO", "ward perimeter sweep complete · 0 breaches")),
    (1.4, card("RELAY JITTER", "18 ms", "all relays nominal")),
    (1.6, row("INFO", "cipher rotation scheduled · key 0x3f9a")),
    (2.0, row("WARN", "relay/05 jitter above threshold · 41 ms")),
    (2.1, card("RELAY JITTER", "41 ms", "relay/05 above threshold", "warn")),
    (2.2, alert("JITTER ON RELAY 05", "warn")),
    (2.3, meter(0.62)),
    (2.6, row("INFO", "operator session resumed · console 3")),
    (3.0, row("BREACH", "unsigned packet on channel 9 · quarantined")),
    (3.05, alert("BREACH · CHANNEL 9", "breach")),
    (3.1, meter(0.91)),
    (3.3, card("QUARANTINE", "1 packet", "channel 9 held", "breach")),
    (3.6, row("INFO", "quarantine held · forensic capture armed")),
    (3.9, row("TRACE", "heartbeat · all wards answering")),
    (4.6, meter(0.72)),
    (5.0, row("INFO", "operator acknowledged · console 3")),
    (5.4, alert("ACKNOWLEDGED · CHANNEL 9", "warn")),
    (5.8, meter(0.48)),
]
PASS = 7.0  # seconds one pass of the show lasts before a live send repeats it


def payload_of(message):
    """A message as the bytes either door reads: JSON text."""
    return json.dumps(message, ensure_ascii=False).encode()


def write_recording(path, show, seconds):
    """Every message the show sends in its first `seconds`, as a
    recording file: one arrival a message, at that message's second."""
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, message in show:
            if at < seconds:
                payload = payload_of(message)
                recording.write(FRAME.pack(at, len(payload)))
                recording.write(payload)


def udp_sender(host):
    """A send over UDP: one datagram a message."""
    door = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    return lambda payload: door.sendto(payload, (host, UDP_PORT)) == len(payload)


def websocket_sender(uri):
    """A send over the WebSocket, through a SigilIO hub's client door.

    The client calls the server on a thread of its own, and its door
    reads Open once the handshake is through, from when a send goes out.
    So the show starts once the door is Open, and stops before it starts
    with the reason when the server cannot be reached."""
    from sigil.io import Hub, registerTransports

    hub = Hub()
    registerTransports(hub, ["ws"])
    door = hub.listen(uri)
    deadline = time.monotonic() + 15.0
    while not door.state().isOpen():
        if door.state().error or time.monotonic() >= deadline:
            raise SystemExit(f"{uri}: {door.state().error or 'no answer'}")
        time.sleep(0.05)
    return door.send


def play(send):
    """The show, pass after pass, each message at its second, until
    interrupted."""
    started = time.monotonic()
    passes = 0
    while True:
        for at, message in SHOW:
            pause = started + passes * PASS + at - time.monotonic()
            if pause > 0:
                time.sleep(pause)
            if not send(payload_of(message)):
                print(f"not sent: {message}")
        passes += 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--door",
        choices=("udp", "ws"),
        default="udp",
        help="which door the show goes through",
    )
    parser.add_argument(
        "--record",
        action="store_true",
        help="write the recordings under data/ instead of sending",
    )
    parser.add_argument(
        "--seconds",
        type=float,
        default=PASS,
        help="how much of the show a recording holds",
    )
    parser.add_argument(
        "--host", default="127.0.0.1", help="where the UDP datagrams go"
    )
    parser.add_argument(
        "--uri", default=WEBSOCKET_URI, help="the WebSocket server to call"
    )
    arguments = parser.parse_args(argv)

    if arguments.record:
        here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
        write_recording(os.path.join(here, "wardnet.feed"), SHOW, arguments.seconds)
        write_recording(os.path.join(here, "silence.feed"), [], 0)
        print(f"{here}: wardnet.feed and silence.feed")
        return 0
    send = (
        udp_sender(arguments.host)
        if arguments.door == "udp"
        else websocket_sender(arguments.uri)
    )
    print(f"playing the show through the {arguments.door} door")
    try:
        play(send)
    except KeyboardInterrupt:
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
