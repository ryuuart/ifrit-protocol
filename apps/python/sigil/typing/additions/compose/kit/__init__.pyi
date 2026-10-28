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
    ground: _t.FillLike
    padding: float
    paddingVertical: float | None
    clip: bool
    corners: float
    keyline: _t.FillLike | None
    keylineWidth: float
    placed: bool
    content: WellContent | None

class _CaptionProperties(typing.TypedDict, total=False):
    where: CaptionWhere
    gap: float
    noteGap: float
    labelMeasure: float
    noteMeasure: float
    align: _t.AlignLike
    justify: _t.AlignLike
    reading: str
    readingLine: _Part[Caption]

class _CellsProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[_t.NodeLike]
    column: bool
    gap: float
    divider: _t.FillLike
    dividerWidth: float
    align: _t.AlignLike

class _PanelGridProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[_t.NodeLike]
    columns: int
    gap: float
    rowGap: float | None
    divider: _t.FillLike
    dividerWidth: float
    align: _t.AlignLike
    measure: _t.DimensionLike

class _SheetProperties(typing.TypedDict, total=False):
    title: str
    subtitle: str
    footer: str
    marginHorizontal: float
    marginTop: float
    marginBottom: float
    subtitleGap: float
    contentGap: float
    ground: _t.FillLike
    rule: _t.FillLike
    ruleWidth: float
    key: str
    titleLine: _Part[Sheet]
    subtitleLine: _Part[Sheet]
    footerLine: _Part[Sheet]

class _BoardProperties(typing.TypedDict, total=False):
    size: _t.SizeLike
    ground: _t.FillLike

class _PanelProperties(typing.TypedDict, total=False):
    eyebrow: str
    title: str
    note: str
    rule: _t.FillLike
    ruleWidth: float
    gap: float
    titleGap: float
    body: Well | None
    eyebrowLine: _Part[Panel]
    titleLine: _Part[Panel]
    noteLine: _Part[Panel]

class _LineProperties(typing.TypedDict, total=False):
    length: _t.DimensionLike
    thickness: float
    column: bool
    fill: _t.FillLike
    inset: float
    pair: LineCompanion | None

class _LadderProperties(typing.TypedDict, total=False):
    count: int
    pitch: float
    thickness: float
    column: bool
    fill: _t.FillLike

_CellPart: typing.TypeAlias = (
    collections.abc.Callable[[], _t.NodeLike]
    | collections.abc.Callable[[str], _t.NodeLike]
    | collections.abc.Callable[[str, Table], _t.NodeLike]
    | collections.abc.Callable[[str, Table, int], _t.NodeLike]
    | collections.abc.Callable[[str, Table, int, int], _t.NodeLike]
    | collections.abc.Callable[[str, Table, int, int, list[str]], _t.NodeLike]
    | None
)
_FigurePart: typing.TypeAlias = (
    collections.abc.Callable[[], _t.NodeLike]
    | collections.abc.Callable[[float], _t.NodeLike]
    | collections.abc.Callable[[float, Bars], _t.NodeLike]
    | None
)

class _TableProperties(typing.TypedDict, total=False):
    columns: collections.abc.Sequence[Column]
    gap: float
    rowGap: float
    divider: _t.FillLike
    dividerWidth: float
    headRuled: bool
    swatches: collections.abc.Sequence[_t.FillLike]
    swatchSide: float
    swatchCorners: float
    keys: collections.abc.Sequence[str]
    headLine: _Part[Table]
    cellLine: _CellPart

class _BarsProperties(typing.TypedDict, total=False):
    length: float
    largest: float
    labelMeasure: float
    barHeight: _t.DimensionLike
    gap: _t.DimensionLike
    rowGap: _t.DimensionLike
    bar: _t.FillLike
    rest: _t.FillLike
    inks: collections.abc.Sequence[_t.FillLike]
    labelLine: _Part[Bars]
    figureLine: _FigurePart

class _RowsProperties(typing.TypedDict, total=False):
    measure: float
    nameMeasure: float
    gap: _t.DimensionLike
    labelGap: _t.DimensionLike
    divider: _t.FillLike
    dividerWidth: float
    swatchSide: float
    swatchCorners: float

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
def panelGrid(
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
def reading(
    one: Reading,
    props: Rows | None = ...,
    **properties: typing.Unpack[_RowsProperties],
) -> sigil.compose.Element: ...
def readout(
    readings: collections.abc.Iterable[Reading],
    props: Rows | None = ...,
    **properties: typing.Unpack[_RowsProperties],
) -> sigil.compose.Element: ...
def table(
    rows: collections.abc.Iterable[str] | collections.abc.Iterable[collections.abc.Iterable[str]],
    props: Table | None = ...,
    **properties: typing.Unpack[_TableProperties],
) -> sigil.compose.Element: ...
def bars(
    labels: collections.abc.Iterable[str],
    values: collections.abc.Iterable[float],
    props: Bars | None = ...,
    **properties: typing.Unpack[_BarsProperties],
) -> sigil.compose.Element: ...
