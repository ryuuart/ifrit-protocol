"""Conversion seams of the natural-media brush engine and its tools."""

from __future__ import annotations

from .table import Table

BRUSH = "_sigil.draw.brush"


def register(table: Table) -> None:
    for record in ("Curl", "Direction", "Vortex", "Wave"):
        table.erased(BRUSH + "." + record, "__call__", "_t.PointLike")
    table.erased(BRUSH + ".Direction", "__init__", "_t.DirectionLike")
    for record in ("Input", "Sample"):
        table.erased(BRUSH + "." + record, "__init__", "_t.PointLike")
    table.erased(BRUSH + ".Line", "__init__", "_t.PointLike", "_t.PointLike")
    table.erased(BRUSH + ".Plot", "path", "_t.PointLike")
    table.erased(
        BRUSH + ".Position", "__init__", "_t.DirectionLike", "_t.RectLike | None"
    )
    table.erased(BRUSH + ".Position", "angle field moveTo", "_t.DirectionLike")
    table.erased(BRUSH + ".Engine", "addField", "_t.DirectionLike")
    table.erased(BRUSH + ".Engine", "beginStroke", "_t.PointLike")
    table.erased(BRUSH + ".Engine", "clip", "_t.RectLike")
    table.erased(BRUSH + ".Engine", "flowLine", "_t.PointLike")
    table.erased(BRUSH + ".Engine", "line", "_t.PointLike", "_t.PointLike")
    table.erased(
        BRUSH + ".Engine", "fill hatchStyle mass set stroke wash", "_t.ColorLike"
    )
    for record in ("Curve", "Pressure"):
        table.erased(BRUSH + "." + record, "curve", "_t.ScalarFunction | None")
    table.erased(BRUSH, "charcoal marker pencil spray watercolor", "_t.ColorLike")
    table.erased(BRUSH, "line segment", "_t.PointLike", "_t.PointLike")
    table.erased(BRUSH, "flowLine trace", "_t.PointLike", "_t.DirectionLike")
    table.erased(BRUSH, "warp", "_t.DirectionLike")
    table.returns(BRUSH, "stockFields", "dict[str, Direction]")
    for key, parameter, value in [
        ("Polygon.__init__", "vertices", "collections.abc.Iterable[_t.PointLike]"),
        ("Polygon.vertices", "value", "collections.abc.Iterable[_t.PointLike]"),
        ("Engine.paint", "path", "collections.abc.Iterable[_t.SampleLike]"),
        ("Engine.polygon", "vertices", "collections.abc.Iterable[_t.PointLike]"),
        ("paint", "stroke", "collections.abc.Iterable[_t.SampleLike]"),
        ("wash", "polygon", "collections.abc.Iterable[_t.PointLike]"),
        ("warp", "polygon", "collections.abc.Iterable[_t.PointLike]"),
    ]:
        table.parameters(BRUSH + "." + key, **{parameter: value})
    for name in ("Engine.spline", "Plot.fromStroke", "spline"):
        table.parameters(
            BRUSH + "." + name,
            controls="collections.abc.Iterable[_t.SampleLike]",
            stroke="collections.abc.Iterable[_t.SampleLike]",
        )
    for name in ("hatch", "mass"):
        table.parameters(
            BRUSH + "." + name, polygon="collections.abc.Iterable[_t.PointLike]"
        )
    for name in ("draw", "fill", "hatch", "mass", "show", "wash"):
        table.parameters(BRUSH + ".Polygon." + name, engine="Engine")
        table.parameters(BRUSH + ".Plot." + name, engine="Engine")
    table.attribute(
        BRUSH + ".Tool.customTip",
        "collections.abc.Callable[[], None] | _t.DrawCallback | collections.abc.Callable[[_sigil.draw.Pen, Dab], None] | None",
    )
