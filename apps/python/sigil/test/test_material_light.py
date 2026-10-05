"""A light value and a relief accept Python authoring forms."""

import copy
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import compose, material, media, motion
from sigil.sketch import render_file


class Light(unittest.TestCase):
    def test_point_and_spot_values_keep_their_native_fields(self):
        for kind in (material.LightKind.Point, material.LightKind.Spot):
            with self.subTest(kind=kind):
                source = material.Light(
                    kind=kind,
                    direction=30,
                    elevation=75,
                    color="#ff8040",
                    intensity=0.7,
                    ambient=0.1,
                    position=[12, 24, 180],
                    range=800,
                    innerAngle=15,
                    outerAngle=35,
                )
                self.assertEqual(source.kind, kind)
                self.assertEqual(source.position, (12, 24, 180))
                self.assertEqual(source.range, 800)
                self.assertEqual(source.innerAngle, 15)
                self.assertEqual(source.outerAngle, 35)
                self.assertAlmostEqual(source.intensity, 0.7)
                self.assertFalse(source.isRunning())
        with self.assertRaises(TypeError):
            material.Light(position=(1, 2))

    def test_shared_live_strength_remains_bound(self):
        strength = motion.animatable(0.25)
        source = material.Light(intensity=strength)
        # The native Light keeps the same cell that the model owns.
        strength.set(0.8)
        self.assertAlmostEqual(source.intensity, 0.8)

    def test_color_inputs_keep_constants_and_described_values(self):
        expected = material.Color(1, 0, 0, 1)
        for value in ("#ff0000", (1, 0, 0), [1, 0, 0, 1], expected):
            with self.subTest(value=value):
                source = material.Light(color=value)
                self.assertEqual(source.color, expected)
                self.assertIsInstance(source.color, material.Color)
                self.assertFalse(source.isRunning())
        described = motion.ColorTween(to="#0000ff")
        self.assertEqual(
            material.Light(color=described).color, material.Color(0, 0, 1, 1)
        )
        self.assertEqual(
            material.Light(color=motion.animate(described)).color,
            material.Color(0, 0, 1, 1),
        )

    def test_copied_lights_share_a_live_color_and_return_owned_samples(self):
        tint = motion.animatable("#ff0000")
        source = material.Light(color=tint)
        peer = material.Light(color=tint)
        copied = copy.copy(source)
        before = copied.color
        tint.copy().set((0, 0, 1))
        for light in (source, peer, copied):
            self.assertEqual(light.color, material.Color(0, 0, 1, 1))
            self.assertTrue(light.isRunning())
            with self.assertRaises(AttributeError):
                light.color = material.Color(0, 1, 0, 1)
        self.assertEqual(before, material.Color(1, 0, 0, 1))
        sample = copied.color
        sample.b = 0
        self.assertEqual(copied.color, material.Color(0, 0, 1, 1))

    def test_color_engine_and_timeline_animate_the_copied_source_cell(self):
        for timeline in (False, True):
            with self.subTest(timeline=timeline):
                tint = motion.animatable("#ff0000")
                copied = copy.copy(material.Light(color=tint))
                engine = motion.Engine()
                tween = motion.ColorTween(
                    to="#0000ff", duration=0.2, ease=motion.ease.linear
                )
                if timeline:
                    engine.timeline().add(tint, tween)
                else:
                    engine.animate(tint, tween)
                engine.advance(0.4)
                self.assertEqual(copied.color, tint.value)
                for actual, expected in zip(copied.color, (0, 0, 1, 1)):
                    self.assertAlmostEqual(actual, expected, places=5)

    def test_a_light_copies_and_deep_copies_as_a_value(self):
        original = material.Light(intensity=0.4, color="#ff8040")
        for made in (copy.copy(original), copy.deepcopy(original)):
            self.assertIsInstance(made, material.Light)
            self.assertEqual(made, original)
            self.assertIsNot(made, original)

    def test_a_deep_copied_light_keeps_reading_its_live_color(self):
        # A memo model is snapshotted by a deep copy, and the light inside
        # it still follows the cell the author holds.
        tint = motion.animatable("#ff0000")
        model = copy.deepcopy({"light": material.Light(color=tint)})
        tint.set("#0000ff")
        self.assertEqual(model["light"].color, material.Color(0, 0, 1, 1))


class Relief(unittest.TestCase):
    def test_relief_decorates_a_shape_and_type_with_a_material(self):
        finish = material.Material("#c0c0c0").surface(roughness=0.4)
        raised = compose.relief(finish)
        self.assertIsInstance(raised, compose.Decoration)
        self.assertEqual(raised, compose.relief(finish, shoulder=2, depth=1))
        self.assertNotEqual(raised, compose.relief(finish, shoulder=4, depth=-1))
        with self.assertRaises(TypeError):
            compose.relief(finish, 2.0)
        panel = compose.box().width(32).height(32)
        self.assertIs(panel.background(raised), panel)
        word = compose.text("Lit")
        self.assertIs(word.foreground(compose.relief(finish, depth=-0.5)), word)
        self.assertIs(word.decorationOutline(compose.Boundary.Glyphs), word)

    def test_a_relief_paints_its_material_flat_on_a_page(self):
        # A stated surface paints its colours too: Compose shades nothing.
        code = """
            from sigil import compose, material
            from sigil.sketch import sketch

            @sketch(size=(32, 32), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    finish = material.Material("#ff0000").surface(
                        metallic=1, roughness=0.2)
                    ctx.render(compose.box().width(32).height(32).background(
                        compose.relief(finish, shoulder=4, depth=1)))
        """
        with tempfile.TemporaryDirectory(prefix="sigil_relief_") as folder:
            source, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            source.write_text(textwrap.dedent(code))
            render_file(source, output, at=0)
            pixels = media.load(output).frameAt(0).image.rgba()
            offset = (16 * 32 + 16) * 4
            for actual, expected in zip(pixels[offset : offset + 4], (255, 0, 0, 255)):
                self.assertAlmostEqual(actual, expected, delta=2)


if __name__ == "__main__":
    unittest.main()
