"""data.connect and data.replay: the Connection, its Message and its state.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/data-connection package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased("_sigil.data.Connection", "send reply", "_t.JsonInput")
    table.parameters(
        "_sigil.data.Connection.on",
        handler="collections.abc.Callable[[Message], object]",
    )
    table.parameters(
        "_sigil.data.Connection.otherwise",
        handler="collections.abc.Callable[[Message], object]",
    )
    table.erased("_sigil.data.Message", "__getitem__", "str | typing.SupportsInt")
    table.returns("_sigil.data.Message", "to_python", "_t.JsonValue")
    table.parameters("_sigil.data.Connection.record", path=PATH_INPUT)
    table.parameters("_sigil.data.replay", recording=PATH_INPUT)
