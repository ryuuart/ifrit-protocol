"""Angles and times written the CSS way. A bare number keeps the meaning
its slot gives it: degrees on compose, material, geometry and the canvas,
the pen's angle mode beside a pen, radians inside a brush value and on the
draw stack, and seconds wherever a length of time is taken. Text and the
typed angle name their own unit, and so read the same on every slot; text
no grammar reads is refused by name."""

import datetime
import math
import unittest

from sigil import compose, draw, geometry, material, motion
from sigil.core import angle
from sigil.compose import Composer, box


def rule():
    """A rule, whose statements compare equal when they say the same."""
    return compose.rule(".turned")


def radiansOf(degrees):
    """Degrees in radians through the one rounding every slot uses."""
    return angle.degrees(degrees).inRadians()


class TheTypedAngle(unittest.TestCase):
    def test_each_constructor_names_its_measure(self):
        self.assertEqual(angle.degrees(90).inDegrees(), 90)
        self.assertEqual(angle.turns(0.25).inDegrees(), 90)
        self.assertEqual(angle.gradians(100).inDegrees(), 90)
        self.assertAlmostEqual(angle.radians(math.pi).inDegrees(), 180, places=4)
        self.assertEqual(angle.radians(1).inRadians(), 1)
        self.assertEqual(angle.degrees(90).inTurns(), 0.25)

    def test_text_reads_to_the_angle_it_writes(self):
        self.assertEqual(angle.Angle("45deg"), angle.degrees(45))
        self.assertEqual(angle.Angle("0.25TURN"), angle.turns(0.25))
        self.assertEqual(angle.Angle(" -1.5rad "), angle.radians(-1.5))
        self.assertEqual(angle.Angle("50grad"), angle.gradians(50))
        written = angle.Angle("0.25turn")
        self.assertEqual((written.value, written.unit), (0.25, "turn"))
        self.assertEqual(str(written), "0.25turn")
        self.assertEqual(repr(written), "Angle('0.25turn')")
        self.assertEqual(angle.Angle(str(angle.degrees(-12.5))), angle.degrees(-12.5))

    def test_text_without_a_unit_is_refused_by_name(self):
        for text in ("45", "45px", "deg", "45 deg", ""):
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, "no CSS angle"):
                    angle.Angle(text)


class ASlotReadsANumberInItsOwnMeasure(unittest.TestCase):
    def test_a_degree_slot_reads_text_and_the_typed_angle_as_degrees(self):
        style = compose.FontStyle.oblique
        for written in (14, 14.0, "14deg", angle.degrees(14)):
            with self.subTest(written=written):
                self.assertEqual(style(written), style(14))
        self.assertEqual(style("0.25turn"), style(90))
        self.assertEqual(compose.FontStyle("100grad"), compose.FontStyle(90))
        self.assertEqual(
            material.Color.hsl("0.5turn", 100, 50), material.Color.hsl(180, 100, 50)
        )
        self.assertEqual(
            material.Color.oklch(0.7, 0.1, angle.degrees(200)),
            material.Color.oklch(0.7, 0.1, 200),
        )
        self.assertEqual(
            material.rotateHue("#ff0000", "120deg"),
            material.rotateHue("#ff0000", 120),
        )
        self.assertEqual(
            geometry.shapes.polygon(6, "0.5turn"), geometry.shapes.polygon(6, 180)
        )

    def test_a_rule_turns_and_leans_alike_whichever_way_it_is_written(self):
        self.assertEqual(rule().rotate("0.25turn"), rule().rotate(90))
        self.assertEqual(rule().skewX(angle.degrees(12)), rule().skewX(12))
        self.assertEqual(rule().fontStyle("14deg"), rule().fontStyle(14))
        with self.assertRaises(TypeError):
            rule().fontStyle("italic")

    def test_a_radian_slot_reads_text_and_the_typed_angle_as_radians(self):
        quarter = draw.LayerTransform(rotation="90deg")
        self.assertEqual(quarter.rotation, radiansOf(90))
        self.assertEqual(
            draw.LayerTransform(rotation=angle.radians(1.25)).rotation,
            draw.LayerTransform(rotation=1.25).rotation,
        )
        self.assertEqual(draw.LayerTransform(rotation=1.25).rotation, 1.25)

    def test_a_record_field_reads_an_angle_and_reads_back_its_number(self):
        ring = geometry.arrange.Ring(fromDegrees="0.5turn")
        self.assertEqual(ring.fromDegrees, 180)
        ring.sweepDegrees = angle.radians(math.pi)
        self.assertAlmostEqual(ring.sweepDegrees, 180, places=4)

    def test_text_no_angle_grammar_reads_is_refused_by_name(self):
        with self.assertRaisesRegex(ValueError, "'45' is no CSS angle"):
            compose.FontStyle.oblique("45")
        with self.assertRaisesRegex(ValueError, "'half' is no CSS angle"):
            draw.LayerTransform(rotation="half")


class AnElementTurnsTheSameWhicheverWayItsAngleIsWritten(unittest.TestCase):
    def turned(self, *writings):
        """The picture of one tall bar turned by each angle in turn."""
        stack = draw.Stack((40, 40))
        pictures = []
        for index, written in enumerate(writings):
            layer = stack.layer(f"turn{index}")
            composer = Composer()
            composer.setSize((40, 40))
            composer.render(
                box()
                .size(40, 40)
                .children(
                    [box().size(8, 30).fill("#ffffff").rotate(written).skewX(written)]
                )
            )
            with layer.pen() as pen:
                pen.background("#000000")
                composer.draw(canvas=pen.canvas())
            pictures.append(layer.image().rgba())
        return pictures

    def test_degrees_text_turns_and_the_typed_angle_agree(self):
        pictures = self.turned(30, "30deg", angle.degrees(30))
        self.assertEqual(pictures[0], pictures[1])
        self.assertEqual(pictures[0], pictures[2])
        self.assertNotEqual(pictures[0], self.turned(0)[0])


class APenAngleIsInThePensModeUnlessItNamesItsOwn(unittest.TestCase):
    def drawn(self, mode, written):
        stack = draw.Stack((40, 40))
        layer = stack.layer("bars")
        with layer.pen() as pen:
            pen.background("#000000")
            pen.angleMode(mode)
            pen.noStroke()
            pen.fill("#ffffff")
            pen.arc(20, 20, 30, 30, 0, written, draw.PIE)
            pen.translate(20, 20)
            pen.rotate(written)
            pen.rect(-2, -16, 4, 32)
        return layer.image().rgba()

    def test_text_is_read_in_its_own_unit_whatever_the_mode(self):
        degreesMode = self.drawn(draw.DEGREES, 45)
        self.assertEqual(self.drawn(draw.DEGREES, "45deg"), degreesMode)
        self.assertEqual(self.drawn(draw.RADIANS, "45deg"), degreesMode)
        self.assertEqual(self.drawn(draw.DEGREES, angle.turns(0.125)), degreesMode)
        self.assertEqual(
            self.drawn(draw.RADIANS, "0.785rad"), self.drawn(draw.RADIANS, 0.785)
        )

    def test_a_bare_number_follows_the_mode(self):
        self.assertNotEqual(self.drawn(draw.RADIANS, 45), self.drawn(draw.DEGREES, 45))


class TheStackPlacementNamesItsMeasure(unittest.TestCase):
    def test_transform_takes_rotate_radians(self):
        stack = draw.Stack((32, 32))
        layer = stack.layer("ink")
        layer.transform(rotateRadians=0.5)
        self.assertEqual(layer.placement.rotation, 0.5)
        layer.transform(rotateRadians="90deg")
        self.assertEqual(layer.placement.rotation, radiansOf(90))
        with self.assertRaises(TypeError):
            layer.transform(rotate=0.5)


class ATimeIsSecondsOrNamesItsUnit(unittest.TestCase):
    def test_text_timedelta_and_seconds_read_to_one_length(self):
        tween = motion.Tween(to=1.0, duration="300ms")
        self.assertAlmostEqual(tween.duration, 0.3)
        for written in (0.3, "0.3s", "300MS", " 300ms ",
                        datetime.timedelta(milliseconds=300)):
            with self.subTest(written=written):
                tween.duration = written
                self.assertAlmostEqual(tween.duration, 0.3)

    def test_a_transition_and_a_staggered_delay_read_text(self):
        node = compose.box().transition("250ms")
        self.assertIsNotNone(node)
        tween = motion.Tween(to=1.0, duration=1, delay="0.5s")
        self.assertEqual(motion.Tween(to=1.0, duration=1, delay=0.5).delay, tween.delay)

    def test_text_without_a_unit_is_refused_by_name(self):
        tween = motion.Tween(to=1.0, duration=1)
        for text in ("0.3", "300", "3 s", "2min"):
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, "no CSS time"):
                    tween.duration = text


if __name__ == "__main__":
    unittest.main()
