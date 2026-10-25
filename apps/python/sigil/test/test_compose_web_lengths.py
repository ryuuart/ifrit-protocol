"""One length reading on every length slot, as CSS writes a length.

The font verbs, the line height, the corner radii and the translates read
text, the typed compose units and weave's lengths alike, and a fraction
slot reads a number or a percentage. Each case compares two spellings of
one statement on a rule, which needs no canvas.
"""

import unittest

from sigil import compose, weave


def rule():
    return compose.rule(".a")


class UnitNameTest(unittest.TestCase):
    def test_the_viewport_units_are_the_canvas(self):
        self.assertEqual(compose.Dimension("10vw"), compose.vw(10))
        self.assertEqual(compose.Dimension("10vh"), compose.vh(10))
        self.assertEqual(compose.Dimension("10vmin"), compose.vmin(10))
        self.assertEqual(compose.Dimension("10vmax"), compose.vmax(10))
        self.assertEqual(compose.vmin(10).unit, compose.Dimension.Unit.Vmin)

    def test_a_percentage_is_spelled_in_full(self):
        self.assertEqual(compose.Dimension("50%"), compose.percent(50))
        self.assertEqual(compose.percent(50).unit, compose.Dimension.Unit.Percent)

    def test_the_old_canvas_words_are_no_units(self):
        with self.assertRaises(ValueError):
            compose.Dimension("10pw")


class FontLengthTest(unittest.TestCase):
    def test_both_ems_and_text_are_one_size(self):
        stated = rule().fontSize(weave.em(1.5))
        self.assertEqual(rule().fontSize("1.5em"), stated)
        self.assertEqual(rule().fontSize(compose.em(1.5)), stated)
        self.assertEqual(rule().fontSize("150%"), stated)
        self.assertEqual(rule().fontSize(compose.percent(150)), stated)

    def test_a_number_and_pixels_are_one_size(self):
        self.assertEqual(rule().fontSize("18px"), rule().fontSize(18))
        self.assertEqual(rule().fontSize("9pt"), rule().fontSize(weave.pt(9)))

    def test_tracking_reads_the_same_lengths(self):
        stated = rule().letterSpacing(weave.em(0.1))
        self.assertEqual(rule().letterSpacing("0.1em"), stated)
        self.assertEqual(rule().letterSpacing(compose.em(0.1)), stated)
        self.assertEqual(rule().letterSpacing("10%"), stated)

    def test_a_length_no_font_holds_is_refused(self):
        for value in ("10vw", "auto", "var(--size)", "calc(1em + 2px)"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                rule().fontSize(value)
        with self.assertRaises(ValueError):
            rule().fontSize("large")


class LineHeightTest(unittest.TestCase):
    def test_a_number_is_a_multiple(self):
        self.assertEqual(rule().lineHeight(1.4), rule().lineHeight(weave.Leading.multiple(1.4)))

    def test_pixels_are_an_absolute_pitch(self):
        absolute = rule().lineHeight(weave.Leading.absolute(24))
        self.assertEqual(rule().lineHeight("24px"), absolute)
        self.assertEqual(rule().lineHeight(compose.Dimension(24)), absolute)
        self.assertEqual(rule().lineHeight("18pt"), absolute)

    def test_normal_is_the_faces_own(self):
        self.assertEqual(rule().lineHeight("normal"), rule().lineHeight(weave.Leading.face()))
        self.assertEqual(rule().lineHeight(" Normal "), rule().lineHeight(weave.Leading.face()))

    def test_a_relative_length_is_one_statement_in_any_spelling(self):
        self.assertEqual(rule().lineHeight("1.5em"), rule().lineHeight(compose.em(1.5)))
        self.assertEqual(rule().lineHeight("1.5em"), rule().lineHeight(weave.em(1.5)))
        self.assertNotEqual(rule().lineHeight("1.5em"), rule().lineHeight(1.5))
        self.assertNotEqual(rule().lineHeight("150%"), rule())

    def test_auto_is_no_pitch(self):
        with self.assertRaises(ValueError):
            rule().lineHeight("auto")


class CornerTest(unittest.TestCase):
    def test_a_radius_is_any_length(self):
        self.assertEqual(rule().borderRadius("8px"), rule().borderRadius(8))
        self.assertEqual(rule().borderRadius("50%"), rule().borderRadius(compose.percent(50)))
        self.assertEqual(rule().borderRadius("1em"), rule().borderRadius(weave.em(1)))
        self.assertNotEqual(rule().borderRadius("50%"), rule().borderRadius(50))

    def test_four_radii_and_the_value_agree(self):
        corners = compose.Corners("50%", 4, "1em", 0)
        self.assertEqual(corners.topLeft, compose.percent(50))
        self.assertEqual(corners.topRight, compose.Dimension(4))
        self.assertEqual(rule().borderRadius("50%", 4, "1em", 0), rule().borderRadius(corners))
        self.assertTrue(compose.Corners("1em").any())
        self.assertFalse(compose.Corners().any())


class TranslateTest(unittest.TestCase):
    def test_pixels_are_the_lanes_own_number(self):
        self.assertEqual(rule().translateX("12px"), rule().translateX(12))

    def test_a_percentage_is_a_length_statement(self):
        self.assertEqual(rule().translateX("50%"), rule().translateX(compose.percent(50)))
        self.assertNotEqual(rule().translateX("50%"), rule().translateX(50))
        # A later number replaces the length.
        self.assertEqual(rule().translateX("50%").translateY(0).translateX(3),
                         rule().translateY(0).translateX(3))


class FractionTest(unittest.TestCase):
    def test_a_fraction_is_a_number_or_a_percentage(self):
        self.assertEqual(compose.parseFraction("50%"), 0.5)
        self.assertEqual(compose.parseFraction("0.25"), 0.25)
        self.assertIsNone(compose.parseFraction("50px"))

    def test_opacity_and_scale_read_a_percentage(self):
        self.assertEqual(rule().opacity("50%"), rule().opacity(0.5))
        self.assertEqual(rule().scale("150%"), rule().scale(1.5))
        self.assertEqual(rule().scaleX(compose.percent(25)), rule().scaleX(0.25))
        with self.assertRaises(ValueError):
            rule().opacity("half")


if __name__ == "__main__":
    unittest.main()
