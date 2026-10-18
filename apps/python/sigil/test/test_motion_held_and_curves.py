"""A number's own motion, and the curves a binding is shaped by.

A held motion is what a consumer keeps for one number while a change
eases it, and `motion.isRunning(held, value)` is the per-value reading of
whether the number is moving now — the middle of the three stillness
words, between a value's declaration and a node's settling. A curve given
to a binding is part of the binding's identity: an `Oscillator` and the
native eases are values and compare by their settings, so a binding built
again the same way is the same binding; a Python function can only be held
as an opaque callable, which compares equal to nothing.
"""

import unittest

from sigil import motion
from sigil.motion import Binding, Engine, HeldMotion, Oscillator, Wave, envelope


class HeldNumbers(unittest.TestCase):
    def test_a_value_eased_by_a_transition_runs_until_it_lands(self):
        engine = Engine()
        held = HeldMotion()
        self.assertFalse(motion.isRunning(held, 1.0))
        self.assertIsNone(held.value())
        self.assertTrue(motion.retarget(engine, held, 0.0, 1.0, transition=0.5))
        self.assertTrue(motion.isRunning(held, 1.0))
        self.assertTrue(held.isRunning())
        self.assertEqual(held.target, 1.0)
        engine.advance(to=0.25)
        self.assertTrue(motion.isRunning(held, 1.0))
        self.assertGreater(motion.valueOf(held, 1.0), 0.0)
        self.assertLess(motion.valueOf(held, 1.0), 1.0)
        engine.advance(to=1.0)
        self.assertFalse(motion.isRunning(held, 1.0))
        self.assertFalse(held.isRunning())
        self.assertAlmostEqual(motion.valueOf(held, 1.0), 1.0)

    def test_a_change_with_no_transition_snaps_and_never_runs(self):
        engine = Engine()
        held = HeldMotion()
        self.assertFalse(motion.retarget(engine, held, 0.0, 1.0))
        self.assertFalse(motion.isRunning(held, 1.0))
        self.assertEqual(motion.valueOf(held, 1.0), 1.0)

    def test_a_live_value_is_always_running(self):
        # A live value is written by a hand that can stop at any frame, so
        # it is moving for as long as it is live, held motion or none.
        self.assertTrue(motion.isRunning(HeldMotion(), motion.animatable(0.0)))

    def test_a_stopped_motion_stays_where_it_was_left(self):
        engine = Engine()
        held = HeldMotion()
        motion.retarget(engine, held, 0.0, 1.0, transition=1.0)
        engine.advance(to=0.5)
        held.stop()
        self.assertFalse(motion.isRunning(held, 1.0))
        stopped = held.value()
        engine.advance(to=0.75)
        self.assertEqual(held.value(), stopped)


class CurvesAsValues(unittest.TestCase):
    def test_an_oscillator_shapes_a_binding_by_its_settings(self):
        flicker = Oscillator(wave=Wave.Triangle, hertz=3)
        again = Oscillator(wave=Wave.Triangle, hertz=3)
        slower = Oscillator(wave=Wave.Triangle, hertz=1.5)
        self.assertEqual(
            Binding(envelope=envelope.shaped(flicker)),
            Binding(envelope=envelope.shaped(again)),
        )
        self.assertNotEqual(
            Binding(envelope=envelope.shaped(flicker)),
            Binding(envelope=envelope.shaped(slower)),
        )
        self.assertEqual(Binding(ease=flicker), Binding(ease=again))
        self.assertNotEqual(Binding(ease=flicker), Binding(ease=slower))
        self.assertNotEqual(Binding(ease=flicker), Binding(ease=motion.ease.inQuad))

    def test_the_native_eases_compare_as_values(self):
        self.assertEqual(Binding(ease=motion.ease.inQuad), Binding(ease=motion.ease.inQuad))
        self.assertEqual(
            Binding(ease=motion.ease.outBack(1.7)), Binding(ease=motion.ease.outBack(1.7))
        )
        self.assertNotEqual(
            Binding(ease=motion.ease.outBack(1.7)), Binding(ease=motion.ease.outBack(2.4))
        )

    def test_a_python_function_is_an_opaque_curve(self):
        def doubled(progress):
            return progress * 2

        self.assertNotEqual(Binding(ease=doubled), Binding(ease=doubled))
        self.assertNotEqual(
            Binding(envelope=envelope.shaped(doubled)),
            Binding(envelope=envelope.shaped(doubled)),
        )

    def test_an_oscillator_held_as_a_curve_still_reads_as_one(self):
        wave = Oscillator(wave=Wave.Sawtooth, hertz=1)
        shaped = Binding(ease=wave)
        self.assertAlmostEqual(shaped.ease(0.25), wave(0.25), places=5)


if __name__ == "__main__":
    unittest.main()
