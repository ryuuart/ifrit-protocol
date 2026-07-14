"""FlatBuffers serialization for SpellCircle scenes.

This is the only module that touches the generated FlatBuffers bindings.
Scene authoring and transport work with ordinary Python values instead of
depending on generated table functions.
"""

from __future__ import annotations

from collections.abc import Callable, Sequence
from dataclasses import dataclass

import flatbuffers

from . import Box as box_schema
from . import Circle as circle_schema
from . import Edge as edge_schema
from . import Point as point_schema
from . import Scene as scene_schema
from .Vec2 import CreateVec2

FlatBufferOffset = int
VectorStartFunction = Callable[[flatbuffers.Builder, int], None]


@dataclass(frozen=True, slots=True)
class CircleDefinition:
    """Describes one circle before it is encoded into the wire format."""

    name: str
    center_x: float
    center_y: float
    radius: int
    text_start: float = 0.75
    active: float = 0.0

    @property
    def x(self) -> float:
        """Compatibility view of ``center_x`` for older authoring scripts."""
        return self.center_x

    @property
    def y(self) -> float:
        """Compatibility view of ``center_y`` for older authoring scripts."""
        return self.center_y


class SceneBuilder:
    """Encodes a SpellCircle scene into one FlatBuffer.

    Each ``build_*`` method appends a table to the same builder and returns
    its offset. ``build_scene_bytes`` assembles those offsets and finishes the
    buffer; a SceneBuilder instance is therefore intended for one scene.
    """

    def __init__(self, initial_size: int = 16_384) -> None:
        """Creates a builder with ``initial_size`` bytes of initial capacity."""
        self._builder = flatbuffers.Builder(initial_size)

    def build_circle(self, circle: CircleDefinition) -> FlatBufferOffset:
        """Encodes ``circle`` and returns its FlatBuffers table offset."""
        builder = self._builder
        name_offset = builder.CreateString(circle.name)
        circle_schema.CircleStart(builder)
        circle_schema.CircleAddPos(
            builder, CreateVec2(builder, circle.center_x, circle.center_y)
        )
        circle_schema.CircleAddName(builder, name_offset)
        circle_schema.CircleAddRadius(builder, circle.radius)
        circle_schema.CircleAddTextStart(builder, circle.text_start)
        circle_schema.CircleAddActive(builder, circle.active)
        return circle_schema.CircleEnd(builder)

    def build_point(
        self,
        circle: CircleDefinition,
        position: float,
        value: str = "",
    ) -> FlatBufferOffset:
        """Encodes a point at fractional ``position`` around ``circle``."""
        builder = self._builder
        circle_offset = self.build_circle(circle)
        value_offset = builder.CreateString(value)
        point_schema.PointStart(builder)
        point_schema.PointAddValue(builder, value_offset)
        point_schema.PointAddCircle(builder, circle_offset)
        point_schema.PointAddPosition(builder, float(position))
        return point_schema.PointEnd(builder)

    def build_edge(
        self,
        first_point_offset: FlatBufferOffset,
        second_point_offset: FlatBufferOffset,
    ) -> FlatBufferOffset:
        """Connects two already-built point tables and returns an edge offset."""
        builder = self._builder
        edge_schema.EdgeStart(builder)
        edge_schema.EdgeAddFirst(builder, first_point_offset)
        edge_schema.EdgeAddSecond(builder, second_point_offset)
        return edge_schema.EdgeEnd(builder)

    def build_box(
        self,
        value: str,
        point_offset: FlatBufferOffset,
        active: float = 0.0,
    ) -> FlatBufferOffset:
        """Encodes a labelled box anchored to an already-built point."""
        builder = self._builder
        value_offset = builder.CreateString(value)
        box_schema.BoxStart(builder)
        box_schema.BoxAddValue(builder, value_offset)
        box_schema.BoxAddPoint(builder, point_offset)
        box_schema.BoxAddActive(builder, float(active))
        return box_schema.BoxEnd(builder)

    def _build_vector(
        self,
        start_vector: VectorStartFunction,
        offsets: Sequence[FlatBufferOffset],
    ) -> FlatBufferOffset:
        """Encodes an offset vector in FlatBuffers' required reverse order."""
        builder = self._builder
        start_vector(builder, len(offsets))
        for offset in reversed(offsets):
            builder.PrependUOffsetTRelative(offset)
        return builder.EndVector()

    def build_scene_bytes(
        self,
        circle_offsets: Sequence[FlatBufferOffset],
        edge_offsets: Sequence[FlatBufferOffset],
        box_offsets: Sequence[FlatBufferOffset],
        canvas_width: float = 0.0,
        canvas_height: float = 0.0,
    ) -> bytes:
        """Finishes the builder and returns the serialized scene.

        Positive canvas dimensions describe the authoring coordinate space so
        the renderer can scale it to the native target. Zero means coordinates
        are already expressed in native space.
        """
        builder = self._builder
        edges_vector = self._build_vector(
            scene_schema.SceneStartEdgesVector, edge_offsets
        )
        boxes_vector = self._build_vector(
            scene_schema.SceneStartBoxesVector, box_offsets
        )
        circles_vector = self._build_vector(
            scene_schema.SceneStartCirclesVector, circle_offsets
        )

        scene_schema.SceneStart(builder)
        scene_schema.SceneAddCircles(builder, circles_vector)
        scene_schema.SceneAddEdges(builder, edges_vector)
        scene_schema.SceneAddBoxes(builder, boxes_vector)
        if canvas_width > 0:
            scene_schema.SceneAddWidth(builder, float(canvas_width))
        if canvas_height > 0:
            scene_schema.SceneAddHeight(builder, float(canvas_height))
        builder.Finish(scene_schema.SceneEnd(builder))
        return bytes(builder.Output())


# Compatibility names retained at the package boundary for existing scene
# scripts. New code should use the descriptive names above.
CircleSpec = CircleDefinition
SCBuilder = SceneBuilder
