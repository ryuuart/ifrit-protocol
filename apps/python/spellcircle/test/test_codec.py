"""The typed scene boundary preserves wire values and reference identity."""

from __future__ import annotations

import struct
import unittest

import flatbuffers
from SpellCircle import Box as box_schema
from SpellCircle import Circle as circle_schema
from SpellCircle import Edge as edge_schema
from SpellCircle import Point as point_schema
from SpellCircle import Scene as scene_schema
from SpellCircle.codec import decode_scene, encode_scene
from SpellCircle.model import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
)


def example() -> SceneDefinition:
    circle = CircleDefinition("圓 / café 🪄", 12.5, -31.25, 42, 0.25, 0.5)
    anchor = CircleDefinition("invisible", 100, 80, 0)
    point = PointReference(circle, 1.25, "shared point λ")
    hidden = PointReference(anchor, -0.5, "anchor")
    return SceneDefinition(
        (circle,),
        (EdgeDefinition(point, hidden), EdgeDefinition(hidden, point)),
        (BoxDefinition("box\n二", point, 0.75), BoxDefinition("", hidden)),
        640,
        480,
    )


def field(raw: object, slot: int) -> int:
    # Generated table positions are used only to corrupt individual wire fields.
    table = raw._tab  # type: ignore[attr-defined]
    return table.Pos + table.Offset(4 + slot * 2)


def target(data: bytes | bytearray, position: int) -> int:
    return position + struct.unpack_from("<I", data, position)[0]


def float32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


class SceneCodec(unittest.TestCase):
    def test_all_values_and_shared_references_round_trip(self) -> None:
        source = example()
        decoded = decode_scene(encode_scene(source))
        self.assertEqual(decoded, source)
        self.assertIs(decoded.circles[0], decoded.edges[0].first.circle)
        self.assertIs(decoded.edges[0].first, decoded.edges[1].second)
        self.assertIs(decoded.edges[0].first, decoded.boxes[0].point)
        self.assertIs(decoded.edges[0].second, decoded.edges[1].first)
        self.assertIs(decoded.edges[0].second, decoded.boxes[1].point)
        self.assertEqual(len(decoded.circles), 1)
        self.assertEqual(decoded.edges[0].second.circle.radius, 0)
        self.assertIsNot(decoded.circles[0], source.circles[0])

    def test_equal_distinct_values_remain_distinct_tables(self) -> None:
        first = CircleDefinition("equal", 10, 20, 30)
        second = CircleDefinition("equal", 10, 20, 30)
        p1 = PointReference(first)
        p2 = PointReference(first)
        p3 = PointReference(second)
        source = SceneDefinition(
            (first, second, first),
            (EdgeDefinition(p1, p2), EdgeDefinition(p2, p3)),
        )
        encoded = encode_scene(source)
        wire = scene_schema.Scene.GetRootAs(encoded)
        self.assertNotEqual(wire.Circles(0)._tab.Pos, wire.Circles(1)._tab.Pos)
        self.assertEqual(wire.Circles(0)._tab.Pos, wire.Circles(2)._tab.Pos)
        self.assertNotEqual(
            wire.Edges(0).First()._tab.Pos, wire.Edges(0).Second()._tab.Pos
        )
        decoded = decode_scene(encoded)
        self.assertEqual(decoded.circles[0], decoded.circles[1])
        self.assertIsNot(decoded.circles[0], decoded.circles[1])
        self.assertIs(decoded.circles[0], decoded.circles[2])
        self.assertEqual(decoded.edges[0].first, decoded.edges[0].second)
        self.assertIsNot(decoded.edges[0].first, decoded.edges[0].second)
        self.assertIs(decoded.edges[0].second, decoded.edges[1].first)
        self.assertIs(decoded.edges[1].second.circle, decoded.circles[1])

    def test_empty_scene_and_missing_optional_vectors(self) -> None:
        self.assertEqual(
            decode_scene(encode_scene(SceneDefinition())), SceneDefinition()
        )
        builder = flatbuffers.Builder(32)
        scene_schema.SceneStart(builder)
        builder.Finish(scene_schema.SceneEnd(builder))
        self.assertEqual(decode_scene(bytes(builder.Output())), SceneDefinition())

    def test_missing_optional_circle_fields_have_schema_defaults(self) -> None:
        builder = flatbuffers.Builder(64)
        circle_schema.CircleStart(builder)
        circle = circle_schema.CircleEnd(builder)
        scene_schema.SceneStartCirclesVector(builder, 1)
        builder.PrependUOffsetTRelative(circle)
        circles = builder.EndVector()
        scene_schema.SceneStart(builder)
        scene_schema.SceneAddCircles(builder, circles)
        builder.Finish(scene_schema.SceneEnd(builder))
        self.assertEqual(
            decode_scene(bytes(builder.Output())),
            SceneDefinition((CircleDefinition("", 0, 0, 0),)),
        )

    def test_decoded_values_own_read_only_and_mutable_inputs(self) -> None:
        source = example()
        encoded = encode_scene(source)
        for data in (
            encoded,
            bytearray(encoded),
            memoryview(encoded),
            memoryview(bytearray(encoded)),
        ):
            with self.subTest(kind=type(data).__name__):
                result = decode_scene(data)
                if isinstance(data, bytearray):
                    data[:] = bytes(len(data))
                elif isinstance(data, memoryview):
                    if not data.readonly:
                        data[:] = bytes(len(data))
                    data.release()
                self.assertEqual(result, source)
                self.assertIs(result.boxes[0].point, result.edges[0].first)

    def test_float_values_use_wire_float32_precision(self) -> None:
        circle = CircleDefinition("rounded", 0.1, -0.2, 0xFFFFFFFF, 0.3, 0.4)
        point = PointReference(circle, 0.6)
        source = SceneDefinition(
            (circle,), (), (BoxDefinition("rounded", point, 0.7),), 123.1, 456.2
        )
        decoded = decode_scene(encode_scene(source))
        self.assertEqual(decoded.circles[0].center_x, float32(0.1))
        self.assertEqual(decoded.circles[0].center_y, float32(-0.2))
        self.assertEqual(decoded.circles[0].text_start, float32(0.3))
        self.assertEqual(decoded.circles[0].active, float32(0.4))
        self.assertEqual(decoded.boxes[0].point.position, float32(0.6))
        self.assertEqual(decoded.boxes[0].active, float32(0.7))
        self.assertEqual(decoded.width, float32(123.1))
        self.assertEqual(decoded.height, float32(456.2))
        self.assertEqual(decoded.circles[0].radius, 0xFFFFFFFF)
        self.assertEqual(decode_scene(encode_scene(decoded)), decoded)

    def test_wrong_input_types_are_rejected(self) -> None:
        with self.assertRaises(TypeError):
            encode_scene({})  # type: ignore[arg-type]
        for data in (None, 128, "scene", [0, 0, 0, 0]):
            with self.subTest(data=data), self.assertRaises(TypeError):
                decode_scene(data)  # type: ignore[arg-type]

    def test_truncated_headers_tables_vectors_and_strings_are_rejected(self) -> None:
        data = encode_scene(example())
        raw = scene_schema.Scene.GetRootAs(data)
        circle = raw.Circles(0)
        string = target(data, field(circle, 1))
        length = struct.unpack_from("<I", data, string)[0]
        vector = target(data, field(raw, 0))
        for end in (0, 1, 3, raw._tab.Pos + 3, vector + 7, string + 4 + length):
            with self.subTest(end=end), self.assertRaises(ValueError):
                decode_scene(data[:end])

    def test_invalid_root_offsets_are_rejected(self) -> None:
        for offset in (0, 1, 0xFFFFFFFF):
            data = bytearray(encode_scene(example()))
            struct.pack_into("<I", data, 0, offset)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                decode_scene(data)

    def test_invalid_vtable_and_field_ranges_are_rejected(self) -> None:
        source = encode_scene(example())
        root = scene_schema.Scene.GetRootAs(source)._tab.Pos
        vtable = root - struct.unpack_from("<i", source, root)[0]
        for position, format_, value in (
            (root, "<i", 0x7FFFFFFF),
            (root, "<i", -0x7FFFFFFF),
            (vtable, "<H", 3),
            (vtable, "<H", 0xFFFE),
            (vtable + 2, "<H", 3),
            (vtable + 4, "<H", 2),
            (vtable + 4, "<H", 0xFFFF),
        ):
            data = bytearray(source)
            struct.pack_into(format_, data, position, value)
            with (
                self.subTest(position=position, value=value),
                self.assertRaises(ValueError),
            ):
                decode_scene(data)

    def test_oversized_vector_and_string_lengths_are_rejected_before_traversal(
        self,
    ) -> None:
        source = encode_scene(example())
        raw = scene_schema.Scene.GetRootAs(source)
        for position in (
            target(source, field(raw, 0)),
            target(source, field(raw, 1)),
            target(source, field(raw, 2)),
            target(source, field(raw.Circles(0), 1)),
        ):
            data = bytearray(source)
            struct.pack_into("<I", data, position, 0xFFFFFFFF)
            with self.subTest(position=position), self.assertRaises(ValueError):
                decode_scene(data)

    def test_vector_entries_and_required_references_cannot_be_null_or_outside_buffer(
        self,
    ) -> None:
        source = encode_scene(example())
        raw = scene_schema.Scene.GetRootAs(source)
        positions = (
            target(source, field(raw, 0)) + 4,
            field(raw.Edges(0), 0),
            field(raw.Edges(0), 1),
            field(raw.Edges(0).First(), 1),
            field(raw.Boxes(0), 1),
        )
        for position in positions:
            for offset in (0, 0xFFFFFFFF):
                data = bytearray(source)
                struct.pack_into("<I", data, position, offset)
                with (
                    self.subTest(position=position, offset=offset),
                    self.assertRaises(ValueError),
                ):
                    decode_scene(data)

    def test_missing_required_point_reference_is_rejected(self) -> None:
        builder = flatbuffers.Builder(64)
        point_schema.PointStart(builder)
        point = point_schema.PointEnd(builder)
        box_schema.BoxStart(builder)
        box_schema.BoxAddPoint(builder, point)
        box = box_schema.BoxEnd(builder)
        scene_schema.SceneStartBoxesVector(builder, 1)
        builder.PrependUOffsetTRelative(box)
        boxes = builder.EndVector()
        scene_schema.SceneStart(builder)
        scene_schema.SceneAddBoxes(builder, boxes)
        builder.Finish(scene_schema.SceneEnd(builder))
        with self.assertRaisesRegex(ValueError, "point.*required"):
            decode_scene(bytes(builder.Output()))

    def test_missing_edge_endpoints_and_box_point_are_rejected(self) -> None:
        for kind in ("edge", "box"):
            builder = flatbuffers.Builder(64)
            if kind == "edge":
                edge_schema.EdgeStart(builder)
                value = edge_schema.EdgeEnd(builder)
                scene_schema.SceneStartEdgesVector(builder, 1)
            else:
                box_schema.BoxStart(builder)
                value = box_schema.BoxEnd(builder)
                scene_schema.SceneStartBoxesVector(builder, 1)
            builder.PrependUOffsetTRelative(value)
            vector = builder.EndVector()
            scene_schema.SceneStart(builder)
            if kind == "edge":
                scene_schema.SceneAddEdges(builder, vector)
            else:
                scene_schema.SceneAddBoxes(builder, vector)
            builder.Finish(scene_schema.SceneEnd(builder))
            with (
                self.subTest(kind=kind),
                self.assertRaisesRegex(ValueError, f"{kind}.*required"),
            ):
                decode_scene(bytes(builder.Output()))

    def test_strings_require_utf8_and_a_terminator(self) -> None:
        source = encode_scene(example())
        raw = scene_schema.Scene.GetRootAs(source)
        string = target(source, field(raw.Circles(0), 1))
        length = struct.unpack_from("<I", source, string)[0]
        for position in (string + 4, string + 4 + length):
            data = bytearray(source)
            data[position] = 0xFF
            with self.subTest(position=position), self.assertRaises(ValueError):
                decode_scene(data)

    def test_nonfinite_or_out_of_domain_wire_values_are_rejected(self) -> None:
        source = encode_scene(example())
        raw = scene_schema.Scene.GetRootAs(source)
        circle = raw.Circles(0)
        for position, value in (
            (field(raw, 3), -1.0),
            (field(raw, 4), float("inf")),
            (field(circle, 0), float("nan")),
            (field(circle, 4), 1.5),
            (field(raw.Edges(0).First(), 2), float("inf")),
            (field(raw.Boxes(0), 2), -0.5),
        ):
            data = bytearray(source)
            struct.pack_into("<f", data, position, value)
            with (
                self.subTest(position=position, value=value),
                self.assertRaises(ValueError),
            ):
                decode_scene(data)


if __name__ == "__main__":
    unittest.main()
