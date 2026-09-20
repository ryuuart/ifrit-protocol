from collections.abc import Callable, Sequence
from typing import TypeAlias, TypedDict, TypeVar

from _sigil._types import AlignLike, DimensionLike, FillLike, SizeLike, SurfacePaintLike
from _sigil.compose import Element
from _sigil.compose.kit import (
    Caption,
    CaptionWhere,
    LineCompanion,
    Panel,
    Sheet,
    Well,
    WellContent,
)

_Props = TypeVar("_Props")
Part: TypeAlias = (
    Callable[[], Element]
    | Callable[[str], Element]
    | Callable[[str, _Props], Element]
    | None
)

class WellProperties(TypedDict, total=False):
    width: DimensionLike
    height: DimensionLike
    ground: SurfacePaintLike
    padding: float
    padding_y: float | None
    paddingY: float | None
    clip: bool
    corners: float
    keyline: FillLike | None
    keyline_width: float
    keylineWidth: float
    placed: bool
    content: WellContent | None

class CaptionProperties(TypedDict, total=False):
    where: CaptionWhere
    gap: float
    note_gap: float
    noteGap: float
    label_measure: float
    labelMeasure: float
    note_measure: float
    noteMeasure: float
    align: AlignLike
    justify: AlignLike
    reading: str
    reading_line: Part[Caption]
    readingLine: Part[Caption]

class CellsProperties(TypedDict, total=False):
    cells: Sequence[Element]
    column: bool
    gap: float
    divider: FillLike
    divider_width: float
    dividerWidth: float
    align: AlignLike

class PanelGridProperties(TypedDict, total=False):
    cells: Sequence[Element]
    columns: int
    gap: float
    row_gap: float | None
    rowGap: float | None
    divider: FillLike
    divider_width: float
    dividerWidth: float
    align: AlignLike
    measure: DimensionLike

class SheetProperties(TypedDict, total=False):
    title: str
    subtitle: str
    footer: str
    margin_x: float
    marginX: float
    margin_top: float
    marginTop: float
    margin_bottom: float
    marginBottom: float
    subtitle_gap: float
    subtitleGap: float
    content_gap: float
    contentGap: float
    ground: SurfacePaintLike
    rule: FillLike
    rule_width: float
    ruleWidth: float
    key: str
    title_line: Part[Sheet]
    titleLine: Part[Sheet]
    subtitle_line: Part[Sheet]
    subtitleLine: Part[Sheet]
    footer_line: Part[Sheet]
    footerLine: Part[Sheet]

class BoardProperties(TypedDict, total=False):
    size: SizeLike
    ground: SurfacePaintLike

class PanelProperties(TypedDict, total=False):
    eyebrow: str
    title: str
    note: str
    rule: FillLike
    rule_width: float
    ruleWidth: float
    gap: float
    title_gap: float
    titleGap: float
    body: Well | None
    eyebrow_line: Part[Panel]
    eyebrowLine: Part[Panel]
    title_line: Part[Panel]
    titleLine: Part[Panel]
    note_line: Part[Panel]
    noteLine: Part[Panel]

class LineProperties(TypedDict, total=False):
    length: DimensionLike
    thickness: float
    column: bool
    fill: SurfacePaintLike
    inset: float
    pair: LineCompanion | None

class LadderProperties(TypedDict, total=False):
    count: int
    pitch: float
    thickness: float
    column: bool
    fill: SurfacePaintLike
