"""The catalogue's layer stack sketch: the stack its setup builds holds
the four layers the sketch's README lists, each meeting the layers beneath
it as stated, and at the capture moment the picture shows the erase, the
mask and the rings."""

import importlib.util
import sys
import tempfile
import unittest
from math import cos, pi, sin
from pathlib import Path

from support.rendering import renderedImage, scaledPixel

SKETCH = (
    Path(__file__).resolve().parents[3]
    / "grimoire"
    / "sketches"
    / "python_layer_stack"
    / "python_layer_stack.py"
)

_specification = importlib.util.spec_from_file_location("layer_stack", SKETCH)
module = importlib.util.module_from_spec(_specification)
sys.modules[_specification.name] = module
_specification.loader.exec_module(module)

CAPTURE = 2.5  # the sketch's declared capture moment


def hexColour(text):
    return tuple(int(text[index : index + 2], 16) for index in (1, 3, 5))


def near(colour, expected, tolerance=6):
    return all(
        abs(actual - wanted) <= tolerance for actual, wanted in zip(colour, expected)
    )


class Layers(unittest.TestCase):
    def test_the_stack_holds_the_four_layers_each_meeting_the_ones_beneath_as_stated(
        self,
    ):
        stack = module.buildStack()
        self.assertEqual(stack.names, ["rings", "field", "hatch", "cut"])
        blends = {layer.name: layer.blend().name.lower() for layer in stack.layers}
        self.assertEqual(
            blends,
            {"rings": "blend", "field": "screen", "hatch": "blend", "cut": "remove"},
        )
        self.assertAlmostEqual(stack.find("field").opacity, 0.8, places=5)
        self.assertEqual(stack.find("hatch").mask(), stack.find("field"))
        self.assertIsNone(stack.find("rings").mask())


class Picture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as directory:
            cls.picture = renderedImage(
                SKETCH, Path(directory) / "layer_stack.png", at=CAPTURE
            )
        cls.ground = hexColour(module.GROUND)

    def colour(self, x, y):
        size = (module.WIDTH, module.HEIGHT)
        return scaledPixel(self.picture, round(x), round(y), size, channels=3)

    def ringPoint(self, radius, turn):
        centerX, centerY = module.CENTER
        return centerX + radius * cos(turn), centerY + radius * sin(turn)

    def test_the_circling_disc_erases_the_ring_beneath_it(self):
        turn = 2 * pi * CAPTURE / module.ORBIT_SECONDS
        middle = module.RADII[1]
        # The middle ring where the disc stands over it, and the same ring
        # on the far side of the centre, which the disc does not reach.
        erased = self.colour(*self.ringPoint(middle, turn))
        kept = self.colour(*self.ringPoint(middle, turn + pi))
        self.assertTrue(near(erased, self.ground), erased)
        self.assertFalse(near(kept, self.ground, tolerance=40), kept)

    def test_the_hatching_shows_only_inside_the_words(self):
        # Unmasked, the hatching would cross any run one spacing long; away
        # from the words and the dial, masked by the words, every run is
        # the ground.
        for left, row in ((1100, 120), (1100, 700), (60, 120)):
            for x in range(left, left + 2 * module.HATCH_SPACING):
                self.assertTrue(near(self.colour(x, row), self.ground), (x, row))
        hatch = hexColour(module.HATCH_INK)
        wordRow = [self.colour(x, module.CENTER[1]) for x in range(170, 260)]
        self.assertTrue(
            any(near(colour, hatch, tolerance=40) for colour in wordRow),
            "no hatching inside the first word",
        )


if __name__ == "__main__":
    unittest.main()
