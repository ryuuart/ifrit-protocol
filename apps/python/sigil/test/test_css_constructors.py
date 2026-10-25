"""Gradients, shadows, filters, keywords and easing written the way CSS
writes them: the CSS-shaped gradient doors and their stops in CSS's order,
one shadow vocabulary with CSS's box-shadow verb, all ten filter functions
with percentage amounts and length blurs, CSS's hyphenated keywords and
timing-function text."""

import unittest

from sigil import compose, material, motion
from sigil.compose import box

Filter = material.Filter
STOPS = ["#ffffff", "#000000"]


class TheGradientDoorsAreCssShaped(unittest.TestCase):
    def test_a_linear_gradient_runs_down_unless_told(self):
        self.assertEqual(
            material.linearGradient(STOPS), material.linearGradient(STOPS, 180)
        )
        self.assertNotEqual(
            material.linearGradient(STOPS), material.linearGradient(STOPS, 90)
        )

    def test_an_angle_reads_its_number_as_degrees_and_its_text_by_unit(self):
        right = material.linearGradient(STOPS, 90)
        for written in ("90deg", "0.25turn", "100grad"):
            with self.subTest(written=written):
                self.assertEqual(material.linearGradient(STOPS, written), right)

    def test_toward_reads_a_side_or_a_corner_in_either_order(self):
        corner = material.linearGradient(STOPS, toward=material.Toward.TopLeft)
        for written in ("top left", "left top", "to top left"):
            with self.subTest(written=written):
                self.assertEqual(material.linearGradient(STOPS, toward=written), corner)
        with self.assertRaisesRegex(ValueError, "one side or one corner"):
            material.linearGradient(STOPS, toward="left right")
        with self.assertRaisesRegex(TypeError, "not both"):
            material.linearGradient(STOPS, 90, toward="right")

    def test_stops_read_cs_order_beside_the_offset_first_order(self):
        css = material.linearGradient([("#ff0000", "40%"), ("#0000ff", 1)])
        self.assertEqual(
            css, material.linearGradient([(0.4, "#ff0000"), (1, "#0000ff")])
        )
        self.assertEqual(
            css, material.linearGradient([("#ff0000", 0.4), ("#0000ff", "100%")])
        )
        with self.assertRaisesRegex(ValueError, "position"):
            material.linearGradient([("#ff0000", "half"), ("#0000ff", 1)])

    def test_a_stop_reads_a_length_along_the_line(self):
        at_twenty = material.linearGradient([("#ff0000", "20px"), ("#0000ff", 1)])
        self.assertEqual(
            at_twenty, material.linearGradient([("#ff0000", "15pt"), ("#0000ff", 1)])
        )
        self.assertNotEqual(
            at_twenty, material.linearGradient([("#ff0000", 0.2), ("#0000ff", 1)])
        )

    def test_a_radial_gradient_names_its_shape_extent_and_centre(self):
        quarter = material.radialGradient(
            STOPS, shape="circle", extent="closest-side", at=("25%", "50%")
        )
        self.assertEqual(
            quarter,
            material.radialGradient(
                STOPS,
                shape=material.RadialShape.Circle,
                extent=material.RadialExtent.ClosestSide,
                at=(0.25, 0.5),
            ),
        )
        self.assertEqual(
            material.radialGradient(STOPS, at="left"),
            material.radialGradient(STOPS, at=(0, 0.5)),
        )
        self.assertEqual(
            material.radialGradient(STOPS), material.radialGradient(STOPS, at="center")
        )
        with self.assertRaisesRegex(ValueError, "farthest-corner"):
            material.radialGradient(STOPS, extent="farthest_corner")
        with self.assertRaisesRegex(ValueError, "circle"):
            material.radialGradient(STOPS, shape="square")

    def test_a_conic_gradient_starts_from_its_angle(self):
        self.assertEqual(
            material.conicGradient(STOPS, from_="90deg", at="top"),
            material.conicGradient(STOPS, from_=90, at=(0.5, 0)),
        )
        self.assertNotEqual(
            material.conicGradient(STOPS), material.conicGradient(STOPS, from_=90)
        )

    def test_a_css_gradient_fills_a_node(self):
        self.assertIsInstance(
            box().fill(material.linearGradient(STOPS, toward="right")), compose.Element
        )


class OneShadowVocabulary(unittest.TestCase):
    def test_a_box_shadow_takes_css_words_and_lengths(self):
        shadow = compose.Shadow(
            offset=(0, "4px"), blur="12px", spread=2, color="#00000066", inset=True
        )
        self.assertEqual((shadow.offset.x, shadow.offset.y), (0, 4))
        self.assertEqual((shadow.blur, shadow.spread, shadow.inset), (12, 2, True))
        with self.assertRaisesRegex(ValueError, "pixels"):
            compose.Shadow(blur="1em")
        with self.assertRaisesRegex(TypeError, "no field named"):
            compose.Shadow(ink="#000")

    def test_the_verb_takes_keywords_one_shadow_or_a_list(self):
        self.assertIsInstance(
            box().boxShadow(offset=(0, 4), blur=12, color="#000"), compose.Element
        )
        self.assertEqual(
            compose.Shadow(offset=(0, 4), blur=12, color="#000"),
            compose.Shadow(offset=("0px", "4px"), blur="12px", color="#000000"),
        )
        self.assertIsInstance(
            box().boxShadow([compose.Shadow(blur=2), compose.Shadow(inset=True)]),
            compose.Element,
        )
        with self.assertRaisesRegex(TypeError, "not both"):
            box().boxShadow(compose.Shadow(), blur=3)

    def test_the_text_and_filter_shadows_share_the_words(self):
        self.assertEqual(compose.TextShadow(offset=("1px", 2), blur="3px").blur, 3)
        options = material.ShadowOptions(blur="3px", spread=1, inset=True)
        self.assertEqual((options.blur, options.spread, options.inset), (3, 1, True))
        self.assertIsInstance(compose.shadow("#000", (0, "2px"), "6px"), compose.Shadow)


class AllTenFilterFunctions(unittest.TestCase):
    def test_the_four_new_colour_functions_exist(self):
        for function in (Filter.grayscale, Filter.sepia, Filter.invert):
            with self.subTest(function=function.__name__):
                self.assertEqual(function(), function(1))
                self.assertFalse(function(0.5).isNone())
        self.assertNotEqual(Filter.grayscale(1), Filter.sepia(1))
        self.assertFalse(Filter.opacity(0.5).isNone())

    def test_amounts_read_percentages_and_blur_reads_a_length(self):
        self.assertEqual(Filter.brightness("150%"), Filter.brightness(1.5))
        self.assertEqual(Filter.grayscale("50%"), Filter.grayscale(0.5))
        self.assertEqual(Filter.opacity("25%"), Filter.opacity(0.25))
        self.assertEqual(Filter.blur("4px"), Filter.blur(4))
        with self.assertRaisesRegex(ValueError, "pixels"):
            Filter.blur("2em")
        with self.assertRaisesRegex(ValueError, "fraction"):
            Filter.saturate("lots")


class CssKeywordsAndTimingFunctions(unittest.TestCase):
    def test_justify_reads_css_hyphenated_keywords(self):
        self.assertIsInstance(box().justifyContent("space-between"), compose.Element)
        with self.assertRaisesRegex(ValueError, "space-between"):
            box().justifyContent("space_between")

    def test_an_easing_reads_css_timing_function_text(self):
        bezier = motion.ease.cubicBezier(0.42, 0, 0.58, 1)
        written = motion.Keyframe(1.0, ease="ease-in-out").ease
        for progress in (0.2, 0.5, 0.8):
            self.assertAlmostEqual(written(progress), bezier(progress), places=5)
        self.assertEqual(
            motion.Keyframe(1.0, ease="cubic-bezier(.2, .8, .2, 1)").ease,
            motion.Keyframe(1.0, ease="cubic-bezier(0.2, 0.8, 0.2, 1)").ease,
        )
        steps = motion.Keyframe(1.0, ease="steps(4, jump-start)").ease
        self.assertEqual(steps(0.1), 0.25)
        none = motion.Keyframe(1.0, ease="steps(3, jump-none)").ease
        self.assertEqual((none(0.1), none(0.5), none(0.9)), (0.0, 0.5, 1.0))
        both = motion.Keyframe(1.0, ease="steps(3, jump-both)").ease
        self.assertEqual((both(0.0), both(0.5), both(1.0)), (0.25, 0.5, 1.0))
        native = motion.ease.steps(3, motion.ease.StepPosition.JumpNone)
        self.assertEqual((native(0.1), native(0.5), native(0.9)), (0.0, 0.5, 1.0))
        self.assertAlmostEqual(motion.Keyframe(1.0, ease="linear").ease(0.3), 0.3)
        with self.assertRaisesRegex(ValueError, "timing function"):
            motion.Keyframe(1.0, ease="ease_in")


if __name__ == "__main__":
    unittest.main()
