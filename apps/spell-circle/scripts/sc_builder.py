"""
Shared primitives for building SpellCircle FlatBuffer scenes.

All scripts that produce scene packets should import from here so the
FlatBuffers building logic lives in one place.

Public API
----------
CircleSpec          — describes a circle (position, label, style)
build_point()       — resolve a position on a circle into a Point offset
build_edge()        — connect two Point offsets into an Edge offset
build_box()         — anchor a labelled box to a Point offset
build_scene_bytes() — serialise all offsets into a complete UDP payload
new_builder()       — create a flatbuffers.Builder with a sensible default size
"""

import os
import sys

# Make the generated FlatBuffers bindings importable from any working directory.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import flatbuffers
from SpellCircle import Box as Bx
from SpellCircle import Circle as Circ
from SpellCircle import Edge as Edg
from SpellCircle import Point as Pt
from SpellCircle import Scene as SC
from SpellCircle.Vec2 import CreateVec2


class CircleSpec:
    """A circle plus the strings needed to rebuild it inside embedded Points."""

    def __init__(self, name, x, y, radius, *, text_start=0.75, active=0.0):
        self.name = name
        self.x = float(x)
        self.y = float(y)
        self.radius = int(radius)
        self.text_start = float(text_start)
        self.active = float(active)  # background fill alpha/intensity [0, 1]

    def build(self, builder):
        name_off = builder.CreateString(self.name)
        Circ.CircleStart(builder)
        Circ.CircleAddPos(builder, CreateVec2(builder, self.x, self.y))
        Circ.CircleAddName(builder, name_off)
        Circ.CircleAddRadius(builder, self.radius)
        Circ.CircleAddTextStart(builder, self.text_start)
        Circ.CircleAddActive(builder, self.active)
        return Circ.CircleEnd(builder)


def build_point(builder, value, spec, position):
    """Embed a CircleSpec as a Point at fractional arc position [0, 1]."""
    circle_off = spec.build(builder)
    value_off = builder.CreateString(value)
    Pt.PointStart(builder)
    Pt.PointAddValue(builder, value_off)
    Pt.PointAddCircle(builder, circle_off)
    Pt.PointAddPosition(builder, float(position))
    return Pt.PointEnd(builder)


def build_edge(builder, a_off, b_off):
    """Connect two pre-built Point offsets into an Edge."""
    Edg.EdgeStart(builder)
    Edg.EdgeAddFirst(builder, a_off)
    Edg.EdgeAddSecond(builder, b_off)
    return Edg.EdgeEnd(builder)


def build_box(builder, value, point_off, active=0.0):
    """Anchor a labelled box to a pre-built Point offset.

    `active` is the background fill alpha/intensity in [0, 1]; 0 draws no fill.
    """
    value_off = builder.CreateString(value)
    Bx.BoxStart(builder)
    Bx.BoxAddValue(builder, value_off)
    Bx.BoxAddPoint(builder, point_off)
    Bx.BoxAddActive(builder, float(active))
    return Bx.BoxEnd(builder)


def _build_vector(builder, start_fn, offsets):
    start_fn(builder, len(offsets))
    for off in reversed(offsets):
        builder.PrependUOffsetTRelative(off)
    return builder.EndVector()


def build_scene_bytes(builder, circle_offsets, edge_offsets, box_offsets,
                      canvas_width=0.0, canvas_height=0.0):
    """
    Finish the builder and return the serialised scene as bytes.

    canvas_width / canvas_height record the authoring coordinate space so the
    app can scale geometry up to its native 4K texture. Pass 0 (default) if
    coordinates are already in native texture space.
    """
    edges_vec   = _build_vector(builder, SC.SceneStartEdgesVector,   edge_offsets)
    boxes_vec   = _build_vector(builder, SC.SceneStartBoxesVector,   box_offsets)
    circles_vec = _build_vector(builder, SC.SceneStartCirclesVector, circle_offsets)

    SC.SceneStart(builder)
    SC.SceneAddCircles(builder, circles_vec)
    SC.SceneAddEdges(builder, edges_vec)
    SC.SceneAddBoxes(builder, boxes_vec)
    if canvas_width > 0:
        SC.SceneAddWidth(builder, float(canvas_width))
    if canvas_height > 0:
        SC.SceneAddHeight(builder, float(canvas_height))
    builder.Finish(SC.SceneEnd(builder))
    return bytes(builder.Output())


def new_builder(size=16384):
    """Return a fresh FlatBuffers builder."""
    return flatbuffers.Builder(size)
