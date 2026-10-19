"""A ribbon brush swept along an open curve, and the width law it reads.

A ribbon is a band the outline carries, as wide across it as its profile
says. Over an open curve it is one band along the curve, and so is a
material keyline: an open curve has no inside, so neither has a side to
keep, and nothing is drawn along the chord between the curve's ends.
"""

import math
import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import compose, material, media
from sigil.geometry import path
from sigil.sketch import renderFile

POINTS = [(20, 100), (70, 40), (130, 160), (180, 100)]


class Profiles(unittest.TestCase):
    def test_a_profile_is_a_width_or_a_law_through_stops(self):
        constant = path.Profile(8)
        self.assertEqual(constant.across(0.3), 8)
        self.assertEqual(constant.max(), 8)
        law = path.Profile([(0, 2), (1, 10)])
        self.assertAlmostEqual(law.across(0.5), 6)
        self.assertEqual(law.max(), 10)
        stepped = path.Profile([(0, 2), (0.5, 10)], between=path.Between.Step)
        self.assertEqual(stepped.across(0.4), 2)
        self.assertTrue(path.Profile([(0, 1), (40, 3)], inPixels=True).keyedInPx())
        self.assertEqual(path.profile.taper(10, 2).across(1), 2)
        self.assertEqual(path.profile.offset(4), path.Profile(4))
        self.assertEqual(path.profile.steps([0.5], [3, 6]).across(0.75), 6)

    def test_a_ribbon_takes_a_width_in_any_spelling_and_any_paint(self):
        for width in (8, [(0, 8), (1, 8)], path.Profile(8)):
            ribbon = compose.brush.ribbon(width, "#ffffff")
            self.assertIsInstance(ribbon, compose.brush.Ribbon)
            self.assertEqual(ribbon.bleed(), 8)
        painted = compose.brush.ribbon(8, material.Material("#ff0000"))
        self.assertIsNotNone(painted.material)
        self.assertEqual(painted, compose.brush.ribbon(8, material.Material("#ff0000")))
        self.assertNotEqual(
            painted, compose.brush.ribbon(6, material.Material("#ff0000"))
        )


class Nibs(unittest.TestCase):
    """A calligraphic ribbon: its width follows the stroke's direction."""

    @staticmethod
    def thickness(band, start, step, count=400):
        """How much of a line of `count` samples from `start` along `step`
        lies inside `band`, in the line's own units."""
        inside = sum(
            1
            for index in range(count)
            if band.contains(start[0] + step[0] * index, start[1] + step[1] * index)
        )
        return inside * math.hypot(*step)

    def test_a_nib_is_widest_across_its_angle_and_thinnest_along_it(self):
        from sigil import skia

        nib = compose.brush.ribbon(12, "#ffffff", nibAngleDegrees=0, nibContrast=0.15)
        self.assertEqual(nib.nibAngleDegrees, 0)
        self.assertAlmostEqual(nib.nibContrast, 0.15)
        self.assertEqual(nib.widthStart, 12)
        self.assertFalse(nib.hasProfile())
        self.assertNotEqual(
            nib, compose.brush.ribbon(12, "#ffffff", nibAngleDegrees=90)
        )
        # Along the nib, then across it: one turn of the same stroke.
        spine = (
            skia.PathBuilder().moveTo(10, 50).lineTo(110, 50).lineTo(110, 150).detach()
        )
        band = nib.band(spine)
        along = self.thickness(band, (60, 40), (0, 0.05))
        across = self.thickness(band, (100, 100), (0.05, 0))
        self.assertAlmostEqual(along, 12 * 0.15, delta=0.3)
        self.assertAlmostEqual(across, 12, delta=0.3)

        # A curve turns the stroke through every direction between: the
        # band widens as the curve turns from along the nib to across it.
        curve = skia.PathBuilder().moveTo(10, 50).quadTo(110, 50, 110, 150).detach()
        curved = nib.band(curve)
        widths = [
            self.thickness(curved, (x, 30), (0, 0.05), count=2400) for x in (20, 60, 90)
        ]
        self.assertLess(widths[0], widths[1])
        self.assertLess(widths[1], widths[2])

    def test_a_taper_runs_from_the_start_width_to_the_end(self):
        taper = compose.brush.ribbon(10, "#ffffff", widthEnd=2)
        self.assertEqual((taper.widthStart, taper.widthEnd), (10, 2))
        self.assertEqual(taper.bleed(), 10)
        from sigil import skia

        band = taper.band(skia.PathBuilder().moveTo(0, 50).lineTo(200, 50).detach())
        start = self.thickness(band, (10, 40), (0, 0.05))
        end = self.thickness(band, (190, 40), (0, 0.05))
        self.assertGreater(start, 8)
        self.assertLess(end, 4)


class Strokes(unittest.TestCase):
    """A curve stroked through the native host, measured in its picture."""

    def render(self, stroke):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        source = Path(directory.name) / "scene.py"
        output = Path(directory.name) / "scene.png"
        source.write_text(
            "from sigil import compose, material\n"
            "from sigil.compose import Composer\n"
            "from sigil.geometry import path\n"
            "from sigil.sketch import sketch\n"
            f"POINTS = {POINTS!r}\n"
            "@sketch(size=(200, 200), captureAt=0)\n"
            "class Scene:\n"
            "    def draw(self, pen):\n"
            "        pen.background('#000000')\n"
            "        outline = path.curveThrough(POINTS)\n"
            "        composer = Composer()\n"
            "        composer.setSize((200, 200))\n"
            "        composer.render(compose.box().width(200).height(200)\n"
            "            .shape(compose.heldPath(outline))\n"
            f"            .stroke({textwrap.dedent(stroke).strip()}))\n"
            "        composer.draw(canvas=pen.canvas())\n"
        )
        renderFile(source, output, at=0)
        picture = media.decode(media.Image, output.read_bytes()).frameAt(0).image
        return picture.width(), picture.rgba()

    def assert_one_band(self, stroke, width):
        """Every inked pixel lies on the band, and the band is `width` wide
        across the curve away from its ends."""
        columns, pixels = self.render(stroke)
        outline = path.curveThrough(POINTS)

        def red(x, y):
            return pixels[4 * (y * columns + x)]

        strays = [
            (x, y)
            for y in range(200)
            for x in range(columns)
            if red(x, y) > 40
            and outline.nearest((x + 0.5, y + 0.5)).gap > width / 2 + 1.5
        ]
        self.assertEqual(strays, [], "ink stands off the curve")
        length = outline.length()
        step = 0.25
        for fraction in (0.15, 0.3, 0.5, 0.7, 0.85):
            pose = outline.poseAt(fraction * length)
            inked = 0
            for index in range(int(24 / step) + 1):
                across = -12 + index * step
                x = pose.position[0] + pose.normal[0] * across
                y = pose.position[1] + pose.normal[1] * across
                if red(math.floor(x), math.floor(y)) > 127:
                    inked += 1
            self.assertAlmostEqual(
                inked * step, width, delta=1.5, msg=f"across at {fraction}"
            )

    def test_a_ribbon_of_width_eight_is_one_band_eight_wide(self):
        self.assert_one_band("compose.brush.ribbon(8, material.Material('#ff0000'))", 8)

    def test_a_material_keyline_over_an_open_curve_draws_no_chord(self):
        self.assert_one_band("material.Material('#ff0000'), 8", 8)


if __name__ == "__main__":
    unittest.main()
