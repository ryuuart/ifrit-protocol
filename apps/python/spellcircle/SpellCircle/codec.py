"""Encode and decode owned scene values without a renderer or transport."""

from __future__ import annotations

import struct
from collections.abc import Callable

from .Box import Box
from .builder import SceneBuilder
from .Circle import Circle
from .model import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
)
from .Point import Point
from .Scene import Scene


def encode_scene(scene: SceneDefinition) -> bytes:
    """Encode a scene, preserving shared circle and point identities.

    Referenced anchors are encoded without adding them to the visible circle
    vector. Distinct equal values remain distinct tables on the wire.
    """
    if not isinstance(scene, SceneDefinition):
        raise TypeError("scene must be a SceneDefinition")
    builder = SceneBuilder()
    points: dict[int, int] = {}

    def point_offset(point: PointReference) -> int:
        identity = id(point)
        if identity not in points:
            points[identity] = builder.build_point(
                point.circle, point.position, point.value
            )
        return points[identity]

    circles = [builder.build_circle(circle) for circle in scene.circles]
    edges = [
        builder.build_edge(point_offset(edge.first), point_offset(edge.second))
        for edge in scene.edges
    ]
    boxes = [
        builder.build_box(box.value, point_offset(box.point), box.active)
        for box in scene.boxes
    ]
    return builder.build_scene_bytes(circles, edges, boxes, scene.width, scene.height)


def _require(data: bytes, position: int, size: int, label: str) -> None:
    if position < 0 or size < 0 or position > len(data) - size:
        raise ValueError(f"Malformed scene: {label} is outside the buffer")


def _target(data: bytes, position: int, label: str) -> int:
    _require(data, position, 4, label)
    offset = struct.unpack_from("<I", data, position)[0]
    if offset < 4:
        raise ValueError(f"Malformed scene: {label} has an invalid offset")
    target = position + offset
    _require(data, target, 4, label)
    return target


class _CheckedTable:
    """Check the ranges the generated reader will access for known fields.

    The FlatBuffers Python reader has no verifier and its string slices may
    silently truncate. Bounds are checked before using its field readers;
    unknown fields remain available to newer versions of the wire schema.
    """

    def __init__(
        self, data: bytes, position: int, widths: tuple[int, ...], label: str
    ) -> None:
        self.data = data
        self.label = label
        _require(data, position, 4, label)
        vtable = position - struct.unpack_from("<i", data, position)[0]
        _require(data, vtable, 4, f"{label} vtable")
        vtable_size, object_size = struct.unpack_from("<HH", data, vtable)
        if vtable_size < 4 or vtable_size % 2 or object_size < 4:
            raise ValueError(f"Malformed scene: {label} has an invalid table size")
        _require(data, vtable, vtable_size, f"{label} vtable")
        _require(data, position, object_size, label)
        self.fields: list[int] = []
        for slot, width in enumerate(widths):
            entry = 4 + slot * 2
            offset = (
                struct.unpack_from("<H", data, vtable + entry)[0]
                if entry < vtable_size
                else 0
            )
            if offset and (offset < 4 or offset > object_size - width):
                raise ValueError(
                    f"Malformed scene: {label} field {slot} is outside its table"
                )
            self.fields.append(position + offset if offset else 0)

    def target(self, slot: int, *, required: bool = True) -> int:
        field = self.fields[slot]
        if not field:
            if required:
                raise ValueError(
                    f"Malformed scene: {self.label} field {slot} is required"
                )
            return 0
        return _target(self.data, field, f"{self.label} field {slot}")

    def vector(self, slot: int) -> range:
        position = self.target(slot, required=False)
        if not position:
            return range(0)
        length = struct.unpack_from("<I", self.data, position)[0]
        start = position + 4
        _require(self.data, start, length * 4, f"{self.label} vector {slot}")
        return range(start, start + length * 4, 4)


class _Decoder:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.circles: dict[int, CircleDefinition] = {}
        self.points: dict[int, PointReference] = {}
        self.strings: dict[int, str] = {}

    def text(
        self, table: _CheckedTable, slot: int, read: Callable[[], bytes | None]
    ) -> str:
        position = table.target(slot, required=False)
        if not position:
            return ""
        if position not in self.strings:
            length = struct.unpack_from("<I", self.data, position)[0]
            _require(self.data, position + 4, length + 1, f"{table.label} string")
            if self.data[position + 4 + length] != 0:
                raise ValueError(
                    f"Malformed scene: {table.label} string is not terminated"
                )
            value = read()
            if value is None:
                raise ValueError(f"Malformed scene: {table.label} string is missing")
            try:
                self.strings[position] = value.decode("utf-8")
            except UnicodeDecodeError as error:
                raise ValueError(
                    f"Malformed scene: {table.label} string is not UTF-8"
                ) from error
        return self.strings[position]

    def circle(self, position: int) -> CircleDefinition:
        if position not in self.circles:
            checked = _CheckedTable(self.data, position, (8, 4, 4, 4, 4), "circle")
            raw = Circle()
            raw.Init(self.data, position)
            center = raw.Pos()
            self.circles[position] = CircleDefinition(
                self.text(checked, 1, raw.Name),
                center.X() if center is not None else 0.0,
                center.Y() if center is not None else 0.0,
                raw.Radius(),
                raw.TextStart(),
                raw.Active(),
            )
        return self.circles[position]

    def point(self, position: int) -> PointReference:
        if position not in self.points:
            checked = _CheckedTable(self.data, position, (4, 4, 4), "point")
            raw = Point()
            raw.Init(self.data, position)
            self.points[position] = PointReference(
                self.circle(checked.target(1)),
                raw.Position(),
                self.text(checked, 0, raw.Value),
            )
        return self.points[position]

    def edge(self, position: int) -> EdgeDefinition:
        checked = _CheckedTable(self.data, position, (4, 4), "edge")
        return EdgeDefinition(
            self.point(checked.target(0)), self.point(checked.target(1))
        )

    def box(self, position: int) -> BoxDefinition:
        checked = _CheckedTable(self.data, position, (4, 4, 4), "box")
        raw = Box()
        raw.Init(self.data, position)
        return BoxDefinition(
            self.text(checked, 0, raw.Value),
            self.point(checked.target(1)),
            raw.Active(),
        )

    def scene(self) -> SceneDefinition:
        position = _target(self.data, 0, "root")
        checked = _CheckedTable(self.data, position, (4, 4, 4, 4, 4), "scene")
        raw = Scene.GetRootAs(self.data)
        return SceneDefinition(
            tuple(
                self.circle(_target(self.data, entry, "circle"))
                for entry in checked.vector(0)
            ),
            tuple(
                self.edge(_target(self.data, entry, "edge"))
                for entry in checked.vector(1)
            ),
            tuple(
                self.box(_target(self.data, entry, "box"))
                for entry in checked.vector(2)
            ),
            raw.Width(),
            raw.Height(),
        )


def decode_scene(data: bytes | bytearray | memoryview) -> SceneDefinition:
    """Decode owned values, validating the modelled fields of the wire format.

    Input is copied before traversal, so returned values survive mutation or
    release of the caller's buffer. Shared circle and point tables retain their
    identity. Missing optional strings and coordinates use their schema defaults;
    missing point references and malformed field values raise ``ValueError``.
    """
    if not isinstance(data, (bytes, bytearray, memoryview)):
        raise TypeError("data must be bytes, bytearray or memoryview")
    return _Decoder(bytes(data)).scene()
