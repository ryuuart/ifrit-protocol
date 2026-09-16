from collections.abc import Callable
from typing import TypeVar, Unpack

from _sigil._types import ColorLike
from _sigil.compose import (
    Align,
    Backface,
    Boundary,
    Cache,
    CellSpan,
    Composer,
    ComposerStats,
    Corners,
    Decoration,
    Dimension,
    Element,
    Fill,
    Fit,
    Justify,
    LayerStyle,
    MotionPath,
    PathFormat,
    Shadow,
    Shape,
    Spans,
    SurfacePaint,
    TextSettling,
    VarRef,
    autoDimension,
    heldPath,
    image,
    pathFigure,
    pct,
    pen,
    ph,
    picture,
    pw,
    shadow,
    shape,
    slot,
    spans,
    stroke,
    var,
)
from _sigil.weave import Length

from ..sketch._typing import Paint as _Paint
from . import kit, layouts
from ._typing import (
    Child as _Child,
)
from ._typing import (
    ElementProperties as _ElementProperties,
)
from ._typing import (
    Layout as _Layout,
)
from ._typing import (
    StyleProperties as _StyleProperties,
)

_Model = TypeVar("_Model")

def box(*children: _Child, **properties: Unpack[_ElementProperties]) -> Element: ...
def row(*children: _Child, **properties: Unpack[_ElementProperties]) -> Element: ...
def column(*children: _Child, **properties: Unpack[_ElementProperties]) -> Element: ...
def text(
    value: str,
    *,
    size: float | Length | None = ...,
    color: ColorLike | None = ...,
    **properties: Unpack[_ElementProperties],
) -> Element: ...
def graphics(
    program: _Paint,
    *,
    key: str,
    **properties: Unpack[_StyleProperties],
) -> Element: ...
def memo(
    properties: _Model,
    describe: Callable[[_Model], Element],
    *,
    key: str | None = ...,
) -> Element: ...
def stack(*children: _Child, **properties: Unpack[_ElementProperties]) -> Element: ...
def positioned(
    *children: _Child, **properties: Unpack[_ElementProperties]
) -> Element: ...
def layout(
    scheme: _Layout,
    *children: _Child,
    **properties: Unpack[_ElementProperties],
) -> Element: ...

__all__ = [
    "Align",
    "Backface",
    "Boundary",
    "Cache",
    "CellSpan",
    "Composer",
    "ComposerStats",
    "Corners",
    "Decoration",
    "Dimension",
    "Element",
    "Fill",
    "Fit",
    "Justify",
    "LayerStyle",
    "MotionPath",
    "PathFormat",
    "Shadow",
    "Shape",
    "Spans",
    "SurfacePaint",
    "TextSettling",
    "VarRef",
    "autoDimension",
    "box",
    "column",
    "graphics",
    "heldPath",
    "image",
    "kit",
    "layout",
    "layouts",
    "memo",
    "pathFigure",
    "pct",
    "pen",
    "ph",
    "picture",
    "positioned",
    "pw",
    "row",
    "shadow",
    "shape",
    "slot",
    "spans",
    "stack",
    "stroke",
    "text",
    "var",
]
