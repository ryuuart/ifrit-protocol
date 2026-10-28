"""The shapes one @sketch decorator reads off a class: a scene3d sketch
that describes a world frame, a scene2d sketch that describes a tree at
every step, a setup that returns its tree, the stylesheet and the capture
moment stated on the decorator."""

import builtins
import tempfile
import textwrap
import unittest

from sigil import media
from pathlib import Path

from sigil import world
from sigil.compose import Element, box
from sigil.sketch import kit, renderFile, sketch
from support.context import DecoratedModule, RecordingContext


class Context(RecordingContext):
    elapsed = 0.25


class SketchShapes(DecoratedModule, unittest.TestCase):
    moduleName = "_sigil_shapes_fixture"

    def test_the_capture_moment_has_one_spelling(self):
        class Body:
            def setup(self, ctx):
                pass

        ctx = Context()
        self.decorate(Body, captureAt=2.5)().setup(ctx)
        self.assertIn(("capture", 2.5), ctx.calls)
        with self.assertRaises(TypeError):
            sketch(capture_at=1)
        with self.assertRaises(ValueError):
            sketch(captureAt=-1)

    def test_a_setup_that_returns_a_tree_renders_it(self):
        tree = box().width(10).height(10)

        class Body:
            def setup(self):
                return tree

        ctx = Context()
        self.decorate(Body)().setup(ctx)
        self.assertEqual(ctx.calls[-1], ("render", tree))

    def test_a_described_tree_is_rendered_at_setup_and_at_every_step(self):
        class Body:
            def describe(self, seconds) -> Element:
                return box().key(f"at {seconds}")

        cls = self.decorate(Body)
        self.assertEqual(cls.__sigil_kind__, "scene2d")
        body, ctx = cls(), Context()
        body.setup(ctx)
        body.update(0.5, ctx)
        rendered = [element for name, element in ctx.calls if name == "render"]
        self.assertEqual(len(rendered), 2)
        self.assertTrue(all(isinstance(element, Element) for element in rendered))

    def test_a_describe_annotated_to_return_a_frame_makes_a_scene3d_sketch(self):
        class Body:
            def setup(self, ctx):
                ctx.calls.append(("setup", None))

            def describe(self, seconds: float) -> world.Frame:
                return world.Frame(world.group().key(f"at {seconds}"))

        cls = self.decorate(Body, size=(64, 48), background="#000000", captureAt=0.5)
        self.assertEqual(cls.__sigil_kind__, "scene3d")
        body, ctx = cls(), Context()
        body.setup(ctx)
        self.assertEqual(
            ctx.calls,
            [
                ("canvas", (64, 48)),
                ("background", "#000000"),
                ("capture", 0.5),
                ("setup", None),
            ],
        )
        self.assertIsInstance(body.describe(1.0), world.Frame)

    def test_a_scene3d_sketch_has_no_update_or_draw(self):
        for extra in ("update", "draw"):
            with self.subTest(extra=extra):
                self.setUp()
                namespace = {
                    "describe": lambda self, seconds: None,
                    extra: lambda self, *arguments: None,
                }
                namespace["describe"].__annotations__ = {"return": "world.Frame"}
                with self.assertRaisesRegex(TypeError, "scene3d"):
                    self.decorate(type("Body", (), namespace))

    def test_a_describe_with_another_signature_is_refused_by_name(self):
        class Body:
            def describe(self, reading, extra):
                return box()

        with self.assertRaisesRegex(TypeError, r"describe\(self, seconds\)"):
            self.decorate(Body)

    def test_a_frame_from_an_unannotated_describe_names_the_annotation(self):
        class Body:
            def describe(self, seconds):
                return world.Frame(world.group())

        with self.assertRaisesRegex(TypeError, "world.Frame"):
            self.decorate(Body)().setup(Context())

    def staged_corner(self, source):
        """The top-left pixel of the sketch @p source declares, rendered
        headless, after checking it was staged at its own size."""
        builtins._sigil_shapes_seen = []
        self.addCleanup(builtins.__dict__.pop, "_sigil_shapes_seen", None)
        with tempfile.TemporaryDirectory() as directory:
            entry = Path(directory) / "staged.py"
            output = Path(directory) / "staged.png"
            entry.write_text(textwrap.dedent(source))
            renderFile(entry, output)
            corner = media.load(output).frameAt(0).image.rgba()[0:4]
        size, *_ = builtins._sigil_shapes_seen
        self.assertEqual(size, (48, 36))
        return corner

    def test_the_decorators_stylesheet_is_staged_at_the_root(self):
        look = kit.featureTheme(kit.Density.Spacious)
        palette = look.palette
        palette.ground = "#336699"
        look.palette = palette
        builtins._sigil_shapes_style_sheet = look.styleSheet()
        self.addCleanup(builtins.__dict__.pop, "_sigil_shapes_style_sheet", None)
        corner = self.staged_corner("""
            import builtins
            from sigil.sketch import kit, sketch

            @sketch(size=(48, 36), captureAt=0,
                    styleSheet=builtins._sigil_shapes_style_sheet)
            class Staged:
                def setup(self, ctx):
                    builtins._sigil_shapes_seen.append(ctx.size)
                    return kit.page(kit.well(), title="Staged")
        """)
        # The page's ground is the staged stylesheet's, read where the page
        # lands.
        self.assertAlmostEqual(corner[0] / 255, 0x33 / 255, places=1)
        self.assertAlmostEqual(corner[2] / 255, 0x99 / 255, places=1)

    def test_a_variant_is_a_stock_stylesheet_plus_a_rule(self):
        corner = self.staged_corner("""
            import builtins
            from sigil.compose import rule
            from sigil.sketch import kit, sketch

            @sketch(size=(48, 36), captureAt=0,
                    styleSheet=kit.featureStyleSheet(kit.Density.Spacious)
                    + rule(":root").var("palette-ground", "#336699"))
            class Variant:
                def setup(self, ctx):
                    builtins._sigil_shapes_seen.append(ctx.size)
                    return kit.page(kit.well(), title="Variant")
        """)
        # The page and the canvas both take the ground the rule states over
        # the stock stylesheet's.
        self.assertAlmostEqual(corner[0] / 255, 0x33 / 255, places=1)
        self.assertAlmostEqual(corner[2] / 255, 0x99 / 255, places=1)

    def test_the_deprecated_theme_and_sheet_keywords_still_stage(self):
        with self.assertWarns(DeprecationWarning):
            sketch(size=(48, 36), theme=kit.houseTheme())
        corner = self.staged_corner("""
            import builtins
            import warnings
            from sigil.compose import StyleSheet, rule
            from sigil.sketch import kit, sketch

            warnings.simplefilter("ignore", DeprecationWarning)

            @sketch(size=(48, 36), captureAt=0,
                    theme=kit.featureTheme(kit.Density.Spacious),
                    sheet=StyleSheet([
                        rule(":root").var("palette-ground", "#336699"),
                    ]))
            class Deprecated:
                def setup(self, ctx):
                    builtins._sigil_shapes_seen.append(ctx.size)
                    return kit.page(kit.well(), title="Deprecated")
        """)
        # The builder's stylesheet, with the rules joined after it.
        self.assertAlmostEqual(corner[0] / 255, 0x33 / 255, places=1)
        self.assertAlmostEqual(corner[2] / 255, 0x99 / 255, places=1)


if __name__ == "__main__":
    unittest.main()
