"""The plot frame and every chart layer.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/kit-charts package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    kit = "_sigil.sketch.kit"
    # A tick line is offered the value and then the ruler, and answers the
    # element set at that tick.
    tick = (
        "collections.abc.Callable[[], _t.NodeLike]"
        " | collections.abc.Callable[[float], _t.NodeLike]"
        " | collections.abc.Callable[[float, Ruler], _t.NodeLike] | None"
    )
    table.parameters(f"{kit}.Ruler.__init__", tickLine=tick)
    table.accessor(
        f"{kit}.Ruler",
        "tickLine",
        "collections.abc.Callable[[float, Ruler], _sigil.compose.Element] | None",
        tick,
    )
    # A run of rows is any iterable, and each reader and mark is a function
    # of one row; the row's type ties the two together.
    reader = "collections.abc.Callable[[Row], typing.SupportsFloat] | None"
    mark = (
        "collections.abc.Callable[[], _t.NodeLike]"
        " | collections.abc.Callable[[Row], _t.NodeLike]"
        " | collections.abc.Callable[[Row, int], _t.NodeLike] | None"
    )
    for member in ("marks", "segments", "bands"):
        table.generic(f"{kit}.{member}", "Row")
    table.parameters(
        f"{kit}.marks",
        rows="collections.abc.Iterable[Row]",
        mark=mark,
        x=reader,
        y=reader,
    )
    table.parameters(
        f"{kit}.segments",
        rows="collections.abc.Iterable[Row]",
        mark=mark,
        x=reader,
        y=reader,
        toX=reader,
        toY=reader,
    )
    table.parameters(
        f"{kit}.bands",
        rows="collections.abc.Iterable[Row]",
        x=reader,
        y=reader,
        part=(
            "collections.abc.Callable[[], _t.NodeLike]"
            " | collections.abc.Callable[[int], _t.NodeLike]"
            " | collections.abc.Callable[[int, float], _t.NodeLike] | None"
        ),
    )
    # A layer is a native one or a function of the frame, the key and the
    # index, answering the element that draws it.
    table.parameters(
        f"{kit}.plot",
        layers=(
            "collections.abc.Iterable[Layer"
            " | collections.abc.Callable[[], _t.NodeLike]"
            " | collections.abc.Callable[[Plot], _t.NodeLike]"
            " | collections.abc.Callable[[Plot, str], _t.NodeLike]"
            " | collections.abc.Callable[[Plot, str, int], _t.NodeLike]]"
        ),
    )
    table.parameters(f"{kit}.Plot.at", size="_t.SizeLike")
    table.parameters(f"{kit}.Plot.scale", size="_t.SizeLike")
    table.returns(f"{kit}.Plot", "at", "tuple[float, float]")
    table.parameters(
        f"{kit}.trace",
        curve=(
            "collections.abc.Callable[[float], typing.SupportsFloat]"
            " | collections.abc.Sequence[typing.SupportsFloat]"
        ),
    )
    table.parameters(
        f"{kit}.area", curve="collections.abc.Callable[[float], typing.SupportsFloat]"
    )
    table.parameters(
        f"{kit}.path",
        at="collections.abc.Callable[[float], Datum | tuple[float, float]]",
    )
