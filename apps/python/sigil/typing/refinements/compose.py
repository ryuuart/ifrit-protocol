"""Conversion seams of the retained element tree and its factories."""

from __future__ import annotations

from .table import Table

ELEMENT = "_sigil.compose.Element"
TEXT = "_sigil.compose.Text"
IMAGE = "_sigil.compose.Image"
# Every kind of node states the same verbs and hands ITSELF back, so the
# refinements are written once and registered for each of them with the
# return type its chain keeps.
NODES = (ELEMENT, TEXT, IMAGE, "_sigil.compose.Band")
# A rule states the declaring half of the same vocabulary, bound once for
# both, so its refinements are the same text over its own name.
RULE = "_sigil.compose.Rule"


def register(table: Table) -> None:
    for node in NODES:
        registerNode(table, node)
    registerDeclarations(_Returning(table, RULE), RULE)
    # A glyph outline is the text leaf's alone, and the region of a source
    # is the image leaf's.
    # The glyph OUTLINE is one comparable Fill on the node, measured with no
    # frame in hand, so it takes the flat-mark set and not the surface one.
    table.erased(TEXT, "textStroke", "_t.FillLike")
    # A rule states the text properties too, glyph outline included.
    table.erased(RULE, "textStroke", "_t.FillLike")
    table.erased(IMAGE, "imageRegion", "_t.RectLike")
    registerValues(table)


class _Returning:
    """The table, with every declaration's return type read as the node's.

    A verb hands back the value it was called on, so the one text these
    refinements are written in says `-> Element` and each node's own
    registration reads its own name there.
    """

    def __init__(self, table: Table, node: str) -> None:
        self._table = table
        self._node = node.rsplit(".", 1)[1]

    def erased(self, prefix: str, names: str, *types: str) -> None:
        self._table.erased(prefix, names, *types)

    def declares(self, prefix: str, name: str, text: str) -> None:
        self._table.declares(
            prefix, name, text.replace("-> Element:", f"-> {self._node}:")
        )


def registerNode(shared: Table, node: str) -> None:
    """The verbs every node states, over the node that states them.

    Every path written here names the node this call is registering, not
    the element: the table is keyed by path, so a path spelling one class
    would be written four times over and only the last registration would
    survive, leaving that class's verbs answering another class.
    """
    table = _Returning(shared, node)
    registerDeclarations(table, node)
    table.declares(
        node,
        "children",
        """@typing.overload
def children(self, children: collections.abc.Iterable[_t.NodeLike], /) -> Element: ...
@typing.overload
def children(self, *children: _t.NodeLike) -> Element: ...
""",
    )
    table.erased(
        node,
        "rotateX rotateY scaleZ translateZ perspective",
        "_t.ScalarLike",
    )
    # An origin takes a length that carries its unit, so the bare number every
    # other dimension accepts as pixels is left out of what these accept.
    origin = "str | Dimension | _sigil.weave.Length | VarRef"
    table.declares(
        node,
        "perspectiveOrigin",
        f"def perspectiveOrigin(self, x: {origin}, y: {origin}) -> Element: ...",
    )
    table.erased(node, "shape", "_t.ShapeLike")
    table.erased(node, "background foreground overlay stroke", "_t.DecorationLike")


def registerDeclarations(table: _Returning, node: str) -> None:
    """The verbs a rule states as well as a node, over the value that states them."""
    table.declares(
        node,
        "varDefaults",
        "def varDefaults(self, defaults: dict[str, _t.DimensionLike | _t.ColorLike]) -> Element: ...",
    )
    table.erased(
        node,
        "width height minWidth minHeight maxWidth maxHeight flexBasis left top right bottom gap",
        "_t.DimensionLike",
    )
    table.erased(node, "size", "_t.DimensionLike", "_t.DimensionLike")
    table.declares(
        node,
        "inset",
        """@typing.overload
def inset(self, all: _t.DimensionLike) -> Element: ...
@typing.overload
def inset(self, vertical: _t.DimensionLike, horizontal: _t.DimensionLike) -> Element: ...
@typing.overload
def inset(self, top: _t.DimensionLike, horizontal: _t.DimensionLike, bottom: _t.DimensionLike) -> Element: ...
@typing.overload
def inset(self, top: _t.DimensionLike, right: _t.DimensionLike, bottom: _t.DimensionLike, left: _t.DimensionLike) -> Element: ...
@typing.overload
def inset(self, *, top: _t.DimensionLike | None = ..., right: _t.DimensionLike | None = ..., bottom: _t.DimensionLike | None = ..., left: _t.DimensionLike | None = ...) -> Element: ...
""",
    )
    table.erased(
        node,
        "opacity rotate scale scaleX scaleY skewX skewY translateX translateY",
        "_t.ScalarLike",
    )
    table.erased(node, "fontSize letterSpacing", "_t.FloatLike | _sigil.weave.Length")
    table.erased(node, "fill", "_t.SurfacePaintLike")
    table.erased(node, "ink", "_t.ElementInkLike")
    table.erased(node, "alignItems alignSelf", "_t.AlignLike")
    table.erased(node, "justifyContent", "_t.JustifyLike")
    table.erased(node, "gridCellAlign", "_t.AlignLike", "_t.AlignLike")
    table.erased(node, "centerAt", "_t.PointLike")
    # A point in hand, in pixels, or the two lengths in any unit; a rect in
    # hand, or its four lengths.
    table.declares(
        node,
        "at",
        """@typing.overload
def at(self, point: _t.PointLike) -> Element: ...
@typing.overload
def at(self, x: _t.DimensionLike, y: _t.DimensionLike) -> Element: ...
""",
    )
    table.declares(
        node,
        "rect",
        """@typing.overload
def rect(self, rect: _t.RectLike) -> Element: ...
@typing.overload
def rect(self, x: _t.DimensionLike, y: _t.DimensionLike, width: _t.DimensionLike, height: _t.DimensionLike) -> Element: ...
""",
    )
    # An origin takes a length that carries its unit, so the bare number every
    # other dimension accepts as pixels is left out of what these accept.
    origin = "str | Dimension | _sigil.weave.Length | VarRef"
    table.declares(
        node,
        "transformOrigin",
        f"def transformOrigin(self, x: {origin}, y: {origin}, z: {origin} | None = None) -> Element: ...",
    )
    table.erased(node, "var", "_t.DimensionLike | _t.ColorLike")
    # Four arities in CSS's order, each with its own names, and each name
    # usable as a keyword; beside them the named-sides form, any subset.
    for edge in ("padding", "margin"):
        table.declares(
            node,
            edge,
            f"""@typing.overload
def {edge}(self, all: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, vertical: _t.DimensionLike, horizontal: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, top: _t.DimensionLike, horizontal: _t.DimensionLike, bottom: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, top: _t.DimensionLike, right: _t.DimensionLike, bottom: _t.DimensionLike, left: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, *, top: _t.DimensionLike | None = ..., right: _t.DimensionLike | None = ..., bottom: _t.DimensionLike | None = ..., left: _t.DimensionLike | None = ...) -> Element: ...
""",
        )
    table.erased(
        node,
        "paddingTop paddingRight paddingBottom paddingLeft "
        "marginTop marginRight marginBottom marginLeft",
        "_t.DimensionLike",
    )


def registerValues(table: Table) -> None:
    """The refinements that speak about a value rather than a node."""
    table.erased("_sigil.compose.TextPath", "path", "_t.ShapeLike")
    table.erased("_sigil.compose.TextPath", "at", "_t.ScalarLike")
    for factory in ("box", "stack", "positioned"):
        table.declares(
            "_sigil.compose",
            factory,
            f"""@typing.overload
def {factory}(children: collections.abc.Iterable[_t.NodeLike], /) -> Element: ...
@typing.overload
def {factory}(*children: _t.NodeLike) -> Element: ...
""",
        )
    # Each stock scheme has a typed overload of its own and one more
    # takes everything else an operator is built from, which is the set
    # the alias names — the stock schemes among them, so these two
    # spellings state the whole verb.
    table.declares(
        "_sigil.compose",
        "layout",
        """@typing.overload
def layout(scheme: _t.OperatorLike, *children: _t.NodeLike) -> Element: ...
@typing.overload
def layout(scheme: _t.OperatorLike, children: collections.abc.Iterable[_t.NodeLike], /) -> Element: ...
""",
    )
    table.erased("_sigil.compose.LayerStyle", "echo", "_t.PointLike", "_t.ColorLike")
    table.erased("_sigil.compose.Composer", "hitTest", "_t.PointLike")
    table.erased("_sigil.compose.Decoration", "__init__", "_t.DecorationLike")
    table.erased("_sigil.compose.Dimension", "__init__", "_t.DimensionLike")
    # A length is added to anything a length is written as.
    table.erased(
        "_sigil.compose.Dimension", "__add__ __radd__ __sub__ __rsub__",
        "_t.DimensionLike",
    )
    table.erased("_sigil.compose.Fill", "color", "_t.ColorLike")
    table.erased("_sigil.compose.Fill", "__init__", "_t.FillLike")
    table.erased(
        "_sigil.compose.MotionPath", "__init__", "_t.ShapeLike", "_t.ScalarLike"
    )
    table.erased("_sigil.compose.MotionPath", "t", "_t.ScalarLike")
    table.erased(
        "_sigil.compose.PathFormat",
        "dashPhaseBinding trimPhase",
        "_t.ScalarLike | None",
    )
    table.erased("_sigil.compose.PathFormat", "strokeFill", "_t.SurfacePaintLike")
    table.erased("_sigil.compose.Shadow", "color", "_t.ColorLike")
    table.erased("_sigil.compose.Shadow", "offset", "_t.PointLike")
    table.erased("_sigil.compose.Shape", "__init__", "_t.ShapeLike")
    table.erased("_sigil.compose.SurfacePaint", "__init__", "_t.SurfacePaintLike")
    table.erased("_sigil.compose", "shadow", "_t.ColorLike", "_t.PointLike")
    table.erased("_sigil.compose", "shape", "_t.ShapeLike")
    # A band's spine is any shape a node takes; the leaf it hands back
    # keeps the band's own verb in reach.
    table.declares(
        "_sigil.compose",
        "band",
        "def band(spine: _t.ShapeLike, width: Across) -> Band: ...\n",
    )
    table.erased("_sigil.compose", "stroke", "_t.SurfacePaintLike")
    table.erased(
        "_sigil.compose",
        "text",
        "_t.FloatLike | _sigil.weave.Length | None",
        "_t.ColorLike | None",
    )
    table.declares(
        "_sigil.compose",
        "memo",
        "def memo[Model](properties: Model, describe: collections.abc.Callable[[Model], _t.NodeLike]) -> Element: ...",
    )
    table.erased("_sigil.compose.spans", "upTo", "_t.ScalarLike")
    table.erased("_sigil.compose.spans", "range wrap", "_t.ScalarLike", "_t.ScalarLike")
