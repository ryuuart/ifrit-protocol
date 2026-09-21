"""Routers, anchors, tethers, connectors, rails and bands.

Input contracts for the erased signatures of the
compose-elements/derive package, and nothing else: a fragment is one
author's alone.

The routers state themselves: a route function, a rail function and the
union a connector or a rail takes are written into the signatures by the
binding, under the names the skia classes register. What is left is every
point, rect and size read through a shared conversion, and the anchors of
a rail, which are read one by one in the three forms a native initializer
list spells.
"""

from __future__ import annotations

from .table import Table

MODULE = "_sigil.compose"
DERIVE = MODULE + ".derive"
ANCHOR = MODULE + ".Anchor"
TETHER = MODULE + ".Tether"

# One anchor of a rail: the value itself, a node's key for that node's
# centre, or the key with its normalized point and, after it, its gap.
ANCHOR_LIKE = (
    "_sigil.compose.Anchor"
    " | builtins.str"
    " | tuple[builtins.str, _t.PointLike]"
    " | tuple[builtins.str, _t.PointLike, _t.FloatLike]"
)


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
    table.parameters(
        MODULE + ".rail",
        anchors=f"collections.abc.Iterable[{ANCHOR_LIKE}]",
    )
    # The family's module holds three of the compose functions themselves
    # beside the one verb it defines. A generated module declares only what
    # it defines, so the three are declared here as the functions they are,
    # after the verb, whose signature is restated because a declaration is
    # replaced whole.
    table.declares(
        DERIVE,
        "contentFlowAround",
        f"def contentFlowAround(text: {MODULE}.Text, key: str, "
        f"margin: typing.SupportsFloat = 0.0) -> {MODULE}.Text:\n"
        '    """A copy of `text` whose lines flow around the keyed node, as\n'
        '    Text.contentFlowAround sets on the leaf itself."""\n'
        f"around = {MODULE}.around\n"
        f"connector = {MODULE}.connector\n"
        f"rail = {MODULE}.rail\n",
    )
