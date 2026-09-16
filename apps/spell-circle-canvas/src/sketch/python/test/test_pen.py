"""Drawing and lifetime contracts exercised through real native sessions."""

import builtins
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import draw, image
from sigil.core import chance
from sigil.sketch import render_file


class PenContracts(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_pen_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_pen_results")

    def render(self, drawing, *, setup="pass", at=0):
        source = Path(self.directory.name) / "drawing.py"
        output = Path(self.directory.name) / "drawing.png"
        source.write_text(
            "import builtins\n"
            "from sigil import draw\n"
            "from sigil.native import compose\n"
            "from sigil.sketch import sketch\n"
            "results = builtins._sigil_pen_results\n"
            "@sketch(size=(32, 32), capture_at=0)\n"
            "class Drawing:\n"
            "    def setup(self, ctx):\n"
            + textwrap.indent(textwrap.dedent(setup).strip(), "        ")
            + "\n    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        render_file(source, output, at=at)
        return image.decode(output.read_bytes())

    def pixel(self, picture, x, y):
        offset = 4 * (y * picture.width() + x)
        return tuple(picture.rgba()[offset : offset + 4])

    def test_native_math_and_copyable_seeded_streams(self):
        self.assertEqual(draw.map(2, 0, 1, 10, 20, True), 20)
        self.assertEqual(draw.dist(0, 0, 3, 4), 5)
        self.assertAlmostEqual(draw.degrees(draw.radians(90)), 90, places=4)
        stream = chance.Stream.xorshift(123)
        stream.bits()
        copied = stream.copy()
        self.assertEqual(
            [stream.bits() for _ in range(8)], [copied.bits() for _ in range(8)]
        )
        self.assertEqual(
            [chance.Stream.halton(2, i).unit() for i in range(4)], [0, 0.5, 0.25, 0.75]
        )

    def test_clip_and_native_point_batches_share_pen_style(self):
        picture = self.render("""
            pen.background('#000000')
            pen.noStroke()
            pen.fill('#ff0000')
            pen.push()
            pen.clip(lambda: pen.rect(0, 0, 16, 32))
            pen.rect(0, 0, 32, 32)
            pen.pop()
            pen.stroke('#00ff00')
            pen.strokeWeight(4)
            canvas = pen.canvas()
            canvas.drawPoints(draw.PointMode.Points, [(24, 8), (24, 24)], pen.strokePaint())
            results['canvas'] = canvas
        """)
        self.assertEqual(self.pixel(picture, 8, 16), (255, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 24, 16), (0, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 24, 8), (0, 255, 0, 255))
        with self.assertRaisesRegex(RuntimeError, "callback"):
            self.results["canvas"].getSaveCount()

    def test_offscreen_frames_close_automatically_and_resize_keeps_pixels(self):
        self.render(
            """
            if pen.frameCount == 1:
                child = self.buffer.begin(pen)
                child.background('#123456')
                results['child'] = child
            else:
                try:
                    _ = results['child'].width
                except RuntimeError:
                    results['expired'] = True
                self.buffer.resize(24, 20)
                self.buffer.begin(pen)
                self.buffer.end()
                results['image'] = self.buffer.image()
            pen.image(self.buffer, 0, 0)
        """,
            setup="self.buffer = draw.Graphics(12, 10)",
            at=1 / 60,
        )
        self.assertTrue(self.results["expired"])
        picture = self.results["image"]
        self.assertEqual((picture.width(), picture.height()), (24, 20))
        self.assertEqual(self.pixel(picture, 12, 10), (0x12, 0x34, 0x56, 255))

    def test_closed_host_does_not_end_a_reopened_graphics_frame(self):
        picture = self.render(
            """
            outer_pen = self.outer.begin(pen)
            self.shared.draw(outer_pen, lambda child: child.background('#000000'))
            current = self.shared.begin(pen)
            self.outer.end()
            current.background('#123456')
            self.shared.end()
            pen.image(self.shared, 0, 0)
        """,
            setup="""
            self.outer = draw.Graphics(32, 32)
            self.shared = draw.Graphics(32, 32)
        """,
        )
        self.assertEqual(self.pixel(picture, 16, 16), (0x12, 0x34, 0x56, 255))

    def test_graphics_requires_a_host_pen(self):
        buffer = draw.Graphics(12, 10)
        with self.assertRaisesRegex(TypeError, "host pen"):
            buffer.begin(None)
        with self.assertRaisesRegex(TypeError, "host pen"):
            buffer.draw(None, lambda pen: None)

    def test_retained_guests_have_distinct_python_call_sites(self):
        picture = self.render(
            """
            pen.background('#000000')
            pen.element(self.guest, (0, 0, 12, 12))
            pen.element(self.guest, (16, 16, 12, 12))
        """,
            setup="""
            from sigil.motion import entrance
            self.guest = compose.box().absolute().inset(0).fill('#ffffff').opacity(entrance(0, 1, duration=0.25))
        """,
            at=0.1,
        )
        first = self.pixel(picture, 6, 6)
        second = self.pixel(picture, 22, 22)
        self.assertEqual(first, second)
        self.assertGreater(first[0], 0)
        self.assertLess(first[0], 255)

    def test_failed_offscreen_program_expires_its_pen(self):
        with self.assertRaisesRegex(RuntimeError, "buffer failed"):
            self.render(
                """
                def paint(child):
                    results['child'] = child
                    child.push()
                    child.translate(20, 20)
                    raise ValueError('buffer failed')
                self.buffer.draw(pen, paint)
            """,
                setup="self.buffer = draw.Graphics(16, 16)",
            )
        with self.assertRaisesRegex(RuntimeError, "callback"):
            _ = self.results["child"].width

    def test_color_models_and_dynamic_dash_sequences(self):
        picture = self.render("""
            pen.background(0)
            pen.colorMode(draw.HSB, 360, 100, 100, 1)
            color = pen.color(120, 100, 100, 1)
            results['green'] = (color.r, color.g, color.b, color.a)
            pen.stroke(color)
            pen.strokeWeight(2)
            pen.strokeDash([4, 4], 0)
            pen.line(0, 16, 32, 16)
        """)
        self.assertEqual(self.results["green"], (0, 1, 0, 1))
        self.assertEqual(self.pixel(picture, 2, 16), (0, 255, 0, 255))
        self.assertEqual(self.pixel(picture, 6, 16), (0, 0, 0, 255))


if __name__ == "__main__":
    unittest.main()
