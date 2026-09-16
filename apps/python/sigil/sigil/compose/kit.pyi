from typing import Unpack

from _sigil.compose import Element
from _sigil.compose.kit import (
    Board,
    Caption,
    CaptionWhere,
    Cells,
    Ladder,
    Line,
    LineCompanion,
    Panel,
    PanelGrid,
    Sheet,
    Well,
    WellContent,
    at,
    disc,
    dot,
    figure,
    ring,
)
from _sigil.compose.kit import (
    captionLabel as caption_label,
)
from _sigil.compose.kit import (
    captionNote as caption_note,
)

from ._kit_typing import (
    BoardProperties as _BoardProperties,
)
from ._kit_typing import (
    CaptionProperties as _CaptionProperties,
)
from ._kit_typing import (
    CellsProperties as _CellsProperties,
)
from ._kit_typing import (
    LadderProperties as _LadderProperties,
)
from ._kit_typing import (
    LineProperties as _LineProperties,
)
from ._kit_typing import (
    PanelGridProperties as _PanelGridProperties,
)
from ._kit_typing import (
    PanelProperties as _PanelProperties,
)
from ._kit_typing import (
    SheetProperties as _SheetProperties,
)
from ._kit_typing import (
    WellProperties as _WellProperties,
)
from ._typing import Child as _Child

def well(
    surface: Element | None = ...,
    props: Well | None = ...,
    **properties: Unpack[_WellProperties],
) -> Element: ...
def cell(
    body: Element,
    props: Caption | None = ...,
    *,
    label: str = ...,
    note: str = ...,
    **properties: Unpack[_CaptionProperties],
) -> Element: ...
def cells(
    *children: _Child,
    props: Cells | None = ...,
    **properties: Unpack[_CellsProperties],
) -> Element: ...
def panel_grid(
    *children: _Child,
    props: PanelGrid | None = ...,
    **properties: Unpack[_PanelGridProperties],
) -> Element: ...
def sheet(
    content: Element,
    props: Sheet | None = ...,
    **properties: Unpack[_SheetProperties],
) -> Element: ...
def board(
    *children: _Child,
    props: Board | None = ...,
    **properties: Unpack[_BoardProperties],
) -> Element: ...
def panel(
    content: Element,
    props: Panel | None = ...,
    **properties: Unpack[_PanelProperties],
) -> Element: ...
def line(
    props: Line | None = ..., **properties: Unpack[_LineProperties]
) -> Element: ...
def ladder(
    props: Ladder | None = ..., **properties: Unpack[_LadderProperties]
) -> Element: ...
def centred(*children: _Child) -> Element: ...

__all__ = [
    "Board",
    "Caption",
    "CaptionWhere",
    "Cells",
    "Ladder",
    "Line",
    "LineCompanion",
    "Panel",
    "PanelGrid",
    "Sheet",
    "Well",
    "WellContent",
    "at",
    "board",
    "caption_label",
    "caption_note",
    "cell",
    "cells",
    "centred",
    "disc",
    "dot",
    "figure",
    "ladder",
    "line",
    "panel",
    "panel_grid",
    "ring",
    "sheet",
    "well",
]
