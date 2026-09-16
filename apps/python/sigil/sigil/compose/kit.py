"""Neutral native components for sheets, specimen wells and construction marks."""

from .._kit import children as _children
from .._kit import specification
from ..native import compose as _compose

_native = _compose.kit

CaptionWhere = _native.CaptionWhere
Caption = _native.Caption
Well = _native.Well
WellContent = _native.WellContent
Cells = _native.Cells
PanelGrid = _native.PanelGrid
Sheet = _native.Sheet
Board = _native.Board
Panel = _native.Panel
Line = _native.Line
LineCompanion = _native.LineCompanion
Ladder = _native.Ladder

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


def well(surface=None, props=None, **properties):
    plate = specification(Well, props, properties)
    return _native.well(plate) if surface is None else _native.well(plate, surface)


def cell(body, props=None, *, label="", note="", **properties):
    return _native.cell(specification(Caption, props, properties), label, note, body)


def cells(*children, props=None, **properties):
    run = specification(Cells, props, properties)
    if children:
        run.cells = list(_children(children))
    return _native.cells(run)


def panel_grid(*children, props=None, **properties):
    grid = specification(PanelGrid, props, properties)
    if children:
        grid.cells = list(_children(children))
    return _native.panelGrid(grid)


def sheet(content, props=None, **properties):
    return _native.sheet(specification(Sheet, props, properties), content)


def board(*children, props=None, **properties):
    element = _native.board(specification(Board, props, properties))
    if children:
        element.children(list(_children(children)))
    return element


def panel(content, props=None, **properties):
    return _native.panel(specification(Panel, props, properties), content)


def line(props=None, **properties):
    return _native.line(specification(Line, props, properties))


def ladder(props=None, **properties):
    return _native.ladder(specification(Ladder, props, properties))


def centred(*children):
    element = _native.centred()
    if children:
        element.children(list(_children(children)))
    return element


at = _native.at
disc = _native.disc
dot = _native.dot
ring = _native.ring
caption_label = _native.captionLabel
caption_note = _native.captionNote
figure = _native.figure
