"""FlatBuffers serialization for SpellCircle scenes.

Scene authoring and transport work with ordinary Python values. This builder
and the decoder contain the generated-table operations at the wire boundary.
"""

from __future__ import annotations

from collections.abc import Callable, Sequence

import flatbuffers

from . import Box as box_schema
from . import Circle as circle_schema
from . import Edge as edge_schema
from . import Point as point_schema
from . import Scene as scene_schema
from .model import CircleDefinition, PointReference, _active, _number, _text
from .Vec2 import CreateVec2

FlatBufferOffset = int
VectorStartFunction = Callable[[flatbuffers.Builder, int], None]


class SceneBuilder:
    """Encodes a SpellCircle scene into one FlatBuffer.

    Each ``build_*`` method appends a table to the same builder and returns
    its offset. ``build_scene_bytes`` assembles those offsets and finishes the
    buffer; a SceneBuilder instance is therefore intended for one scene.
    """

    def __init__(self, initial_size: int = 16_384) -> None:
        """Creates a builder with ``initial_size`` bytes of initial capacity."""
        self._builder = flatbuffers.Builder(initial_size)
        self._circles: dict[int, tuple[CircleDefinition, FlatBufferOffset]] = {}
        self._finished = False

    def _open(self) -> None:
        if self._finished:
            raise RuntimeError("A SceneBuilder encodes one scene; create a new builder")

    def build_circle(self, circle: CircleDefinition) -> FlatBufferOffset:
        """Encodes ``circle`` and returns its FlatBuffers table offset."""
        self._open()
        if not isinstance(circle, CircleDefinition):
            raise TypeError("circle must be a CircleDefinition")
        cached = self._circles.get(id(circle))
        if cached is not None:
            return cached[1]
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
        offset = circle_schema.CircleEnd(builder)
        # Retain the value with its offset so Python cannot reuse its identity.
        self._circles[id(circle)] = (circle, offset)
        return offset

    def build_point(
        self,
        circle: CircleDefinition,
        position: float,
        value: str = "",
    ) -> FlatBufferOffset:
        """Encodes a point at fractional ``position`` around ``circle``."""
        self._open()
        point = PointReference(circle, position, value)
        builder = self._builder
        circle_offset = self.build_circle(circle)
        value_offset = builder.CreateString(point.value)
        point_schema.PointStart(builder)
        point_schema.PointAddValue(builder, value_offset)
        point_schema.PointAddCircle(builder, circle_offset)
        point_schema.PointAddPosition(builder, point.position)
        return point_schema.PointEnd(builder)

    def build_edge(
        self,
        first_point_offset: FlatBufferOffset,
        second_point_offset: FlatBufferOffset,
    ) -> FlatBufferOffset:
        """Connects two already-built point tables and returns an edge offset."""
        self._open()
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
        self._open()
        value = _text("value", value)
        active = _active(active)
        builder = self._builder
        value_offset = builder.CreateString(value)
        box_schema.BoxStart(builder)
        box_schema.BoxAddValue(builder, value_offset)
        box_schema.BoxAddPoint(builder, point_offset)
        box_schema.BoxAddActive(builder, active)
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
        self._open()
        canvas_width = _number("canvas_width", canvas_width, 0)
        canvas_height = _number("canvas_height", canvas_height, 0)
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
        self._finished = True
        return bytes(builder.Output())
