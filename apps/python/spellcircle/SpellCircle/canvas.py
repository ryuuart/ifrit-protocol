"""Canvas-style recording API for authoring SpellCircle scenes.

The canvas records plain Python values. A snapshot can be inspected, encoded,
saved, transported or consumed by a host without retaining the canvas.
"""

from __future__ import annotations

from .codec import encode_scene
from .model import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
)


class SpellCircleCanvas:
    """Records scene primitives and serializes them on demand."""

    def __init__(self, width: float = 4000.0, height: float = 4000.0) -> None:
        """Creates an empty scene in the supplied authoring coordinate space."""
        empty = SceneDefinition(width=width, height=height)
        self.width = empty.width
        self.height = empty.height
        self._circles: list[CircleDefinition] = []
        self._edges: list[EdgeDefinition] = []
        self._boxes: list[BoxDefinition] = []

    @property
    def circles(self) -> tuple[CircleDefinition, ...]:
        """Returns an immutable snapshot of visible circles."""
        return tuple(self._circles)

    @property
    def edges(self) -> tuple[EdgeDefinition, ...]:
        """Returns an immutable snapshot of recorded edges."""
        return tuple(self._edges)

    @property
    def boxes(self) -> tuple[BoxDefinition, ...]:
        """Returns an immutable snapshot of recorded boxes."""
        return tuple(self._boxes)

    def circle(
        self,
        name: str,
        center_x: float,
        center_y: float,
        radius: int,
        *,
        text_start: float = 0.0,
        active: float = 0.0,
    ) -> CircleDefinition:
        """Adds a visible circle and returns its reusable definition.

        ``text_start`` anchors the middle of the name label, as a fraction
        of one full turn measured clockwise from 12 o'clock — the same
        origin ``point`` positions use. The default 0.0 centers the label
        at the top.
        """
        circle = CircleDefinition(
            name=name,
            center_x=center_x,
            center_y=center_y,
            radius=radius,
            text_start=text_start,
            active=active,
        )
        self._circles.append(circle)
        return circle

    def anchor(self, x_position: float, y_position: float) -> CircleDefinition:
        """Creates an invisible radius-zero anchor at an arbitrary position."""
        return CircleDefinition(
            name="",
            center_x=x_position,
            center_y=y_position,
            radius=0,
        )

    def point(
        self,
        circle: CircleDefinition,
        position: float = 0.0,
        value: str = "",
    ) -> PointReference:
        """Creates a point reference, optionally carrying its own text label."""
        return PointReference(circle=circle, position=position, value=value)

    def edge(self, first: PointReference, second: PointReference) -> EdgeDefinition:
        """Records a line connecting two point references."""
        edge = EdgeDefinition(first, second)
        self._edges.append(edge)
        return edge

    def box(
        self,
        value: str,
        point: PointReference,
        *,
        active: float = 0.0,
    ) -> BoxDefinition:
        """Records a labelled box anchored to ``point``."""
        box = BoxDefinition(value, point, active)
        self._boxes.append(box)
        return box

    def snapshot(self) -> SceneDefinition:
        """Returns an immutable scene sharing this recording's immutable values."""
        return SceneDefinition(
            self.circles, self.edges, self.boxes, self.width, self.height
        )

    def clear(self) -> None:
        """Clears recorded geometry while retaining the canvas dimensions."""
        self._circles.clear()
        self._edges.clear()
        self._boxes.clear()

    def to_bytes(self) -> bytes:
        """Serializes the current recording into a FlatBuffers Scene."""
        return encode_scene(self.snapshot())
