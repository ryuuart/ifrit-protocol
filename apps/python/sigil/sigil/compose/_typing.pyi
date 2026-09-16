from collections.abc import Iterable
from typing import Literal, TypeAlias, TypedDict

from _sigil._types import (
    ColorLike,
    DimensionLike,
    PaintLike,
    PointLike,
    RectLike,
    ScalarLike,
    ShapeLike,
)
from _sigil.compose import (
    Align,
    Backface,
    Boundary,
    Cache,
    Corners,
    Element,
    Justify,
    LayerStyle,
    MotionPath,
    VarRef,
)
from _sigil.compose.layouts import (
    AlongPath,
    BaselineGrid,
    Diagonal,
    Grid,
    Jittered,
    Radial,
)
from _sigil.material.skia import Effect
from _sigil.motion import Transition
from _sigil.skia import BlendMode, SamplingOptions
from _sigil.weave import Block, Length, StyleSheet, Type

Child: TypeAlias = Element | str | None | Iterable[Child]
Layout: TypeAlias = AlongPath | BaselineGrid | Diagonal | Grid | Jittered | Radial
Alignment: TypeAlias = (
    Align | Literal["auto", "start", "center", "end", "stretch", "baseline"]
)
ItemsAlignment: TypeAlias = (
    Align | Literal["start", "center", "end", "stretch", "baseline"]
)
Justification: TypeAlias = (
    Justify
    | Literal["start", "center", "end", "space_between", "space_around", "space_evenly"]
)
Edges: TypeAlias = (
    DimensionLike
    | tuple[DimensionLike]
    | tuple[DimensionLike, DimensionLike]
    | tuple[DimensionLike, DimensionLike, DimensionLike, DimensionLike]
    | list[DimensionLike]
)
Inset: TypeAlias = (
    float
    | tuple[float]
    | tuple[DimensionLike, DimensionLike, DimensionLike, DimensionLike]
    | list[DimensionLike]
)
Radii: TypeAlias = (
    Corners | float | tuple[float] | tuple[float, float, float, float] | list[float]
)
Origin: TypeAlias = tuple[float, float] | list[float]

class StyleProperties(TypedDict, total=False):
    width: DimensionLike
    height: DimensionLike
    gap: DimensionLike
    padding: Edges
    margin: Edges
    corners: Radii
    fill: PaintLike
    ink: ColorLike | VarRef
    grow: float
    shrink: float
    inset: Inset
    left: DimensionLike
    top: DimensionLike
    right: DimensionLike
    bottom: DimensionLike
    opacity: ScalarLike
    rotate: ScalarLike
    scale: ScalarLike
    justify: Justification
    aspect: float
    basis: DimensionLike
    font: Type
    block: Block
    shape: ShapeLike
    clip: bool
    cache: Cache
    boundary: Boundary
    threshold: float
    style: LayerStyle
    effect: Effect
    backdrop: Effect
    appear: Transition
    blend: BlendMode
    travel: MotionPath
    area: str
    rect: RectLike
    at: PointLike
    thread: str
    region: RectLike
    ellipsis: str
    sampling: SamplingOptions
    scale_x: ScalarLike
    scale_y: ScalarLike
    scale_z: ScalarLike
    translate_x: ScalarLike
    translate_y: ScalarLike
    translate_z: ScalarLike
    rotate_x: ScalarLike
    rotate_y: ScalarLike
    rotate_z: ScalarLike
    skew_x: ScalarLike
    skew_y: ScalarLike
    font_size: float | Length
    font_weight: float
    font_track: float | Length
    align_items: ItemsAlignment
    align_self: Alignment
    min_width: DimensionLike
    min_height: DimensionLike
    max_width: DimensionLike
    max_height: DimensionLike
    wrap_lines: bool
    style_sheet: StyleSheet
    style_class: str
    center_at: PointLike
    hit_testable: bool
    z_index: int
    bake_scale: float
    max_lines: int
    preserve_3d: bool
    backface: Backface
    perspective: ScalarLike
    transform_origin: Origin
    perspective_origin: Origin
    absolute: bool

class ElementProperties(StyleProperties, total=False):
    key: str
