#!/usr/bin/env python3
"""The Python client's socket, held to a host of this file's own.

    python_connection.py

The transport under ``sigil.protocol.connect`` is written by hand — a
websocket client on the standard library alone — so it is checked apart
from any real host: a server in this process speaks the endpoint's words
(the definition served at ``/protocol``, the upgrade on the address's
path, text frames each way, a ping), and the cases hold the client to
them. It reads the definition before it attaches; its first command is
``host.describe``; a host that answers from a definition with another
breaking revision is refused and let go; a state directory names the host
through the address file under it; an event that arrives while a command
waits for its answer reaches the listeners of its method; an error the
host answers is raised as the refusal it is; a ping is answered; and a
host that serves no definition is refused before anything is sent.
"""

from __future__ import annotations

import base64
import hashlib
import json
import pathlib
import socket
import struct
import sys
import tempfile
import threading
import typing
import unittest

HERE = pathlib.Path(__file__).resolve().parent
APPLICATION = HERE.parents[4]
PACKAGE_ROOT = APPLICATION.parent / "python" / "sigil"

sys.path.insert(0, str(PACKAGE_ROOT))

from sigil.protocol import (  # noqa: E402
    ErrorCode,
    Host,
    ProtocolError,
    Revision,
    connect,
)
from sigil.protocol.connection import ADDRESS_FILE  # noqa: E402

DEFINITION = b"the reflected definition"
"""What the host of this file serves at /protocol."""

UPGRADE_KEY_SUFFIX = b"258EAFA5-E914-47DA-95CA-C5AB0DC11B65"


class LoopbackHost(threading.Thread):
    """A host of this file's own on loopback: one connection at a time,
    answered by method as `answer` says, keeping the methods it was asked
    in order and the pongs it heard."""

    def __init__(self, *, breaking: int = 0, serves_definition: bool = True) -> None:
        super().__init__(daemon=True)
        self.breaking = breaking
        self.serves_definition = serves_definition
        self.asked: list[str] = []
        self.pongs = 0
        self.ponged = threading.Event()
        self.upgrades = 0
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(8)
        self.port = self.listener.getsockname()[1]
        self.address = f"ws://127.0.0.1:{self.port}/sigil"
        self.start()

    def close(self) -> None:
        self.listener.close()

    def run(self) -> None:
        while True:
            try:
                peer, _ = self.listener.accept()
            except OSError:
                return
            with peer:
                try:
                    self.serve(peer)
                except (OSError, ValueError):
                    pass

    # --- one connection --------------------------------------------------

    def serve(self, peer: socket.socket) -> None:
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = peer.recv(4096)
            if not chunk:
                return
            head += chunk
        request, _, rest = head.partition(b"\r\n\r\n")
        lines = request.split(b"\r\n")
        path = lines[0].split(b" ")[1]
        fields = {
            name.strip().lower(): value.strip()
            for name, _, value in (line.partition(b":") for line in lines[1:])
        }
        if b"sec-websocket-key" not in fields:
            if path == b"/protocol" and self.serves_definition:
                peer.sendall(
                    b"HTTP/1.1 200 OK\r\nContent-Length: "
                    + str(len(DEFINITION)).encode()
                    + b"\r\nConnection: close\r\n\r\n"
                    + DEFINITION
                )
            else:
                peer.sendall(
                    b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                    b"Connection: close\r\n\r\n"
                )
            return
        accept = base64.b64encode(
            hashlib.sha1(fields[b"sec-websocket-key"] + UPGRADE_KEY_SUFFIX).digest()
        )
        peer.sendall(
            b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
            b"Connection: Upgrade\r\nSec-WebSocket-Accept: " + accept + b"\r\n\r\n"
        )
        self.upgrades += 1
        buffer = bytearray(rest)
        while True:
            frame = self.read_frame(peer, buffer)
            if frame is None:
                return
            opcode, payload = frame
            if opcode == 0x8:
                return
            if opcode == 0xA:
                self.pongs += 1
                self.ponged.set()
                continue
            message = json.loads(payload)
            self.asked.append(message["method"])
            self.answer(peer, message)

    def answer(self, peer: socket.socket, message: dict[str, typing.Any]) -> None:
        method = message["method"]
        identity = message["id"]
        if method == "host.describe":
            result: dict[str, typing.Any] = {
                "version": {
                    "program": "Stub",
                    "revision": {
                        "breaking": self.breaking,
                        "compatible": Revision().compatible,
                    },
                },
                "domains": ["clock", "host"],
            }
            self.send(peer, {"id": identity, "result": result})
        elif method == "clock.current":
            # A ping and an event first, as a host that is busy sends them:
            # the client answers the one and hands the other on while it
            # waits for its answer.
            self.send_frame(peer, 0x9, b"are you there")
            self.send(
                peer,
                {"method": "clock.budgetExpired", "parameters": {"seconds": 2.5}},
            )
            self.send(
                peer,
                {"id": identity, "result": {"seconds": 1.25, "frame": 75}},
            )
        else:
            self.send(
                peer,
                {
                    "id": identity,
                    "error": {"code": "notMounted", "message": f"{method}: no"},
                },
            )

    @staticmethod
    def send(peer: socket.socket, message: dict[str, typing.Any]) -> None:
        LoopbackHost.send_frame(peer, 0x1, json.dumps(message).encode())

    @staticmethod
    def send_frame(peer: socket.socket, opcode: int, payload: bytes) -> None:
        length = len(payload)
        if length < 126:
            header = struct.pack("!BB", 0x80 | opcode, length)
        elif length < 1 << 16:
            header = struct.pack("!BBH", 0x80 | opcode, 126, length)
        else:
            header = struct.pack("!BBQ", 0x80 | opcode, 127, length)
        peer.sendall(header + payload)

    @staticmethod
    def read_frame(
        peer: socket.socket, buffer: bytearray
    ) -> tuple[int, bytes] | None:
        def take(count: int) -> bytes | None:
            while len(buffer) < count:
                chunk = peer.recv(65536)
                if not chunk:
                    return None
                buffer.extend(chunk)
            taken = bytes(buffer[:count])
            del buffer[:count]
            return taken

        head = take(2)
        if head is None:
            return None
        opcode = head[0] & 0x0F
        length = head[1] & 0x7F
        if not head[1] & 0x80:
            raise ValueError("a client's frame is always masked")
        if length == 126:
            extended = take(2)
            if extended is None:
                return None
            length = struct.unpack("!H", extended)[0]
        elif length == 127:
            extended = take(8)
            if extended is None:
                return None
            length = struct.unpack("!Q", extended)[0]
        mask = take(4)
        payload = take(length) if length else b""
        if mask is None or payload is None:
            return None
        return opcode, bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))


class PythonConnection(unittest.TestCase):
    def serve(self, **options: typing.Any) -> LoopbackHost:
        host = LoopbackHost(**options)
        self.addCleanup(host.close)
        return host

    def test_it_reads_the_definition_then_asks_describe_first(self) -> None:
        host = self.serve()
        with connect(host.address, timeout=10) as connection:
            self.assertEqual(connection.definition, DEFINITION)
            self.assertEqual(host.asked, ["host.describe"])
            described = Host(connection).describe()
            self.assertEqual(described.version.program, "Stub")
            self.assertEqual(described.domains, ("clock", "host"))
        self.assertEqual(host.upgrades, 1)

    def test_a_host_of_another_breaking_revision_is_refused_and_let_go(self) -> None:
        host = self.serve(breaking=Revision().breaking + 1)
        with self.assertRaises(ProtocolError) as refused:
            connect(host.address, timeout=10)
        self.assertEqual(refused.exception.error.code, ErrorCode.unreadable)
        self.assertTrue(refused.exception.error.message.startswith("client:"))
        self.assertIn("revision", refused.exception.error.message)
        self.assertEqual(host.asked, ["host.describe"])

    def test_a_state_directory_names_the_host_through_its_address_file(self) -> None:
        host = self.serve()
        with tempfile.TemporaryDirectory(prefix="sigil_connection_") as folder:
            (pathlib.Path(folder) / ADDRESS_FILE).write_text(host.address + "\n")
            with connect(folder, timeout=10) as connection:
                self.assertEqual(connection.address, host.address)
                self.assertEqual(Host(connection).describe().version.program, "Stub")

    def test_an_event_and_a_ping_while_a_command_waits_are_both_handled(self) -> None:
        host = self.serve()
        heard: list[typing.Any] = []
        with connect(host.address, timeout=10) as connection:
            connection.listen("clock.budgetExpired", heard.append)
            result = connection.call("clock.current", {})
            self.assertEqual(result, {"seconds": 1.25, "frame": 75})
            self.assertEqual(heard, [{"seconds": 2.5}])
        # The pong went out before the answer was read; the host reads it on
        # its own thread, which may come to it after the client is done.
        self.assertTrue(host.ponged.wait(10))
        self.assertEqual(host.pongs, 1)

    def test_an_error_the_host_answers_is_raised_as_its_refusal(self) -> None:
        host = self.serve()
        with connect(host.address, timeout=10) as connection:
            with self.assertRaises(ProtocolError) as refused:
                connection.call("session.open", {"sketch": "anything"})
        self.assertEqual(refused.exception.error.code, ErrorCode.notMounted)
        self.assertEqual(refused.exception.error.message, "session.open: no")

    def test_a_host_that_serves_no_definition_is_refused_before_anything_is_sent(
        self,
    ) -> None:
        host = self.serve(serves_definition=False)
        with self.assertRaises(ConnectionError):
            connect(host.address, timeout=10)
        self.assertEqual(host.upgrades, 0)
        self.assertEqual(host.asked, [])

    def test_an_address_that_is_no_websocket_is_refused(self) -> None:
        with self.assertRaises(ValueError):
            connect("http://127.0.0.1:1/sigil")


if __name__ == "__main__":
    unittest.main()
