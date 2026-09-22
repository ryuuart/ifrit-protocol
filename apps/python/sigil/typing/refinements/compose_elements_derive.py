"""Routers, anchors, tethers and bands.

Input contracts for the erased signatures of the
compose-elements/derive package, and nothing else: a fragment is one
author's alone.

The routers state themselves: a route function and a run-route function
are written into the signatures by the binding, under the names the skia
classes register. What is left is every point, rect and size read through
a shared conversion.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.compose"
ANCHOR = MODULE + ".Anchor"
TETHER = MODULE + ".Tether"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A router asked directly reads its rects and its anchor run the way
    # every other rect and point is read.
    table.parameters(MODULE + ".Router.route", from_="_t.RectLike", to="_t.RectLike")
    table.parameters(
        MODULE + ".RailRouter.route",
        anchors="collections.abc.Iterable[_t.PointLike]",
    )
    # The bound form names its normalized point and the free form its
    # point; the default constructor has nothing erased.
    table.parameters(ANCHOR + ".__init__", norm="_t.PointLike")
    table.parameters(ANCHOR + ".on", norm="_t.PointLike")
    table.parameters(ANCHOR + ".at", point="_t.PointLike")
    # A property's setter is the one erased argument under its name; the
    # getter beside it has none, so the same row leaves it alone.
    table.erased(ANCHOR + ".OnNode", "norm", "_t.PointLike")
    table.erased(ANCHOR + ".FreePoint", "point", "_t.PointLike")
    table.erased(TETHER, "on at offset", "_t.PointLike")
    table.erased(TETHER, "within", "_t.RectLike")
    table.parameters(TETHER + ".place", anchor="_t.RectLike", size="_t.SizeLike")
