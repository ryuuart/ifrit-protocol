"""UDP transport for already-serialized SpellCircle scene data."""

from __future__ import annotations

import operator
import socket
from types import TracebackType

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 27015


class SceneSender:
    """Owns a reusable UDP socket bound to one destination."""

    def __init__(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT) -> None:
        """Opens a datagram socket that sends to ``host`` and ``port``."""
        if isinstance(port, bool) or not 1 <= operator.index(port) <= 65535:
            raise ValueError("port must be between 1 and 65535")
        self.host = host
        self.port = port
        addresses = socket.getaddrinfo(host, port, type=socket.SOCK_DGRAM)
        error: OSError | None = None
        for family, kind, protocol, _, address in addresses:
            try:
                connection = socket.socket(family, kind, protocol)
            except OSError as failure:
                error = failure
                continue
            try:
                connection.connect(address)
            except OSError as failure:
                connection.close()
                error = failure
                continue
            self._socket = connection
            break
        else:
            raise error or OSError("No UDP address was resolved for the destination")

    def send(self, data: bytes | bytearray | memoryview) -> None:
        """Sends one serialized scene datagram to the configured destination."""
        self._socket.send(data)

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
