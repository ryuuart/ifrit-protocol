"""The type verbs CSS names beyond family, size and weight.

Each writes one longhand of the font or the paragraph a rule states, and
reads CSS's text where CSS writes the value as text. A rule is read back
without a canvas.
"""

import unittest

from sigil import compose, weave
from sigil.sketch import kit


def rule():
    return compose.rule(".a")


class FontLonghandTest(unittest.TestCase):
    def test_feature_settings_read_text_and_values_alike(self):
        written = rule().fontFeatureSettings('"tnum", "liga" 0').type().features
        self.assertEqual(
            written, [weave.FontFeature("tnum", 1), weave.FontFeature("liga", 0)]
        )
        self.assertEqual(
            rule().fontFeatureSettings(["tnum", ("liga", 0)]).type().features, written
        )
        with self.assertRaises(ValueError):
            rule().fontFeatureSettings("tabular-nums")

    def test_variation_settings_read_text_dicts_and_pairs(self):
        axis = [weave.FontVariation("wdth", 75)]
        self.assertEqual(rule().fontVariationSettings('"wdth" 75').type().variations, axis)
        self.assertEqual(rule().fontVariationSettings({"wdth": 75}).type().variations, axis)
        self.assertEqual(rule().fontVariationSettings([("wdth", 75)]).type().variations, axis)

    def test_each_font_verb_writes_its_field(self):
        stated = (
            rule()
            .wordSpacing("4px")
            .fontKerning(compose.FontKerning.Optical)
            .textTransform(weave.TextTransform.Uppercase)
            .textOrientation(compose.TextOrientation.Sideways)
            .baselineShift("3pt")
            .type()
        )
        self.assertEqual(stated.wordSpacing.value, 4)
        self.assertTrue(stated.opticalKerning)
        self.assertEqual(stated.textTransform, weave.TextTransform.Uppercase)
        self.assertEqual(stated.verticalForm, weave.VerticalForm.Rotated)
        self.assertEqual(stated.baselineShift, 4)
        self.assertEqual(
            rule().textCombineUpright(compose.TextCombineUpright.All).type().verticalForm,
            weave.VerticalForm.TateChuYoko,
        )
        with self.assertRaises(ValueError):
            rule().baselineShift("1em")

    def test_the_partial_names_its_fields_as_css_does(self):
        font = weave.Type(ink="#ff0000", letterSpacing=2)
        self.assertEqual(font.letterSpacing.value, 2)
        self.assertIsNotNone(font.ink)
        self.assertEqual(rule().ink("#ff0000").type().ink, font.ink)


class ParagraphLonghandTest(unittest.TestCase):
    def test_each_paragraph_verb_writes_its_field(self):
        Block = weave.ParagraphBlock
        self.assertEqual(
            rule().textAlignLast(weave.TextAlignment.Justify),
            rule().paragraph(
                Block(lastLineAlignment=weave.TextAlignment.Justify, justifyLastLine=True)
            ),
        )
        self.assertEqual(rule().widows(2), rule().paragraph(Block(widowLines=2)))
        self.assertEqual(rule().orphans(3), rule().paragraph(Block(orphanLines=3)))
        self.assertNotEqual(rule().hangingPunctuation("first allow-end"), rule())
        self.assertEqual(
            rule().hangingPunctuation("none"),
            rule().paragraph(Block(hanging=weave.HangingTable())),
        )
        with self.assertRaises(ValueError):
            rule().hangingPunctuation("hang")


class DarkThemeTest(unittest.TestCase):
    def test_the_dark_theme_sets_its_label_in_the_terminal_voice(self):
        look = kit.darkTheme()
        label = look.type.label
        self.assertEqual((label.size, label.letterSpacing, label.mono), (12, 2, True))
        self.assertEqual(label.ink, look.palette.ash)
        self.assertEqual(look.type.mono, kit.houseFace(kit.Voice.Terminal))

    def test_a_register_carries_its_features(self):
        look = kit.featureTheme()
        line = look.type.control
        line.features = [weave.FontFeature("tnum", 1)]
        self.assertEqual(look.font(line).features, [weave.FontFeature("tnum", 1)])


if __name__ == "__main__":
    unittest.main()
