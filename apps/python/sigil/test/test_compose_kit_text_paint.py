"""The kit's text paints reach Python: a field, a moment and a box make a
material a run of type is inked with, as `compose::kit::textPaint` does."""

import tempfile
import unittest
from pathlib import Path

from sigil import material, media, motion
from sigil.compose import kit
from sigil.geometry.path import Rect
from sigil.sketch import renderFile

SCENE = """
from sigil.compose import box, kit, text
from sigil.sketch import sketch

@sketch(size=(480, 200), background="#000000")
class Scene:
    def setup(self, ctx):
        ctx.render(box().padding(20).children([
            text("SIGIL").fontSize(140).ink({ink})
        ]))
"""


def lit(ink):
    """How many pixels of the word inked with @p ink are lit on black."""
    with tempfile.TemporaryDirectory() as folder:
        source, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
        source.write_text(SCENE.format(ink=ink))
        renderFile(source, output)
        pixels = media.load(output).frameAt(0).image.rgba()
    return sum(
        1
        for offset in range(0, len(pixels), 4)
        if pixels[offset] + pixels[offset + 1] + pixels[offset + 2] > 96
    )


class TextPaint(unittest.TestCase):
    def test_a_word_inked_with_the_sparkle_is_painted(self):
        sparkle = lit("kit.textPaint(field=kit.TextField.Sparkle, time=6.4)")
        self.assertGreater(sparkle, 0)
        # The sparkle is points of light on a clear ground, so it lights
        # far less of the word than a solid ink does.
        self.assertLess(sparkle, lit("'#ffffff'"))
        self.assertGreater(lit("kit.textPaint(field=kit.TextField.Water)"), 0)

    def test_the_paint_is_stated_by_keywords_or_by_its_record(self):
        stated = kit.textPaint(field=kit.TextField.Sparkle, time=6.4)
        self.assertIsInstance(stated, material.Material)
        record = kit.TextPaint(field=kit.TextField.Sparkle, time=6.4)
        self.assertEqual(record.field, kit.TextField.Sparkle)
        self.assertAlmostEqual(record.time.value, 6.4, places=5)
        self.assertEqual(kit.textPaint(record), stated)
        self.assertNotEqual(kit.textPaint(field=kit.TextField.Water), stated)
        boxed = kit.textPaint(field=kit.TextField.Clouds, box=Rect(min=(0, 0), max=(200, 80)))
        self.assertNotEqual(boxed, kit.textPaint(field=kit.TextField.Clouds))
        self.assertEqual(kit.TextPaint(box=(0, 0, 200, 80)).box.width(), 200)
        # A bound moment is taken as it is.
        kit.textPaint(field=kit.TextField.Stars, time=motion.animatable(1.0))


if __name__ == "__main__":
    unittest.main()
