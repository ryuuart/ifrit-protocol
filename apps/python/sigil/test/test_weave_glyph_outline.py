"""A string as the contours of its glyphs, stood where it was asked.

The outline is set on one line at a size and placed by its anchor: the
centre of its own tight bounds by default, or the start of its baseline.
"""

import unittest

from sigil import weave


class GlyphOutline(unittest.TestCase):
    def setUp(self):
        self.fonts = weave.FontContext()

    def test_one_glyph_is_centred_where_it_was_asked(self):
        outline = weave.glyphOutline(self.fonts, "H", size=100, at=(240, 130))
        self.assertFalse(outline.empty())
        bounds = outline.bounds()
        self.assertGreater(bounds.width(), 0)
        self.assertGreater(bounds.height(), 0)
        centre = bounds.centre()
        self.assertAlmostEqual(centre[0], 240, delta=1e-3)
        self.assertAlmostEqual(centre[1], 130, delta=1e-3)
        options = weave.GlyphOutlineOptions(size=100, at=(240, 130))
        self.assertEqual(weave.glyphOutline(self.fonts, "H", options).bounds(), bounds)

    def test_a_baseline_anchor_begins_the_pen_there(self):
        outline = weave.glyphOutline(
            self.fonts,
            "H",
            size=100,
            at=(40, 300),
            anchor=weave.GlyphOutlineAnchor.Baseline,
        )
        bounds = outline.bounds()
        self.assertGreaterEqual(bounds.left(), 40 - 1e-3)
        self.assertLessEqual(bounds.bottom(), 300 + 1e-3)
        self.assertLess(bounds.top(), 300)


if __name__ == "__main__":
    unittest.main()
