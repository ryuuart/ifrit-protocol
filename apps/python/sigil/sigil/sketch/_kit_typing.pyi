from collections.abc import Sequence
from typing import TypedDict

from _sigil._types import (
    AlignLike,
    ColorLike,
    DimensionLike,
    FillLike,
    SizeLike,
    SurfacePaintLike,
)
from _sigil.compose import Element
from _sigil.sketch.kit import Recess, Relief, Well, WellContent

class StageProperties(TypedDict, total=False):
    size: SizeLike
    capture_at: float
    captureAt: float
    background: ColorLike | None
    oversample: int
    plate_only: bool
    plateOnly: bool
    nonlinear_picture: bool
    nonlinearPicture: bool

class PageProperties(TypedDict, total=False):
    title: str
    subtitle: str
    footer: str
    ruled: bool
    ground: SurfacePaintLike | None
    key: str

class WellProperties(TypedDict, total=False):
    width: DimensionLike
    height: DimensionLike
    ground: SurfacePaintLike | None
    padding: float | None
    padding_y: float | None
    paddingY: float | None
    clip: bool
    corners: float | None
    keyline: FillLike | None
    keyline_width: float
    keylineWidth: float
    placed: bool
    content: WellContent | None
    recess: Recess | None
    relief: Relief | None

class CellProperties(TypedDict, total=False):
    plate: Well
    measure: float | None

class RunProperties(TypedDict, total=False):
    cells: Sequence[Element]
    column: bool
    gap: float | None
    ruled: bool
    align: AlignLike

class PanelGridProperties(TypedDict, total=False):
    cells: Sequence[Element]
    columns: int
    gap: float | None
    row_gap: float | None
    rowGap: float | None
    ruled: bool
    align: AlignLike
    measure: DimensionLike
