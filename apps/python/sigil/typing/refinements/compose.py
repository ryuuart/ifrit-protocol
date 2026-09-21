"""Conversion seams of the retained element tree and its factories."""

from __future__ import annotations

from .table import Table

ELEMENT = "_sigil.compose.Element"


def register(table: Table) -> None:
    table.erased("_sigil.compose.TextPath", "path", "_t.ShapeLike")
    table.erased("_sigil.compose.TextPath", "at", "_t.ScalarLike")
    for factory in ("box", "stack", "positioned"):
        table.declares(
            "_sigil.compose",
            factory,
            f"""@typing.overload
def {factory}(children: collections.abc.Iterable[Element], /) -> Element: ...
@typing.overload
def {factory}(*children: Element) -> Element: ...
""",
        )
    table.declares(
        "_sigil.compose",
        "layout",
        """@typing.overload
def layout(scheme: layouts.Grid | layouts.Radial | layouts.Diagonal | layouts.BaselineGrid | layouts.Jittered | layouts.AlongPath, *children: Element) -> Element: ...
@typing.overload
def layout(scheme: layouts.Grid | layouts.Radial | layouts.Diagonal | layouts.BaselineGrid | layouts.Jittered | layouts.AlongPath, children: collections.abc.Iterable[Element], /) -> Element: ...
""",
    )
    table.declares(
        ELEMENT,
        "varDefaults",
        "def varDefaults(self, defaults: dict[str, _t.DimensionLike | _t.ColorLike]) -> Element: ...",
    )
    table.declares(
        ELEMENT,
        "children",
        """@typing.overload
def children(self, children: collections.abc.Iterable[Element], /) -> Element: ...
@typing.overload
def children(self, *children: Element) -> Element: ...
""",
    )
    table.erased(
        ELEMENT,
        "width height minWidth minHeight maxWidth maxHeight flexBasis left top right bottom gap",
        "_t.DimensionLike",
    )
    table.erased(ELEMENT, "size", "_t.DimensionLike", "_t.DimensionLike")
    table.declares(
        ELEMENT,
        "inset",
        """@typing.overload
def inset(self, all: _t.DimensionLike) -> Element: ...
@typing.overload
def inset(self, left: _t.DimensionLike, top: _t.DimensionLike, right: _t.DimensionLike, bottom: _t.DimensionLike) -> Element: ...
@typing.overload
def inset(self, *, top: _t.DimensionLike | None = ..., right: _t.DimensionLike | None = ..., bottom: _t.DimensionLike | None = ..., left: _t.DimensionLike | None = ...) -> Element: ...
""",
    )
    table.erased(
        ELEMENT,
        "opacity rotate rotateX rotateY scale scaleX scaleY scaleZ skewX skewY translateX translateY translateZ perspective",
        "_t.ScalarLike",
    )
    table.erased(ELEMENT, "fontSize fontTrack", "_t.FloatLike | _sigil.weave.Length")
    table.erased(ELEMENT, "fill", "_t.SurfacePaintLike")
    table.erased(ELEMENT, "ink", "_t.ElementInkLike")
    table.erased(ELEMENT, "alignItems alignSelf", "_t.AlignLike")
    table.erased(ELEMENT, "justifyContent", "_t.JustifyLike")
    table.erased(ELEMENT, "gridCellAlign", "_t.AlignLike", "_t.AlignLike")
    table.erased(ELEMENT, "centerAt", "_t.PointLike")
    # A point in hand, in pixels, or the two lengths in any unit; a rect in
    # hand, or its four lengths.
    table.declares(
        ELEMENT,
        "at",
        """@typing.overload
def at(self, point: _t.PointLike) -> Element: ...
@typing.overload
def at(self, x: _t.DimensionLike, y: _t.DimensionLike) -> Element: ...
""",
    )
    table.declares(
        ELEMENT,
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
        ELEMENT,
        "transformOrigin",
        f"def transformOrigin(self, x: {origin}, y: {origin}, z: {origin} | None = None) -> Element: ...",
    )
    table.declares(
        ELEMENT,
        "perspectiveOrigin",
        f"def perspectiveOrigin(self, x: {origin}, y: {origin}) -> Element: ...",
    )
    table.erased(ELEMENT, "imageRegion", "_t.RectLike")
    table.erased(ELEMENT, "shape", "_t.ShapeLike")
    table.erased(ELEMENT, "background foreground overlay stroke", "_t.DecorationLike")
    table.erased(ELEMENT, "textFill", "_t.SurfacePaintLike")
    # The glyph OUTLINE is one comparable Fill on the node, measured with no
    # frame in hand, so it takes the flat-mark set and not the surface one.
    table.erased(ELEMENT, "textStroke", "_t.FillLike")
    table.erased("_sigil.compose.LayerStyle", "echo", "_t.PointLike", "_t.ColorLike")
    table.erased(ELEMENT, "var", "_t.DimensionLike | _t.ColorLike")
    # Three arities, each with its own names, and each name usable as a
    # keyword; beside them the named-sides form, which takes any subset.
    for edge in ("padding", "margin"):
        table.declares(
            ELEMENT,
            edge,
            f"""@typing.overload
def {edge}(self, all: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, horizontal: _t.DimensionLike, vertical: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, left: _t.DimensionLike, top: _t.DimensionLike, right: _t.DimensionLike, bottom: _t.DimensionLike) -> Element: ...
@typing.overload
def {edge}(self, *, top: _t.DimensionLike | None = ..., right: _t.DimensionLike | None = ..., bottom: _t.DimensionLike | None = ..., left: _t.DimensionLike | None = ...) -> Element: ...
""",
        )
    table.erased(
        ELEMENT,
        "paddingTop paddingRight paddingBottom paddingLeft "
        "marginTop marginRight marginBottom marginLeft",
        "_t.DimensionLike",
    )
    table.erased("_sigil.compose.Composer", "hitTest", "_t.PointLike")
    table.erased("_sigil.compose.Decoration", "__init__", "_t.DecorationLike")
    table.erased("_sigil.compose.Dimension", "__init__", "_t.DimensionLike")
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
        "def memo[Model](properties: Model, describe: collections.abc.Callable[[Model], Element]) -> Element: ...",
    )
    table.erased("_sigil.compose.spans", "upTo", "_t.ScalarLike")
    table.erased("_sigil.compose.spans", "range wrap", "_t.ScalarLike", "_t.ScalarLike")
