"""Values that change over time, the cells they share, and the session engine."""

import builtins
import copy
import gc
import tempfile
import textwrap
import unittest
import weakref
from datetime import timedelta
from pathlib import Path

from _sigil import motion as native
from sigil import media
from sigil.compose import Fill, box
from sigil.motion import (
    Animatable,
    AnimatableForm,
    Binding,
    ColorAnimatable,
    FillAnimatable,
    Keyframe,
    Oscillator,
    Place,
    Schedule,
    Spring,
    SpringParameters,
    Staggered,
    StaggerFrom,
    Timing,
    Tween,
    Wave,
    Wiggle,
    animatable,
    animate,
    bind,
    cues,
    ease,
    envelope,
    phase,
    quantizeTime,
    stagger,
    stepIndex,
)
from sigil.sketch import render_file


class Motion(unittest.TestCase):
    def test_native_curve_callbacks_close_abandoned_theme_providers(self):
        from sigil.draw import brush
        from sigil.sketch import kit

        original = kit.theme()
        opened = []

        def easing(progress):
            provider = kit.provide(original)
            provider.__enter__()
            opened.append(provider)
            return progress

        curve = Tween(ease=easing).ease
        self.assertEqual(curve(0.5), 0.5)
        self.assertFalse(opened[-1].active)
        pressure = brush.Pressure(curve=easing, variation=None)
        self.assertEqual(pressure.at(0.5), 0.5)
        self.assertFalse(opened[-1].active)
        self.assertEqual(kit.theme(), original)

    def render(self, source, at=0, width=64):
        with tempfile.TemporaryDirectory(prefix="sigil_motion_") as folder:
            entry, output = Path(folder) / "scene.py", Path(folder) / "scene.png"
            entry.write_text(textwrap.dedent(source))
            render_file(entry, output, at=at)
            pixels = media.load(output).frameAt(0).image.rgba()
        return lambda x, y: pixels[(y * width + x) * 4 : (y * width + x + 1) * 4]

    def clean_globals(self, *names):
        for name in names:
            self.addCleanup(lambda name=name: builtins.__dict__.pop(name, None))

    def test_descriptions_keep_native_identity_and_seconds(self):
        self.assertIs(Animatable, native.Animatable)
        self.assertIs(Tween, native.Tween)
        spec = Tween(duration=0.8, delay=0.2, ease=ease.cubicBezier(0.25, 0.1, 0.25, 1))
        self.assertAlmostEqual(spec.duration, 0.8)
        self.assertAlmostEqual(spec.delay, 0.2)
        # A length of time is a number of seconds or a timedelta.
        self.assertEqual(
            Tween(
                duration=timedelta(milliseconds=800),
                delay=timedelta(seconds=0.2),
                ease=spec.ease,
            ),
            spec,
        )
        self.assertEqual(spec.copy(), spec)
        entered = animate(from_=0, to=1, duration=0.8, delay=0.2)
        self.assertIsInstance(entered, native.Animatable)
        self.assertEqual(entered.form, AnimatableForm.Described)
        self.assertFalse(entered.isRunning())
        described = entered.described
        self.assertTrue(described.isEntrance())
        self.assertEqual((described.from_, described.to), (0, 1))
        self.assertAlmostEqual(described.duration, 0.8)
        # A described motion reads where it comes to rest.
        self.assertEqual(entered.value, 1)
        changed = animate(to=4)
        self.assertIsNone(changed.described.from_)
        self.assertFalse(changed.described.isEntrance())
        self.assertEqual(changed.value, 4)
        self.assertEqual(animate(Tween(to=4)), changed)
        with self.assertRaises(ValueError):
            Tween(to=1, duration=-1)
        with self.assertRaises(ValueError):
            animate()

    def test_keyframes_are_read_with_no_engine(self):
        path = Tween(
            from_=0,
            keyframes=[Keyframe(1, duration=0.25), Keyframe(0.5, duration=0.35)],
            ease=ease.linear,
        )
        self.assertEqual(len(path.keyframes), 2)
        self.assertAlmostEqual(path.keyframes[0].duration, 0.25)
        self.assertAlmostEqual(path.at(0.125), 0.5, places=5)
        self.assertAlmostEqual(path.at(0.25), 1.0, places=5)
        self.assertAlmostEqual(path.at(10), 0.5, places=5)
        self.assertEqual(path.rest(), 0.5)
        # Undurationed keyframes share the tween's duration.
        shared = Tween(from_=0, keyframes=[2, 4], duration=1, ease=ease.linear)
        self.assertAlmostEqual(shared.at(0.25), 1.0, places=5)
        self.assertAlmostEqual(shared.at(0.75), 3.0, places=5)
        self.assertEqual(copy.deepcopy(path), path)

    def test_the_three_value_families_make_their_own_animatables(self):
        self.assertIsInstance(animate(from_="#ff0000", to="#0000ff"), ColorAnimatable)
        self.assertIsInstance(animate(to=Fill.color("#ff0000")), FillAnimatable)
        self.assertIsInstance(animatable("#ff0000"), ColorAnimatable)
        self.assertIsInstance(animatable(Fill.color("#ff0000")), FillAnimatable)
        live = animatable(0.25)
        self.assertEqual(live.form, AnimatableForm.Live)
        self.assertTrue(live.isRunning())

    def test_a_live_value_is_one_cell_every_copy_reads(self):
        live = animatable(0.5)
        other = live.copy()
        self.assertEqual(live, other)
        self.assertNotEqual(live, animatable(0.5))
        other.value = 3
        self.assertEqual(live.value, 3)
        live.set(4)
        self.assertEqual(other(), 4)
        # A constant takes the number and stays a constant.
        still = Animatable(2)
        still.value = 5
        self.assertEqual(still.form, AnimatableForm.Constant)
        self.assertFalse(still.isRunning())
        with self.assertRaises(TypeError):
            bind(None)

    def test_a_binding_shapes_a_source_it_keeps_alive(self):
        live = animatable(0.5)
        reference = weakref.ref(live)
        chain = bind(live, from_=(0, 1), ease=ease.outBack(), to=(10, 30))
        same = bind(live, from_=(0, 1), ease=ease.outBack(), to=(10, 30))
        self.assertEqual(chain, same)
        self.assertEqual(chain.form, AnimatableForm.Bound)
        self.assertTrue(chain.isRunning())
        changed = bind(live, chain.binding, to=(11, 31))
        self.assertNotEqual(changed, chain)
        before = chain.value
        del live
        gc.collect()
        self.assertIsNone(reference())
        self.assertAlmostEqual(chain.value, before)
        self.assertAlmostEqual(changed.value, before + 1, places=4)
        # A binding of a constant is as still as its source.
        self.assertFalse(bind(0.5).isRunning())

    def test_a_binding_runs_its_stages_in_declared_order(self):
        stages = Binding(from_=(0, 2), to=(0, 100))
        self.assertAlmostEqual(stages.apply(1), 50)
        self.assertEqual(stages.from_.high, 2)
        folded = Binding(alternate=True, to=(0, 10))
        self.assertAlmostEqual(folded.apply(1.25), 5)
        gated = Binding(envelope=envelope.square(0.5))
        self.assertEqual(gated.apply(0.25), 1)
        self.assertEqual(gated.apply(0.75), 0)
        self.assertEqual(envelope.square(0.5), gated.envelope)
        clamped = Binding(to=(0, 10), clamp=(2, 8))
        self.assertEqual(clamped.apply(1), 8)
        shaken = Binding(wiggle=Wiggle(amount=0), to=(0, 1))
        self.assertEqual(shaken.apply(0.5), 0.5)
        self.assertEqual(Binding(quantize=5).apply(0.3), 0.25)

    def test_a_stagger_is_a_value_resolved_at_a_place(self):
        ladder = stagger(0.04)
        self.assertIsInstance(ladder, Staggered)
        self.assertTrue(ladder.isStaggered())
        self.assertAlmostEqual(ladder.at(Place(2, 5)), 0.08)
        spread = stagger((0, 360), from_=StaggerFrom.Last)
        self.assertAlmostEqual(spread.at(Place(0, 5)), 360)
        self.assertAlmostEqual(spread.at(Place(4, 5)), 0)
        centred = stagger(timedelta(milliseconds=100), from_=StaggerFrom.Center)
        self.assertAlmostEqual(centred.at(Place(2, 5)), 0.0)
        self.assertAlmostEqual(centred.at(Place(0, 5)), centred.at(Place(4, 5)))
        self.assertGreater(centred.at(Place(0, 5)), centred.at(Place(1, 5)))
        table = cues([0, 0.34, timedelta(milliseconds=720)])
        self.assertAlmostEqual(table.at(Place(2, 3)), 0.72)
        self.assertAlmostEqual(table.at(Place(9, 10)), 0.72)
        # A tween field takes a stagger, and reads it back.
        tween = Tween(to=stagger((0, 90)), delay=stagger(0.05))
        self.assertTrue(tween.isStaggered())
        self.assertIsInstance(tween.delay, Staggered)
        placed = tween.resolved(Place(2, 3))
        self.assertAlmostEqual(placed.delay, 0.1)
        self.assertAlmostEqual(placed.to, 90)
        self.assertFalse(placed.isStaggered())

    def test_a_schedule_resolves_a_timing_over_its_units(self):
        timing = Timing(delay=stagger(0.1), duration=0.2)
        self.assertAlmostEqual(timing.span(3), 0.4, places=5)
        self.assertEqual(timing, timing.copy())
        schedule = Schedule(timing, 3)
        self.assertAlmostEqual(schedule.total(), 0.4, places=5)
        self.assertAlmostEqual(schedule.start(2), 0.2, places=5)
        beat = schedule.beat(0.5, 1)
        self.assertEqual(beat.unitIndex, 1)
        self.assertAlmostEqual(beat.start, 0.1, places=5)
        self.assertAlmostEqual(beat.localProgress, 0.5, places=4)
        self.assertTrue(beat.running)
        self.assertAlmostEqual(schedule.localProgress(1.0, 0), 1.0)

    def test_signals_are_functions_of_time(self):
        wave = Oscillator(wave=Wave.Square, hertz=2)
        self.assertEqual(wave.at(0.1), 1)
        self.assertEqual(wave.at(timedelta(seconds=0.3)), -1)
        self.assertAlmostEqual(wave.fold(0.3), 0.6, places=5)
        self.assertEqual(wave(0.1), wave.at(0.1))
        spring = Spring()
        settled = spring.step(10, 5, SpringParameters(period=0.2, damping=1))
        self.assertTrue(settled.isSettled(10))
        self.assertFalse(spring.isSettled(10))
        halves = spring.step(10, 0.05).step(10, 0.05)
        whole = spring.step(10, 0.1)
        self.assertAlmostEqual(halves.value, whole.value, places=4)

    def test_element_owns_bare_and_shaped_cells_after_model_collection(self):
        self.clean_globals("_motion_tree")
        for shaped in (False, True):
            with self.subTest(shaped=shaped):

                class Model:
                    pass

                model = Model()
                model.level = animatable(0.5)
                model_ref, value_ref = weakref.ref(model), weakref.ref(model.level)
                opacity = bind(model.level, to=(0, 1)) if shaped else model.level
                builtins._motion_tree = (
                    box().width(16).height(16).fill("#ff0000").opacity(opacity)
                )
                del opacity, model
                gc.collect()
                self.assertIsNone(model_ref())
                self.assertIsNone(value_ref())
                pixel = self.render("""
                    import builtins
                    from sigil.sketch import sketch
                    @sketch(size=(64, 24), background="#000000", capture_at=0)
                    class Scene:
                        def setup(self, ctx):
                            ctx.render(builtins._motion_tree.copy())
                """)
                self.assertIn(pixel(8, 8)[0], (127, 128))
                self.assertEqual(pixel(8, 8)[1:], bytes([0, 0, 255]))

    def test_retained_node_and_live_value_can_outlive_their_context(self):
        self.clean_globals(
            "_motion_node", "_motion_live", "_motion_context", "_motion_model"
        )
        self.render("""import builtins
import weakref
from sigil.compose import box
from sigil.motion import animatable
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        self.x = animatable(4)
        builtins._motion_model = weakref.ref(self)
        builtins._motion_live = self.x
        builtins._motion_context = ctx
        builtins._motion_node = (
            box().width(12).height(12).fill("#00ff00").translateX(self.x)
        )
        ctx.render(builtins._motion_node)
""")
        gc.collect()
        self.assertIsNone(builtins._motion_model())
        with self.assertRaisesRegex(RuntimeError, "session"):
            _ = builtins._motion_context.elapsed
        builtins._motion_live.value = 24
        del builtins._motion_live
        gc.collect()
        pixel = self.render("""
            import builtins
            from sigil.sketch import sketch
            @sketch(size=(64, 24), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    ctx.render(builtins._motion_node.copy())
        """)
        self.assertEqual(pixel(28, 6), bytes([0, 255, 0, 255]))
        self.assertEqual(pixel(6, 6), bytes([0, 0, 0, 255]))

    def test_a_scene_timer_drives_a_retained_binding_on_scene_time(self):
        self.clean_globals("_motion_updates", "_motion_owner")
        source = """import builtins
import weakref
from sigil.compose import box
from sigil.motion import animatable, bind
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        self.seconds = animatable(0)
        builtins._motion_updates = []
        builtins._motion_owner = weakref.ref(self)
        ctx.engine.timer(self.advance)
        ctx.render(
            (
                box()
                .width(12)
                .height(12)
                .fill("#ff0000")
                .translateX(bind(self.seconds, to=(0, 32)))
            )
        )

    def advance(self, delta, elapsed):
        self.seconds.value = elapsed
        builtins._motion_updates.append((delta, elapsed))
        return elapsed < 1
"""
        initial = self.render(source, at=0)
        moved = self.render(source, at=0.5)
        self.assertEqual(initial(5, 5), bytes([255, 0, 0, 255]))
        self.assertEqual(moved(20, 5), bytes([255, 0, 0, 255]))
        self.assertEqual(moved(5, 5), bytes([0, 0, 0, 255]))
        # The still is the sweep's photograph, one frame past the moment.
        self.assertAlmostEqual(builtins._motion_updates[-1][1], 0.5 + 1 / 60, places=5)
        gc.collect()
        self.assertIsNone(builtins._motion_owner())

    def test_keyframes_and_color_entrances_render_at_the_requested_time(self):
        source = """from sigil.compose import box
from sigil.motion import Keyframe, animate, ease
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        ctx.render(
            (
                box()
                .width(12)
                .height(12)
                .translateX(
                    animate(
                        from_=0,
                        keyframes=[Keyframe(32, duration=0.5), Keyframe(0, duration=0.5)],
                        ease=ease.linear,
                    )
                )
                .fill(animate(from_="#ff0000", to="#0000ff", duration=1, ease=ease.linear))
            )
        )
"""
        pixel = self.render(source, at=0.5)
        self.assertEqual(pixel(37, 5)[1], 0)
        self.assertGreater(pixel(37, 5)[0], 100)
        self.assertGreater(pixel(37, 5)[2], 100)
        self.assertEqual(pixel(5, 5), bytes([0, 0, 0, 255]))

    def test_callback_easing_does_not_keep_a_closed_model_alive(self):
        self.clean_globals("_motion_curve_owner")
        self.render(
            """import builtins
import weakref
from sigil.compose import box
from sigil.motion import animate
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        builtins._motion_curve_owner = weakref.ref(self)
        self.node = (
            box()
            .width(12)
            .height(12)
            .fill("#ffffff")
            .opacity(animate(from_=0, to=1, duration=1, ease=self.curve))
        )
        ctx.render(self.node)

    def curve(self, progress):
        return progress * progress
""",
            at=0.25,
        )
        gc.collect()
        self.assertIsNone(builtins._motion_curve_owner())

    def test_an_engine_animation_writes_the_live_value_a_node_reads(self):
        source = """from sigil.compose import box
from sigil.motion import Tween, animatable, ease
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        x = animatable(0)
        ctx.engine.animate(x, Tween(to=32, duration=1, ease=ease.linear))
        ctx.render((box().width(12).height(12).fill("#ff0000").translateX(x)))
"""
        pixel = self.render(source, at=0.5)
        self.assertEqual(pixel(20, 5), bytes([255, 0, 0, 255]))
        self.assertEqual(pixel(5, 5), bytes([0, 0, 0, 255]))

    def test_fixed_steps_share_the_scene_clock_and_views_expire(self):
        self.clean_globals(
            "_motion_fixed_steps", "_motion_timer", "_motion_engine"
        )
        self.render(
            """
            import builtins
            from sigil.sketch import sketch
            @sketch(size=(64, 24), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    builtins._motion_fixed_steps = []
                    builtins._motion_engine = ctx.engine
                    def fixed():
                        builtins._motion_fixed_steps.append(1)
                    builtins._motion_timer = ctx.engine.timer(fixed, stepRate=10)
                    try:
                        ctx.engine.advance()
                    except RuntimeError as refusal:
                        assert "host moves the engine it lends" in str(refusal)
                    else:
                        raise AssertionError("a session engine moved itself")
        """,
            at=0.25,
        )
        # The still is the sweep's photograph, one sixtieth of a second past
        # the moment: two whole tenths stepped and two thirds of the third.
        self.assertEqual(len(builtins._motion_fixed_steps), 2)
        self.assertAlmostEqual(builtins._motion_timer.betweenSteps(), 2 / 3, places=5)
        self.assertFalse(builtins._motion_timer.droppedTime())
        with self.assertRaisesRegex(RuntimeError, "session"):
            builtins._motion_engine.elapsed()
        with self.assertRaisesRegex(RuntimeError, "session"):
            builtins._motion_engine.isRunning()

    def test_a_false_return_retires_a_timer(self):
        self.clean_globals("_motion_retired_calls")
        self.render(
            """
            import builtins
            from sigil.sketch import sketch
            @sketch(size=(64, 24), capture_at=0)
            class Scene:
                def setup(self, ctx):
                    builtins._motion_retired_calls = []
                    def once(delta, elapsed):
                        builtins._motion_retired_calls.append(elapsed)
                        return False
                    ctx.engine.timer(once)
        """,
            at=0.5,
        )
        self.assertEqual(len(builtins._motion_retired_calls), 1)

    def test_phase_helpers_use_native_clock_math(self):
        self.assertAlmostEqual(phase(-0.25, 1), 0.75)
        self.assertAlmostEqual(phase(timedelta(seconds=1.25), 1), 0.25)
        self.assertEqual(phase(8, 0), 0)
        self.assertAlmostEqual(quantizeTime(0.39, 10), 0.3)
        self.assertEqual(stepIndex(0.39, 10), 3)


if __name__ == "__main__":
    unittest.main()
