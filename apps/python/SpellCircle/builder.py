"""
FlatBuffers serialization for SpellCircle scenes.

This is the only module that touches the generated FlatBuffers bindings.
Scene authoring (`canvas.py`) and transport (`network.py`) both stay
ignorant of the wire format; they hand plain data to `SCBuilder` and get
bytes back.

Public API
----------
CircleSpec — plain data describing a circle (position, label, style)
SCBuilder  — encodes CircleSpecs/points/edges/boxes into a Scene FlatBuffer
"""

import flatbuffers
from . import Box as Bx
from . import Circle as Circ
from . import Edge as Edg
from . import Point as Pt
from . import Scene as SC
from .Vec2 import CreateVec2


class CircleSpec:
    """Plain data describing a circle: position, radius, label, style."""

    def __init__(self, name, x, y, radius, *, text_start=0.75, active=0.0):
        self.name = name
        self.x = float(x)
        self.y = float(y)
        self.radius = int(radius)
        self.text_start = float(text_start)
        self.active = float(active)  # background fill alpha/intensity [0, 1]


class SCBuilder:
    """Encodes SpellCircle scene data into a single FlatBuffer.

    Wraps one `flatbuffers.Builder` and exposes a `build_*` method per
    table in `SpellCircle.fbs`. Each `build_*` call appends to the same
    underlying buffer and returns an offset to be passed into whichever
    call assembles the next table up (Point -> Edge/Box -> Scene).

    Usage:
        b = SCBuilder()
        circle_off = b.build_circle(spec)
        point_off = b.build_point(spec, position=0.25, value="foo")
        edge_off = b.build_edge(point_a_off, point_b_off)
        box_off = b.build_box("ANGER", point_off, active=0.4)
        data = b.build_scene_bytes([circle_off], [edge_off], [box_off], width, height)
    """

    def __init__(self, size=16384):
        self._builder = flatbuffers.Builder(size)

    def build_circle(self, spec):
        """Encode a CircleSpec as a Circle table and return its offset."""
        builder = self._builder
        name_off = builder.CreateString(spec.name)
        Circ.CircleStart(builder)
        Circ.CircleAddPos(builder, CreateVec2(builder, spec.x, spec.y))
        Circ.CircleAddName(builder, name_off)
        Circ.CircleAddRadius(builder, spec.radius)
        Circ.CircleAddTextStart(builder, spec.text_start)
        Circ.CircleAddActive(builder, spec.active)
        return Circ.CircleEnd(builder)

    def build_point(self, spec, position, value=""):
        """Embed a CircleSpec as a Point at fractional arc position [0, 1]."""
        builder = self._builder
        circle_off = self.build_circle(spec)
        value_off = builder.CreateString(value)
        Pt.PointStart(builder)
        Pt.PointAddValue(builder, value_off)
        Pt.PointAddCircle(builder, circle_off)
        Pt.PointAddPosition(builder, float(position))
        return Pt.PointEnd(builder)

    def build_edge(self, first_off, second_off):
        """Connect two pre-built Point offsets into an Edge."""
        builder = self._builder
        Edg.EdgeStart(builder)
        Edg.EdgeAddFirst(builder, first_off)
        Edg.EdgeAddSecond(builder, second_off)
        return Edg.EdgeEnd(builder)

    def build_box(self, value, point_off, active=0.0):
        """Anchor a labelled box to a pre-built Point offset.

        `active` is the background fill alpha/intensity in [0, 1]; 0 draws no fill.
        """
        builder = self._builder
        value_off = builder.CreateString(value)
        Bx.BoxStart(builder)
        Bx.BoxAddValue(builder, value_off)
        Bx.BoxAddPoint(builder, point_off)
        Bx.BoxAddActive(builder, float(active))
        return Bx.BoxEnd(builder)

    def _build_vector(self, start_fn, offsets):
        builder = self._builder
        start_fn(builder, len(offsets))
        for off in reversed(offsets):
            builder.PrependUOffsetTRelative(off)
        return builder.EndVector()

    def build_scene_bytes(self, circle_offsets, edge_offsets, box_offsets,
                           canvas_width=0.0, canvas_height=0.0):
        """
        Finish the builder and return the serialised scene as bytes.

        canvas_width / canvas_height record the authoring coordinate space so the
        app can scale geometry up to its native 4K texture. Pass 0 (default) if
        coordinates are already in native texture space.
        """
        builder = self._builder
        edges_vec   = self._build_vector(SC.SceneStartEdgesVector,   edge_offsets)
        boxes_vec   = self._build_vector(SC.SceneStartBoxesVector,   box_offsets)
        circles_vec = self._build_vector(SC.SceneStartCirclesVector, circle_offsets)

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
