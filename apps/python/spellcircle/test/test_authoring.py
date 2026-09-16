"""Typed scene authoring stays independent of serialization and native hosts."""

import math
import subprocess
import sys
import unittest
from dataclasses import FrozenInstanceError, replace
from pathlib import Path

from SpellCircle import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneBuilder,
    SceneDefinition,
    SpellCircleCanvas,
    decode_scene,
    encode_scene,
)


class Authoring(unittest.TestCase):
    def test_canvas_snapshots_survive_clear_and_keep_shared_values(self):
        canvas = SpellCircleCanvas(640, 360)
        circle = canvas.circle("ring", 320, 180, 80)
        point = canvas.point(circle, 0.25, "east")
        edge = canvas.edge(point, canvas.point(canvas.anchor(20, 30)))
        box = canvas.box("value", point, active=0.5)
        scene = canvas.snapshot()
        self.assertIs(scene.edges[0], edge)
        self.assertIs(scene.boxes[0], box)
        self.assertIs(scene.edges[0].first.circle, scene.circles[0])
        self.assertIs(scene.boxes[0].point, point)
        self.assertEqual(canvas.to_bytes(), encode_scene(scene))
        canvas.clear()
        self.assertEqual(canvas.snapshot(), SceneDefinition(width=640, height=360))
        self.assertEqual(len(scene.circles), 1)
        self.assertEqual(len(scene.edges), 1)
        self.assertEqual(len(scene.boxes), 1)
        self.assertEqual(decode_scene(encode_scene(scene)), scene)

    def test_scene_values_are_frozen_and_can_be_replaced_without_mutation(self):
        original = CircleDefinition("ring", 1, 2, 3)
        updated = replace(original, active=0.75)
        self.assertEqual(original.active, 0)
        self.assertEqual(updated.active, 0.75)
        with self.assertRaises(FrozenInstanceError):
            original.radius = 4

    def test_invalid_wire_numbers_are_rejected_at_authoring_boundary(self):
        for value in (-1, 1 << 32):
            with self.subTest(radius=value), self.assertRaises(ValueError):
                CircleDefinition("ring", 0, 0, value)
        for value in (math.nan, math.inf, -math.inf, 1e40, 10**1000):
            with self.subTest(coordinate=value), self.assertRaises(ValueError):
                CircleDefinition("ring", value, 0, 1)
        for value in (-0.1, 1.1, math.nan):
            with self.subTest(active=value), self.assertRaises(ValueError):
                BoxDefinition(
                    "box", PointReference(CircleDefinition("", 0, 0, 0)), value
                )
        with self.assertRaises(ValueError):
            SceneDefinition(width=-1)
        with self.assertRaises(TypeError):
            SpellCircleCanvas().circle("ring", 0, 0, 2.5)  # type: ignore[arg-type]

    def test_builder_is_single_use_and_reuses_a_shared_circle_table(self):
        circle = CircleDefinition("ring", 4, 8, 16)
        builder = SceneBuilder()
        offset = builder.build_circle(circle)
        self.assertEqual(builder.build_circle(circle), offset)
        point = builder.build_point(circle, 0.25)
        box = builder.build_box("label", point)
        decoded = decode_scene(builder.build_scene_bytes([offset], [], [box]))
        self.assertIs(decoded.boxes[0].point.circle, decoded.circles[0])
        with self.assertRaises(RuntimeError):
            builder.build_circle(circle)
        with self.assertRaises(RuntimeError):
            builder.build_scene_bytes([], [], [])

    def test_model_types_validate_their_children(self):
        circle = CircleDefinition("ring", 0, 0, 1)
        with self.assertRaises(TypeError):
            EdgeDefinition(circle, circle)  # type: ignore[arg-type]
        with self.assertRaises(TypeError):
            SceneDefinition(circles=(PointReference(circle),))  # type: ignore[arg-type]

    def test_import_needs_no_native_or_user_interface_modules(self):
        project = Path(__file__).resolve().parents[1]
        result = subprocess.run(
            [
                sys.executable,
                "-I",
                "-c",
                """import sys
sys.path.insert(0, sys.argv[1])
import SpellCircle
assert not any(name == '_sigil' or name.startswith(('sigil.', 'PySide', 'PyQt')) for name in sys.modules)
assert SpellCircle.SceneDefinition().width == 0
""",
                str(project),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
