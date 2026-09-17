from collections.abc import Buffer, Callable, Iterable, Sequence
from typing import Literal, SupportsFloat, SupportsIndex, TypeAlias

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
    "InkLike",
    "JsonInput",
    "JsonValue",
    "JustifyLike",
    "MotionFillLike",
    "PaintLike",
    "PointBatch",
    "PointLike",
    "RectLike",
    "SampleLike",
    "ScalarFunction",
    "ScalarLike",
    "ShapeFunction",
    "ShapeLike",
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
    skia.Color
    | material.Color
    | str
    | tuple[FloatLike, FloatLike, FloatLike]
    | tuple[FloatLike, FloatLike, FloatLike, FloatLike]
    | list[FloatLike]
)
PointLike: TypeAlias = skia.Point | Sequence[FloatLike]
RectLike: TypeAlias = skia.Rect | Sequence[FloatLike]
SizeLike: TypeAlias = skia.Size | Sequence[FloatLike]
ScalarLike: TypeAlias = (
    FloatLike | motion.Animatable | motion.Transitioned | motion.Output | motion.Bound
)
DimensionLike: TypeAlias = (
    FloatLike | str | compose.Dimension | weave.Length | compose.VarRef
)
FillLike: TypeAlias = ColorLike | compose.Fill | compose.VarRef | None
MotionFillLike: TypeAlias = (
    FillLike | motion.FillTransitioned | motion.ColorTransitioned | motion.FillOutput
)
PaintLike: TypeAlias = MotionFillLike | material.skia.Paint | compose.SurfacePaint
SurfacePaintLike: TypeAlias = PaintLike | material.Material
InkLike: TypeAlias = ColorLike | motion.ColorTransitioned | motion.ColorOutput
ElementInkLike: TypeAlias = ColorLike | compose.VarRef
AlignLike: TypeAlias = (
    compose.Align | Literal["auto", "start", "center", "end", "stretch", "baseline"]
)
JustifyLike: TypeAlias = (
    compose.Justify
    | Literal["start", "center", "end", "space_between", "space_around", "space_evenly"]
)
ShapeFunction: TypeAlias = Callable[[float, float], skia.Path]
ShapeLike: TypeAlias = compose.Shape | skia.Path | ShapeFunction
DecorationLike: TypeAlias = compose.Decoration | compose.PathFormat | compose.Shadow
PointBatch: TypeAlias = Buffer | Iterable[PointLike]
DrawCallback: TypeAlias = Callable[[draw.Pen], None]
ScalarFunction: TypeAlias = Callable[[float], float]
EaseLike: TypeAlias = motion.Easing | motion.Curve | ScalarFunction | None
GradientStops: TypeAlias = Iterable[tuple[FloatLike, ColorLike]]
UniformValue: TypeAlias = int | float | skia.Color | Sequence[FloatLike]
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
