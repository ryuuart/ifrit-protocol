"""Native lighting values and composition scopes accept Python authoring forms."""

import copy
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import compose, material, media, motion
from sigil.sketch import render_file


class Lighting(unittest.TestCase):
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
        self.assertEqual(material.Light(), material.studio())
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
            for make in (material.Light, material.studio):
                with self.subTest(value=value, constructor=make):
                    source = make(color=value)
                    self.assertEqual(source.color, expected)
                    self.assertIsInstance(source.color, material.Color)
                    self.assertFalse(source.isRunning())
        described = motion.ColorTween(to="#0000ff")
        self.assertEqual(
            material.Light(color=described).color, material.Color(0, 0, 1, 1)
        )
        self.assertEqual(
            material.studio(color=motion.animate(described)).color,
            material.Color(0, 0, 1, 1),
        )

    def test_copied_lights_share_a_live_color_and_return_owned_samples(self):
        tint = motion.animatable("#ff0000")
        source = material.Light(color=tint)
        peer = material.studio(color=tint)
        copied = material.Lighting(lights=[source]).lights[0]
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
                copied = material.Lighting(material.studio(color=tint)).lights[0]
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

    def test_retained_compose_samples_a_copied_source_color_without_redescribing(self):
        code = """
            from sigil import compose, material, motion
            from sigil.sketch import sketch

            @sketch(size=(32, 32), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    self.tint = motion.animatable("#ff0000")
                    original = material.Light(color=self.tint, elevation=90,
                                              intensity=1, ambient=0)
                    copied = material.Lighting(original).lights[0]
                    receiver = (compose.box().width(32).height(32)
                                .cache(compose.Cache.Texture)
                                .fill(material.Material("#ffffff")
                                      .surface(metallic=0, roughness=.5)))
                    ctx.render(compose.scene().width(32).height(32).children(
                        compose.stack().cache(compose.Cache.Texture).children(
                            compose.light(copied).key("key")), receiver))

                def update(self, elapsed):
                    self.tint.set("#ff0000" if elapsed < .5 else "#0000ff")
        """
        with tempfile.TemporaryDirectory(prefix="sigil_light_color_") as folder:
            source, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            source.write_text(textwrap.dedent(code))
            for seconds, dominant, absent in ((0.25, 0, 2), (1, 2, 0)):
                with self.subTest(seconds=seconds):
                    render_file(source, output, at=seconds)
                    pixels = media.load(output).frameAt(0).image.rgba()
                    offset = (16 * 32 + 16) * 4
                    rgba = pixels[offset : offset + 4]
                    self.assertGreater(rgba[dominant], 40)
                    self.assertLess(rgba[absent], 5)
                    self.assertLess(rgba[1], 5)
                    self.assertEqual(rgba[3], 255)

    def test_sources_are_copied_from_an_iterable_into_one_context(self):
        first, second = material.studio(), material.Light(intensity=0.2)
        sources = [first, second]
        lighting = material.Lighting(lights=iter(sources))
        sources.clear()
        self.assertEqual(lighting.lights, [first, second])
        detached = lighting.lights
        detached.clear()
        self.assertEqual(len(lighting.lights), 2)
        self.assertTrue(lighting)
        self.assertFalse(material.Lighting())
        self.assertEqual(material.Lighting(first).lights, [first])
        with self.assertRaises(TypeError):
            material.Lighting(lights=[first, object()])

    def test_coordinate_frame_is_explicit_for_each_constructor_form(self):
        source = material.studio()
        around = material.environment(material.Material("#ffffff"), size=(32, 16))
        for make in (
            lambda **kwargs: material.Lighting(lights=[source], **kwargs),
            lambda **kwargs: material.Lighting(source, **kwargs),
            lambda **kwargs: material.Lighting(environment=around, **kwargs),
            lambda **kwargs: material.Lighting(around, **kwargs),
        ):
            with self.subTest(constructor=make):
                self.assertEqual(make().frame, material.LightingFrame.Surface)
                lighting = make(frame=material.LightingFrame.Scene)
                self.assertEqual(lighting.frame, material.LightingFrame.Scene)
                with self.assertRaises(AttributeError):
                    lighting.frame = material.LightingFrame.Surface

    def test_lighting_conversions_and_environment_are_available_on_receivers(self):
        source = material.studio()
        around = material.environment(material.Material("#ffffff"), size=(32, 16))
        lighting = material.Lighting(lights=[source], environment=around)
        self.assertEqual(lighting.environment, around)
        self.assertEqual(material.Lighting(environment=around).environment, around)
        self.assertEqual(material.Lighting(around).environment, around)
        self.assertEqual(material.Lighting(source, around), lighting)
        pixel = media.fromRgba(bytes([0, 0, 0, 255]), 1, 1)
        for node in (compose.box(), compose.text("Lit"), compose.image(pixel)):
            for context in (source, around, lighting):
                self.assertIs(node.lighting(context), node)

    def test_lighting_values_copy_and_deep_copy_as_values(self):
        source = material.Light(intensity=0.4, color="#ff8040")
        around = material.environment(material.Material("#ffffff"), size=(32, 16))
        for original in (source, around, material.Lighting(source, around)):
            with self.subTest(value=type(original).__name__):
                for made in (copy.copy(original), copy.deepcopy(original)):
                    self.assertIsInstance(made, type(original))
                    self.assertEqual(made, original)
                    self.assertIsNot(made, original)

    def test_a_deep_copied_light_keeps_reading_its_live_color(self):
        # A memo model is snapshotted by a deep copy, and the light inside
        # it still follows the cell the author holds.
        tint = motion.animatable("#ff0000")
        model = copy.deepcopy({"light": material.Light(color=tint)})
        tint.set("#0000ff")
        self.assertEqual(model["light"].color, material.Color(0, 0, 1, 1))

    def test_a_surface_is_lit_by_a_light_an_environment_or_a_lighting(self):
        source = material.studio()
        around = material.environment(material.Material("#ffffff"), size=(32, 16))
        for context in (source, around, material.Lighting(source, around)):
            with self.subTest(context=type(context).__name__):
                made = material.Material("#808080")
                self.assertIs(made.surface(roughness=0.4, lighting=context), made)
        self.assertEqual(
            material.Material("#808080").surface(lighting=source),
            material.Material("#808080").surface(lighting=material.Lighting(source)),
        )
        self.assertNotEqual(
            material.Material("#808080").surface(lighting=source),
            material.Material("#808080").surface(),
        )

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

    def test_relief_with_no_lighting_in_force_paints_its_material_flat(self):
        code = """
            from sigil import compose, material
            from sigil.sketch import sketch

            @sketch(size=(32, 32), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    ctx.render(compose.box().width(32).height(32).background(
                        compose.relief(material.Material("#ff0000"))))
        """
        with tempfile.TemporaryDirectory(prefix="sigil_relief_") as folder:
            source, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            source.write_text(textwrap.dedent(code))
            render_file(source, output, at=0)
            pixels = media.load(output).frameAt(0).image.rgba()
            offset = (16 * 32 + 16) * 4
            for actual, expected in zip(pixels[offset : offset + 4], (255, 0, 0, 255)):
                self.assertAlmostEqual(actual, expected, delta=2)

    def test_scene_accepts_the_same_child_forms_as_other_containers(self):
        source = material.Light(kind=material.LightKind.Point, position=(0, 0, 180))
        nodes = (compose.light(source), compose.text("Lit"))
        for children in (nodes, list(nodes), iter(nodes)):
            self.assertIsInstance(compose.scene(children), compose.Element)
        self.assertIsInstance(compose.scene(*nodes), compose.Element)
        around = material.environment(material.Material("#ffffff"), size=(32, 16))
        owner = compose.scene()
        self.assertIs(owner.environment(around), owner)
        with self.assertRaises(TypeError):
            compose.scene(compose.box(), object())

    def test_light_is_a_leaf_and_child_rejection_does_not_modify_it(self):
        source = compose.light(source=material.studio())
        self.assertIs(source.children([]), source)
        for children in ((compose.box(),), [compose.box()], iter([compose.box()])):
            with self.assertRaises(ValueError):
                source.children(children)
        with self.assertRaises(ValueError):
            source.children(compose.box())
        self.assertIs(source.translateX(12), source)


if __name__ == "__main__":
    unittest.main()
