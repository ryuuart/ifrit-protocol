"""One channel of a material mapped from one stretch of values onto another.

A remap reads its source's channel over `from_` and answers `to` as a
grey, held past either end, so a ramp whose ends lie beyond the stretch
read paints the stated values at its two edges. A layer's mask takes the
mask's own fields as a tuple, the way the native layer takes them in
braces.
"""

import tempfile
import textwrap
import unittest
from pathlib import Path

from sigil import material, media
from sigil.sketch import render_file


class Remap(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="sigil_remap_")
        self.addCleanup(self.directory.cleanup)

    def render(self, body, *, size):
        """Render @p body as a sketch's setup and answer the decoded frame."""
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            "from _sigil import compose\n"
            "from sigil import material\n"
            "from sigil.sketch import sketch\n"
            f"@sketch(size={size!r}, background='#000000', capture_at=0)\n"
            "class Scene:\n"
            + textwrap.indent(textwrap.dedent(body).strip("\n"), "    ")
            + "\n"
        )
        render_file(source, output, at=0)
        return media.decode(media.Image, output.read_bytes()).frameAt(0).image

    def grey(self, picture, x, y, size):
        """The red channel at @p x, @p y of a canvas @p size wide."""
        column = x * picture.width() // size[0]
        row = y * picture.height() // size[1]
        return picture.rgba()[4 * (row * picture.width() + column)]

    def test_a_remap_answers_the_stated_values_at_the_maps_extremes(self):
        size = (40, 30)
        picture = self.render(
            """
            def setup(self, ctx):
                pixels = material.GradientOptions(
                    units=material.GradientUnits.Pixels)
                ramp = material.Paint.linearGradient(
                    (0, 0), (40, 0), ['#000000', '#ffffff'], pixels)
                ctx.render(compose.box(
                    compose.box().size(40, 10).fill(material.remap(
                        ramp, from_=(0.1, 0.9), to=(0.2, 0.7))),
                    compose.box().size(40, 10).fill(
                        material.Material(material.Color(0.2, 0.2, 0.2))),
                    compose.box().size(40, 10).fill(
                        material.Material(material.Color(0.7, 0.7, 0.7))),
                ).column().size(40, 30))
            """,
            size=size,
        )
        low = self.grey(picture, 20, 15, size)
        high = self.grey(picture, 20, 25, size)
        self.assertLess(low, high)
        self.assertAlmostEqual(self.grey(picture, 1, 5, size), low, delta=1)
        self.assertAlmostEqual(self.grey(picture, 38, 5, size), high, delta=1)
        middle = self.grey(picture, 20, 5, size)
        self.assertGreater(middle, low)
        self.assertLess(middle, high)

    def test_the_record_form_is_the_keyword_form(self):
        ramp = material.Material(material.Color(0.5, 0.5, 0.5))
        options = material.RemapOptions(
            from_=material.ValueRange(0.1, 0.9), to=material.ValueRange(0.2, 0.7)
        )
        self.assertEqual(
            material.remap(ramp, options),
            material.remap(ramp, from_=(0.1, 0.9), to=(0.2, 0.7)),
        )

    def test_a_layer_mask_takes_its_fields_as_a_tuple(self):
        depth = material.Material(material.Color(0.5, 0.5, 0.5))
        white = material.Color(1, 1, 1)
        by_tuple = material.Material(material.Color(0, 0, 0)).layer(
            white, mask=(depth, material.ColorChannel.Luminance)
        )
        by_record = material.Material(material.Color(0, 0, 0)).layer(
            white, mask=material.Mask(depth, material.ColorChannel.Luminance)
        )
        self.assertEqual(by_tuple, by_record)
        with self.assertRaises(TypeError):
            material.Material(material.Color(0, 0, 0)).layer(white, mask=(depth,))


if __name__ == "__main__":
    unittest.main()
