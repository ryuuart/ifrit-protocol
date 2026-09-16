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

    def test_explicit_children_and_native_motion_share_one_element_type(self):
        tree = (
            column()
            .padding(12, 18)
            .opacity(entrance(0, 1, duration=0.4))
            .children(
                [
                    text("A heading"),
                    *[
                        row().children([text(str(i)), box().size(20, 8)])
                        for i in range(2)
                    ],
                    *[box().height(i + 1) for i in range(2)],
                ]
            )
        )
        self.assertIsInstance(tree, Element)
        self.assertIsInstance(tree.copy(), Element)

    def test_public_factories_are_native_and_elements_mix_without_conversion(self):
        self.assertIs(raw.Element, Element)
        self.assertIs(raw.box, box)
        self.assertIs(raw.text, text)
        self.assertIs(raw.graphics, graphics)
        native = raw.box().width(24).height(12).fill("#8bd0bd")
        self.assertIs(native, native.width(36))
        tree = row().gap(8).children([text("Native composition"), native])
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
        labels = [
            compose.text("HI"),
            text("HI"),
            compose.text("HI", size=30, color="#ff0000"),
        ]
        panels = [
            compose.box().width(64).height(64).children([label]) for label in labels
        ]
        ctx.render(
            compose.box()
            .row()
            .absolute()
            .inset(0)
            .font(font)
            .ink("#ff0000")
            .children(panels)
        )
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

    def test_children_variadic_and_sequence_forms_preserve_order_and_append(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "children.py"
            output = Path(folder) / "children.png"
            source.write_text("""from sigil.compose import box
from sigil.sketch import sketch

@sketch(size=(12, 32), capture_at=0)
class Children:
    def setup(self, ctx):
        a, b, c = [box().size(4, 4).fill(color) for color in ("#ff0000", "#00ff00", "#0000ff")]
        items = [a, b, c]
        rows = [box().row().size(12, 4) for _ in range(8)]
        assert rows[0].children(items) is rows[0]
        rows[1].children(tuple(items))
        rows[2].children(a, b, c)
        rows[3].children(*items)
        rows[4].children(a).children((b,)).children(c).children().children([]).children(())
        rows[5].children(a)
        try:
            rows[5].children(b, "not an element")
        except TypeError:
            pass
        else:
            raise AssertionError("Invalid children must raise TypeError")
        rows[5].children(b, c)
        rows[6].children(child for child in items)
        rows[7].children(iter(items))
        ctx.render(box().column().children(rows))
""")
            render_file(source, output, at=0)
            pixels = image.load(output).rgba()
            line = bytes((255, 0, 0, 255)) * 4
            line += bytes((0, 255, 0, 255)) * 4
            line += bytes((0, 0, 255, 255)) * 4
            self.assertEqual(pixels, line * 32)

    def test_named_binding_inputs_render_through_both_authoring_paths(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "keywords.py"
            output = Path(folder) / "keywords.png"
            source.write_text("""from sigil.compose import box, column, pen
from sigil.material import skia
from sigil.sketch import sketch


def draw(p):
    p.noStroke()
    p.fill("#ff0000")
    p.rect(x=0, y=0, width=12, height=4)
    p.fill("#00ff00")
    p.rect(x=0, y=4, width=12, height=4)
    p.fill("#0000ff")
    p.rect(x=0, y=8, width=12, height=4)
    p.circle(x=30, y=30, diameter=4)
    p.line(start=(40, 40), end=(48, 48))


@sketch(size=(12, 24), capture_at=0)
class Keywords:
    def setup(self, ctx):
        colors = ("#ff0000", "#00ff00", "#0000ff")
        composed = column().children(tuple(
            box().size(width=12, height=4).fill(skia.Paint.solid(color=color))
            for color in colors
        ))
        ctx.render(element=column().children(composed, pen(key="drawing", program=draw).size(12, 12)))
""")
            render_file(source, output, at=0)
            pixels = image.load(output).rgba()
            self.assertEqual(pixels[: 12 * 12 * 4], pixels[12 * 12 * 4 :])
            self.assertEqual(pixels[:4], bytes((255, 0, 0, 255)))
            self.assertEqual(
                pixels[12 * 4 * 4 : 12 * 4 * 4 + 4], bytes((0, 255, 0, 255))
            )
            self.assertEqual(
                pixels[12 * 8 * 4 : 12 * 8 * 4 + 4], bytes((0, 0, 255, 255))
            )

    def test_children_require_elements_in_both_forms(self):
        for value in (42, True, "hello", None, [text("nested")], {"label": "hello"}):
            with self.subTest(value=value):
                with self.assertRaises(TypeError):
                    row().children([value])
                with self.assertRaises(TypeError):
                    row().children((value,))
                with self.assertRaises(TypeError):
                    row().children(text("valid"), value)
        for value in ("", "text", b"", b"text", None):
            with self.subTest(collection=value), self.assertRaises(TypeError):
                row().children(value)

    def test_constructors_keep_properties_and_children_explicit(self):
        for constructor in (box, row, column):
            with self.subTest(constructor=constructor):
                with self.assertRaises(TypeError):
                    constructor(width=30)
                with self.assertRaises(TypeError):
                    constructor(text("child"))
        with self.assertRaises(AttributeError):
            box().wdith(30)

    def test_invalid_graphics_are_rejected_at_authorship_boundary(self):
        with self.assertRaises(TypeError):
            graphics("drawing", None)

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
