from collections.abc import Buffer, Callable, Iterable, Sequence
from typing import Literal, Protocol, SupportsFloat, SupportsIndex, TypeAlias

from _sigil import compose, data, draw, material, motion, skia, weave

__all__ = [
    "AlignLike",
    "CellInput",
    "CellValue",
    "ColorLike",
    "DecorationLike",
    "DimensionLike",
    "DirectionLike",
    "DrawCallback",
    "EaseLike",
    "ElementInkLike",
    "FillLike",
    "FloatLike",
    "GradientStops",
    "JsonInput",
    "JsonValue",
    "JustifyLike",
    "PointBatch",
    "PointLike",
    "RectLike",
    "SampleLike",
    "ScalarFunction",
    "ScalarLike",
    "ShapeFunction",
    "ShapeLike",
    "SilhouetteLike",
    "SizeLike",
    "SurfacePaintLike",
    "TickCallback",
    "UniformValue",
    "Vec2",
    "Vec2Like",
    "Vec3",
    "Vec3Like",
    "Vec4",
    "Vec4Like",
]

FloatLike: TypeAlias = SupportsFloat | SupportsIndex
ColorLike: TypeAlias = (
    material.Color
    | str
    | tuple[FloatLike, FloatLike, FloatLike]
    | tuple[FloatLike, FloatLike, FloatLike, FloatLike]
    | list[FloatLike]
)
"""A FLAT COLOUR VALUE. The colour class, a CSS string, or three or four
unit channels. A slot that takes it promises only to read a colour: it
resolves nothing from the tree and follows no binding, so what it is
given is what it paints. Colours read back out of the libraries are the
colour class, so a colour taken off one value passes into the next."""
PointLike: TypeAlias = skia.Point | Sequence[FloatLike]
RectLike: TypeAlias = skia.Rect | Sequence[FloatLike]
SizeLike: TypeAlias = skia.Size | Sequence[FloatLike]
ScalarLike: TypeAlias = (
    FloatLike | motion.Animatable | motion.Transitioned | motion.Output | motion.Bound
)
DimensionLike: TypeAlias = (
    FloatLike | str | compose.Dimension | weave.Length | compose.VarRef
)
FillLike: TypeAlias = (
    ColorLike | compose.Fill | compose.VarRef | material.skia.Paint | None
)
"""A FLAT MARK, which may be a reference the tree resolves. Everything a
colour is, plus a Fill, a custom-property reference, and None for no
mark at all. A slot that takes it stores one comparable fill, so a paint
is accepted only where it collapses to one: a solid or a static shader
passes and a live or geometry-dependent paint raises, naming the verb
that does take it."""
SurfacePaintLike: TypeAlias = (
    FillLike
    | compose.SurfacePaint
    | material.Material
    | motion.FillOutput
    | motion.FillTransitioned
    | motion.ColorTransitioned
)
"""ANYTHING THAT CAN COLOUR A SURFACE, and the widest of the three.
Everything a flat mark is, plus a paint of any tier, a recipe instance,
a bound fill and a fill transition. A slot that takes it resolves
against the frame it paints at, so a gradient measured on the node and a
material that reads the clock both belong in it."""
ElementInkLike: TypeAlias = ColorLike | compose.VarRef
"""THE INK AN ELEMENT SETS for itself and everything under it. A colour
or a custom-property reference, and deliberately nothing animatable: a
bound ink would make every inheriting node volatile, so an ink that has
to move is set on a fill that names it."""
AlignLike: TypeAlias = (
    compose.Align | Literal["auto", "start", "center", "end", "stretch", "baseline"]
)
JustifyLike: TypeAlias = (
    compose.Justify
    | Literal["start", "center", "end", "space_between", "space_around", "space_evenly"]
)
ShapeFunction: TypeAlias = Callable[[float, float], skia.Path]
ShapeLike: TypeAlias = compose.Shape | skia.Path | ShapeFunction

class SilhouetteLike(Protocol):
    """Anything that answers a path over a size: the pen calls
    ``path((width, height))`` and draws what comes back. The size is the
    pair the pen passes, not a wider size input, because a protocol's
    parameter is contravariant and a wider one would reject an author's
    own two-float signature."""

    def path(self, size: tuple[float, float]) -> skia.Path: ...

DecorationLike: TypeAlias = compose.Decoration | compose.PathFormat | compose.Shadow
PointBatch: TypeAlias = Buffer | Iterable[PointLike]
DrawCallback: TypeAlias = Callable[[draw.Pen], None]
ScalarFunction: TypeAlias = Callable[[float], float]
EaseLike: TypeAlias = motion.Easing | motion.Curve | ScalarFunction | None
GradientStops: TypeAlias = Iterable[tuple[FloatLike, ColorLike]]
UniformValue: TypeAlias = ScalarLike | material.Color | str | Sequence[FloatLike]
"""A NAMED UNIFORM on an SkSL paint or effect. A number, a live scalar
that makes the value re-read every frame, a colour written as the colour
class or a CSS string, or a flat array matched against the declared
uniform's total float count. A paint and an effect take the same set, so
one uniform is written the same way on either seam."""
CellValue: TypeAlias = None | bool | int | float | str | data.Instant
CellInput: TypeAlias = CellValue | data.Flag
JsonValue: TypeAlias = (
    None | bool | int | float | str | list[JsonValue] | dict[str, JsonValue]
)
JsonInput: TypeAlias = (
    None
    | bool
    | int
    | float
    | str
    | data.Json
    | list[JsonInput]
    | tuple[JsonInput, ...]
    | dict[str, JsonInput]
)
Vec2: TypeAlias = tuple[float, float]
Vec3: TypeAlias = tuple[float, float, float]
Vec4: TypeAlias = tuple[float, float, float, float]
Vec2Like: TypeAlias = Sequence[FloatLike]
Vec3Like: TypeAlias = Sequence[FloatLike]
Vec4Like: TypeAlias = Sequence[FloatLike]
SampleLike: TypeAlias = (
    draw.brush.Sample
    | PointLike
    | tuple[PointLike, FloatLike]
    | tuple[FloatLike, FloatLike, FloatLike]
)
DirectionLike: TypeAlias = (
    draw.brush.Direction
    | draw.brush.Curl
    | draw.brush.Vortex
    | draw.brush.Wave
    | Callable[[skia.Point, float], float]
    | None
)
TickCallback: TypeAlias = (
    Callable[[], bool | None]
    | Callable[[float], bool | None]
    | Callable[[float, float], bool | None]
)
