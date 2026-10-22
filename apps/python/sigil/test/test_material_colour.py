"""A colour written the CSS way: one CSS colour in the constructor, the
channels through constructors named after CSS's functions, text no
grammar reads refused by name, and the tree's two colour words read where
a fill is taken."""

import unittest

from sigil import compose, material

Color = material.Color


def near(first, second, tolerance=2 / 255):
    return all(abs(a - b) <= tolerance for a, b in zip(first, second))


class TheConstructorTakesOneCssColour(unittest.TestCase):
    def test_every_rgb_spelling_reads_to_one_colour(self):
        red = Color.srgb(1, 0, 0)
        for written in (
            "#f00",
            "#ff0000ff",
            "red",
            "Red ",
            "rgb(255 0 0)",
            "rgb(255, 0, 0)",
            "rgba(255,0,0,1)",
            "rgb(100% 0% 0%)",
            "rgb(255 none none / 100%)",
            "color(srgb 1 0 0)",
            (1, 0, 0),
            [1, 0, 0, 1],
            red,
        ):
            with self.subTest(written=written):
                self.assertEqual(Color(written), red)
        self.assertEqual(Color("rgb(110 153 187)"), Color("#6e99bb"))
        self.assertEqual(Color.rgb(110, 153, 187), Color("#6e99bb"))

    def test_the_specifications_colour_reads_the_same_in_every_notation(self):
        written = Color("#7654cd")
        for css in (
            "rgb(46.27% 32.94% 80.39%)",
            "lab(44.36% 36.05 -58.99)",
            "lch(44.36% 69.13 301.43)",
            "oklab(54.43% 0.0682 -0.1657)",
            "oklch(54.43% 0.1791 292.37)",
            "color(xyz-d50 0.2005 0.14089 0.4472)",
            "color(xyz-d65 0.21661 0.14602 0.59452)",
        ):
            with self.subTest(css=css):
                self.assertTrue(near(Color(css), written), (css, tuple(Color(css))))

    def test_hsl_and_hwb_read_their_css_forms(self):
        self.assertEqual(Color("hsl(120, 100%, 50%)"), Color("lime"))
        self.assertEqual(Color("hsl(120 100 50)"), Color("lime"))
        self.assertTrue(near(Color("hsl(0.5turn 100% 50%)"), Color("cyan"), 1e-6))
        self.assertEqual(Color("hwb(0 0% 0%)"), Color("red"))
        self.assertTrue(near(Color("hwb(0 60% 60%)"), (0.5, 0.5, 0.5, 1), 1e-6))

    def test_a_colour_outside_srgb_gives_up_chroma_and_keeps_its_hue(self):
        mapped = Color("color(display-p3 1 0 0)")
        self.assertTrue(all(0 <= channel <= 1 for channel in mapped))
        fitted = material.toOklch(mapped)
        self.assertAlmostEqual(fitted.L, 0.6486, delta=2e-3)
        self.assertAlmostEqual(fitted.hueDegrees, 28.96, delta=1.0)
        self.assertTrue(
            near(
                Color("oklch(0.7 0.4 150)"),
                material.fitToSrgb(material.Oklch(0.7, 0.4, 150, 1)),
                2e-3,
            )
        )
        # color(srgb ...) alone keeps a channel past white.
        self.assertEqual(Color("color(srgb 1.5 0 0)").r, 1.5)

    def test_color_mix_reads_as_the_mix_does(self):
        self.assertEqual(Color("color-mix(in srgb, red, blue)"), Color.srgb(0.5, 0, 0.5))
        self.assertEqual(
            Color("color-mix(in srgb, red 25%, blue)"),
            Color.mix("red", "blue", 0.75, space="srgb"),
        )
        self.assertTrue(
            near(
                Color("color-mix(in lch, peru 40%, palegoldenrod)"),
                Color.lch(79.7256, 40.448, 84.771),
            )
        )
        shorter = material.toOklch(Color.mix("red", "blue", space="oklch"))
        longer = material.toOklch(Color.mix("red", "blue", space="oklch", hue="longer"))
        self.assertGreater(shorter.hueDegrees, 300)
        self.assertLess(longer.hueDegrees, 200)
        with self.assertRaises(ValueError):
            Color.mix("red", "blue", space="cmyk")


class ChannelsGoThroughNamedConstructors(unittest.TestCase):
    def test_positional_channels_are_refused(self):
        with self.assertRaises(TypeError):
            Color(255, 0, 0)

    def test_rgb_reads_bytes_and_srgb_reads_unit_floats(self):
        self.assertEqual(Color.rgb(255, 0, 0), Color.srgb(1.0, 0.0, 0.0))
        self.assertEqual(Color.rgb("100%", "0%", 0, alpha="50%"), Color.srgb(1, 0, 0, alpha=0.5))
        # rgb() clamps as CSS clamps; srgb() keeps a float colour as written.
        self.assertEqual(Color.rgb(300, -4, 0), Color("red"))
        self.assertEqual(tuple(Color.srgb(2.0, 0, 0)), (2.0, 0.0, 0.0, 1.0))
        # A sequence is unit floats and refuses bytes rather than glowing.
        with self.assertRaises(ValueError):
            material.Paint.solid((255, 0, 0))

    def test_hsl_and_hwb_take_css_ranges(self):
        self.assertEqual(Color.hsl(0, 100, 50), Color("red"))
        self.assertEqual(Color.hsl(120, "100%", "50%"), Color("lime"))
        self.assertEqual(Color.hsl(210, 40, 60), Color("hsl(210 40% 60%)"))
        self.assertEqual(Color.hwb(200, 10, 20, alpha=0.5), Color("hwb(200 10% 20% / 0.5)"))

    def test_the_lab_family_reads_as_its_text(self):
        for built, css in (
            (Color.lab(44.36, 36.05, -58.99), "lab(44.36 36.05 -58.99)"),
            (Color.lch(44.36, 69.13, 301.43), "lch(44.36 69.13 301.43)"),
            (Color.oklab(0.5443, 0.0682, -0.1657), "oklab(0.5443 0.0682 -0.1657)"),
            (Color.oklch(0.5443, 0.1791, 292.37), "oklch(0.5443 0.1791 292.37)"),
            (Color.oklch("54.43%", 0.1791, 292.37), "oklch(54.43% 0.1791 292.37)"),
        ):
            with self.subTest(css=css):
                self.assertTrue(near(built, Color(css), 1e-5))

    def test_hex_and_named(self):
        self.assertEqual(Color.hex("#6e99bb"), Color("#6e99bb"))
        self.assertEqual(Color.hex("6e99bb"), Color("#6e99bb"))
        self.assertEqual(Color.hex(0x6E99BB, alpha=0.5), material.hexColor(0x6E99BB, 0.5))
        self.assertEqual(Color.named("RebeccaPurple"), Color("#663399"))
        with self.assertRaises(ValueError):
            Color.hex("rgb(1 2 3)")
        with self.assertRaises(ValueError):
            Color.named("transparent")


class TextNoGrammarReadsIsRefusedByName(unittest.TestCase):
    def test_the_constructor_and_every_flat_slot_name_the_text(self):
        for reading in (
            Color,
            material.Paint.solid,
            lambda text: material.withAlpha(text, 0.5),
            lambda text: material.ColorStop(0.5, text),
        ):
            with self.assertRaises(ValueError) as raised:
                reading("rebecapurple")
            self.assertIn("'rebecapurple'", str(raised.exception))

    def test_a_flat_slot_says_why_the_tree_words_do_not_read(self):
        for word in ("currentcolor", "var(--accent)"):
            with self.subTest(word=word), self.assertRaises(ValueError) as raised:
                material.Paint.solid(word)
            self.assertIn("resolves nothing from the tree", str(raised.exception))


class TheTreeWordsReadWhereAFillIsTaken(unittest.TestCase):
    def test_currentcolor_and_var_are_the_ink_and_the_property(self):
        self.assertEqual(compose.Fill("currentcolor"), compose.Fill.currentInk())
        self.assertEqual(compose.Fill("CurrentColor"), compose.Fill.currentInk())
        self.assertEqual(compose.Fill("var(--accent)"), compose.Fill.var("accent"))
        self.assertEqual(compose.Fill("var(accent)"), compose.Fill.var("accent"))
        def rule():
            return compose.rule(".card")

        self.assertEqual(rule().fill("var(--accent)"), rule().fill(compose.Fill.var("accent")))
        self.assertEqual(rule().fill("currentcolor"), rule().fill(compose.Fill.currentInk()))
        self.assertEqual(rule().ink("var(--accent)"), rule().ink(compose.var("accent")))
        self.assertNotEqual(rule().ink("var(--accent)"), rule().ink("#000"))

    def test_a_custom_property_tells_a_length_from_a_colour_by_its_grammar(self):
        # A rule's verbs state onto the rule itself, so each side is a
        # fresh one.
        def stated(value):
            return compose.rule(".card").var("gap", value)

        self.assertEqual(stated("12px"), stated(compose.Dimension(12)))
        self.assertEqual(stated("1.5em"), stated(compose.em(1.5)))
        self.assertEqual(stated("50%"), stated(compose.pct(50)))
        self.assertEqual(stated("#18252e"), stated(Color("#18252e")))
        self.assertEqual(stated("steelblue"), stated(Color("#4682b4")))
        self.assertNotEqual(stated("12px"), stated(Color("black")))
        self.assertNotEqual(stated("1.5em"), stated(Color("black")))
        with self.assertRaises(ValueError):
            stated("twelve")


class AColourReadsBackAsCss(unittest.TestCase):
    def test_repr_and_css_round_trip(self):
        for colour in (
            Color("#6e99bb"),
            Color("#6e99bb80"),
            Color.srgb(0.43, 0.6, 0.73),
            Color.srgb(1.5, 0.25, 0, alpha=0.5),
            Color("oklch(0.7 0.1 240)"),
        ):
            with self.subTest(colour=repr(colour)):
                self.assertEqual(eval(repr(colour), {"Color": Color}), colour)
                self.assertEqual(Color(colour.css()), colour)
                self.assertEqual(str(colour), colour.css())
        self.assertEqual(repr(Color("steelblue")), "Color('#4682b4')")
        self.assertEqual(Color("#6e99bb80").css(), "#6e99bb80")
        self.assertEqual(Color.srgb(0.5, 0.25, 1, alpha=0.5).css(), "color(srgb 0.5 0.25 1 / 0.5)")


if __name__ == "__main__":
    unittest.main()
