"""The longhands that take CSS's value spaces: a family by name, a style
that is upright, italic or an oblique angle, and an indent in any length.

Each case asks the value a rule states, which needs no canvas: two
spellings of one statement describe the same rule, and a later statement
replaces an earlier one of the same property.
"""

import unittest

from sigil import compose, weave

FontStyle = compose.FontStyle


def rule():
    return compose.rule(".a")


class FontFamilyTest(unittest.TestCase):
    def test_a_family_is_named(self):
        self.assertEqual(rule().fontFamily("Georgia"), rule().fontFamily("Georgia"))
        self.assertNotEqual(rule().fontFamily("Georgia"), rule().fontFamily("Impact"))
        self.assertNotEqual(rule().fontFamily("Georgia"), rule())


class FontStyleTest(unittest.TestCase):
    def test_a_number_is_an_oblique_leaning_right_when_positive(self):
        self.assertEqual(rule().fontStyle(12), rule().font(weave.Type(slant=-12)))
        self.assertEqual(rule().fontStyle(FontStyle.oblique(12)), rule().fontStyle(12))
        self.assertEqual(rule().fontStyle(FontStyle.oblique()), rule().fontStyle(14))
        self.assertEqual(FontStyle.oblique(), FontStyle(14))
        self.assertEqual(FontStyle.oblique().degrees, 14)

    def test_normal_and_italic_are_styles_of_their_own(self):
        self.assertEqual(rule().fontStyle(FontStyle.Normal), rule().font(weave.Type(slant=0)))
        self.assertNotEqual(rule().fontStyle(FontStyle.Italic), rule().fontStyle(FontStyle.Normal))
        self.assertEqual(FontStyle.Italic.kind, FontStyle.Kind.Italic)
        self.assertEqual(repr(FontStyle.Italic), "FontStyle.Italic")
        self.assertEqual(repr(FontStyle.oblique(8)), "FontStyle.oblique(8.0)")
        # A lean written after an italic replaces it.
        self.assertEqual(rule().fontStyle(FontStyle.Italic).fontStyle(8), rule().fontStyle(8))


class TextIndentTest(unittest.TestCase):
    def test_a_bare_number_is_pixels(self):
        self.assertEqual(
            rule().textIndent(24),
            rule().paragraph(weave.ParagraphBlock(firstLineIndent=24)),
        )

    def test_any_length_is_an_indent(self):
        self.assertNotEqual(rule().textIndent(compose.em(2)), rule().textIndent(2))
        self.assertNotEqual(rule().textIndent(compose.pct(10)), rule().textIndent(10))
        self.assertEqual(rule().textIndent("2em"), rule().textIndent(compose.em(2)))

    def test_the_later_of_pixels_and_another_unit_stands(self):
        self.assertEqual(rule().textIndent(compose.em(2)).textIndent(8), rule().textIndent(8))
        self.assertEqual(
            rule().textIndent(8).textIndent(compose.pct(5)),
            rule().textIndent(compose.pct(5)),
        )


if __name__ == "__main__":
    unittest.main()
