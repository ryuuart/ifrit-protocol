"""UDP transport for already-serialized SpellCircle scene data."""

from __future__ import annotations

import socket
from types import TracebackType

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 27015


class SceneSender:
    """Owns a reusable UDP socket bound to one destination."""

    def __init__(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT) -> None:
        """Opens a datagram socket that sends to ``host`` and ``port``."""
        self.host = host
        self.port = port
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, data: bytes | bytearray | memoryview) -> None:
        """Sends one serialized scene datagram to the configured destination."""
        self._socket.sendto(data, (self.host, self.port))

    def close(self) -> None:
        """Closes the underlying socket; repeated calls are safe."""
        self._socket.close()

    def __enter__(self) -> SceneSender:
        """Returns this sender for use as a context manager."""
        return self

    def __exit__(
        self,
        exception_type: type[BaseException] | None,
        exception: BaseException | None,
        traceback: TracebackType | None,
    ) -> None:
        """Closes the socket when leaving a context-manager scope."""
        del exception_type, exception, traceback
        self.close()


def send_once(
    data: bytes | bytearray | memoryview,
    host: str = DEFAULT_HOST,
    port: int = DEFAULT_PORT,
) -> None:
    """Sends one scene payload with a short-lived UDP socket."""
    with SceneSender(host, port) as sender:
        sender.send(data)


# Compatibility for existing streaming scripts; prefer SceneSender.
SCSender = SceneSender
