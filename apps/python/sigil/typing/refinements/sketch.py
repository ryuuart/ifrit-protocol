"""Conversion seams of the sketch context, its assets and the specimen kit."""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.erased("_sigil.Context", "background", "_t.ColorLike")
    table.returns("_sigil.Context", "size", "tuple[float, float]")
    table.returns("_sigil.sketch.Assets", "database", "_sigil.data.Database | None")
    table.erased("_sigil.sketch.kit", "stage", "_sigil.Context")
    table.erased("_sigil.sketch.kit.Theme", "font style", "_t.ColorLike")
    table.erased(
        "_sigil.sketch.kit.Provide",
        "__exit__",
        "type[BaseException] | None",
        "BaseException | None",
        "types.TracebackType | None",
    )
