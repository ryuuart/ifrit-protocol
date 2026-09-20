"""Native animation values, retained source ownership, and session clocks."""

import builtins
import gc
import tempfile
import textwrap
import unittest
import weakref
from pathlib import Path

from _sigil import motion as native
from sigil import image
from sigil.compose import box
from sigil.motion import (
    Output,
    Transition,
    animate,
    bind,
    ease,
    from_,
    phase,
    quantizeTime,
    stepIndex,
    through,
    to,
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

        curve = Transition(ease=easing).ease
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
            pixels = image.load(output).rgba()
        return lambda x, y: pixels[(y * width + x) * 4 : (y * width + x + 1) * 4]

    def clean_globals(self, *names):
        for name in names:
            self.addCleanup(lambda name=name: builtins.__dict__.pop(name, None))

    def test_descriptions_keep_native_identity_and_seconds(self):
        self.assertIs(Output, native.Output)
        self.assertIs(Transition, native.Transition)
        spec = Transition(0.8, ease.cubicBezier(0.25, 0.1, 0.25, 1), 0.2)
        entered = animate(from_(0).to(1), spec)
        self.assertIsInstance(entered, native.Transitioned)
        self.assertEqual(entered.from_value, 0)
        self.assertEqual(entered.value, 1)
        self.assertEqual(entered.spec, spec.copy())
        changed = animate(to(4), spec)
        self.assertIsNone(changed.from_value)
        self.assertEqual(changed.value, 4)
        keyed = animate(through([(0, 0), (0.25, 1), (0.6, 0.5)]), ease=ease.linear)
        self.assertEqual(keyed.waypoints, [(0, 0), (0.25, 1), (0.6, 0.5)])
        self.assertAlmostEqual(keyed.spec.duration, 0.6)
        with self.assertRaises(ValueError):
            through([(0.5, 1), (0.1, 2)])
        with self.assertRaises(ValueError):
            Transition(-1)

    def test_bound_copy_retains_source_and_compares_native_shapes(self):
        output = Output(0.5)
        reference = weakref.ref(output)
        chain = bind(output).window(0, 1).map(ease.outBack()).target(10, 30)
        same = bind(output).window(0, 1).map(ease.outBack()).target(10, 30)
        self.assertEqual(chain, same)
        changed = chain.copy().offset(1)
        self.assertNotEqual(changed, chain)
        before = chain.sample()
        del output
        gc.collect()
        self.assertIsNone(reference())
        self.assertAlmostEqual(chain.sample(), before)
        self.assertAlmostEqual(changed.sample(), before + 1, places=5)
        with self.assertRaises(TypeError):
            bind(None)

    def test_element_owns_bare_and_shaped_cells_after_model_collection(self):
        self.clean_globals("_motion_tree")
        for shaped in (False, True):
            with self.subTest(shaped=shaped):

                class Model:
                    pass

                model = Model()
                model.level = Output(0.5)
                model_ref, output_ref = weakref.ref(model), weakref.ref(model.level)
                opacity = bind(model.level).target(0, 1) if shaped else model.level
                builtins._motion_tree = (
                    box().width(16).height(16).fill("#ff0000").opacity(opacity)
                )
                del opacity, model
                gc.collect()
                self.assertIsNone(model_ref())
                self.assertIsNone(output_ref())
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

    def test_retained_node_and_output_can_outlive_their_context(self):
        self.clean_globals(
            "_motion_node", "_motion_output", "_motion_context", "_motion_model"
        )
        self.render("""import builtins
import weakref
from sigil.compose import box
from sigil.motion import Output
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        self.x = Output(4)
        builtins._motion_model = weakref.ref(self)
        builtins._motion_output = self.x
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
        builtins._motion_output.value = 24
        del builtins._motion_output
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

    def test_scene_ticker_drives_a_retained_binding_on_scene_time(self):
        self.clean_globals("_motion_ticks", "_motion_owner")
        source = """import builtins
import weakref
from sigil.compose import box
from sigil.motion import Output, bind
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        self.seconds = Output(0)
        builtins._motion_ticks = []
        builtins._motion_owner = weakref.ref(self)
        ctx.ticker.add(self.advance)
        ctx.render(
            (
                box()
                .width(12)
                .height(12)
                .fill("#ff0000")
                .translateX(bind(self.seconds).target(0, 32))
            )
        )

    def advance(self, dt, elapsed):
        self.seconds.value = elapsed
        builtins._motion_ticks.append((dt, elapsed))
        return elapsed < 1
"""
        initial = self.render(source, at=0)
        moved = self.render(source, at=0.5)
        self.assertEqual(initial(5, 5), bytes([255, 0, 0, 255]))
        self.assertEqual(moved(20, 5), bytes([255, 0, 0, 255]))
        self.assertEqual(moved(5, 5), bytes([0, 0, 0, 255]))
        self.assertAlmostEqual(builtins._motion_ticks[-1][1], 0.5, places=5)
        gc.collect()
        self.assertIsNone(builtins._motion_owner())

    def test_keyframes_and_color_entrances_render_at_the_requested_time(self):
        source = """from sigil.compose import box
from sigil.motion import Transition, animate, ease, from_, through
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
                    animate(through([(0, 0), (0.5, 32), (1, 0)]), ease=ease.linear)
                )
                .fill(
                    animate(from_("#ff0000").to("#0000ff"), Transition(1, ease.linear))
                )
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
from sigil.motion import Transition, animate, from_
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
            .opacity(animate(from_(0).to(1), Transition(1, self.curve)))
        )
        ctx.render(self.node)

    def curve(self, progress):
        return progress * progress
""",
            at=0.25,
        )
        gc.collect()
        self.assertIsNone(builtins._motion_curve_owner())

    def test_ticker_derivation_runs_after_its_writer_and_owns_cells(self):
        source = """import gc
from sigil.compose import box
from sigil.motion import Output, bind
from sigil.sketch import sketch


@sketch(size=(64, 24), background="#000000", capture_at=0)
class Scene:
    def setup(self, ctx):
        source = Output(0)
        target = Output(0)
        assert ctx.ticker.derive(target, bind(source).target(0, 32))
        assert ctx.ticker.add(lambda dt, elapsed: source.set(elapsed)) is None
        assert ctx.ticker.derive(Output(0), bind(Output(0.5)))
        ctx.render((box().width(12).height(12).fill("#ff0000").translateX(target)))
        del target
        gc.collect()
"""
        pixel = self.render(source, at=0.5)
        self.assertEqual(pixel(20, 5), bytes([255, 0, 0, 255]))
        self.assertEqual(pixel(5, 5), bytes([0, 0, 0, 255]))

    def test_fixed_steps_share_the_scene_clock_and_views_expire(self):
        self.clean_globals(
            "_motion_fixed_ticks", "_motion_alpha", "_motion_status", "_motion_ticker"
        )
        self.render(
            """
            import builtins
            from sigil.motion import FixedStatus, Output
            from sigil.sketch import sketch
            @sketch(size=(64, 24), background="#000000", capture_at=0)
            class Scene:
                def setup(self, ctx):
                    builtins._motion_fixed_ticks = []
                    builtins._motion_alpha = Output(0)
                    builtins._motion_status = FixedStatus()
                    builtins._motion_ticker = ctx.ticker
                    def fixed():
                        builtins._motion_fixed_ticks.append(1)
                    assert ctx.ticker.addFixed(10, fixed, alphaOut=builtins._motion_alpha,
                                               statusOut=builtins._motion_status) is None
                    try:
                        ctx.ticker.tick(0.1)
                    except RuntimeError as refusal:
                        assert "host steps the ticker it lends" in str(refusal)
                    else:
                        raise AssertionError("a session ticker stepped itself")
        """,
            at=0.25,
        )
        self.assertEqual(len(builtins._motion_fixed_ticks), 2)
        self.assertAlmostEqual(builtins._motion_alpha.value, 0.5, places=5)
        self.assertFalse(builtins._motion_status.clamped)
        with self.assertRaisesRegex(RuntimeError, "session"):
            builtins._motion_ticker.elapsed()
        with self.assertRaisesRegex(RuntimeError, "session"):
            builtins._motion_ticker.active()

    def test_a_false_return_retires_a_ticker_callback(self):
        self.clean_globals("_motion_retired_calls")
        self.render(
            """
            import builtins
            from sigil.sketch import sketch
            @sketch(size=(64, 24), capture_at=0)
            class Scene:
                def setup(self, ctx):
                    builtins._motion_retired_calls = []
                    def once(dt, elapsed):
                        builtins._motion_retired_calls.append(elapsed)
                        return False
                    assert ctx.ticker.add(once) is None
        """,
            at=0.5,
        )
        self.assertEqual(len(builtins._motion_retired_calls), 1)

    def test_phase_helpers_use_native_clock_math(self):
        self.assertAlmostEqual(phase(-0.25, 1), 0.75)
        self.assertEqual(phase(8, 0), 0)
        self.assertAlmostEqual(quantizeTime(0.39, 10), 0.3)
        self.assertEqual(stepIndex(0.39, 10), 3)


if __name__ == "__main__":
    unittest.main()
