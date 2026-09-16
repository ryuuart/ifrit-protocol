"""Native specimen pages and their inherited theme."""

from .._kit import children as _children
from .._kit import specification
from ..native import sketch as _sketch

_native = _sketch.kit

Palette = _native.Palette
Register = _native.Register
TypeScale = _native.TypeScale
Spacing = _native.Spacing
Theme = _native.Theme
Voice = _native.Voice
Stage = _native.Stage
Page = _native.Page
Well = _native.Well
WellContent = _native.WellContent
Recess = _native.Recess
Relief = _native.Relief
Cell = _native.Cell
Run = _native.Run
PanelGrid = _native.PanelGrid
Provide = _native.Provide

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
    "study_theme",
    "theme",
    "well",
]


def house_theme():
    """Return an editable copy of the native house theme."""
    return _native.houseTheme()


def study_theme():
    """Return an editable copy of the native study theme."""
    return _native.studyTheme()


def house_face(voice, weight=400, italic=False):
    return _native.houseFace(voice, weight, italic)


def theme():
    """Read a copy of the native theme inherited by this describe scope."""
    return _native.theme()


def provide(look):
    """Bind a native theme inside ``with provide(look):`` on this thread."""
    return Provide(look)


def stage(ctx, props=None, **properties):
    """Declare the native stage; an omitted background reads the scoped theme."""
    return _native.stage(ctx, specification(Stage, props, properties))


def page(content, props=None, **properties):
    """Put content on a native page with themed title, subtitle and footer."""
    return _native.page(specification(Page, props, properties), content)


def well(surface=None, props=None, **properties):
    """Apply the native well spec to a surface, or create an empty well."""
    plate = specification(Well, props, properties)
    return _native.well(plate) if surface is None else _native.well(plate, surface)


def caption(body, *, label="", note="", measure=0):
    return _native.caption(measure, label, note, body)


def cell(picture, props=None, *, label="", note="", **properties):
    return _native.cell(specification(Cell, props, properties), label, note, picture)


def cells(*children, props=None, **properties):
    run = specification(Run, props, properties)
    if children:
        run.cells = list(_children(children))
    return _native.cells(run)


def panel_grid(*children, props=None, **properties):
    grid = specification(PanelGrid, props, properties)
    if children:
        grid.cells = list(_children(children))
    return _native.panelGrid(grid)
