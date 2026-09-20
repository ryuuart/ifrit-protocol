"""Conversion seams of the neutral Compose kit and the layout schemes.

Kit fields reuse the conversion policy implemented by the native field helper.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.erased("_sigil.compose.kit", "at", "_t.DimensionLike", "_t.DimensionLike")
    table.erased("_sigil.compose.kit", "disc", "_t.PointLike")
    table.erased("_sigil.compose.kit", "dot", "_t.PointLike", "_t.SurfacePaintLike")
    table.erased("_sigil.compose.kit", "ring", "_t.PointLike")
    for record, names in {
        "Caption": "label note readingLine",
        "Panel": "eyebrowLine noteLine titleLine",
        "Sheet": "footerLine subtitleLine titleLine",
    }.items():
        for name in names.split():
            table.returns(
                "_sigil.compose.kit." + record,
                name,
                f"collections.abc.Callable[[str, {record}], _sigil.compose.Element]",
            )
            table.erased(
                "_sigil.compose.kit." + record,
                name,
                f"collections.abc.Callable[[], _sigil.compose.Element] | collections.abc.Callable[[str], _sigil.compose.Element] | collections.abc.Callable[[str, {record}], _sigil.compose.Element] | None",
            )
    table.erased("_sigil.compose.layouts.AlongPath", "path", "_t.ShapeLike")
    table.returns("_sigil.compose.layouts.AlongPath", "path", "_t.ShapeFunction")
