from typing import Unpack

from _sigil.compose import Element
from _sigil.sketch import Context
from _sigil.sketch.kit import (
    Cell,
    Page,
    Palette,
    PanelGrid,
    Provide,
    Recess,
    Register,
    Relief,
    Run,
    Spacing,
    Stage,
    Theme,
    TypeScale,
    Voice,
    Well,
    WellContent,
    theme,
)
from _sigil.sketch.kit import (
    houseFace as house_face,
)
from _sigil.sketch.kit import (
    houseTheme as house_theme,
)

from ..compose._typing import Child as _Child
from ._kit_typing import (
    CellProperties as _CellProperties,
)
from ._kit_typing import (
    PageProperties as _PageProperties,
)
from ._kit_typing import (
    PanelGridProperties as _PanelGridProperties,
)
from ._kit_typing import (
    RunProperties as _RunProperties,
)
from ._kit_typing import (
    StageProperties as _StageProperties,
)
from ._kit_typing import (
    WellProperties as _WellProperties,
)

def provide(look: Theme) -> Provide: ...
def stage(
    ctx: Context, props: Stage | None = ..., **properties: Unpack[_StageProperties]
) -> None: ...
def page(
    content: Element, props: Page | None = ..., **properties: Unpack[_PageProperties]
) -> Element: ...
def well(
    surface: Element | None = ...,
    props: Well | None = ...,
    **properties: Unpack[_WellProperties],
) -> Element: ...
def caption(
    body: Element, *, label: str = ..., note: str = ..., measure: float = ...
) -> Element: ...
def cell(
    picture: Element,
    props: Cell | None = ...,
    *,
    label: str = ...,
    note: str = ...,
    **properties: Unpack[_CellProperties],
) -> Element: ...
def cells(
    *children: _Child, props: Run | None = ..., **properties: Unpack[_RunProperties]
) -> Element: ...
def panel_grid(
    *children: _Child,
    props: PanelGrid | None = ...,
    **properties: Unpack[_PanelGridProperties],
) -> Element: ...

__all__ = [
    "Cell",
    "Page",
    "Palette",
    "PanelGrid",
    "Provide",
    "Recess",
    "Register",
    "Relief",
    "Run",
    "Spacing",
    "Stage",
    "Theme",
    "TypeScale",
    "Voice",
    "Well",
    "WellContent",
    "caption",
    "cell",
    "cells",
    "house_face",
    "house_theme",
    "page",
    "panel_grid",
    "provide",
    "stage",
    "theme",
    "well",
]
