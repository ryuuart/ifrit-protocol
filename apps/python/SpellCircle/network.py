"""
UDP transport for SpellCircle scene bytes.

Kept separate from scene authoring (`canvas.py`) and serialization
(`builder.py`): this module only knows how to move already-serialized
bytes over the wire.

Public API
----------
SCSender  — a reusable UDP socket, for streaming many frames
send_once() — open a socket, send one payload, close it
"""

import socket

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 27015


class SCSender:
    """A reusable UDP socket bound to one destination.

    Usage:
        with SCSender(host, port) as sender:
            sender.send(scene_bytes)
    """

    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT):
        self.host = host
        self.port = port
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, data):
        self._sock.sendto(data, (self.host, self.port))

    def close(self):
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()


def send_once(data, host=DEFAULT_HOST, port=DEFAULT_PORT):
    """Send a single scene payload without keeping a socket around."""
    with SCSender(host, port) as sender:
        sender.send(data)
