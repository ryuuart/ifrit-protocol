"""The composer: one class over a tree Python owns or one a host lends.

A body builds a composer of its own to measure or probe a tree beside
the one its session draws, and a session hands its own out as the same
class. The cases below ask the owned one everything that needs no
canvas, draw it inside a frame where one is lent, and ask a session only
what only a session can say: which calls stay the host's, and what a
wrapper that outlives its session answers.

A query is valid once a layout has run, and layout runs inside a draw,
so every case that reads a rect draws first. The two text queries answer
in the typography vocabulary and are registered with it, so the cases
that read them stand down where that vocabulary is not there yet.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import threading
import unittest
import weakref
from pathlib import Path

from _sigil import compose as native
from sigil import compose, image, material, motion, skia, weave
from sigil.compose import Composer, box, slot
from sigil.sketch import render_file

# Every name this package nests inside the class it reports about.
NESTED = ("CacheState", "CompositePlane", "InputSpace", "NodeCost", "Promotion")

# The calls a sketch's composer answered while the sketch adapter owned
# the class, which the one class still answers.
HOSTED = (
    "active",
    "bounds",
    "dirty",
    "hitTest",
    "purgeCaches",
    "render",
    "renderSlot",
    "settling",
    "stats",
)

# The calls this package adds, on either owner.
GROWN = (
    "autoTexturePromotion",
    "autoTexturePromotionPolicy",
    "bakeDensity",
    "cascadeSpanMs",
    "compositeCounting",
    "compositePlane",
    "declareInputSpace",
    "declaredInputSpace",
    "draw",
    "profile",
    "profiling",
    "promotionReason",
    "setAutoTexturePromotion",
    "setBakeDensity",
    "setClock",
    "setCompositeCounting",
    "setInherited",
    "setKey",
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

    def test_the_promotion_policy_has_its_three_values(self):
        policy = native.PromotionPolicy
        self.assertEqual(set(policy.__members__), {"Off", "ByCost", "Eager"})

    def test_the_nested_enumerations_carry_the_native_values(self):
        self.assertEqual(
            list(Composer.InputSpace.__members__),
            ["EncodedSRGB", "LinearSRGB", "DisplayP3"],
        )
        self.assertEqual(
            list(Composer.CacheState.__members__),
            ["Live", "Picture", "Texture", "Promoted", "SplitOwn", "Group"],
        )
        self.assertEqual(
            list(Composer.Promotion.__members__),
            [
                "Cheap",
                "Warming",
                "Promoted",
                "AskedFor",
                "OptedOut",
                "Volatile",
                "Composited",
                "Transformed",
                "Filtered",
                "ReadsBackdrop",
                "TooBig",
                "SplitBaked",
                "HostsSpace",
            ],
        )


class Owned(unittest.TestCase):
    def test_a_composer_is_built_with_or_without_a_ticker(self):
        self.assertIsInstance(Composer(), Composer)
        self.assertIsInstance(Composer(None), Composer)
        self.assertIsInstance(Composer(motion.Ticker()), Composer)
        self.assertIsInstance(Composer(ticker=motion.Ticker()), Composer)
        with self.assertRaises(TypeError):
            Composer("ticker")

    def test_a_composer_keeps_the_ticker_it_was_made_over(self):
        ticker = motion.Ticker()
        composer = Composer(ticker)
        del ticker
        gc.collect()
        composer.render(plate())
        self.assertTrue(composer.dirty())

    def test_a_description_marks_the_tree_and_is_counted(self):
        composer = Composer()
        self.assertEqual(composer.stats().instances, 0)
        composer.render(root=plate())
        self.assertTrue(composer.dirty())
        self.assertTrue(composer.active())
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
        self.assertEqual(composer.cascadeSpanMs("missing", 0), 0)
        self.assertEqual(composer.cascadeSpanMs(key="missing", trackIndex=3), 0)
        settling = composer.settling("missing")
        self.assertEqual(
            (settling.live, settling.reused, settling.degraded), (False, 0, 0)
        )

    @unittest.skipUnless(hasattr(Composer, "beatsOf"), "no text effects")
    def test_a_schedule_on_an_unknown_key_is_empty(self):
        composer = Composer()
        composer.render(plate())
        self.assertEqual(composer.beatsOf("missing", 0), [])
        self.assertEqual(composer.beatsOf(key="plate", trackIndex=0), [])

    @unittest.skipUnless(hasattr(Composer, "units"), "no text units")
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

        self.assertFalse(composer.compositeCounting())
        composer.setCompositeCounting(on=True)
        self.assertTrue(composer.compositeCounting())

        self.assertEqual(composer.bakeDensity(), 0)
        composer.setBakeDensity(devicePixelsPerUnit=2.0)
        self.assertEqual(composer.bakeDensity(), 2.0)

    def test_the_input_space_is_a_declaration_that_reads_back(self):
        composer = Composer()
        space = Composer.InputSpace
        self.assertEqual(composer.declaredInputSpace(), space.EncodedSRGB)
        composer.declareInputSpace(space=space.DisplayP3)
        self.assertEqual(composer.declaredInputSpace(), space.DisplayP3)

    def test_promotion_takes_a_policy_or_a_truth_value(self):
        policy = native.PromotionPolicy
        composer = Composer()
        self.assertEqual(composer.autoTexturePromotionPolicy(), policy.ByCost)
        self.assertTrue(composer.autoTexturePromotion())
        composer.setAutoTexturePromotion(policy.Eager)
        self.assertEqual(composer.autoTexturePromotionPolicy(), policy.Eager)
        composer.setAutoTexturePromotion(False)
        self.assertEqual(composer.autoTexturePromotionPolicy(), policy.Off)
        self.assertFalse(composer.autoTexturePromotion())
        composer.setAutoTexturePromotion(on=True)
        self.assertEqual(composer.autoTexturePromotionPolicy(), policy.ByCost)
        composer.setAutoTexturePromotion(policy=policy.Off)
        self.assertFalse(composer.autoTexturePromotion())
        with self.assertRaises(TypeError):
            composer.setAutoTexturePromotion("Eager")

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
        composer.setView(material.Effect())
        composer.setView(view=material.Effect.blur(2.0))
        with self.assertRaises(TypeError):
            composer.setView("blur")

    def test_the_pointer_and_the_keys_are_fed_by_name(self):
        composer = Composer()
        composer.setPointer((3, 4), True)
        composer.setPointer(canvasPoint=skia.Point(3, 4), pressed=False)
        composer.setKey("a", 65, True)
        composer.setKey(name="a", code=65, pressed=False)
        with self.assertRaises(TypeError):
            composer.setPointer("here", True)

    def test_a_clock_is_kept_for_as_long_as_the_composer_is(self):
        composer = Composer()
        clock = motion.FrameClock()
        kept = weakref.ref(clock)
        composer.setClock(clock)
        del clock
        gc.collect()
        self.assertIsNotNone(kept())
        composer.setClock(None)
        composer.setClock(clock=None)
        del composer
        gc.collect()
        self.assertIsNone(kept())

    def test_drawing_takes_a_lent_canvas_and_nothing_else(self):
        composer = Composer()
        composer.render(plate())
        for value in (None, object(), "canvas"):
            with self.subTest(value=value), self.assertRaises(TypeError):
                composer.draw(value)

    def test_nothing_is_reported_before_a_frame_was_profiled_or_counted(self):
        composer = Composer()
        self.assertEqual(composer.profile(), [])
        plane = composer.compositePlane()
        self.assertEqual((plane.width, plane.height, plane.counts), (0, 0, b""))
        self.assertEqual(plane.at(0, 0), 0)
        self.assertEqual(plane.at(x=-1, y=-1), 0)
        self.assertEqual(plane.copy().counts, b"")
        self.assertEqual(copy.deepcopy(plane).width, 0)

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
    def test_every_promotion_has_a_reason_of_its_own(self):
        promotions = list(Composer.Promotion.__members__.values())
        reasons = [Composer.promotionReason(each) for each in promotions]
        self.assertTrue(all(isinstance(reason, str) and reason for reason in reasons))
        self.assertEqual(len(set(reasons)), len(promotions))
        self.assertEqual(
            Composer.promotionReason(promotion=Composer.Promotion.OptedOut),
            reasons[promotions.index(Composer.Promotion.OptedOut)],
        )

    def test_a_node_cost_is_a_record(self):
        promotion, state = Composer.Promotion, Composer.CacheState
        cost = Composer.NodeCost(
            label="plate",
            selfMs=1.5,
            totalMs=2.5,
            depth=3,
            cacheState=state.Picture,
            promotion=promotion.Volatile,
            refusals=(1 << promotion.Volatile.value) | (1 << promotion.Filtered.value),
            effectDeferred=True,
        )
        self.assertEqual(
            (cost.label, cost.selfMs, cost.totalMs, cost.depth),
            ("plate", 1.5, 2.5, 3),
        )
        self.assertTrue(cost.effectDeferred)
        self.assertTrue(cost.cached())
        self.assertTrue(cost.refused(promotion.Volatile))
        self.assertTrue(cost.refused(promotion=promotion.Filtered))
        self.assertFalse(cost.refused(promotion.OptedOut))
        for duplicate in (cost.copy(), copy.copy(cost), copy.deepcopy(cost)):
            self.assertIsNot(duplicate, cost)
            self.assertEqual(duplicate.label, "plate")
            self.assertEqual(duplicate.refusals, cost.refusals)
        duplicate = cost.copy()
        duplicate.label = "other"
        self.assertEqual(cost.label, "plate")

    def test_a_fresh_node_cost_is_live_and_cheap(self):
        cost = Composer.NodeCost()
        self.assertEqual(cost.cacheState, Composer.CacheState.Live)
        self.assertEqual(cost.promotion, Composer.Promotion.Cheap)
        self.assertEqual(cost.refusals, 0)
        self.assertFalse(cost.cached())
        with self.assertRaisesRegex(TypeError, "Unknown NodeCost field"):
            Composer.NodeCost(selfMilliseconds=1)


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
            "@sketch(size=(48, 32), capture_at=0)\n"
            "class Scene:\n"
            "    def setup(self, ctx):\n"
            + textwrap.indent(textwrap.dedent(setup).strip(), "        ")
            + "\n    def update(self, elapsed, ctx):\n"
            + textwrap.indent(textwrap.dedent(update).strip(), "        ")
            + "\n    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        render_file(source, output, at=at)
        return image.decode(output.read_bytes())

    def pixel(self, picture, x, y):
        offset = 4 * (y * picture.width() + x)
        return tuple(picture.rgba()[offset : offset + 4])


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
        self.assertEqual(self.pixel(picture, 4, 4), (255, 0, 0, 255))
        self.assertEqual(self.pixel(picture, 40, 28), (0, 0, 0, 255))

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

    def test_declaring_an_input_space_moves_no_pixel(self):
        drawing = """
            pen.background('#000000')
            probe = Composer()
            probe.setSize((48, 32))
            probe.declareInputSpace(Composer.InputSpace.{space})
            probe.render(box().children([plate().opacity(0.5)]))
            probe.draw(pen.canvas())
        """
        encoded = self.render(drawing.format(space="EncodedSRGB")).rgba()
        linear = self.render(drawing.format(space="LinearSRGB")).rgba()
        self.assertEqual(bytes(encoded), bytes(linear))

    def test_a_profiled_frame_is_rows_worst_first_with_a_reason_each(self):
        self.render("""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((48, 32))
            probe.setProfiling(True)
            probe.setAutoTexturePromotion(native.PromotionPolicy.Off)
            probe.render(box().key('root').children([plate()]))
            probe.draw(pen.canvas())
            results['rows'] = probe.profile()
            probe.setProfiling(False)
            results['cleared'] = probe.profile()
        """)
        rows = self.results["rows"]
        self.assertGreater(len(rows), 0)
        self.assertEqual(
            [row.selfMs for row in rows],
            sorted((row.selfMs for row in rows), reverse=True),
        )
        for row in rows:
            self.assertIsInstance(row, Composer.NodeCost)
            self.assertTrue(Composer.promotionReason(row.promotion))
            self.assertGreaterEqual(row.totalMs, 0)
        self.assertTrue(any(row.refused(Composer.Promotion.OptedOut) for row in rows))
        self.assertTrue(any("plate" in row.label for row in rows))
        self.assertEqual(self.results["cleared"], [])

    def test_a_counted_frame_gives_a_plane_the_size_of_its_canvas(self):
        self.render("""
            pen.background('#000000')
            probe = Composer()
            probe.setSize((48, 32))
            probe.setCompositeCounting(True)
            probe.render(box().children([plate()]))
            probe.draw(pen.canvas())
            results['plane'] = probe.compositePlane()
        """)
        plane = self.results["plane"]
        self.assertGreaterEqual(plane.width, 48)
        self.assertGreaterEqual(plane.height, 32)
        self.assertEqual(len(plane.counts), plane.width * plane.height)
        view = memoryview(plane)
        self.assertTrue(view.readonly)
        self.assertEqual(view.format, "B")
        self.assertEqual(view.shape, (plane.height, plane.width))
        self.assertEqual(view.tobytes(), plane.counts)
        self.assertEqual(plane.at(0, 0), plane.counts[0])
        self.assertEqual(plane.at(plane.width, 0), 0)

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

    def test_a_probe_may_run_on_the_ticker_its_session_lends(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                probe = Composer(ctx.ticker)
                probe.render(plate())
                results['probe'] = probe
                results['dirty'] = probe.dirty()
                ctx.render(box())
            """,
        )
        self.assertTrue(self.results["dirty"])
        # The ticker was the session's, so the composer over it stops
        # answering when the session that lent it has closed.
        with self.assertRaisesRegex(RuntimeError, "closed session"):
            self.results["probe"].dirty()

    @unittest.skipUnless(hasattr(Composer, "units"), "no text units")
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
                    ('clock', lambda: ctx.composer.setClock(None)),
                ):
                    try:
                        call()
                    except RuntimeError as error:
                        results[name] = str(error)
            """,
        )
        for name in ("size", "clock", "draw"):
            with self.subTest(name=name):
                self.assertIn("host", self.results[name])

    def test_a_session_composer_takes_the_dials_and_reports_a_profile(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                composer = ctx.composer
                composer.setProfiling(True)
                composer.setAutoTexturePromotion(native.PromotionPolicy.Off)
                composer.setBakeDensity(1.0)
                composer.setPointer((4, 4), False)
                composer.setKey('a', 65, True)
                composer.declareInputSpace(Composer.InputSpace.EncodedSRGB)
                composer.render(box().key('root').children([plate()]))
            """,
            update="""
                if elapsed > 1 / 60:
                    results['rows'] = ctx.composer.profile()
                    results['policy'] = ctx.composer.autoTexturePromotionPolicy()
                    results['density'] = ctx.composer.bakeDensity()
                    results['span'] = ctx.composer.cascadeSpanMs('plate', 0)
                    results['composer'] = ctx.composer
            """,
            at=0.05,
        )
        self.assertGreater(len(self.results["rows"]), 0)
        self.assertEqual(self.results["policy"], native.PromotionPolicy.Off)
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
