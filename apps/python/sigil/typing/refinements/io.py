"""Conversion seams of the resource hub, its feeds, leases and recordings."""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"


def register(table: Table) -> None:
    table.returns("_sigil.io.Hub", "fetch", "bytes | None")
    table.returns("_sigil.io.Feed", "latest", "bytes | None")
    table.erased("_sigil.io.Hub", "write", "collections.abc.Buffer")
    table.erased("_sigil.io.ResourceLease", "__exit__", "object", "object", "object")
    table.declares(
        "_sigil.io.Hub",
        "load",
        """@typing.overload
def load(self, type: type[_sigil.data.Table], uri: str) -> _sigil.data.Table | None: ...
@typing.overload
def load(self, type: type[_sigil.data.Json], uri: str) -> _sigil.data.Json | None: ...
@typing.overload
def load(self, type: type[_sigil.data.Database], uri: str) -> _sigil.data.Database | None: ...
@typing.overload
def load(self, type: type[_sigil.image.ImageAsset], uri: str) -> _sigil.image.ImageAsset | None: ...
""",
    )
    table.erased("_sigil.io.Arrival", "__init__ bytes", "collections.abc.Buffer")
    table.erased("_sigil.io.Feed", "deliver send sendTo", "collections.abc.Buffer")
    table.erased("_sigil.io.SharedMemoryWriter", "write", "collections.abc.Buffer")
    for path in (
        "_sigil.io.Hub.mount",
        "_sigil.io.Hub.setNetworkCacheDirectory",
        "_sigil.io.Feed.record",
        "_sigil.io.RecordingWriter.__init__",
        "_sigil.io.readRecording",
    ):
        table.parameters(path, path=PATH_INPUT)
