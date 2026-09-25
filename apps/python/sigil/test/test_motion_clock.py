"""The engine: one a body owns and moves, and the one a session lends.

One Engine class covers both owners: the one a body constructs and moves
itself, and the one a session lends, which refuses every call that moves
its clock and runs animations, timelines and timers all the same. The
cases below ask the owned one everything, because it is the one a test
can move a frame at a time, and ask a session only what only a session
can say.

A frame a test states moves the engine to an ABSOLUTE time, so every case
steps by naming where the engine is to stand, and the cases that check a
value along the way ask for a linear easing, so a number here is the
motion's arithmetic and not the default ease-out's.
"""

import builtins
import gc
import tempfile
import textwrap
import unittest
import weakref
from pathlib import Path

from _sigil import motion as native
from sigil import motion
from sigil.motion import (
    Animatable,
    AnimatableForm,
    Animation,
    ClockPolicy,
    Engine,
    EngineOptions,
    Position,
    Timeline,
    Timer,
    Tween,
    afterPrevious,
    animatable,
    at,
    atLabel,
    ease,
    withPrevious,
)
from sigil.sketch import render_file

# A step rate and a frame length that share no common step: twenty frames
# of this length run the whole second's steps, one or two per frame,
# without ever reaching the catch-up limit below.
STEP_RATE = 27.0
FRAME = 0.05
CATCH_UP = 2

# Every name the clock package registers, as an author writes it.
SURFACE = (
    "Animation",
    "ClockPolicy",
    "Engine",
    "EngineOptions",
    "Playback",
    "Position",
    "Timeline",
    "Timer",
    "afterEnd",
    "afterPrevious",
    "at",
    "atLabel",
    "withPrevious",
)


def linear(to, duration):
    return Tween(to=to, duration=duration, ease=ease.linear)


class Spellings(unittest.TestCase):
    def test_the_package_re_exports_everything_the_clock_registers(self):
        # A registration the package does not follow is a name an author
        # cannot reach at all, so the two lists are asked to agree.
        for name in SURFACE:
            with self.subTest(name=name):
                self.assertIs(getattr(motion, name), getattr(native, name))
                self.assertIn(name, motion.__all__)


class Frames(unittest.TestCase):
    def test_a_stated_frame_moves_to_an_absolute_time_going_forward(self):
        engine = Engine()
        self.assertFalse(engine.isRunning())
        self.assertAlmostEqual(engine.advance(0.016), 0.016)
        self.assertAlmostEqual(engine.elapsed(), 0.016)
        self.assertEqual(engine.advance(0.01), 0.0)
        self.assertAlmostEqual(engine.elapsed(), 0.016)
        self.assertEqual(engine.frames(), 2)

    def test_a_fixed_step_moves_every_frame_the_engine_takes(self):
        engine = Engine(EngineOptions(fixedStep=0.1))
        self.assertAlmostEqual(engine.advance(), 0.1)
        self.assertAlmostEqual(engine.advance(), 0.1)
        self.assertAlmostEqual(engine.elapsed(), 0.2)

    def test_the_wall_is_read_as_stated_scaled_and_clamped(self):
        engine = Engine()
        self.assertEqual(engine.advanceWall(10.0), 0.0)
        self.assertAlmostEqual(engine.advanceWall(10.1), 0.1)
        self.assertAlmostEqual(engine.advanceWall(20.0), 0.25)
        engine.setSpeed(0.5)
        self.assertEqual(engine.speed(), 0.5)
        self.assertAlmostEqual(engine.advanceWall(20.1), 0.05)
        wide = Engine(EngineOptions(maxWallStep=0.5))
        wide.advanceWall(0.0)
        self.assertAlmostEqual(wide.advanceWall(10.0), 0.5)

    def test_a_policy_decides_who_moves_the_clock(self):
        engine = Engine()
        self.assertTrue(engine.isWall())
        engine.setPolicy(ClockPolicy.Pause)
        self.assertEqual(engine.policy(), ClockPolicy.Pause)
        self.assertTrue(engine.isPaused())
        self.assertEqual(engine.advance(1.0), 0.0)
        engine.setPolicy(ClockPolicy.Advance)
        self.assertFalse(engine.isWall())
        self.assertAlmostEqual(engine.advance(1.0), 1.0)
        engine.advanceWall(0.0)
        self.assertEqual(engine.advanceWall(1.0), 0.0)
        engine.setHeld(True)
        self.assertTrue(engine.isHeld())
        self.assertEqual(engine.advance(2.0), 0.0)
        engine.setHeld(False)
        engine.setPolicy(ClockPolicy.PauseWhileLoading)
        engine.setArriving(True)
        self.assertTrue(engine.isPaused())

    def test_a_budget_runs_out_once(self):
        engine = Engine()
        engine.setPolicy(ClockPolicy.Advance, budget=0.5)
        self.assertAlmostEqual(engine.budgetRemaining(), 0.5)
        engine.advance(0.25)
        self.assertFalse(engine.isBudgetExpired())
        engine.advance(0.5)
        self.assertTrue(engine.isBudgetExpired())
        self.assertIsNone(engine.budgetRemaining())
        engine.advance(0.75)
        self.assertFalse(engine.isBudgetExpired())

    def test_a_restart_counts_from_zero(self):
        engine = Engine()
        engine.advance(1.0)
        engine.restart()
        self.assertEqual(engine.elapsed(), 0.0)
        self.assertEqual(engine.frames(), 0)

    def test_a_host_moves_its_own_engine_only(self):
        with self.assertRaisesRegex(ValueError, "finite"):
            Engine().advanceWall(float("inf"))
        with self.assertRaisesRegex(ValueError, "speed"):
            Engine().setSpeed(-1)


class Timers(unittest.TestCase):
    def test_a_timer_names_the_arguments_it_reads(self):
        engine = Engine()
        seen = []
        engine.timer(lambda: seen.append("nothing"))
        engine.timer(lambda delta: seen.append(delta))
        engine.timer(lambda delta, elapsed: seen.append((delta, elapsed)))
        engine.advance(0.5)
        self.assertEqual(seen, ["nothing", 0.5, (0.5, 0.5)])
        # Saying nothing about being finished is taken as running on.
        self.assertTrue(engine.isRunning())
        with self.assertRaises(TypeError):
            engine.timer(lambda first, second, third: None)

    def test_a_timer_that_answers_false_is_retired(self):
        engine = Engine()
        runs = []

        def once():
            runs.append(1)
            return False

        timer = engine.timer(once)
        self.assertIsInstance(timer, Timer)
        engine.advance(0.1)
        self.assertFalse(engine.isRunning())
        engine.advance(0.2)
        self.assertEqual(runs, [1])

    def test_a_step_rate_runs_the_whole_second_at_its_own_rate(self):
        engine = Engine()
        runs = []
        timer = engine.timer(
            lambda: runs.append(1), stepRate=STEP_RATE, catchUp=CATCH_UP
        )
        for frame in range(round(1.0 / FRAME)):
            engine.advance((frame + 1) * FRAME)
            self.assertLessEqual(timer.stepsThisFrame(), CATCH_UP)
            self.assertFalse(timer.droppedTime())
        self.assertEqual(len(runs), int(STEP_RATE))
        self.assertAlmostEqual(timer.betweenSteps(), 0.0, places=4)

    def test_a_long_frame_drops_simulated_time_and_says_so(self):
        engine = Engine()
        runs = []
        timer = engine.timer(
            lambda: runs.append(1), stepRate=STEP_RATE, catchUp=CATCH_UP
        )
        engine.advance(1.0)
        self.assertEqual(len(runs), CATCH_UP)
        self.assertEqual(timer.stepsThisFrame(), CATCH_UP)
        self.assertTrue(timer.droppedTime())

    def test_the_leftover_fraction_of_a_step_is_the_render_interpolant(self):
        engine = Engine()
        timer = engine.timer(lambda: None, stepRate=10)
        engine.advance(0.25)
        self.assertAlmostEqual(timer.betweenSteps(), 0.5, places=5)

    def test_a_throttled_timer_skips_the_frames_between(self):
        engine = Engine()
        deltas = []
        engine.timer(lambda delta: deltas.append(round(delta, 6)), frameRate=10)
        engine.advance(0.05)
        self.assertEqual(deltas, [])
        engine.advance(0.1)
        self.assertEqual(deltas, [0.1])

    def test_a_timer_waits_its_delay_and_ends_after_its_duration(self):
        engine = Engine()
        seen = []
        timer = engine.timer(
            lambda delta, elapsed: seen.append(round(elapsed, 6)),
            delay=0.1,
            duration=0.2,
        )
        engine.advance(0.05)
        self.assertEqual(seen, [])
        engine.advance(0.15)
        self.assertEqual(seen, [0.05])
        engine.advance(0.35)
        self.assertTrue(timer.isCompleted())
        self.assertFalse(engine.isRunning())

    def test_a_timer_needs_sane_rates(self):
        with self.assertRaisesRegex(ValueError, "rates"):
            Engine().timer(lambda: None, stepRate=-1)
        with self.assertRaisesRegex(ValueError, "catch-up"):
            Engine().timer(lambda: None, catchUp=0)


class Animations(unittest.TestCase):
    def test_an_animation_writes_a_live_value(self):
        engine = Engine()
        value = animatable(0)
        animation = engine.animate(value, linear(1.0, 0.2))
        self.assertIsInstance(animation, Animation)
        self.assertTrue(engine.isRunning())
        engine.advance(0.1)
        self.assertAlmostEqual(value.value, 0.5, places=5)
        self.assertAlmostEqual(animation.progress(), 0.5, places=5)
        self.assertAlmostEqual(animation.currentTime(), 0.1, places=5)
        engine.advance(0.3)
        self.assertAlmostEqual(value.value, 1.0, places=6)
        self.assertTrue(animation.isCompleted())
        self.assertFalse(engine.isRunning())

    def test_a_constant_is_made_live_by_the_animation_on_it(self):
        engine = Engine()
        value = Animatable(3)
        engine.animate(value, Tween(to=5, duration=0.1))
        self.assertEqual(value.form, AnimatableForm.Live)
        engine.advance(0.2)
        self.assertAlmostEqual(value.value, 5.0, places=6)

    def test_the_playback_verbs_answer_the_playback(self):
        engine = Engine()
        value = animatable(0)
        animation = engine.animate(value, linear(1.0, 0.2))
        self.assertIs(animation.pause(), animation)
        self.assertTrue(animation.isPaused())
        engine.advance(0.1)
        self.assertEqual(value.value, 0.0)
        animation.resume()
        engine.advance(0.2)
        self.assertAlmostEqual(value.value, 0.5, places=5)
        self.assertIs(animation.seek(0.05), animation)
        self.assertAlmostEqual(value.value, 0.25, places=5)
        animation.revert()
        self.assertEqual(value.value, 0.0)
        self.assertFalse(animation.isRunning())

    def test_an_animation_reports_its_completion(self):
        engine = Engine()
        events = []
        value = animatable(0)
        animation = engine.animate(value, linear(1.0, 0.2))
        animation.onComplete(lambda: events.append("done"))
        engine.advance(0.1)
        self.assertEqual(events, [])
        engine.advance(0.3)
        self.assertEqual(events, ["done"])

    def test_a_python_easing_shapes_an_animation(self):
        engine = Engine()
        value = animatable(0)
        engine.animate(value, Tween(to=1, duration=0.2, ease=lambda unit: unit**2))
        engine.advance(0.1)
        self.assertAlmostEqual(value.value, 0.25, places=5)

    def test_an_animation_needs_a_tween(self):
        with self.assertRaisesRegex(TypeError, "Tween"):
            Engine().animate(animatable(0), 5)


class Timelines(unittest.TestCase):
    def test_an_item_with_no_position_starts_after_everything_before_it(self):
        engine = Engine()
        first, second = animatable(0), animatable(0)
        timeline = engine.timeline().add(first, linear(1, 0.2)).add(
            second, linear(1, 0.2)
        )
        self.assertIsInstance(timeline, Timeline)
        engine.advance(0.1)
        self.assertAlmostEqual(first.value, 0.5, places=5)
        self.assertEqual(second.value, 0.0)
        engine.advance(0.3)
        self.assertAlmostEqual(first.value, 1.0, places=5)
        self.assertAlmostEqual(second.value, 0.5, places=5)

    def test_positions_place_items_against_what_is_there(self):
        engine = Engine()
        first, second, third = animatable(0), animatable(0), animatable(0)
        (
            engine.timeline()
            .label("mid", at(0.1))
            .add(first, linear(1, 0.2), atLabel("mid"))
            .add(second, linear(1, 0.2), withPrevious())
            .add(third, linear(1, 0.2), afterPrevious(-0.1))
        )
        engine.advance(0.2)
        self.assertAlmostEqual(first.value, 0.5, places=5)
        self.assertAlmostEqual(second.value, 0.5, places=5)
        self.assertEqual(third.value, 0.0)
        engine.advance(0.3)
        self.assertAlmostEqual(third.value, 0.5, places=5)
        position = at(0.25)
        self.assertEqual(position.kind, Position.Kind.At)
        self.assertAlmostEqual(position.offset, 0.25)

    def call(self, engine, calls):
        """Place a call reading a value only the call's closure names."""

        class Reading:
            word = "held"

        reading = Reading()
        timeline = engine.timeline().call(
            lambda: calls.append(reading.word), at(0.2)
        )
        return timeline, weakref.ref(reading)

    def test_a_call_fires_once_going_forward(self):
        engine = Engine()
        calls = []
        timeline, held = self.call(engine, calls)
        gc.collect()
        self.assertIsNotNone(held(), "the timeline must hold what a call reads")
        engine.advance(0.1)
        self.assertEqual(calls, [])
        engine.advance(0.25)
        self.assertEqual(calls, ["held"])
        engine.advance(0.5)
        self.assertEqual(calls, ["held"])
        self.assertTrue(timeline.isCompleted())


class Sessions(unittest.TestCase):
    def test_a_session_lends_its_engine_and_takes_it_back(self):
        for name in ("_clock_engine", "_clock_ramped", "_clock_timeline"):
            self.addCleanup(lambda name=name: builtins.__dict__.pop(name, None))
        source = """
            import builtins
            from sigil.motion import Tween, animatable
            from sigil.sketch import sketch

            @sketch(size=(64, 24), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    ramped = animatable(0)
                    timeline = ctx.engine.timeline().add(
                        ramped, Tween(to=32.0, duration=0.2)
                    )
                    builtins._clock_ramped = ramped
                    builtins._clock_timeline = timeline
                    builtins._clock_engine = ctx.engine
        """
        with tempfile.TemporaryDirectory(prefix="sigil_clock_") as folder:
            entry = Path(folder) / "scene.py"
            entry.write_text(textwrap.dedent(source))
            render_file(entry, Path(folder) / "scene.png", at=0.5)
        self.assertAlmostEqual(builtins._clock_ramped.value, 32.0, places=4)
        self.assertTrue(builtins._clock_timeline.isCompleted())
        with self.assertRaisesRegex(RuntimeError, "session"):
            builtins._clock_engine.timeline()


if __name__ == "__main__":
    unittest.main()
