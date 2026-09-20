"""Neutral native components for sheets, specimen wells and construction marks."""

from _sigil.compose import kit as _native

from ..._kit import children as _children
from ..._kit import specification as _specification


def well(surface=None, props=None, **properties):
    plate = _specification(Well, props, properties)
    return _native.well(plate) if surface is None else _native.well(plate, surface)


def cell(body, props=None, *, label="", note="", **properties):
    return _native.cell(_specification(Caption, props, properties), label, note, body)


def cells(*children, props=None, **properties):
    run = _specification(Cells, props, properties)
    if children:
        run.cells = list(_children(children))
    return _native.cells(run)


def panel_grid(*children, props=None, **properties):
    grid = _specification(PanelGrid, props, properties)
    if children:
        grid.cells = list(_children(children))
    return _native.panelGrid(grid)


def sheet(content, props=None, **properties):
    return _native.sheet(_specification(Sheet, props, properties), content)


def board(*children, props=None, **properties):
    element = _native.board(_specification(Board, props, properties))
    if children:
        element.children(list(_children(children)))
    return element


def panel(content, props=None, **properties):
    return _native.panel(_specification(Panel, props, properties), content)


def line(props=None, **properties):
    return _native.line(_specification(Line, props, properties))


def ladder(props=None, **properties):
    return _native.ladder(_specification(Ladder, props, properties))


def centred(*children):
    element = _native.centred()
    if children:
        element.children(list(_children(children)))
    return element
