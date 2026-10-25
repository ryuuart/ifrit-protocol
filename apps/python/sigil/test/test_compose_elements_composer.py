"""The composer: one class over a tree Python owns or one a host lends.

A body builds a composer of its own to measure or probe a tree beside
the one its session draws, and a session hands its own out as the same
class. The cases below ask the owned one everything that needs no
canvas, draw it inside a frame where one is lent, and ask a session only
what only a session can say: which calls stay the host's, and what a
wrapper that outlives its session answers.

A query is valid once a layout has run, and layout runs inside a draw,
so every case that reads a rect draws first. The two text queries answer
in the typography vocabulary, `compose.Beat` and `compose.TextUnit`.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import threading
import unittest
from pathlib import Path

from _sigil import compose as native
from sigil import compose, material, motion, skia, weave
from sigil.compose import Composer, box, slot
from support.rendering import pixel, renderedImage

# Every name this package nests inside the class it reports about.
NESTED = ("CacheState", "NodeCost")

# The calls a sketch's composer answered while the sketch adapter owned
# the class, which the one class still answers.
HOSTED = (
    "bounds",
    "dirty",
    "hitTest",
    "isRunning",
    "purgeCaches",
    "render",
    "renderSlot",
    "settling",
    "stats",
)

# The calls this package adds, on either owner.
GROWN = (
    "bakeDensity",
    "draw",
    "groundHeld",
    "live",
    "profile",
    "profiling",
    "scheduleSpan",
    "setBakeDensity",
    "setGroundHeld",
    "setInherited",
    "setKey",
    "setLive",
    "setPointer",
    "setProfiling",
    "setSize",
    "setView",
)


def plate():
    return box().key("plate").width(30).height(20).fill("#ff0000")


class Spellings(unittest.TestCase):
    def test_the_package_names_the_class_the_extension_registers(self):
        self.assertIs(Composer, native.Composer)
        self.assertIs(compose.Composer, native.Composer)
        for name in NESTED:
            with self.subTest(name=name):
                self.assertTrue(hasattr(Composer, name))

    def test_every_call_is_on_the_one_class(self):
        for name in HOSTED + GROWN:
            with self.subTest(name=name):
                self.assertTrue(callable(getattr(Composer, name)))

    def test_the_cache_state_carries_the_native_values(self):
        self.assertEqual(
            list(Composer.CacheState.__members__),
            ["Live", "Picture", "Texture", "Group", "Moving"],
        )


class Owned(unittest.TestCase):
    def test_a_composer_is_built_with_or_without_an_engine(self):
        self.assertIsInstance(Composer(), Composer)
        self.assertIsInstance(Composer(None), Composer)
        self.assertIsInstance(Composer(motion.Engine()), Composer)
        self.assertIsInstance(Composer(engine=motion.Engine()), Composer)
        with self.assertRaises(TypeError):
            Composer("engine")

    def test_a_composer_keeps_the_engine_it_was_made_over(self):
        engine = motion.Engine()
        composer = Composer(engine)
        del engine
        gc.collect()
        composer.render(plate())
        self.assertTrue(composer.dirty())

    def test_a_description_marks_the_tree_and_is_counted(self):
        composer = Composer()
        self.assertEqual(composer.stats().instances, 0)
        composer.render(root=plate())
        self.assertTrue(composer.dirty())
        self.assertTrue(composer.isRunning())
        stats = composer.stats()
        self.assertGreater(stats.describedNodes, 0)
        self.assertGreater(stats.instances, 0)

    def test_a_slot_is_replaced_by_name(self):
        composer = Composer()
        composer.render(box().key("root").children([slot("swap")]))
        before = composer.stats().instances
        composer.renderSlot(name="swap", content=plate())
        self.assertGreater(composer.stats().instances, before)
        # A name that matches no slot does nothing, silently.
        composer.renderSlot("absent", plate())

    def test_stats_are_a_copy_taken_when_asked(self):
        composer = Composer()
        first = composer.stats()
        composer.render(plate())
        self.assertEqual(first.describedNodes, 0)
        self.assertGreater(composer.stats().describedNodes, 0)

    def test_every_query_on_an_unknown_key_is_empty_and_silent(self):
        composer = Composer()
        composer.render(plate())
        self.assertIsNone(composer.bounds("missing"))
        self.assertIsNone(composer.bounds(key="missing"))
        self.assertEqual(composer.scheduleSpan("missing", 0), 0)
        self.assertEqual(composer.scheduleSpan(key="missing", trackIndex=3), 0)
        settling = composer.settling("missing")
        self.assertEqual(
            (settling.live, settling.reused, settling.degraded), (False, 0, 0)
        )

    def test_the_text_queries_answer_in_the_typography_vocabulary(self):
        self.assertTrue(callable(Composer.beatsOf))
        self.assertTrue(callable(Composer.units))
        self.assertTrue(issubclass(compose.Beat, motion.Beat))
        self.assertIs(compose.TextUnit, native.TextUnit)

    def test_a_schedule_on_an_unknown_key_is_empty(self):
        composer = Composer()
        composer.render(plate())
        self.assertEqual(composer.beatsOf("missing", 0), [])
        self.assertEqual(composer.beatsOf(key="plate", trackIndex=0), [])

    def test_units_on_an_unknown_key_are_empty(self):
        composer = Composer()
        composer.render(plate())
        every = weave.selectors.each(weave.Unit.Word)
        self.assertEqual(composer.units("missing", every, weave.Unit.Word), [])
        self.assertEqual(
            composer.units(key="plate", selector=every, unit=weave.Unit.Word), []
        )

    def test_the_dials_read_back_what_they_were_set_to(self):
        composer = Composer()
        self.assertFalse(composer.profiling())
        composer.setProfiling(True)
        self.assertTrue(composer.profiling())
        composer.setProfiling(on=False)
        self.assertFalse(composer.profiling())

        self.assertEqual(composer.bakeDensity(), 0)
        composer.setBakeDensity(devicePixelsPerUnit=2.0)
        self.assertEqual(composer.bakeDensity(), 2.0)

        self.assertFalse(composer.live())
        composer.setLive(True)
        self.assertTrue(composer.live())
        composer.setLive(live=False)
        self.assertFalse(composer.live())

        self.assertFalse(composer.groundHeld())
        composer.setGroundHeld(True)
        self.assertTrue(composer.groundHeld())
        composer.setGroundHeld(held=False)
        self.assertFalse(composer.groundHeld())

    def test_the_viewport_is_a_size_however_it_is_spelled(self):
        composer = Composer()
        composer.setSize((80, 48))
        composer.setSize([80, 48])
        composer.setSize(size=skia.Size(80, 48))
        for value in ("wide", 80, None, (1, 2, 3)):
            with self.subTest(value=value), self.assertRaises(TypeError):
                composer.setSize(value)

    def test_the_inherited_type_and_the_view_are_native_values(self):
        composer = Composer()
        composer.setInherited(weave.Type(size=20), "#112233")
        composer.setInherited(font=weave.Type(), ink=(0.0, 0.5, 1.0))
        with self.assertRaises(TypeError):
            composer.setInherited("serif", "#112233")
        composer.setView(material.Filter())
        composer.setView(view=material.Filter.blur(2.0))
        # A view is any filter, CSS's filter text among them; text no
        # filter grammar reads is refused by name.
        composer.setView("blur(2px)")
        with self.assertRaisesRegex(ValueError, "'blur' is no CSS filter"):
            composer.setView("blur")
        with self.assertRaises(TypeError):
            composer.setView(2.0)

    def test_the_pointer_and_the_keys_are_fed_by_name(self):
        composer = Composer()
        composer.setPointer((3, 4), True)
        composer.setPointer(canvasPoint=skia.Point(3, 4), pressed=False)
        composer.setKey("a", 65, True)
        composer.setKey(name="a", code=65, pressed=False)
        with self.assertRaises(TypeError):
            composer.setPointer("here", True)

    def test_drawing_takes_a_lent_canvas_and_nothing_else(self):
        composer = Composer()
        composer.render(plate())
        for value in (None, object(), "canvas"):
            with self.subTest(value=value), self.assertRaises(TypeError):
                composer.draw(value)

    def test_nothing_is_reported_before_a_frame_was_profiled(self):
        composer = Composer()
        self.assertEqual(composer.profile(), [])

    def test_a_composer_refuses_a_thread_it_was_not_made_on(self):
        composer = Composer()
        refusals = []

        def ask():
            try:
                composer.dirty()
            except RuntimeError as error:
                refusals.append(str(error))

        worker = threading.Thread(target=ask)
        worker.start()
        worker.join()
        self.assertEqual(len(refusals), 1)
        self.assertIn("thread", refusals[0])
        # The thread that made it is still answered.
        self.assertTrue(composer.dirty())


class Reports(unittest.TestCase):
    def test_a_node_cost_is_a_record(self):
        state = Composer.CacheState
        cost = Composer.NodeCost(
            label="plate",
            selfMilliseconds=1.5,
            totalMilliseconds=2.5,
            depth=3,
            cacheState=state.Picture,
            effectDeferred=True,
        )
        self.assertEqual(
            (cost.label, cost.selfMilliseconds, cost.totalMilliseconds, cost.depth),
            ("plate", 1.5, 2.5, 3),
        )
        self.assertTrue(cost.effectDeferred)
        self.assertTrue(cost.cached())
        for duplicate in (cost.copy(), copy.copy(cost), copy.deepcopy(cost)):
            self.assertIsNot(duplicate, cost)
            self.assertEqual(duplicate.label, "plate")
            self.assertEqual(duplicate.cacheState, cost.cacheState)
        duplicate = cost.copy()
        duplicate.label = "other"
        self.assertEqual(cost.label, "plate")

    def test_a_fresh_node_cost_is_live(self):
        cost = Composer.NodeCost()
        self.assertEqual(cost.cacheState, Composer.CacheState.Live)
        self.assertFalse(cost.cached())
        with self.assertRaisesRegex(TypeError, "Unknown NodeCost field"):
            Composer.NodeCost(selfSeconds=1)


class Session(unittest.TestCase):
    """A scene rendered through the native host, which lends the canvas."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_composer_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_composer_results")

    def render(self, drawing, *, setup="pass", update="pass", at=0):
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            "import builtins\n"
            "from _sigil import compose as native\n"
            "from sigil import compose, motion, skia, weave\n"
            "from sigil.compose import Composer, box\n"
            "from sigil.sketch import sketch\n"
            "results = builtins._sigil_composer_results\n"
            "def plate():\n"
            "    return box().key('plate').width(30).height(20).fill('#ff0000')\n"
            "@sketch(size=(48, 32), captureAt=0)\n"
            "class Scene:\n"
            "    def setup(self, ctx):\n"
            + textwrap.indent(textwrap.dedent(setup).strip(), "        ")
            + "\n    def update(self, elapsed, ctx):\n"
            + textwrap.indent(textwrap.dedent(update).strip(), "        ")
            + "\n    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        return renderedImage(source, output, at=at)


class Frames(Session):
    """What needs a canvas, asked inside a frame that lends one."""

    def test_an_owned_composer_draws_and_then_answers_where_things_landed(self):
        picture = self.render("""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((48, 32))
            probe.render(box().children([plate().hitTestable(True)]))
            results['before'] = probe.bounds('plate')
            probe.draw(canvas=pen.canvas())
            results['bounds'] = probe.bounds('plate')
            results['hit'] = probe.hitTest((4, 4))
            results['miss'] = probe.hitTest(canvasPoint=skia.Point(40, 28))
            results['nowhere'] = probe.hitTest((400, 280))
            results['dirty'] = probe.dirty()
            results['painted'] = probe.stats().nodesPainted
        """)
        self.assertIsNone(self.results["before"])
        bounds = self.results["bounds"]
        self.assertEqual((bounds.width(), bounds.height()), (30, 20))
        self.assertEqual(self.results["hit"], "plate")
        self.assertIsNone(self.results["nowhere"])
        self.assertNotEqual(self.results["miss"], "plate")
        self.assertFalse(self.results["dirty"])
        self.assertGreater(self.results["painted"], 0)
        self.assertEqual(pixel(picture, 4, 4), (255, 0, 0, 255))
        self.assertEqual(pixel(picture, 40, 28), (0, 0, 0, 255))

    def test_a_live_composer_holds_a_turning_node_and_says_so(self):
        self.render("""
            pen.background('#000000')
            turn = motion.animatable(0.0)
            probe = Composer()
            probe.setSize((48, 32))
            probe.setLive(True)
            wheel = (
                box().key('wheel').absolute().left(8).top(4).width(24).height(24)
                .rotate(motion.bind(turn, to=(0.0, 360.0)))
                .children([box().width(12).height(6).fill('#ff0000')])
            )
            probe.render(box().width(48).height(32).children([wheel]))
            probe.draw(pen.canvas())
            turn.set(0.05)
            probe.draw(pen.canvas())
            results['held'] = probe.stats().nodesHeldWhileMoving
            probe.setLive(False)
            turn.set(0.10)
            probe.draw(pen.canvas())
            results['unwatched'] = probe.stats().nodesHeldWhileMoving
        """)
        self.assertEqual(self.results["held"], 1)
        self.assertEqual(self.results["unwatched"], 0)

    def test_the_inherited_ink_is_what_an_unstyled_leaf_is_set_in(self):
        def reds(picture):
            data = picture.rgba()
            return sum(
                1
                for offset in range(0, len(data), 4)
                if data[offset] > 200 and data[offset + 1] < 80
            )

        drawing = """
            pen.background('#ffffff')
            probe = Composer()
            probe.setSize((48, 32))
            {inherit}
            probe.render(compose.text('M'))
            probe.draw(pen.canvas())
        """
        plain = self.render(drawing.format(inherit="pass"))
        inked = self.render(
            drawing.format(inherit="probe.setInherited(weave.Type(size=24), '#ff0000')")
        )
        self.assertEqual(reds(plain), 0)
        self.assertGreater(reds(inked), 0)

    def test_a_profiled_frame_is_rows_worst_first(self):
        self.render("""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((48, 32))
            probe.setProfiling(True)
            probe.render(box().key('root').children([plate()]))
            probe.draw(pen.canvas())
            results['rows'] = probe.profile()
            probe.setProfiling(False)
            results['cleared'] = probe.profile()
        """)
        rows = self.results["rows"]
        self.assertGreater(len(rows), 0)
        self.assertEqual(
            [row.selfMilliseconds for row in rows],
            sorted((row.selfMilliseconds for row in rows), reverse=True),
        )
        for row in rows:
            self.assertIsInstance(row, Composer.NodeCost)
            self.assertGreaterEqual(row.totalMilliseconds, 0)
        self.assertTrue(any("plate" in row.label for row in rows))
        self.assertEqual(self.results["cleared"], [])

    def test_a_composer_is_not_touched_from_inside_its_own_draw(self):
        self.render("""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((48, 32))

            def program(inner):
                try:
                    probe.stats()
                except RuntimeError as error:
                    results['refusal'] = str(error)

            probe.render(box().children([compose.pen(program).size(12, 12)]))
            probe.draw(pen.canvas())
            results['after'] = probe.stats().nodesPainted
        """)
        self.assertIn("inside its own draw", self.results["refusal"])
        self.assertGreater(self.results["after"], 0)

    def test_a_probe_may_run_on_the_engine_its_session_lends(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                probe = Composer(ctx.engine)
                probe.render(plate())
                results['probe'] = probe
                results['dirty'] = probe.dirty()
                ctx.render(box())
            """,
        )
        self.assertTrue(self.results["dirty"])
        # The engine was the session's, so the composer over it stops
        # answering when the session that lent it has closed.
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            self.results["probe"].dirty()

    def test_a_track_at_rest_reads_back_one_beat_per_unit(self):
        self.render("""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((96, 32))
            probe.render(box().children([
                compose.text('a b c', 10).key('line')
                .textFx(compose.textFx.enter(compose.textFx.rise(6)),
                        unit=weave.Unit.Word, progress=1)]))
            probe.draw(pen.canvas())
            results['beats'] = probe.beatsOf('line', 0)
            results['bounds'] = probe.bounds('line')
        """)
        beats, bounds = self.results["beats"], self.results["bounds"]
        self.assertEqual(len(beats), 3)
        self.assertEqual([beat.unitIndex for beat in beats], [0, 1, 2])
        for beat in beats:
            self.assertIsInstance(beat, compose.Beat)
            self.assertAlmostEqual(beat.localProgress, 1.0)
            self.assertGreater(beat.rect.width(), 0)
            self.assertGreater(beat.rect.height(), 0)
            self.assertGreaterEqual(beat.rect.left() + 0.5, bounds.left())
            self.assertLessEqual(beat.rect.right() - 0.5, bounds.right())

    def ring(self, letters, fit):
        """The rect of each letter of a ring run, read back from its track."""
        self.render(f"""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((160, 160))
            probe.render(box().children([
                compose.text('{letters}', 24).key('ring')
                .width(160).height(160).absolute().left(0).top(0)
                .textOnPath(compose.TextPath(
                    path=skia.Path.Circle(80, 80, 64),
                    orient=compose.TextPath.Orient.Upright,
                    fit=compose.TextPath.Fit.{fit}))
                .textFx(compose.textFx.enter(compose.textFx.rise(6)), progress=1)]))
            probe.draw(pen.canvas())
            results['letters'] = [beat.rect for beat in probe.beatsOf('ring', 0)]
        """)
        return self.results["letters"]

    @staticmethod
    def chords(letters):
        """From each letter's middle to the next one's, and last across the
        seam from the last letter back to the first."""
        middles = [
            ((rect.left() + rect.right()) / 2, (rect.top() + rect.bottom()) / 2)
            for rect in letters
        ]
        return [
            (
                (middles[(index + 1) % len(middles)][0] - middle[0]) ** 2
                + (middles[(index + 1) % len(middles)][1] - middle[1]) ** 2
            )
            ** 0.5
            for index, middle in enumerate(middles)
        ]

    def test_a_ring_run_fitted_by_size_closes_on_its_seam(self):
        fitted = self.ring("OOOOO", "Size")
        self.assertEqual(len(fitted), 5)
        chords = self.chords(fitted)
        for index, chord in enumerate(chords):
            with self.subTest(chord=index):
                self.assertAlmostEqual(chord, chords[0], delta=1.0)
        # Without a fit the five letters do not reach round the ring.
        unfitted = self.chords(self.ring("OOOOO", "None_"))
        self.assertGreater(unfitted[-1], unfitted[0] + 10)

    def test_units_land_inside_the_node_that_set_them(self):
        self.render("""
            pen.background('#ffffff')
            probe = Composer()
            probe.setSize((48, 32))
            probe.render(box().children([compose.text('a b c', 8).key('line')]))
            probe.draw(pen.canvas())
            every = weave.selectors.each(weave.Unit.Word)
            results['units'] = probe.units('line', every, weave.Unit.Word)
            results['bounds'] = probe.bounds('line')
        """)
        units, bounds = self.results["units"], self.results["bounds"]
        self.assertEqual(len(units), 3)
        self.assertEqual([unit.index for unit in units], [0, 1, 2])
        for unit in units:
            self.assertGreaterEqual(unit.rect.left() + 0.5, bounds.left())
            self.assertLessEqual(unit.rect.right() - 0.5, bounds.right())


class Lent(Session):
    """What only a session can say about the composer it hands out."""

    def test_a_sketch_is_handed_the_one_composer_class(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                results['type'] = type(ctx.composer)
                results['names'] = sorted(dir(ctx.composer))
                ctx.render(plate())
            """,
        )
        self.assertIs(self.results["type"], Composer)
        self.assertEqual(self.results["names"], sorted(dir(Composer)))

    def test_the_size_the_clock_and_the_draw_stay_the_hosts(self):
        # The setup renders nothing of its own: a render there replaces
        # the node the draw method runs in, and the draw is where the
        # third refusal is asked for.
        self.render(
            """
            pen.background('#000000')
            try:
                results['composer'].draw(pen.canvas())
            except RuntimeError as error:
                results['draw'] = str(error)
            """,
            setup="""
                results['composer'] = ctx.composer
                for name, call in (
                    ('size', lambda: ctx.composer.setSize((10, 10))),
                ):
                    try:
                        call()
                    except RuntimeError as error:
                        results[name] = str(error)
            """,
        )
        for name in ("size", "draw"):
            with self.subTest(name=name):
                self.assertIn("host", self.results[name])

    def test_a_session_composer_takes_the_dials_and_reports_a_profile(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                composer = ctx.composer
                composer.setProfiling(True)
                composer.setBakeDensity(1.0)
                composer.setPointer((4, 4), False)
                composer.setKey('a', 65, True)
                composer.render(box().key('root').children([plate()]))
            """,
            update="""
                if elapsed > 1 / 60:
                    results['rows'] = ctx.composer.profile()
                    results['density'] = ctx.composer.bakeDensity()
                    results['span'] = ctx.composer.scheduleSpan('plate', 0)
                    results['composer'] = ctx.composer
            """,
            at=0.05,
        )
        self.assertGreater(len(self.results["rows"]), 0)
        self.assertEqual(self.results["density"], 1.0)
        self.assertEqual(self.results["span"], 0)
        # Every call asks the session again, the grown ones as the old.
        composer = self.results["composer"]
        for call in (composer.stats, composer.profile, composer.profiling):
            with (
                self.subTest(call=call.__name__),
                self.assertRaisesRegex(RuntimeError, "closed session"),
            ):
                call()
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            composer.setSize((10, 10))


if __name__ == "__main__":
    unittest.main()
