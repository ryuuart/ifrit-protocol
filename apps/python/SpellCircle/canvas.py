"""Canvas-style recording API for authoring SpellCircle scenes.

The canvas records plain Python values. It delegates wire-format details to
``SceneBuilder`` only when ``to_bytes`` is called, and leaves transport to
``network.py``.
"""

from __future__ import annotations

from dataclasses import dataclass

from .builder import CircleDefinition, FlatBufferOffset, SceneBuilder


@dataclass(frozen=True, slots=True)
class PointReference:
    """References a fractional position around a recorded circle."""

    circle: CircleDefinition
    position: float = 0.0
    value: str = ""


class SpellCircleCanvas:
    """Records scene primitives and serializes them on demand."""

    def __init__(self, width: float = 4000.0, height: float = 4000.0) -> None:
        """Creates an empty scene in the supplied authoring coordinate space."""
        self.width = float(width)
        self.height = float(height)
        self._circles: list[CircleDefinition] = []
        self._edges: list[tuple[PointReference, PointReference]] = []
        self._boxes: list[tuple[str, PointReference, float]] = []

    @property
    def circles(self) -> tuple[CircleDefinition, ...]:
        """Returns an immutable snapshot of visible circles."""
        return tuple(self._circles)

    @property
    def edges(self) -> tuple[tuple[PointReference, PointReference], ...]:
        """Returns an immutable snapshot of recorded edges."""
        return tuple(self._edges)

    @property
    def boxes(self) -> tuple[tuple[str, PointReference, float], ...]:
        """Returns an immutable snapshot of recorded boxes."""
        return tuple(self._boxes)

    def circle(
        self,
        name: str,
        center_x: float,
        center_y: float,
        radius: int,
        *,
        text_start: float = 0.75,
        active: float = 0.0,
    ) -> CircleDefinition:
        """Adds a visible circle and returns its reusable definition."""
        circle = CircleDefinition(
            name=name,
            center_x=float(center_x),
            center_y=float(center_y),
            radius=int(radius),
            text_start=float(text_start),
            active=float(active),
        )
        self._circles.append(circle)
        return circle

    def anchor(self, x_position: float, y_position: float) -> CircleDefinition:
        """Creates an invisible radius-zero anchor at an arbitrary position."""
        return CircleDefinition(
            name="",
            center_x=float(x_position),
            center_y=float(y_position),
            radius=0,
        )

    def point(
        self,
        circle: CircleDefinition,
        position: float = 0.0,
        value: str = "",
    ) -> PointReference:
        """Creates a point reference, optionally carrying its own text label."""
        return PointReference(circle=circle, position=float(position), value=value)

    def edge(self, first: PointReference, second: PointReference) -> None:
        """Records a line connecting two point references."""
        self._edges.append((first, second))

    def box(
        self,
        value: str,
        point: PointReference,
        *,
        active: float = 0.0,
    ) -> None:
        """Records a labelled box anchored to ``point``."""
        self._boxes.append((value, point, float(active)))

    def to_bytes(self) -> bytes:
        """Serializes the current recording into a FlatBuffers Scene."""
        scene_builder = SceneBuilder()
        point_offsets_by_identity: dict[int, FlatBufferOffset] = {}

        def build_point(point: PointReference) -> FlatBufferOffset:
            """Builds each shared PointReference exactly once per scene."""
            identity = id(point)
            point_offset = point_offsets_by_identity.get(identity)
            if point_offset is None:
                point_offset = scene_builder.build_point(
                    point.circle, point.position, point.value
                )
                point_offsets_by_identity[identity] = point_offset
            return point_offset

        circle_offsets = [
            scene_builder.build_circle(circle) for circle in self._circles
        ]
        edge_offsets = [
            scene_builder.build_edge(build_point(first), build_point(second))
            for first, second in self._edges
        ]
        box_offsets = [
            scene_builder.build_box(value, build_point(point), active)
            for value, point, active in self._boxes
        ]

        return scene_builder.build_scene_bytes(
            circle_offsets,
            edge_offsets,
            box_offsets,
            self.width,
            self.height,
        )


# Existing TouchDesigner projects import these names directly from this
# module, so keep non-advertised aliases while the public API migrates.
PointRef = PointReference
SCCanvas = SpellCircleCanvas
