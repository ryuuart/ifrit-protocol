"""The envelope every caller speaks, and the socket a host is reached over.

``Envelopes`` is a caller made of one exchange of envelope text: a request
``{"id", "method", "parameters"}`` out, its answer ``{"id", "result"}`` or
``{"id", "error"}`` back, and every event ``{"method", "parameters"}`` handed
to the listeners of its method. Whatever carries the text — a socket, or a
host in the same process — speaks it the same way.

``connect(address)`` attaches to a running host — Sketchbook, Seer, the
receiver — at ``ws://127.0.0.1:PORT/sigil`` or through the state directory
whose ``protocol-address`` names it, reads the definition the host serves at
``/protocol``, and refuses a host built from a definition that breaks this
client. ``launch(state=…)`` starts a headless Sketchbook serving the protocol
under a state directory and connects to it; closing the connection ends the
process.
"""

from __future__ import annotations

import base64
import collections.abc
import http.client
import itertools
import json
import os
import pathlib
import secrets
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import types
import typing
import urllib.parse

from . import messages
from .host import Host
from .shared import Error, ErrorCode, ProtocolError, Revision, client_refusal

ADDRESS_FILE = "protocol-address"
"""The file under a host's state root that names the address it listens on."""

_TEXT = 0x1
_CLOSE = 0x8
_PING = 0x9
_PONG = 0xA
_CONTINUATION = 0x0


class Envelopes:
    """A caller made of one exchange of envelope text.

    ``exchange`` sends one request's text and answers its answer's text; a
    subclass supplies it, and hands every event it meets to ``deliver``.
    """

    def __init__(self) -> None:
        self._identities = itertools.count(1)
        self._listeners: dict[
            str, list[collections.abc.Callable[[messages.Json], None]]
        ] = {}

    def exchange(self, envelope: str) -> str:
        """One request's envelope text out, its answer's text back."""
        raise NotImplementedError

    def call(self, method: str, parameters: dict[str, messages.Json]) -> messages.Json:
        """Sends @p method with @p parameters and answers the result's JSON
        object, raising ProtocolError where the host answered an error."""
        request: dict[str, messages.Json] = {
            "id": next(self._identities),
            "method": method,
        }
        if parameters:
            request["parameters"] = parameters
        text = self.exchange(json.dumps(request))
        try:
            answer = json.loads(text)
        except json.JSONDecodeError as error:
            raise ProtocolError(
                client_refusal(
                    ErrorCode.unreadable, method, f"the answer is no JSON: {error}"
                )
            ) from error
        if not isinstance(answer, dict):
            raise ProtocolError(
                client_refusal(
                    ErrorCode.unreadable, method, "the answer is no JSON object"
                )
            )
        fields = typing.cast(dict[str, messages.Json], answer)
        if "error" in fields:
            raise ProtocolError(Error.from_json(fields["error"]))
        return fields.get("result", {})

    def listen(
        self, method: str, listener: collections.abc.Callable[[messages.Json], None]
    ) -> None:
        """Hands every event of @p method to @p listener as its JSON object."""
        self._listeners.setdefault(method, []).append(listener)

    def deliver(self, method: str, parameters: messages.Json) -> None:
        """One event, handed to the listeners of its method."""
        for listener in self._listeners.get(method, []):
            listener(parameters)

    def refused(self, error: Error) -> None:
        """A refusal no listener hears, written where it is seen."""
        print(f"sigil.protocol: {error.message}", file=sys.stderr)


class Connection(Envelopes):
    """A host reached over its socket: the caller every domain class speaks
    through, the definition the host serves, and — for a host this client
    launched — the process, which closing the connection ends."""

    def __init__(
        self,
        address: str,
        *,
        timeout: float = 120.0,
        process: subprocess.Popen[bytes] | None = None,
    ) -> None:
        super().__init__()
        parts = urllib.parse.urlsplit(address)
        if parts.scheme != "ws" or not parts.hostname or not parts.port:
            raise ValueError(f"a host's address is ws://HOST:PORT/PATH, not {address}")
        self.address = address
        self.process = process
        self._timeout = timeout
        self._buffer = b""
        self.definition = _served_definition(parts.hostname, parts.port, timeout)
        """The reflected definition the host serves at /protocol."""
        self._socket = socket.create_connection((parts.hostname, parts.port), timeout)
        self._handshake(parts.hostname, parts.port, parts.path or "/")
        revision = Host(self).describe().version.revision
        if revision.breaking != Revision().breaking:
            self.close()
            raise ProtocolError(
                client_refusal(
                    ErrorCode.unreadable,
                    "host.describe",
                    f"the host speaks revision {revision.breaking}.{revision.compatible}, "
                    f"and this client was built against {Revision().breaking}",
                )
            )

    def __enter__(self) -> Connection:
        return self

    def __exit__(
        self,
        kind: type[BaseException] | None,
        value: BaseException | None,
        trace: types.TracebackType | None,
    ) -> None:
        self.close()

    def exchange(self, envelope: str) -> str:
        identity = json.loads(envelope)["id"]
        self._send(_TEXT, envelope.encode())
        while True:
            message = self._receive(self._timeout)
            if message is None:
                raise ProtocolError(
                    client_refusal(
                        ErrorCode.notSent, "request", "the host closed the socket"
                    )
                )
            if message.get("id") == identity:
                return json.dumps(message)
            self._event(message)

    def wait(self, seconds: float) -> None:
        """Hands every event that arrives within @p seconds to its listeners."""
        deadline = time.monotonic() + seconds
        while (left := deadline - time.monotonic()) > 0:
            message = self._receive(left)
            if message is None:
                return
            self._event(message)

    def close(self) -> None:
        """Lets the host go, and ends it where this client launched it."""
        if self._socket is not None:
            try:
                self._send(_CLOSE, b"")
            except OSError:
                pass
            self._socket.close()
            self._socket = None
        if self.process is not None:
            if self.process.poll() is None:
                self.process.send_signal(signal.SIGTERM)
                try:
                    self.process.wait(30)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
            self.process = None

    # --- the websocket underneath ---------------------------------------

    _socket: socket.socket | None

    def _handshake(self, host: str, port: int, path: str) -> None:
        key = base64.b64encode(secrets.token_bytes(16)).decode()
        request = (
            f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
            f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        assert self._socket is not None
        self._socket.sendall(request.encode())
        while b"\r\n\r\n" not in self._buffer:
            chunk = self._socket.recv(4096)
            if not chunk:
                raise ConnectionError(f"{self.address} closed before it upgraded")
            self._buffer += chunk
        head, _, self._buffer = self._buffer.partition(b"\r\n\r\n")
        status = head.split(b"\r\n", 1)[0]
        if b" 101 " not in status + b" ":
            raise ConnectionError(
                f"{self.address} refused the upgrade: {status.decode()}"
            )

    def _send(self, opcode: int, payload: bytes) -> None:
        assert self._socket is not None
        length = len(payload)
        if length < 126:
            header = struct.pack("!BB", 0x80 | opcode, 0x80 | length)
        elif length < 1 << 16:
            header = struct.pack("!BBH", 0x80 | opcode, 0x80 | 126, length)
        else:
            header = struct.pack("!BBQ", 0x80 | opcode, 0x80 | 127, length)
        mask = secrets.token_bytes(4)
        masked = bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))
        self._socket.sendall(header + mask + masked)

    def _read(self, count: int, deadline: float) -> bytes | None:
        assert self._socket is not None
        while len(self._buffer) < count:
            left = deadline - time.monotonic()
            if left <= 0:
                return None
            self._socket.settimeout(left)
            try:
                chunk = self._socket.recv(65536)
            except TimeoutError:
                return None
            if not chunk:
                return None
            self._buffer += chunk
        taken, self._buffer = self._buffer[:count], self._buffer[count:]
        return taken

    def _receive(self, seconds: float) -> dict[str, typing.Any] | None:
        """The next whole text message as JSON; nothing when the socket
        closed or nothing arrived in time."""
        deadline = time.monotonic() + seconds
        text = b""
        while True:
            head = self._read(2, deadline)
            if head is None:
                return None
            opcode = head[0] & 0x0F
            final = bool(head[0] & 0x80)
            length = head[1] & 0x7F
            if length == 126:
                extended = self._read(2, deadline)
                if extended is None:
                    return None
                length = struct.unpack("!H", extended)[0]
            elif length == 127:
                extended = self._read(8, deadline)
                if extended is None:
                    return None
                length = struct.unpack("!Q", extended)[0]
            mask = self._read(4, deadline) if head[1] & 0x80 else b""
            payload = self._read(length, deadline) if length else b""
            if payload is None or mask is None:
                return None
            if mask:
                payload = bytes(
                    byte ^ mask[index % 4] for index, byte in enumerate(payload)
                )
            if opcode == _CLOSE:
                return None
            if opcode == _PING:
                self._send(_PONG, payload)
                continue
            if opcode in (_TEXT, _CONTINUATION):
                text += payload
                if final:
                    return json.loads(text)

    def _event(self, message: dict[str, typing.Any]) -> None:
        method = message.get("method")
        if isinstance(method, str) and "id" not in message:
            self.deliver(method, message.get("parameters", {}))


def _served_definition(host: str, port: int, timeout: float) -> bytes:
    served = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        served.request("GET", "/protocol")
        response = served.getresponse()
        body = response.read()
    finally:
        served.close()
    if response.status != 200:
        raise ConnectionError(f"{host}:{port} serves no definition at /protocol")
    return body


def _address_of(where: str | os.PathLike[str]) -> str:
    text = os.fspath(where)
    # Anything with a scheme is an address, which the connection refuses
    # unless it is a websocket's; only a plain path names a directory.
    if "://" in text:
        return text
    return (pathlib.Path(text) / ADDRESS_FILE).read_text().strip()


def connect(address: str | os.PathLike[str], *, timeout: float = 120.0) -> Connection:
    """Attaches to the host at @p address: its ``ws://`` address, or the
    state directory whose address file names it."""
    return Connection(_address_of(address), timeout=timeout)


def launch(
    *,
    state: str | os.PathLike[str] | None = None,
    executable: str | os.PathLike[str] | None = None,
    port: int = 0,
    timeout: float = 120.0,
) -> Connection:
    """Starts a headless Sketchbook serving the protocol under @p state — a
    directory of its own where none is named — and connects to it once its
    address file is written. @p executable is Sketchbook's own, or the one
    ``SIGIL_SKETCHBOOK`` or the path names. Closing the connection ends the
    process."""
    program = (
        executable or os.environ.get("SIGIL_SKETCHBOOK") or shutil.which("Sketchbook")
    )
    if not program:
        raise FileNotFoundError(
            "Sketchbook was not found: pass executable=, or set SIGIL_SKETCHBOOK"
        )
    root = (
        pathlib.Path(state)
        if state is not None
        else pathlib.Path(tempfile.mkdtemp(prefix="sigil-host-"))
    )
    root.mkdir(parents=True, exist_ok=True)
    address_file = root / ADDRESS_FILE
    address_file.unlink(missing_ok=True)
    inspect = "--inspect" if port == 0 else f"--inspect={port}"
    process = subprocess.Popen(
        [os.fspath(program), "--headless", inspect, "--state", os.fspath(root)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    deadline = time.monotonic() + timeout
    while not (address_file.exists() and address_file.read_text().strip()):
        if process.poll() is not None:
            raise ConnectionError(
                f"Sketchbook ended with status {process.returncode} before it listened"
            )
        if time.monotonic() > deadline:
            process.kill()
            process.wait()
            raise TimeoutError(
                f"Sketchbook wrote no address under {root} in {timeout} s"
            )
        time.sleep(0.05)
    try:
        return Connection(
            address_file.read_text().strip(), timeout=timeout, process=process
        )
    except BaseException:
        process.kill()
        process.wait()
        raise
