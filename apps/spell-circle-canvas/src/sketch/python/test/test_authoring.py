"""Authoring contracts exercised against the built native extension."""

import sys
import tempfile
import unittest
from pathlib import Path
from types import ModuleType

from sigil import image
from sigil.compose import Element, box, column, graphics, row, text
from sigil.motion import entrance
from sigil.native import compose as raw
from sigil.sketch import render_file, sketch


class Context:
    def __init__(self):
        self.calls = []

    def canvas(self, *size):
        self.calls.append(("canvas", size))

    def background(self, color):
        self.calls.append(("background", color))

    def captureAt(self, seconds):
        self.calls.append(("capture", seconds))

    def render(self, element):
        self.calls.append(("render", element))


class Authoring(unittest.TestCase):
    def setUp(self):
        self.module = ModuleType("_sigil_authoring_fixture")
        sys.modules[self.module.__name__] = self.module
        self.addCleanup(sys.modules.pop, self.module.__name__, None)

    def decorate(self, cls, **options):
        cls.__module__ = self.module.__name__
        return sketch(**options)(cls)

    def test_decorator_exports_class_and_configures_before_setup(self):
        class Body:
            def setup(self, ctx):
                ctx.calls.append(("setup", self))

        cls = self.decorate(Body, size=(300, 200), background="#abcdef", capture_at=2.5)
        body, ctx = cls(), Context()
        body.setup(ctx)
        self.assertIs(cls, Body)
        self.assertIs(self.module.__sigil_sketch__, Body)
        self.assertEqual(
            ctx.calls,
            [
                ("canvas", (300, 200)),
                ("background", "#abcdef"),
                ("capture", 2.5),
                ("setup", body),
            ],
        )

    def test_draw_installs_native_element_before_explicit_setup(self):
        class Body:
            def draw(self, pen):
                pass

            def setup(self, ctx):
                ctx.calls.append(("setup", None))

        ctx = Context()
        self.decorate(Body)().setup(ctx)
        self.assertEqual(ctx.calls[-2][0], "render")
        self.assertIsInstance(ctx.calls[-2][1], Element)
        self.assertEqual(ctx.calls[-1][0], "setup")

    def test_nested_children_and_native_motion_share_one_element_type(self):
        tree = column(
            "A heading",
            None,
            [row(text(str(i)), box(width=20, height=8)) for i in range(2)],
            (box(height=i + 1) for i in range(2)),
            padding=(12, 18),
            opacity=entrance(0, 1, duration=0.4),
        )
        self.assertIsInstance(tree, Element)
        self.assertIsInstance(tree.copy(), Element)

    def test_raw_and_convenience_elements_mix_without_conversion(self):
        self.assertIs(raw.Element, Element)
        native = raw.box().width(24).height(12).fill("#8bd0bd")
        self.assertIs(native, native.width(36))
        tree = row("Native beside convenient", native, gap=8)
        self.assertIsInstance(tree, raw.Element)
        self.assertIs(tree, tree.gap(12))
        self.assertIs(native, native.opacity(0.5))

    def test_raw_and_convenience_text_inherit_size_and_ink(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "inheritance.py"
            output = Path(folder) / "inheritance.png"
            source.write_text("""from sigil.compose import text
from sigil.native import compose
from sigil.sketch import sketch
from sigil.weave import Type
@sketch(size=(192, 64), capture_at=0, background="#000000")
class Inheritance:
    def setup(self, ctx):
        font = Type()
        font.size = 30
        labels = [compose.text("HI"), text("HI"), compose.text("HI", size=30, color="#ff0000")]
        panels = [compose.box().width(64).height(64).children([label]) for label in labels]
        ctx.render(compose.box().row().absolute().inset(0).font(font).ink("#ff0000").children(panels))
""")
            render_file(source, output, at=0)
            pixels = image.load(output).rgba()
            panels = [
                b"".join(
                    pixels[(y * 192 + x) * 4 : (y * 192 + x + 64) * 4]
                    for y in range(64)
                )
                for x in (0, 64, 128)
            ]
            self.assertEqual(panels[0], panels[2])
            self.assertEqual(panels[1], panels[2])
            self.assertTrue(any(panels[2][::4]))

    def test_invalid_children_and_recursive_collections_explain_the_input(self):
        for value in (42, True, b"hello", {"label": "hello"}, {"unordered"}):
            with (
                self.subTest(value=value),
                self.assertRaisesRegex(TypeError, "children must"),
            ):
                row(value)
        recursive = []
        recursive.append(recursive)
        with self.assertRaisesRegex(ValueError, "recursive collection"):
            column(recursive)

    def test_unknown_property_suggests_the_supported_name(self):
        with self.assertRaisesRegex(TypeError, "did you mean 'width'"):
            box(wdith=30)
        with self.assertRaisesRegex(TypeError, "absolute must"):
            box(absolute="yes")

    def test_invalid_graphics_are_rejected_at_authorship_boundary(self):
        with self.assertRaisesRegex(TypeError, "must be callable"):
            graphics(None, key="drawing")
        with self.assertRaisesRegex(ValueError, "nonempty string"):
            graphics(lambda pen: None, key="")

    def test_invalid_canvas_metadata_fails_when_declared(self):
        for size in ((0, 100), (float("nan"), 100), (100, -2)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                sketch(size=size)
        with self.assertRaises(TypeError):
            sketch(size="300,200")
        with self.assertRaises(ValueError):
            sketch(capture_at=-1)

    def test_multiple_sketch_exports_are_rejected(self):
        class First:
            def setup(self, ctx):
                pass

        class Second:
            def setup(self, ctx):
                pass

        self.decorate(First)
        with self.assertRaisesRegex(ValueError, "only one"):
            self.decorate(Second)


if __name__ == "__main__":
    unittest.main()
