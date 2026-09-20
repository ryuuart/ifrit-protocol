"""The canvas a frame lends: its verbs, its transform, and where the loan ends."""

import builtins
import tempfile
import textwrap
import unittest
from array import array
from pathlib import Path

from sigil import draw, image
from sigil.sketch import render_file


class CanvasSeamContracts(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_canvas_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_canvas_results")

    def render(self, drawing, *, setup="pass"):
        source = Path(self.directory.name) / "drawing.py"
        output = Path(self.directory.name) / "drawing.png"
        source.write_text(
            "import builtins\n"
            "from sigil import draw, skia\n"
            "from sigil.sketch import sketch\n"
            "results = builtins._sigil_canvas_results\n"
            "@sketch(size=(32, 32), capture_at=0)\n"
            "class Drawing:\n"
            "    def setup(self, ctx):\n"
            + textwrap.indent(textwrap.dedent(setup).strip(), "        ")
            + "\n    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        render_file(source, output, at=0)
        return image.decode(output.read_bytes())

    def pixel(self, picture, x, y):
        offset = 4 * (y * picture.width() + x)
        return tuple(picture.rgba()[offset : offset + 4])

    def test_a_canvas_is_lent_by_a_frame_and_never_built(self):
        with self.assertRaises(TypeError):
            draw.Canvas()

    def test_the_lent_canvas_is_the_one_the_pen_is_drawing_on(self):
        picture = self.render("""
            pen.background('#000000')
            pen.translate(16, 0)
            paint = skia.Paint()
            paint.setAntiAlias(False)
            paint.setColor('#ff0000')
            canvas = pen.canvas()
            canvas.drawRect((0, 0, 8, 8), paint)
            canvas.translate(0, 16)
            canvas.drawCircle(4, 4, 4, paint)
        """)
        # The pen's transform is in force when the canvas draws, and the
        # canvas's own translate carries on from it.
        self.assertEqual(self.pixel(picture, 20, 4), (255, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 4, 4), (0, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 20, 20), (255, 0, 0, 255))

    def test_paths_and_vertices_draw_through_the_same_loan(self):
        picture = self.render("""
            pen.background('#000000')
            paint = skia.Paint()
            paint.setAntiAlias(False)
            paint.setColor('#ffffff')
            canvas = pen.canvas()
            canvas.drawPath(skia.Path.Rect((0, 0, 8, 8)), paint)
            canvas.drawVertices(
                skia.Vertices.MakeCopy(
                    skia.VertexMode.Triangles, [(16, 16), (31, 16), (16, 31)]
                ),
                skia.BlendMode.SrcOver,
                paint,
            )
        """)
        self.assertEqual(self.pixel(picture, 4, 4), (255, 255, 255, 255))
        self.assertEqual(self.pixel(picture, 19, 19), (255, 255, 255, 255))
        self.assertEqual(self.pixel(picture, 28, 28), (0, 0, 0, 255))

    def test_saves_restore_to_the_depth_the_canvas_reports(self):
        self.render("""
            pen.background('#000000')
            canvas = pen.canvas()
            results['opened'] = canvas.getSaveCount()
            results['top'] = canvas.save()
            canvas.translate(8, 8)
            results['saved'] = canvas.getSaveCount()
            canvas.restoreToCount(results['top'])
            results['counted'] = canvas.getSaveCount()
            canvas.save()
            canvas.restore()
            results['balanced'] = canvas.getSaveCount()
        """)
        opened = self.results["opened"]
        self.assertEqual(self.results["top"], opened)
        self.assertEqual(self.results["saved"], opened + 1)
        self.assertEqual(self.results["counted"], opened)
        self.assertEqual(self.results["balanced"], opened)

    def test_an_inverted_clip_keeps_what_the_rectangle_leaves(self):
        picture = self.render("""
            pen.background('#000000')
            paint = skia.Paint()
            paint.setAntiAlias(False)
            paint.setColor('#00ff00')
            canvas = pen.canvas()
            canvas.save()
            canvas.clipRect((0, 0, 16, 32), invert=True)
            canvas.drawRect((0, 0, 32, 32), paint)
            canvas.restore()
            canvas.save()
            canvas.clipPath(skia.Path.Rect((0, 0, 8, 8)))
            canvas.drawRect((0, 0, 32, 32), paint)
            canvas.restore()
        """)
        self.assertEqual(self.pixel(picture, 4, 4), (0, 255, 0, 255))
        self.assertEqual(self.pixel(picture, 4, 24), (0, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 24, 16), (0, 255, 0, 255))

    def test_a_point_batch_reads_a_float_buffer_as_it_reads_a_sequence(self):
        drawing = """
            pen.background('#102030')
            pen.stroke('#f0e0d0')
            pen.strokeWeight(3)
            pen.canvas().drawPoints(
                draw.PointMode.Lines, results['points'], pen.strokePaint()
            )
        """
        pairs = [(4, 4), (28, 4), (4, 28), (28, 28)]
        self.results["points"] = pairs
        expected = self.render(drawing).rgba()
        self.results["points"] = array(
            "f", (coordinate for pair in pairs for coordinate in pair)
        )
        self.assertEqual(self.render(drawing).rgba(), expected)

    def test_a_canvas_kept_past_its_frame_refuses_every_verb(self):
        self.render("""
            pen.background('#000000')
            pen.fill('#ffffff')
            results['canvas'] = pen.canvas()
            results['paint'] = pen.fillPaint()
        """)
        canvas, paint = self.results["canvas"], self.results["paint"]
        verbs = {
            "getSaveCount": canvas.getSaveCount,
            "save": canvas.save,
            "translate": lambda: canvas.translate(1, 1),
            "clear": lambda: canvas.clear("#000000"),
            "drawRect": lambda: canvas.drawRect((0, 0, 4, 4), paint),
            "drawCircle": lambda: canvas.drawCircle(4, 4, 2, paint),
            "drawPoints": lambda: canvas.drawPoints(
                draw.PointMode.Points, [(1, 1)], paint
            ),
        }
        for name, verb in verbs.items():
            with (
                self.subTest(verb=name),
                self.assertRaisesRegex(RuntimeError, "callback"),
            ):
                verb()

    def test_the_canvas_of_an_offscreen_frame_closes_with_that_frame(self):
        self.render("""
            pen.background('#000000')
            buffer = draw.Graphics(16, 16)
            child = buffer.begin(pen)
            canvas = child.canvas()
            buffer.end()
            try:
                canvas.getSaveCount()
            except RuntimeError as refusal:
                results['refused'] = str(refusal)
        """)
        self.assertIn("callback", self.results["refused"])

    def test_a_pen_program_runs_on_the_canvas_it_is_given(self):
        picture = self.render("""
            pen.background('#000000')
            def program(inner):
                inner.noStroke()
                inner.fill('#0000ff')
                inner.rect(0, 0, 16, 16)
            draw.on(pen.canvas(), (32, 32), program)
        """)
        self.assertEqual(self.pixel(picture, 8, 8), (0, 0, 255, 255))
        self.assertEqual(self.pixel(picture, 24, 24), (0, 0, 0, 255))


if __name__ == "__main__":
    unittest.main()
