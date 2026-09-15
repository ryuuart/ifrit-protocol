#!/usr/bin/env python3
"""A phone that is not there: the peer phone_sky's page would be.

Two modes, one phone:

    python3 phone.py
        connects to ws://127.0.0.1:8848/sky, turns the palette every two
        seconds and gusts the wind between turns, and prints the sky it
        is handed back — which is what makes the sketch move in a window
        with nobody holding a real phone.

    python3 phone.py --record data/phone.feed --seconds 6
        writes those same messages, at those same seconds, as a feed
        recording, which is what the sketch's plate replays.

The messages are the two things a hand can do, and are exactly what the
page beside this file sends:

    {"kind": "Palette", "colors": [[r, g, b, a], ...]}
    {"kind": "Gust", "strength": s, "seconds": t}

The schedule is a function of the run's own time and holds no
randomness, so the file and the live connection carry the same messages
at the same seconds, and a still of the window is a still of the plate.

Standard library only: this runs wherever python3 does, with nothing
installed. There is no websocket in that library, so the handshake and
the frames are written out below — which is little enough for one door,
and keeps this file something a reader can check against the protocol.
"""

import argparse
import base64
import json
import os
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

# EDIT THESE FIRST: what this phone does, and when.
PALETTES = [
    [[0.44, 0.60, 0.90, 0.82], [0.54, 0.50, 0.86, 0.82], [0.38, 0.72, 0.84, 0.82]],
    [[0.95, 0.62, 0.35, 0.82], [0.92, 0.42, 0.42, 0.82], [0.99, 0.80, 0.45, 0.82]],
    [[0.40, 0.85, 0.68, 0.82], [0.30, 0.72, 0.80, 0.82], [0.70, 0.90, 0.55, 0.82]],
]
PALETTE_FIRST = 0.6  # when the first palette turn lands
GUST_FIRST = 2.0  # when the first gust lands
GUST_STRENGTH = 140.0  # how hard a gust blows, in px a second
GUST_FALL = 1.6  # how long it takes to die away


def moments(palette_every, gust_every):
    """Every message this phone sends, in order, for as long as it is
    asked for: a palette turn every `palette_every` seconds from the
    first one, and a gust every `gust_every` from the first of those.

    The turns start at the SECOND palette, because the sketch opens on
    the first one and a turn nobody can see is not a turn.
    """
    next_palette, next_gust, turn = PALETTE_FIRST, GUST_FIRST, 1
    while True:
        if next_palette <= next_gust:
            yield next_palette, {
                "kind": "Palette",
                "colors": PALETTES[turn % len(PALETTES)],
            }
            turn += 1
            next_palette += palette_every
        else:
            yield next_gust, {
                "kind": "Gust",
                "strength": GUST_STRENGTH,
                "seconds": GUST_FALL,
            }
            next_gust += gust_every


def payload_of(message):
    """The bytes of one message: compact JSON, as it goes on the wire."""
    return json.dumps(message, separators=(",", ":")).encode("utf-8")


class Peer:
    """A WEBSOCKET CLIENT WRITTEN OUT BY HAND: the upgrade request over a
    plain TCP socket, then frames masked the way a client must mask them
    and read back unmasked the way a server sends them.

    A message this size arrives in one frame, which is what this reads; a
    ping is answered where it arrives, because a door that pings wants to
    know somebody is still holding the line.
    """

    def __init__(self, host, port, path):
        self.socket = socket.create_connection((host, port))
        self.buffer = b""
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        request = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}:{port}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.socket.sendall(request.encode("ascii"))
        while b"\r\n\r\n" not in self.buffer:
            block = self.socket.recv(4096)
            if not block:
                raise ConnectionError("the door closed during the handshake")
            self.buffer += block
        greeting, self.buffer = self.buffer.split(b"\r\n\r\n", 1)
        if b"101" not in greeting.split(b"\r\n")[0]:
            raise ConnectionError(
                "the door answered with a page rather than switching protocols"
            )

    def close(self):
        self.socket.close()

    def send(self, message):
        """One message, as a text frame."""
        self._write(0x1, payload_of(message))

    def receive(self, timeout):
        """The next whole frame as (opcode, payload), or None when none
        arrived within `timeout` seconds — or when what arrived was a
        ping, which is answered here and handed back to nobody."""
        if not self.buffer and not select.select([self.socket], [], [], timeout)[0]:
            return None
        head = self._take(2)
        opcode = head[0] & 0x0F
        size = head[1] & 0x7F
        if size == 126:
            size = struct.unpack(">H", self._take(2))[0]
        elif size == 127:
            size = struct.unpack(">Q", self._take(8))[0]
        payload = self._take(size)
        if opcode == 0x9:
            self._write(0xA, payload)
            return None
        return opcode, payload

    def _take(self, count):
        """The next `count` bytes off the socket."""
        while len(self.buffer) < count:
            block = self.socket.recv(65536)
            if not block:
                raise ConnectionError("the door closed mid-frame")
            self.buffer += block
        taken, self.buffer = self.buffer[:count], self.buffer[count:]
        return taken

    def _write(self, opcode, payload):
        """One whole message in one masked frame. A client masks every
        frame it sends; a server masks none of them."""
        mask = os.urandom(4)
        frame = bytearray([0x80 | opcode])
        if len(payload) < 126:
            frame.append(0x80 | len(payload))
        elif len(payload) < 65536:
            frame.append(0x80 | 126)
            frame += struct.pack(">H", len(payload))
        else:
            frame.append(0x80 | 127)
            frame += struct.pack(">Q", len(payload))
        frame += mask
        frame += bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self.socket.sendall(frame)


def write_recording(path, seconds, palette_every, gust_every):
    """Every message of a run of `seconds`, as a recording file."""
    written = 0
    with open(path, "wb") as recording:
        recording.write(HEADER)
        for at, message in moments(palette_every, gust_every):
            if at >= seconds:
                break
            payload = payload_of(message)
            recording.write(FRAME.pack(at, len(payload)))
            recording.write(payload)
            written += 1
    return written


def speak(host, port, path, palette_every, gust_every):
    """The same messages, to the door, until interrupted — and the sky it
    hands back, one line a second so a terminal stays readable."""
    peer = Peer(host, port, path)
    print(f"connected to ws://{host}:{port}{path}")
    started = time.monotonic()
    upcoming = moments(palette_every, gust_every)
    at, message = next(upcoming)
    printed = -1.0
    while True:
        now = time.monotonic() - started
        while at <= now:
            peer.send(message)
            print(f"{now:6.2f}s  sent {message['kind']}")
            at, message = next(upcoming)
        frame = peer.receive(timeout=0.05)
        if frame is None:
            continue
        opcode, payload = frame
        if opcode == 0x8:
            print("the door closed")
            return
        try:
            sky = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            continue
        if sky.get("kind") != "Sky" or now - printed < 1.0:
            continue
        printed = now
        print(
            f"{now:6.2f}s  sky: wind {sky['wind']}, {len(sky['bands'])} bands, "
            f"first tint {sky['palette'][0]}"
        )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--record",
        metavar="PATH",
        help="write a recording at PATH instead of connecting to the door",
    )
    parser.add_argument(
        "--seconds", type=float, default=6.0, help="how long a recording runs"
    )
    parser.add_argument(
        "--palette-every", type=float, default=2.0, help="seconds between turns"
    )
    parser.add_argument(
        "--gust-every", type=float, default=2.0, help="seconds between gusts"
    )
    parser.add_argument("--host", default="127.0.0.1", help="where the door is")
    parser.add_argument("--port", type=int, default=8848, help="the port it holds")
    parser.add_argument("--path", default="/sky", help="the path it answers")
    arguments = parser.parse_args(argv)

    if arguments.record:
        written = write_recording(
            arguments.record,
            arguments.seconds,
            arguments.palette_every,
            arguments.gust_every,
        )
        print(f"{arguments.record}: {written} messages over {arguments.seconds:g}s")
        return 0

    try:
        speak(
            arguments.host,
            arguments.port,
            arguments.path,
            arguments.palette_every,
            arguments.gust_every,
        )
    except KeyboardInterrupt:
        print()
    except (ConnectionError, OSError) as trouble:
        print(f"could not reach ws://{arguments.host}:{arguments.port}: {trouble}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
