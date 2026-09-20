"""Conversion seams of the bound Skia values: paints, paths and rectangles."""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.erased("_sigil.skia.Paint", "setColor", "_t.ColorLike")
    table.erased("_sigil.skia.Path", "Oval Rect", "_t.RectLike")
    table.erased("_sigil.skia.PathBuilder", "addArc addOval addRect", "_t.RectLike")
    table.returns("_sigil.skia.Path", "getBounds", "_sigil.skia.Rect")
    table.returns("_sigil.skia.Picture", "cullRect", "_sigil.skia.Rect")
    for value in ("Point", "Rect"):
        table.parameters(
            f"_sigil.skia.{value}.__init__",
            coordinates="collections.abc.Sequence[_t.FloatLike]",
        )
