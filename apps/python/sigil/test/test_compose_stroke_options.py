"""A material keyline's options: how wide, on which side, and how its ends
and corners are cut.

The options are one record beside the material, as the C++ verb takes them:
a width, the side of a closed contour, the cap an open contour ends with,
the join its corners meet with and the miter limit. The band they cut is
the one Skia strokes, so a square cap reaches past the end a butt cap stops
at, and a miter fills the corner a bevel cuts.
"""

import tempfile
import unittest
from pathlib import Path

from sigil import compose, material, media
from sigil.geometry import path
from sigil.sketch import renderFile


class Options(unittest.TestCase):
    def test_the_defaults_are_a_hairline_keyline_outside_with_skias_corners(self):
        options = compose.StrokeOptions()
        self.assertEqual(options.width, 1)
        self.assertEqual(options.position, material.StrokePosition.Outside)
        self.assertEqual(options.cap, path.Cap.Butt)
        self.assertEqual(options.join, path.Join.Miter)
        self.assertEqual(options.miterLimit, 4)

    def test_every_field_is_a_keyword_and_the_record_compares_by_value(self):
        options = compose.StrokeOptions(
            width=6,
            position=material.StrokePosition.Center,
            cap=path.Cap.Round,
            join=path.Join.Bevel,
            miterLimit=2,
        )
        self.assertEqual(options.cap, path.Cap.Round)
        self.assertEqual(options.join, path.Join.Bevel)
        self.assertEqual(options, options.copy())
        self.assertNotEqual(options, compose.StrokeOptions(width=6))
        with self.assertRaises(TypeError):
            compose.StrokeOptions(thickness=2)

    def test_a_node_takes_a_material_or_a_colour_with_the_options(self):
        options = compose.StrokeOptions(width=2, cap=path.Cap.Square)
        for node in (compose.box(), compose.text("A")):
            self.assertIs(node.stroke(material.Material("#ff0000"), options), node)
            self.assertIs(node.stroke("#00ff00", options), node)
        # The width-and-side form still stands beside it.
        box = compose.box()
        self.assertIs(box.stroke(material.Material("#ff0000"), 2), box)


class Silhouettes(unittest.TestCase):
    """A zigzag keylined through the native host, read back from its picture."""

    def render(self, options):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        source = Path(directory.name) / "scene.py"
        output = Path(directory.name) / "scene.png"
        source.write_text(
            "from sigil import compose, material, skia\n"
            "from sigil.compose import Composer\n"
            "from sigil.geometry import path\n"
            "from sigil.sketch import sketch\n"
            "@sketch(size=(200, 200), captureAt=0)\n"
            "class Scene:\n"
            "    def draw(self, pen):\n"
            "        pen.background('#000000')\n"
            "        spine = (skia.PathBuilder().moveTo(30, 150).lineTo(70, 50)\n"
            "                 .lineTo(110, 150).lineTo(170, 110).detach())\n"
            "        composer = Composer()\n"
            "        composer.setSize((200, 200))\n"
            "        composer.render(compose.box().width(200).height(200)\n"
            "            .shape(compose.heldPath(path.fromSk(spine)))\n"
            f"            .stroke('#ff0000', {options}))\n"
            "        composer.draw(canvas=pen.canvas())\n"
        )
        renderFile(source, output, at=0)
        picture = media.decode(media.Image, output.read_bytes()).frameAt(0).image
        columns, pixels = picture.width(), picture.rgba()
        return lambda x, y: pixels[4 * (y * columns + x)]

    def keyline(self, cap, join):
        return self.render(
            "compose.StrokeOptions(width=14, "
            "position=material.StrokePosition.Center, "
            f"cap=path.Cap.{cap}, join=path.Join.{join})"
        )

    def test_a_square_cap_reaches_past_the_end_a_butt_cap_stops_at(self):
        # Five pixels past the far end, along the last leg.
        self.assertEqual(self.keyline("Butt", "Miter")(174, 107), 0)
        self.assertGreater(self.keyline("Square", "Miter")(174, 107), 200)

    def test_a_miter_fills_the_corner_a_bevel_cuts(self):
        # Above the bevel's chord at the sharp corner, within the miter.
        self.assertEqual(self.keyline("Butt", "Bevel")(70, 39), 0)
        self.assertGreater(self.keyline("Butt", "Miter")(70, 39), 200)


if __name__ == "__main__":
    unittest.main()
