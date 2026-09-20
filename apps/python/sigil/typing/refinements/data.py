"""Conversion seams of tables, JSON, databases and the domain mapping.

Data callbacks preserve the model and result types supplied by Python.
"""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"


def register(table: Table) -> None:
    table.erased("_sigil.data.Column", "__init__", "_sigil.data.ColumnType | None")
    table.parameters(
        "_sigil.data.Column.__init__", values="collections.abc.Iterable[_t.CellInput]"
    )
    table.returns("_sigil.data.Column", "__getitem__ at", "_t.CellValue")
    table.returns("_sigil.data.Column", "values", "list[_t.CellValue]")
    table.returns("_sigil.data.Group", "key", "_t.CellValue")
    # A flag compares with a flag or with the boolean a table's boolean cell
    # reads as; anything else answers NotImplemented, which Python resolves
    # into an equality of identity or an ordering's TypeError, so every
    # comparison declares the boolean Python ends up with.
    table.returns("_sigil.data.Flag", "__eq__ __lt__ __le__ __gt__ __ge__", "bool")
    for name in ("__lt__", "__le__", "__gt__", "__ge__"):
        table.parameters("_sigil.data.Flag." + name, other="Flag | bool")
    table.returns("_sigil.data.Database", "fromBytes", "Database")
    table.erased("_sigil.data", "encodeOsc encodeMidi encodeArtNet", "_t.JsonInput")
    table.parameters("_sigil.data.Database.open", path=PATH_INPUT)
    table.parameters("_sigil.data.engineOf", uri=PATH_INPUT)
    table.erased("_sigil.data.Json", "__init__", "_t.JsonInput")
    table.erased("_sigil.data.Json", "__getitem__", "str | typing.SupportsInt")
    table.parameters(
        "_sigil.data.Json.object",
        items="collections.abc.Iterable[tuple[str, _t.JsonInput]]",
    )
    table.returns("_sigil.data.Json", "to_python", "_t.JsonValue")
    table.erased("_sigil.data", "encodeJson tableFromJson", "_t.JsonInput")
    table.returns("_sigil.data", "tableFromJson", "Table | None")
    table.erased("_sigil.data.Table", "add derive", "ColumnType | None")
    table.parameters(
        "_sigil.data.Table.add", values="collections.abc.Iterable[_t.CellInput]"
    )
    table.parameters(
        "_sigil.data.Table.derive",
        value="collections.abc.Callable[[int], _t.CellInput]",
    )
    table.parameters(
        "_sigil.data.Table.filter", predicate="collections.abc.Callable[[int], bool]"
    )
    table.returns("_sigil.data.Table", "cell", "_t.CellValue")
    table.returns("_sigil.data.Table", "row", "dict[str, _t.CellValue]")
    table.erased(
        "_sigil.data.Scale",
        "domain range",
        "Interval | collections.abc.Sequence[_t.FloatLike]",
    )
    table.declares(
        "_sigil.data.Scale",
        "through",
        "def through[Result](self, input: _t.FloatLike, interpolate: collections.abc.Callable[[float], Result]) -> Result: ...",
    )
