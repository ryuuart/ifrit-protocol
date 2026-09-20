"""The frame clock, a ticker Python owns, and the timeline inside one.

One Ticker class covers both owners: the one a body constructs and
steps itself, and the one a session lends, which refuses the step and
answers its timeline all the same. The cases below ask the owned one
everything, because it is the one a test can drive a frame at a time,
and ask a session only what only a session can say.

A motion is written as a list of phrase values rather than a fluent
object, because choreograph's own builder holds references into the
timeline and into a motion the timeline removes the moment it finishes.
Every phrase case therefore states the whole sequence in one call, and
the ones that check a value along the way ask for a linear easing, so a
number here is the sequence's arithmetic and not the default ease-out's.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import unittest
import weakref
from pathlib import Path

from _sigil import motion as native
from sigil import motion
from sigil.motion import Output, bind, ease
from sigil.sketch import render_file

# A fixed-step rate and a frame length that share no common step: twenty
# frames of this length run the whole second's steps, one or two per
# frame, without ever reaching the catch-up limit below.
FIXED_RATE = 27.0
FRAME = 0.05
CATCH_UP = 2

# Every name this package registers, as an author writes it.
SURFACE = (
    "FrameClock",
    "FrameClockOptions",
    "Phrase",
    "Ticker",
    "Timeline",
    "hold",
    "holdUntil",
    "rampTo",
    "setValue",
)


class Spellings(unittest.TestCase):
    def test_the_package_re_exports_everything_the_clock_registers(self):
        # A registration the package does not follow is a name an author
        # cannot reach at all, so the two lists are asked to agree.
        for name in SURFACE:
            with self.subTest(name=name):
                self.assertIs(getattr(motion, name), getattr(native, name))
                self.assertIn(name, motion.__all__)


class Clock(unittest.TestCase):
    def test_the_first_reading_only_sets_the_clock(self):
        clock = native.FrameClock()
        self.assertEqual(clock.tick(0.0), 0.0)
        self.assertAlmostEqual(clock.tick(0.1), 0.1)
        self.assertAlmostEqual(clock.tick(0.25), 0.15)
        self.assertAlmostEqual(clock.elapsed(), 0.25)

    def test_a_clock_reads_the_system_clock_when_told_nothing(self):
        clock = native.FrameClock()
        self.assertEqual(clock.tick(), 0.0)
        second = clock.tick()
        self.assertGreaterEqual(second, 0.0)
        self.assertLessEqual(second, 0.25)

    def test_a_stated_delta_goes_forward_only(self):
        clock = native.FrameClock()
        self.assertAlmostEqual(clock.advance(0.1), 0.1)
        self.assertEqual(clock.advance(-1), 0.0)
        self.assertAlmostEqual(clock.elapsed(), 0.1)

    def test_a_long_step_is_clamped_to_the_largest_delta(self):
        self.assertAlmostEqual(native.FrameClock().advance(10), 0.25)
        options = native.FrameClockOptions(maxDelta=0.5)
        self.assertAlmostEqual(native.FrameClock(options).advance(10), 0.5)
        self.assertAlmostEqual(native.FrameClock(options=options).advance(10), 0.5)

    def test_a_paused_clock_reports_nothing_and_freezes(self):
        clock = native.FrameClock()
        clock.advance(0.1)
        clock.setPaused(True)
        self.assertTrue(clock.paused())
        self.assertEqual(clock.advance(0.1), 0.0)
        self.assertAlmostEqual(clock.elapsed(), 0.1)
        clock.setPaused(False)
        self.assertFalse(clock.paused())
        self.assertAlmostEqual(clock.advance(0.1), 0.1)
        self.assertAlmostEqual(clock.elapsed(), 0.2)

    def test_the_time_scale_shapes_the_delta_the_clamp_let_through(self):
        clock = native.FrameClock()
        clock.setTimeScale(0.5)
        self.assertEqual(clock.timeScale(), 0.5)
        self.assertAlmostEqual(clock.advance(0.1), 0.05)
        # The clamp is the step a frame may report; the scale is what the
        # scene makes of it, so ten seconds arrive as half of the clamp.
        self.assertAlmostEqual(clock.advance(10), 0.125)

    def test_the_options_are_a_record(self):
        options = native.FrameClockOptions(maxDelta=0.5)
        self.assertEqual(options.maxDelta, 0.5)
        self.assertEqual(options.copy().maxDelta, 0.5)
        self.assertEqual(copy.deepcopy(options).maxDelta, 0.5)
        self.assertEqual(copy.copy(options).maxDelta, 0.5)
        self.assertEqual(native.FrameClockOptions().maxDelta, 0.25)
        with self.assertRaisesRegex(TypeError, "FrameClockOptions"):
            native.FrameClockOptions(largest=1)


class Steps(unittest.TestCase):
    def test_a_ticker_of_ones_own_steps_and_counts(self):
        ticker = native.Ticker()
        self.assertFalse(ticker.active())
        self.assertFalse(ticker.tick(0.016))
        self.assertAlmostEqual(ticker.elapsed(), 0.016)
        with self.assertRaisesRegex(ValueError, "finite"):
            ticker.tick(float("inf"))

    def test_a_steppable_names_the_arguments_it_reads(self):
        ticker = native.Ticker()
        seen = []
        ticker.add(lambda: seen.append("nothing"))
        ticker.add(lambda deltaSeconds: seen.append(deltaSeconds))
        ticker.add(lambda deltaSeconds, elapsed: seen.append((deltaSeconds, elapsed)))
        self.assertTrue(ticker.tick(0.5))
        self.assertEqual(seen, ["nothing", 0.5, (0.5, 0.5)])
        # Saying nothing about being finished is taken as never finished.
        self.assertTrue(ticker.active())
        with self.assertRaises(TypeError):
            ticker.add(lambda first, second, third: None)

    def test_a_steppable_that_answers_false_is_retired(self):
        ticker = native.Ticker()
        runs = []

        def once():
            runs.append(1)
            return False

        ticker.add(once)
        self.assertFalse(ticker.tick(0.1))
        self.assertFalse(ticker.active())
        ticker.tick(0.1)
        self.assertEqual(runs, [1])

    def test_a_fixed_step_runs_the_whole_second_at_its_own_rate(self):
        ticker = native.Ticker()
        alpha = Output(0)
        status = native.FixedStatus()
        runs = []
        ticker.addFixed(
            FIXED_RATE,
            lambda: runs.append(1),
            maxCatchUp=CATCH_UP,
            alphaOut=alpha,
            statusOut=status,
        )
        for _ in range(round(1.0 / FRAME)):
            ticker.tick(FRAME)
        self.assertEqual(len(runs), int(FIXED_RATE))
        self.assertAlmostEqual(alpha.value, 0.0, places=5)
        self.assertLessEqual(status.stepsRun, CATCH_UP)
        self.assertFalse(status.clamped)

    def test_a_long_frame_drops_simulated_time_and_says_so(self):
        ticker = native.Ticker()
        status = native.FixedStatus()
        runs = []
        ticker.addFixed(
            FIXED_RATE,
            lambda: runs.append(1),
            maxCatchUp=CATCH_UP,
            statusOut=status,
        )
        ticker.tick(1.0)
        self.assertEqual(len(runs), CATCH_UP)
        self.assertEqual(status.stepsRun, CATCH_UP)
        self.assertTrue(status.clamped)

    def test_the_leftover_fraction_of_a_step_is_the_render_interpolant(self):
        ticker = native.Ticker()
        alpha = Output(0)
        ticker.addFixed(10.0, lambda: None, alphaOut=alpha)
        ticker.tick(0.25)
        self.assertAlmostEqual(alpha.value, 0.5, places=5)

    def test_a_fixed_step_needs_a_rate_and_a_budget(self):
        ticker = native.Ticker()
        with self.assertRaisesRegex(ValueError, "positive"):
            ticker.addFixed(0, lambda: None)
        with self.assertRaisesRegex(ValueError, "positive"):
            ticker.addFixed(10, lambda: None, maxCatchUp=0)

    def test_a_derived_output_is_right_before_the_first_tick(self):
        ticker = native.Ticker()
        source = Output(0.25)
        target = Output(0)
        self.assertTrue(ticker.derive(target, bind(source)))
        self.assertAlmostEqual(target.value, 0.25, places=6)
        source.set(0.5)
        self.assertAlmostEqual(target.value, 0.25, places=6)
        ticker.tick(0.1)
        self.assertAlmostEqual(target.value, 0.5, places=6)
        # A derivation is pure in its source, so it holds nothing awake.
        self.assertFalse(ticker.active())

    def test_derivations_are_one_level_and_one_writer(self):
        ticker = native.Ticker()
        source = Output(0)
        target = Output(0)
        self.assertTrue(ticker.derive(target, bind(source)))
        self.assertFalse(ticker.derive(target, bind(Output(0))))
        self.assertFalse(ticker.derive(Output(0), bind(target)))
        self.assertFalse(ticker.derive(source, bind(Output(0))))
        cell = Output(0)
        self.assertFalse(ticker.derive(cell, bind(cell)))
        with self.assertRaisesRegex(TypeError, "Output"):
            ticker.derive(None, bind(source))


class Phrases(unittest.TestCase):
    def test_a_phrase_is_a_value(self):
        phrase = native.rampTo(1.0, 0.2)
        self.assertIsInstance(phrase, native.Phrase)
        self.assertIsInstance(phrase.copy(), native.Phrase)
        self.assertIsInstance(copy.deepcopy(phrase), native.Phrase)
        self.assertIsInstance(copy.copy(phrase), native.Phrase)

    def test_a_phrase_lasts_a_nonnegative_finite_time(self):
        with self.assertRaisesRegex(ValueError, "ramp's duration"):
            native.rampTo(1.0, -1)
        with self.assertRaisesRegex(ValueError, "hold's duration"):
            native.hold(float("nan"))
        with self.assertRaisesRegex(ValueError, "hold's end"):
            native.holdUntil(-0.5)

    def test_a_ramp_writes_its_output_and_leaves(self):
        ticker = native.Ticker()
        ramped = Output(0)
        timeline = ticker.timeline()
        timeline.apply(ramped, [native.rampTo(1.0, 0.2, ease=ease.linear)])
        self.assertEqual(timeline.size(), 1)
        self.assertEqual(len(timeline), 1)
        self.assertFalse(timeline.empty())
        self.assertAlmostEqual(timeline.duration(), 0.2, places=6)
        self.assertAlmostEqual(timeline.timeUntilFinish(), 0.2, places=6)
        ticker.tick(0.1)
        self.assertAlmostEqual(ramped.value, 0.5, places=5)
        self.assertAlmostEqual(timeline.timeUntilFinish(), 0.1, places=6)
        self.assertTrue(ticker.active())
        ticker.tick(0.15)
        self.assertAlmostEqual(ramped.value, 1.0, places=6)
        self.assertTrue(ticker.timeline().empty())
        self.assertFalse(ticker.active())

    def test_a_motion_that_is_kept_stays_until_it_is_cleared(self):
        ticker = native.Ticker()
        timeline = ticker.timeline()
        timeline.apply(Output(0), [native.rampTo(1.0, 0.1)], removeOnFinish=False)
        ticker.tick(0.2)
        self.assertEqual(timeline.size(), 1)
        self.assertTrue(ticker.active())
        timeline.clear()
        self.assertTrue(timeline.empty())
        self.assertFalse(ticker.active())

    def test_the_default_for_later_motions_can_be_kept_too(self):
        ticker = native.Ticker()
        timeline = ticker.timeline()
        timeline.setDefaultRemoveOnFinish(False)
        timeline.apply(Output(0), [native.rampTo(1.0, 0.1)])
        ticker.tick(0.2)
        self.assertEqual(timeline.size(), 1)

    def test_a_sequence_plays_its_phrases_in_order(self):
        ticker = native.Ticker()
        ramped = Output(0)
        ticker.timeline().apply(
            ramped,
            [
                native.hold(0.1),
                native.rampTo(1.0, 0.2, ease=ease.linear),
                native.holdUntil(0.5),
            ],
        )
        # The hold that runs until half a second ends the sequence there,
        # whatever the phrases before it took.
        self.assertAlmostEqual(ticker.timeline().duration(), 0.5, places=6)
        ticker.tick(0.1)
        self.assertAlmostEqual(ramped.value, 0.0, places=6)
        ticker.tick(0.1)
        self.assertAlmostEqual(ramped.value, 0.5, places=5)
        ticker.tick(0.2)
        self.assertAlmostEqual(ramped.value, 1.0, places=6)
        self.assertFalse(ticker.timeline().empty())
        ticker.tick(0.2)
        self.assertTrue(ticker.timeline().empty())

    def test_a_set_value_opens_a_sequence_where_it_says(self):
        ticker = native.Ticker()
        ramped = Output(0)
        ticker.timeline().apply(
            ramped,
            [native.setValue(3.0), native.rampTo(4.0, 0.1, ease=ease.linear)],
        )
        ticker.tick(0.0)
        self.assertAlmostEqual(ramped.value, 3.0, places=6)
        ticker.tick(0.2)
        self.assertAlmostEqual(ramped.value, 4.0, places=6)

    def test_appending_adds_to_the_motion_already_driving_a_cell(self):
        ticker = native.Ticker()
        ramped = Output(0)
        ticker.timeline().apply(ramped, [native.rampTo(1.0, 0.1, ease=ease.linear)])
        ticker.timeline().append(ramped, [native.rampTo(2.0, 0.1, ease=ease.linear)])
        self.assertAlmostEqual(ticker.timeline().duration(), 0.2, places=6)
        self.assertEqual(ticker.timeline().size(), 1)
        for _ in range(3):
            ticker.tick(0.1)
        self.assertAlmostEqual(ramped.value, 2.0, places=6)

    def test_a_motion_starts_when_it_is_told_to_and_at_the_speed_it_is_given(
        self,
    ):
        ticker = native.Ticker()
        delayed = Output(0)
        ticker.timeline().apply(
            delayed, [native.rampTo(1.0, 0.2, ease=ease.linear)], startTime=0.1
        )
        ticker.tick(0.1)
        self.assertAlmostEqual(delayed.value, 0.0, places=6)
        ticker.tick(0.1)
        self.assertAlmostEqual(delayed.value, 0.5, places=5)
        quick = Output(0)
        fast = native.Ticker()
        fast.timeline().apply(
            quick, [native.rampTo(1.0, 0.2, ease=ease.linear)], playbackSpeed=2.0
        )
        fast.tick(0.1)
        self.assertAlmostEqual(quick.value, 1.0, places=6)
        with self.assertRaisesRegex(ValueError, "start time"):
            ticker.timeline().apply(Output(0), [], startTime=-1)

    def test_a_motion_needs_a_cell_to_write_into(self):
        with self.assertRaisesRegex(TypeError, "Output"):
            native.Ticker().timeline().apply(None, [])

    def test_a_python_easing_shapes_a_ramp(self):
        ticker = native.Ticker()
        ramped = Output(0)
        ticker.timeline().apply(
            ramped, [native.rampTo(1.0, 0.2, ease=lambda progress: progress**2)]
        )
        ticker.tick(0.1)
        self.assertAlmostEqual(ramped.value, 0.25, places=5)


class Reports(unittest.TestCase):
    def test_a_motion_reports_its_start_its_values_and_its_finish(self):
        ticker = native.Ticker()
        ramped = Output(0)
        events = []
        ticker.timeline().apply(
            ramped,
            [native.rampTo(1.0, 0.2, ease=ease.linear)],
            onStart=lambda: events.append("start"),
            onUpdate=lambda value: events.append(round(value, 3)),
            onFinish=lambda: events.append("finish"),
        )
        for _ in range(3):
            ticker.tick(0.1)
        self.assertEqual(events, ["start", 0.5, 1.0, "finish"])

    def test_a_timeline_that_keeps_its_motions_reports_reaching_the_end(self):
        ticker = native.Ticker()
        events = []
        timeline = ticker.timeline()
        timeline.setFinishFn(lambda: events.append("finish"))
        timeline.apply(Output(0), [native.rampTo(1.0, 0.2)], removeOnFinish=False)
        ticker.tick(0.1)
        self.assertEqual(events, [])
        ticker.tick(0.15)
        self.assertEqual(events, ["finish"])

    def test_a_timeline_reports_the_frame_that_empties_it(self):
        ticker = native.Ticker()
        events = []
        timeline = ticker.timeline()
        timeline.setClearedFn(lambda: events.append("cleared"))
        timeline.apply(Output(0), [native.rampTo(1.0, 0.2)])
        ticker.tick(0.1)
        self.assertEqual(events, [])
        ticker.tick(0.15)
        self.assertEqual(events, ["cleared"])
        self.assertTrue(timeline.empty())

    def test_a_cue_fires_once_and_the_timeline_holds_it_until_it_does(self):
        ticker = native.Ticker()
        calls = []
        held = self.cue(ticker, calls)
        gc.collect()
        self.assertIsNotNone(held(), "the timeline must hold what a cue reads")
        ticker.tick(0.1)
        self.assertEqual(calls, [])
        ticker.tick(0.15)
        self.assertEqual(calls, ["held"])
        self.assertTrue(ticker.timeline().empty())
        gc.collect()
        self.assertIsNone(held(), "a fired cue must let its closure go")
        ticker.tick(0.1)
        self.assertEqual(calls, ["held"])

    def cue(self, ticker, calls):
        """Cue a call reading a value only the cue's closure names."""

        class Reading:
            word = "held"

        reading = Reading()
        ticker.timeline().cue(lambda: calls.append(reading.word), 0.2)
        return weakref.ref(reading)

    def test_a_cue_waits_a_nonnegative_finite_time(self):
        with self.assertRaisesRegex(ValueError, "cue's delay"):
            native.Ticker().timeline().cue(lambda: None, -1)


class Sessions(unittest.TestCase):
    def test_a_session_lends_its_timeline_and_takes_it_back(self):
        for name in ("_clock_timeline", "_clock_ramped"):
            self.addCleanup(lambda name=name: builtins.__dict__.pop(name, None))
        source = """
            import builtins
            from _sigil import motion as native
            from sigil.motion import Output
            from sigil.sketch import sketch

            @sketch(size=(64, 24), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    ramped = Output(0)
                    timeline = ctx.ticker.timeline()
                    assert timeline.empty()
                    timeline.apply(ramped, [native.rampTo(32.0, 0.2)])
                    assert timeline.size() == 1
                    builtins._clock_ramped = ramped
                    builtins._clock_timeline = timeline
        """
        with tempfile.TemporaryDirectory(prefix="sigil_clock_") as folder:
            entry = Path(folder) / "scene.py"
            entry.write_text(textwrap.dedent(source))
            render_file(entry, Path(folder) / "scene.png", at=0.5)
        self.assertAlmostEqual(builtins._clock_ramped.value, 32.0, places=4)
        with self.assertRaisesRegex(RuntimeError, "session"):
            builtins._clock_timeline.size()


if __name__ == "__main__":
    unittest.main()
