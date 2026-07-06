"""
Canvas-style recording API for authoring SpellCircle scenes in Python.

This module knows nothing about FlatBuffers or the network. It just records
circles, points, edges, and boxes as plain Python objects — like a painter
recording draw calls — and hands them to `SCBuilder` (the serialization
layer, in `builder.py`) only when `to_bytes()` is called. Sending the
result over the wire is `network.py`'s job. Scripts that author scenes
should only ever import from here.

Public API
----------
SCCanvas — records a scene and serializes it to FlatBuffer bytes on demand.
PointRef — handle returned by `SCCanvas.point()`, passed to `edge()`/`box()`.

Usage
-----
    canvas = SCCanvas(width=4000, height=4000)
    sigil = canvas.circle("FLARE SIGIL", 2000, 2000, 1150, text_start=0.62)
    p1 = canvas.point(sigil, 0.10)
    p2 = canvas.point(sigil, 0.60)
    canvas.edge(p1, p2)
    canvas.box("ANGER", p1, active=0.4)

    from SpellCircle.network import send_once
    send_once(canvas.to_bytes(), host="127.0.0.1", port=27015)
"""

from .builder import CircleSpec, SCBuilder


class PointRef:
    """A recorded Point: a fractional `position` [0, 1] along `circle`'s
    perimeter (or, for a radius-0 anchor circle, an arbitrary (x, y),
    ignoring `position`). Returned by `SCCanvas.point()`; pass it to
    `edge()`/`box()` to attach geometry to it."""

    __slots__ = ("circle", "position", "value")

    def __init__(self, circle, position, value):
        self.circle = circle
        self.position = position
        self.value = value


class SCCanvas:
    """Records a SpellCircle scene as plain Python objects and builds the
    FlatBuffers `Scene` only when asked. See module docstring for usage."""

    def __init__(self, width=4000.0, height=4000.0):
        self.width = float(width)
        self.height = float(height)
        self._circles = []
        self._edges = []
        self._boxes = []

    @property
    def circles(self):
        return tuple(self._circles)

    @property
    def edges(self):
        return tuple(self._edges)

    @property
    def boxes(self):
        return tuple(self._boxes)

    def circle(self, name, x, y, radius, *, text_start=0.75, active=0.0):
        """Add a visible circle to the scene and return a handle to it.

        `active` is the background fill alpha/intensity in [0, 1]; 0 draws
        no fill. A `radius` of 0 makes it an invisible anchor — see `anchor()`.
        """
        spec = CircleSpec(name, x, y, radius, text_start=text_start, active=active)
        self._circles.append(spec)
        return spec

    def anchor(self, x, y):
        """An invisible radius-0 circle for pinning a point/box at an
        arbitrary (x, y) rather than along a visible circle's perimeter.
        Unlike `circle()`, this is not added to the scene's rendered circle
        list — it only exists to be passed to `point()`."""
        return CircleSpec("", x, y, 0)

    def point(self, circle, position=0.0, value=""):
        """A fractional position along `circle`'s perimeter, optionally
        carrying its own `value` label (drawn like a box). Does nothing on
        its own until passed to `edge()` or `box()`."""
        return PointRef(circle, position, value)

    def edge(self, first, second):
        """Draw a line connecting two points."""
        self._edges.append((first, second))

    def box(self, value, point, *, active=0.0):
        """Anchor a labelled box to a point.

        `active` is the background fill alpha/intensity in [0, 1]; 0 draws
        no fill.
        """
        self._boxes.append((value, point, active))

    def to_bytes(self):
        """Serialize everything recorded so far into a FlatBuffers `Scene`."""
        builder = SCBuilder()
        built_points = {}

        def build_point(ref):
            # Memoize by identity so the same PointRef shared between e.g. an
            # edge and a box (a box "riding" an edge's endpoint) resolves to
            # one Point offset instead of a redundant duplicate.
            off = built_points.get(id(ref))
            if off is None:
                off = builder.build_point(ref.circle, ref.position, ref.value)
                built_points[id(ref)] = off
            return off

        circle_offsets = [builder.build_circle(c) for c in self._circles]
        edge_offsets = [
            builder.build_edge(build_point(a), build_point(b))
            for a, b in self._edges
        ]
        box_offsets = [
            builder.build_box(value, build_point(pt), active)
            for value, pt, active in self._boxes
        ]

        return builder.build_scene_bytes(
            circle_offsets, edge_offsets, box_offsets,
            self.width, self.height,
        )
