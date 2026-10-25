"""Gradients, shadows and filters written as CSS text, beside the typed
doors: a fill reads a gradient function, the shadow verbs read CSS's
shadow lists, every filter slot reads CSS's filter functions, and text
none of them reads raises with the text named."""

import unittest

from sigil import compose, material
from sigil.compose import box, text

Filter = material.Filter


class AGradientIsReadFromItsCssText(unittest.TestCase):
    def test_a_fill_reads_the_three_gradient_functions(self):
        for written, typed in (
            (
                "linear-gradient(45deg, red, blue 40%)",
                material.linearGradient([("red", 0), ("blue", "40%")], 45),
            ),
            (
                "radial-gradient(circle closest-side at 30% 40%, red, blue)",
                material.radialGradient(
                    [("red", 0), ("blue", 1)],
                    shape="circle",
                    extent="closest-side",
                    at=(0.3, 0.4),
                ),
            ),
            (
                "conic-gradient(from 90deg, red, blue)",
                material.conicGradient([("red", 0), ("blue", 1)], from_=90),
            ),
        ):
            with self.subTest(written=written):
                self.assertEqual(compose.Fill(written), compose.Fill(typed))
                self.assertIsInstance(box().fill(written), compose.Element)

    def test_a_colour_is_still_a_colour(self):
        self.assertEqual(compose.Fill("#ff0000"), compose.Fill.color("#ff0000"))

    def test_a_gradient_the_reader_cannot_hold_raises_naming_it(self):
        for written in (
            "linear-gradient(red, 30%, blue)",
            "radial-gradient(40px, red, blue)",
            "linear-gradient(red 1em, blue)",
        ):
            with self.subTest(written=written):
                with self.assertRaisesRegex(ValueError, "no CSS gradient") as raised:
                    box().fill(written)
                self.assertIn(written, str(raised.exception))


class AShadowListIsReadFromItsCssText(unittest.TestCase):
    def test_box_shadow_reads_the_css_list(self):
        self.assertIsInstance(
            box().boxShadow("0 4px 12px 2px rgba(0 0 0 / .3) inset"), compose.Element
        )
        self.assertIsInstance(
            box().boxShadow("3px 3px red, -1px 0 2px 1px olive"), compose.Element
        )
        self.assertIsInstance(box().boxShadow("none"), compose.Element)

    def test_text_shadow_reads_the_css_list(self):
        self.assertIsInstance(
            text("a").textShadow("3px 3px red, yellow -3px 3px 2px, 3px -3px"),
            compose.Text,
        )

    def test_a_shadow_the_reader_cannot_hold_raises_naming_it(self):
        with self.assertRaisesRegex(ValueError, "no CSS box-shadow") as raised:
            box().boxShadow("0.1em 0.1em #333")
        self.assertIn("0.1em 0.1em #333", str(raised.exception))
        with self.assertRaisesRegex(ValueError, "no CSS text-shadow"):
            text("a").textShadow("1px 1px inset")


class AFilterIsReadFromItsCssText(unittest.TestCase):
    def test_the_filter_text_is_the_chain_written(self):
        self.assertEqual(
            Filter("blur(4px) sepia(50%)"), Filter.blur(4).then(Filter.sepia(0.5))
        )
        self.assertEqual(Filter("hue-rotate(0.25turn)"), Filter.hueRotate(90))
        self.assertEqual(
            Filter("drop-shadow(16px 16px 20px blue)"),
            Filter.dropShadow(
                "blue", material.ShadowOptions(offset=(16, 16), blur=20)
            ),
        )
        self.assertTrue(Filter("none").isNone())

    def test_every_filter_slot_reads_the_text(self):
        self.assertIsInstance(
            box().filter("blur(4px) sepia(50%) drop-shadow(0 2px 4px #0008)"),
            compose.Element,
        )
        self.assertIsInstance(box().backdropFilter("blur(8px)"), compose.Element)

    def test_a_filter_the_reader_cannot_hold_raises_naming_it(self):
        for written in ("blur(1em)", "drop-shadow(2px 2px)", "wobble(2)"):
            with self.subTest(written=written):
                with self.assertRaisesRegex(ValueError, "no CSS filter") as raised:
                    box().filter(written)
                self.assertIn(written, str(raised.exception))


if __name__ == "__main__":
    unittest.main()
