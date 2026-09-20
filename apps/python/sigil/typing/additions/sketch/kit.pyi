import collections.abc
import typing

import sigil._types as _t
import sigil.compose
import sigil.sketch
import sigil.skia

class _StageProperties(typing.TypedDict, total=False):
    size: _t.SizeLike
    capture_at: float
    captureAt: float
    background: _t.ColorLike | None
    oversample: int
    plate_only: bool
    plateOnly: bool
    nonlinear_picture: bool
    nonlinearPicture: bool

class _PageProperties(typing.TypedDict, total=False):
    title: str
    subtitle: str
    footer: str
    ruled: bool
    ground: _t.SurfacePaintLike | None
    key: str

class _WellProperties(typing.TypedDict, total=False):
    width: _t.DimensionLike
    height: _t.DimensionLike
    ground: _t.SurfacePaintLike | None
    padding: float | None
    padding_y: float | None
    paddingY: float | None
    clip: bool
    corners: float | None
    keyline: _t.FillLike | None
    keyline_width: float
    keylineWidth: float
    placed: bool
    content: WellContent | None
    recess: Recess | None
    relief: Relief | None

class _CellProperties(typing.TypedDict, total=False):
    plate: Well
    measure: float | None

class _RunProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[sigil.compose.Element]
    column: bool
    gap: float | None
    ruled: bool
    align: _t.AlignLike

class _PanelGridProperties(typing.TypedDict, total=False):
    cells: collections.abc.Sequence[sigil.compose.Element]
    columns: int
    gap: float | None
    row_gap: float | None
    rowGap: float | None
    ruled: bool
    align: _t.AlignLike
    measure: _t.DimensionLike

class _ComparisonProperties(typing.TypedDict, total=False):
    measure: float
    gap: float | None
    track_gap: float | None
    trackGap: float | None

def house_theme() -> Theme: ...
def study_theme() -> Theme: ...
def house_face(
    voice: Voice, weight: typing.SupportsInt = ..., italic: bool = ...
) -> sigil.skia.Typeface: ...
def provide(look: Theme) -> Provide: ...
def stage(
    ctx: sigil.sketch.SketchContext,
    props: Stage | None = ...,
    **properties: typing.Unpack[_StageProperties],
) -> None: ...
def page(
    content: sigil.compose.Element,
    props: Page | None = ...,
    **properties: typing.Unpack[_PageProperties],
) -> sigil.compose.Element: ...
def well(
    surface: sigil.compose.Element | None = ...,
    props: Well | None = ...,
    **properties: typing.Unpack[_WellProperties],
) -> sigil.compose.Element: ...
def caption(
    body: sigil.compose.Element,
    *,
    label: str = ...,
    note: str = ...,
    measure: float = ...,
) -> sigil.compose.Element: ...
def cell(
    picture: sigil.compose.Element,
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
def panel_grid(
    *children: _t.ChildLike,
    props: PanelGrid | None = ...,
    **properties: typing.Unpack[_PanelGridProperties],
) -> sigil.compose.Element: ...
