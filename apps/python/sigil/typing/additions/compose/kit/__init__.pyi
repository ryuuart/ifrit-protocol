import collections.abc
import typing

import sigil._types as _t
import sigil.compose

_Props = typing.TypeVar("_Props")
_Part: typing.TypeAlias = (
    collections.abc.Callable[[], _t.NodeLike]
    | collections.abc.Callable[[str], _t.NodeLike]
    | collections.abc.Callable[[str, _Props], _t.NodeLike]
    | None
)

class _WellProperties(typing.TypedDict, total=False):
    width: _t.DimensionLike
    height: _t.DimensionLike
    ground: _t.SurfacePaintLike
    padding: float
    padding_y: float | None
    paddingY: float | None
    clip: bool
    corners: float
    keyline: _t.FillLike | None
    keyline_width: float
    keylineWidth: float
    placed: bool
    content: WellContent | None

class _CaptionProperties(typing.TypedDict, total=False):
    where: CaptionWhere
    gap: float
    note_gap: float
    noteGap: float
    label_measure: float
    labelMeasure: float
    note_measure: float
    noteMeasure: float
    align: _t.AlignLike
    justify: _t.AlignLike
    reading: str
    reading_line: _Part[Caption]
    readingLine: _Part[Caption]

class _CellsProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[_t.NodeLike]
    column: bool
    gap: float
    divider: _t.FillLike
    divider_width: float
    dividerWidth: float
    align: _t.AlignLike

class _PanelGridProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[_t.NodeLike]
    columns: int
    gap: float
    row_gap: float | None
    rowGap: float | None
    divider: _t.FillLike
    divider_width: float
    dividerWidth: float
    align: _t.AlignLike
    measure: _t.DimensionLike

class _SheetProperties(typing.TypedDict, total=False):
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
    ground: _t.SurfacePaintLike
    rule: _t.FillLike
    rule_width: float
    ruleWidth: float
    key: str
    title_line: _Part[Sheet]
    titleLine: _Part[Sheet]
    subtitle_line: _Part[Sheet]
    subtitleLine: _Part[Sheet]
    footer_line: _Part[Sheet]
    footerLine: _Part[Sheet]

class _BoardProperties(typing.TypedDict, total=False):
    size: _t.SizeLike
    ground: _t.SurfacePaintLike

class _PanelProperties(typing.TypedDict, total=False):
    eyebrow: str
    title: str
    note: str
    rule: _t.FillLike
    rule_width: float
    ruleWidth: float
    gap: float
    title_gap: float
    titleGap: float
    body: Well | None
    eyebrow_line: _Part[Panel]
    eyebrowLine: _Part[Panel]
    title_line: _Part[Panel]
    titleLine: _Part[Panel]
    note_line: _Part[Panel]
    noteLine: _Part[Panel]

class _LineProperties(typing.TypedDict, total=False):
    length: _t.DimensionLike
    thickness: float
    column: bool
    fill: _t.SurfacePaintLike
    inset: float
    pair: LineCompanion | None

class _LadderProperties(typing.TypedDict, total=False):
    count: int
    pitch: float
    thickness: float
    column: bool
    fill: _t.SurfacePaintLike

def well(
    surface: _t.NodeLike | None = ...,
    props: Well | None = ...,
    **properties: typing.Unpack[_WellProperties],
) -> sigil.compose.Element: ...
def cell(
    body: _t.NodeLike,
    props: Caption | None = ...,
    *,
    label: str = ...,
    note: str = ...,
    **properties: typing.Unpack[_CaptionProperties],
) -> sigil.compose.Element: ...
def cells(
    *children: _t.ChildLike,
    props: Cells | None = ...,
    **properties: typing.Unpack[_CellsProperties],
) -> sigil.compose.Element: ...
def panel_grid(
    *children: _t.ChildLike,
    props: PanelGrid | None = ...,
    **properties: typing.Unpack[_PanelGridProperties],
) -> sigil.compose.Element: ...
def sheet(
    content: _t.NodeLike,
    props: Sheet | None = ...,
    **properties: typing.Unpack[_SheetProperties],
) -> sigil.compose.Element: ...
def board(
    *children: _t.ChildLike,
    props: Board | None = ...,
    **properties: typing.Unpack[_BoardProperties],
) -> sigil.compose.Element: ...
def panel(
    content: _t.NodeLike,
    props: Panel | None = ...,
    **properties: typing.Unpack[_PanelProperties],
) -> sigil.compose.Element: ...
def line(
    props: Line | None = ..., **properties: typing.Unpack[_LineProperties]
) -> sigil.compose.Element: ...
def ladder(
    props: Ladder | None = ..., **properties: typing.Unpack[_LadderProperties]
) -> sigil.compose.Element: ...
def centred(*children: _t.ChildLike) -> sigil.compose.Element: ...
