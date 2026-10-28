import collections.abc
import typing

import sigil._types as _t
import sigil.compose
import sigil.compose.feed
import sigil.compose.kit
import sigil.data
import sigil.sketch
import sigil.skia
import sigil.weave

# A verdict's columns are the compose kit's; the one class is offered here too.
Column = sigil.compose.kit.Column

class _StageProperties(typing.TypedDict, total=False):
    styleSheet: sigil.compose.StyleSheet | sigil.compose.Rule | None
    # Deprecated: a builder, and rules joined after its stylesheet.
    theme: Theme | None
    sheet: sigil.compose.StyleSheet | sigil.compose.Rule | None
    size: _t.SizeLike
    captureAt: float
    background: _t.ColorLike | None
    oversample: int
    plateOnly: bool

class _PageProperties(typing.TypedDict, total=False):
    title: str
    subtitle: str
    footer: str
    ruled: bool
    ground: _t.FillLike | None
    key: str

class _WellProperties(typing.TypedDict, total=False):
    width: _t.DimensionLike
    height: _t.DimensionLike
    ground: _t.FillLike | None
    padding: float | None
    paddingVertical: float | None
    clip: bool
    corners: float | None
    keyline: _t.FillLike | None
    keylineWidth: float
    placed: bool
    content: WellContent | None
    recess: Recess | None
    relief: Relief | None

class _CellProperties(typing.TypedDict, total=False):
    plate: Well
    measure: float | None
    where: sigil.compose.kit.CaptionWhere

class _RunProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[_t.NodeLike]
    column: bool
    gap: float | None
    ruled: bool
    align: _t.AlignLike

class _PanelGridProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[_t.NodeLike]
    columns: int
    gap: float | None
    rowGap: float | None
    ruled: bool
    align: _t.AlignLike
    measure: _t.DimensionLike

class _ComparisonProperties(typing.TypedDict, total=False):
    measure: float
    gap: float | None
    trackGap: float | None

class _LegendProperties(typing.TypedDict, total=False):
    entries: collections.abc.Sequence[LegendEntry]
    column: bool
    swatchSide: float | None
    gap: float | None
    corners: float
    strokeWidth: float
    keylineWidth: float
    labelGap: float | None
    wrap: bool

class _SwatchStripProperties(typing.TypedDict, total=False):
    swatches: collections.abc.Sequence[_t.FillLike]
    labels: collections.abc.Sequence[str]
    inks: collections.abc.Sequence[_t.FillLike]
    width: _t.DimensionLike
    height: _t.DimensionLike
    gap: float | None
    corners: float
    opacity: _t.ScalarLike | None

class _ChipProperties(typing.TypedDict, total=False):
    label: str
    ground: _t.FillLike | None
    ink: _t.FillLike | None
    corners: float | None

class _MeterProperties(typing.TypedDict, total=False):
    fraction: float
    level: _t.ScalarLike | None
    label: str
    reading: str
    width: _t.DimensionLike
    height: _t.DimensionLike | None
    track: _t.FillLike | None
    bar: _t.FillLike | None
    corners: float
    keyline: _t.FillLike | None
    keylineWidth: float
    inset: float | None

class _GaugeProperties(typing.TypedDict, total=False):
    fraction: float
    diameter: float
    thickness: float
    startDegrees: float
    sweepDegrees: float
    track: _t.FillLike | None
    bar: _t.FillLike | None
    reading: str

class _BackdropProperties(typing.TypedDict, total=False):
    ground: _t.FillLike | None
    vignette: float
    edge: _t.ColorLike | None
    grain: float
    grainScale: float
    key: str

class _PanelProperties(typing.TypedDict, total=False):
    eyebrow: str
    title: str
    note: str
    ruled: bool
    body: Well

class _FrameProperties(typing.TypedDict, total=False):
    width: _t.DimensionLike
    height: _t.DimensionLike
    shell: _t.FillLike | None
    corners: float | None
    bezel: float | None
    screen: _t.FillLike | None
    screenCorners: float | None
    keyline: _t.FillLike | None
    plate: str

class _ScrollbarProperties(typing.TypedDict, total=False):
    column: bool
    leading: _t.NodeLike | None
    trailing: _t.NodeLike | None
    thumb: _t.NodeLike | None
    scrolled: Scrolled
    thumbLength: _t.DimensionLike
    at: float
    position: _t.ScalarLike | None
    thumbInset: float
    track: _t.FillLike | None

class _TickerProperties(typing.TypedDict, total=False):
    contentWidth: float
    phase: _t.ScalarLike
    gap: float | None
    width: _t.DimensionLike
    height: _t.DimensionLike

class _TimelineProperties(typing.TypedDict, total=False):
    marks: collections.abc.Sequence[Timeline.Mark]
    width: _t.DimensionLike
    height: float | None
    tick: float | None
    rail: _t.FillLike | None
    ink: _t.FillLike | None
    below: bool

class _TitleCardProperties(typing.TypedDict, total=False):
    eyebrow: Line | str
    title: Line | str
    subtitle: Line | str
    notes: collections.abc.Sequence[Line | str]
    align: _t.AlignLike
    ruled: bool
    key: str

class _ConsoleProperties(typing.TypedDict, total=False):
    stacked: int
    visible: int
    levels: collections.abc.Sequence[tuple[str, _t.ColorLike]]
    size: float | None
    ink: _t.FillLike | None
    ground: _t.FillLike | None
    border: _t.FillLike | None

class _ConnectionReadoutProperties(typing.TypedDict, total=False):
    door: str
    rows: sigil.compose.kit.Rows
    readings: collections.abc.Sequence[sigil.compose.kit.Reading]

class _InstrumentProperties(typing.TypedDict, total=False):
    page: Page
    pictureSize: _t.SizeLike
    pictureWidth: float
    pictureLabel: str
    readingsLabel: str
    note: str

class _RulerProperties(typing.TypedDict, total=False):
    of: Axis
    at: float | None
    ticks: collections.abc.Sequence[float]
    count: int
    line: bool
    width: float
    reach: float | None
    gap: float
    numbers: bool

class _RulesProperties(typing.TypedDict, total=False):
    x: collections.abc.Sequence[float]
    y: collections.abc.Sequence[float]
    pen: sigil.compose.PathFormat | float
    styleClass: str

class _TraceProperties(typing.TypedDict, total=False):
    pen: sigil.compose.PathFormat | float
    samples: int
    along: sigil.compose.Spans | None
    styleClass: str

class _AreaProperties(typing.TypedDict, total=False):
    base: float
    samples: int
    styleClass: str

class _PathProperties(typing.TypedDict, total=False):
    pen: sigil.compose.PathFormat | float
    samples: int
    over: sigil.data.Interval
    along: sigil.compose.Spans | None
    styleClass: str

class _LabelProperties(typing.TypedDict, total=False):
    anchor: Anchor
    styleClass: str

def houseTheme() -> Theme: ...
def studyTheme() -> Theme: ...
def houseFace(
    voice: Voice,
    weight: typing.SupportsInt = ...,
    slant: sigil.weave.FaceSlant | None = ...,
    *,
    italic: bool = ...,
) -> sigil.weave.Face: ...
def stage(
    ctx: sigil.sketch.Scene2DContext | sigil.sketch.Scene3DContext,
    props: Stage | None = ...,
    **properties: typing.Unpack[_StageProperties],
) -> None: ...
def page(
    content: _t.NodeLike,
    props: Page | None = ...,
    **properties: typing.Unpack[_PageProperties],
) -> sigil.compose.Element: ...
def well(
    surface: _t.NodeLike | None = ...,
    props: Well | None = ...,
    **properties: typing.Unpack[_WellProperties],
) -> sigil.compose.Element: ...
def caption(
    body: _t.NodeLike,
    *,
    label: str = ...,
    note: str = ...,
    measure: float = ...,
    where: sigil.compose.kit.CaptionWhere = ...,
) -> sigil.compose.Element: ...
def cell(
    picture: _t.NodeLike,
    props: Cell | None = ...,
    *,
    label: str = ...,
    note: str = ...,
    **properties: typing.Unpack[_CellProperties],
) -> sigil.compose.Element: ...
def comparison(
    cases: collections.abc.Sequence[ComparisonCase],
    props: Comparison | None = ...,
    **properties: typing.Unpack[_ComparisonProperties],
) -> sigil.compose.Element: ...
def cells(
    *children: _t.ChildLike,
    props: Run | None = ...,
    **properties: typing.Unpack[_RunProperties],
) -> sigil.compose.Element: ...
def panelGrid(
    *children: _t.ChildLike,
    props: PanelGrid | None = ...,
    **properties: typing.Unpack[_PanelGridProperties],
) -> sigil.compose.Element: ...
def legend(
    *entries: LegendEntry,
    props: Legend | None = ...,
    **properties: typing.Unpack[_LegendProperties],
) -> sigil.compose.Element: ...
def swatchStrip(
    props: SwatchStrip | None = ...,
    **properties: typing.Unpack[_SwatchStripProperties],
) -> sigil.compose.Element: ...
def chip(
    props: Chip | None = ..., **properties: typing.Unpack[_ChipProperties]
) -> sigil.compose.Element: ...
def meter(
    props: Meter | None = ..., **properties: typing.Unpack[_MeterProperties]
) -> sigil.compose.Element: ...
def gauge(
    props: Gauge | None = ..., **properties: typing.Unpack[_GaugeProperties]
) -> sigil.compose.Element: ...
def backdrop(
    props: Backdrop | None = ..., **properties: typing.Unpack[_BackdropProperties]
) -> sigil.compose.Element: ...
def panel(
    content: _t.NodeLike,
    props: Panel | None = ...,
    **properties: typing.Unpack[_PanelProperties],
) -> sigil.compose.Element: ...
def frame(
    content: _t.NodeLike | None = ...,
    props: Frame | None = ...,
    **properties: typing.Unpack[_FrameProperties],
) -> sigil.compose.Element: ...
def scrollbar(
    props: Scrollbar | None = ..., **properties: typing.Unpack[_ScrollbarProperties]
) -> sigil.compose.Element: ...
def ticker(
    content: _t.NodeLike,
    props: Ticker | None = ...,
    **properties: typing.Unpack[_TickerProperties],
) -> sigil.compose.Element: ...
def timeline(
    *marks: Timeline.Mark,
    props: Timeline | None = ...,
    **properties: typing.Unpack[_TimelineProperties],
) -> sigil.compose.Element: ...
def titleCard(
    props: TitleCard | None = ..., **properties: typing.Unpack[_TitleCardProperties]
) -> sigil.compose.Element: ...
def console(
    *feeds: sigil.compose.feed.TextRing,
    props: Console | None = ...,
    **properties: typing.Unpack[_ConsoleProperties],
) -> sigil.compose.Element: ...
def connectionReadout(
    connection: sigil.data.Connection,
    props: ConnectionReadout | None = ...,
    **properties: typing.Unpack[_ConnectionReadoutProperties],
) -> sigil.compose.Element: ...
def instrument(
    picture: _t.NodeLike,
    readings: _t.NodeLike,
    props: Instrument | None = ...,
    **properties: typing.Unpack[_InstrumentProperties],
) -> sigil.compose.Element: ...
def axis(
    props: Ruler | None = ..., **properties: typing.Unpack[_RulerProperties]
) -> Layer: ...
def rules(
    props: Rules | None = ..., **properties: typing.Unpack[_RulesProperties]
) -> Layer: ...
def trace(
    curve: collections.abc.Callable[[float], float] | collections.abc.Sequence[float],
    props: Trace | None = ...,
    **properties: typing.Unpack[_TraceProperties],
) -> Layer: ...
def area(
    curve: collections.abc.Callable[[float], float],
    props: Area | None = ...,
    **properties: typing.Unpack[_AreaProperties],
) -> Layer: ...
def path(
    at: collections.abc.Callable[[float], Datum | tuple[float, float]],
    props: Path | None = ...,
    **properties: typing.Unpack[_PathProperties],
) -> Layer: ...
def label(
    words: str,
    x: float,
    y: float,
    props: Label | None = ...,
    **properties: typing.Unpack[_LabelProperties],
) -> Layer: ...
