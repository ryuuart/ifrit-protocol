"""Feeds: a ring of rows, a window of the newest, a clipped column.

A ring keeps the newest rows under sequence ids that never repeat, and a
feed builds only the window's worth of them, each keyed by its id. The
cases below ask the records and the rings everything that needs no
canvas, count what a description mounts through a composer of their own,
and go to a session for the two things only a session can say: where the
rows landed once a layout ran, and what a ring of Python's own values
answers after the session that retained them has closed.

The entrance is written in the schedule vocabulary and is registered
with it, so the case that reads it stands down where that vocabulary is
not there yet.
"""

import builtins
import copy
import gc
import tempfile
import textwrap
import unittest
import weakref
from pathlib import Path

from _sigil.compose import feed as native
from sigil import compose, motion, weave
from sigil.compose import Composer, box, feed, text
from sigil.sketch import render_file

# Every name this package registers under the feed module.
NAMES = (
    "Options",
    "Ring",
    "Row",
    "TextOptions",
    "TextRing",
    "TextRow",
    "feed",
    "rowKey",
    "textRow",
)


def sheet(size=12.0):
    base = weave.textStyle(weave.Type(size=size, color="#ffffff"))
    return weave.StyleSheet(base).set("dim", weave.Type(color="#808080"))


def line(words, style=""):
    return feed.TextRow(text=words, style=style)


def log(count, capacity=512):
    ring = feed.TextRing(capacity)
    for index in range(count):
        ring.append(line(f"line {index + 1}"))
    return ring


def windowed(visible):
    return feed.TextOptions(window=feed.Options(visible=visible), styles=sheet())


def instances(root):
    composer = Composer()
    composer.render(box().children([root]))
    return composer.stats().instances


class Spellings(unittest.TestCase):
    def test_the_package_names_what_the_extension_registers(self):
        self.assertIs(compose.feed, feed)
        for name in NAMES:
            with self.subTest(name=name):
                self.assertIs(getattr(feed, name), getattr(native, name))


class Records(unittest.TestCase):
    def test_options_default_to_the_native_window(self):
        options = feed.Options()
        self.assertEqual(options.visible, 24)
        self.assertEqual(options.gap, 2.0)

    def test_options_are_built_from_keywords_and_written_by_name(self):
        options = feed.Options(visible=6, gap=0.5)
        self.assertEqual((options.visible, options.gap), (6, 0.5))
        options.visible = 9
        self.assertEqual(options.visible, 9)
        with self.assertRaisesRegex(TypeError, "Unknown Options field: windw"):
            feed.Options(windw=3)
        with self.assertRaises(TypeError):
            options.visible = -1

    def test_options_compare_by_value_and_copy_apart(self):
        options = feed.Options(visible=6)
        self.assertEqual(options, feed.Options(visible=6))
        self.assertNotEqual(options, feed.Options(visible=7))
        for duplicate in (options.copy(), copy.copy(options), copy.deepcopy(options)):
            with self.subTest(duplicate=duplicate):
                self.assertEqual(duplicate, options)
                duplicate.gap = 11.0
                self.assertNotEqual(duplicate, options)
                self.assertEqual(options.gap, 2.0)

    @unittest.skipUnless(
        hasattr(motion, "Spread") and hasattr(native.Options, "entrance"),
        "no schedule vocabulary",
    )
    def test_the_entrance_is_a_spread_that_mounts_at_once_by_default(self):
        options = feed.Options()
        self.assertEqual(options.entrance.eachMs, 0)
        options.entrance = motion.Spread(eachMs=40)
        self.assertEqual(options.entrance.eachMs, 40)
        self.assertNotEqual(options, feed.Options())
        self.assertEqual(options, feed.Options(entrance=motion.Spread(eachMs=40)))
        options.entrance = feed.Options().entrance
        self.assertEqual(options, feed.Options())

    def test_a_text_row_reads_and_writes_its_line_as_a_string(self):
        row = feed.TextRow()
        self.assertEqual((row.text, row.style), ("", ""))
        row = feed.TextRow(text="naïve — 行", style="dim")
        self.assertIsInstance(row.text, str)
        self.assertEqual((row.text, row.style), ("naïve — 行", "dim"))
        row.text = "second"
        self.assertEqual(row.text, "second")
        with self.assertRaises(TypeError):
            row.text = 5
        with self.assertRaisesRegex(TypeError, "Unknown TextRow field: line"):
            feed.TextRow(line="first")

    def test_text_rows_compare_by_value_and_copy_apart(self):
        row = line("first", "dim")
        self.assertEqual(row, line("first", "dim"))
        self.assertNotEqual(row, line("first"))
        self.assertNotEqual(row, line("second", "dim"))
        duplicate = copy.deepcopy(row)
        duplicate.style = "pass"
        self.assertEqual(row.style, "dim")

    def test_text_options_are_written_through_their_window_and_styles(self):
        options = feed.TextOptions()
        self.assertEqual(options, feed.TextOptions())
        self.assertEqual(options.window, feed.Options())
        options.window.visible = 8
        options.window.gap = 0.0
        self.assertEqual((options.window.visible, options.window.gap), (8, 0.0))
        options.styles = sheet()
        options.styles.set("pass", weave.Type(color="#00ff00"))
        self.assertTrue(options.styles.contains("dim"))
        self.assertTrue(options.styles.contains("pass"))
        built = feed.TextOptions(
            window=feed.Options(visible=8, gap=0.0),
            styles=sheet().set("pass", weave.Type(color="#00ff00")),
        )
        self.assertEqual(options, built)
        self.assertNotEqual(options, feed.TextOptions())

    def test_text_options_copy_apart(self):
        options = windowed(6)
        duplicate = options.copy()
        duplicate.window.visible = 7
        self.assertEqual(options.window.visible, 6)
        self.assertNotEqual(options, duplicate)

    def test_a_row_is_a_sequence_id_and_the_value_under_it(self):
        row = feed.Row()
        self.assertEqual((row.sequence, row.value), (0, None))
        row = feed.Row(sequence=3, value={"reading": 1.5})
        self.assertEqual(row, feed.Row(sequence=3, value={"reading": 1.5}))
        self.assertNotEqual(row, feed.Row(sequence=4, value={"reading": 1.5}))
        self.assertNotEqual(row, feed.Row(sequence=3, value={"reading": 2.5}))
        self.assertNotEqual(row, (3, {"reading": 1.5}))


class TextRings(unittest.TestCase):
    def test_a_ring_is_built_with_or_without_a_capacity(self):
        self.assertEqual(feed.TextRing().capacity(), 512)
        self.assertEqual(feed.TextRing(3).capacity(), 3)
        self.assertEqual(feed.TextRing(capacity=3).capacity(), 3)
        with self.assertRaises(TypeError):
            feed.TextRing(-1)

    def test_sequence_ids_start_at_one_and_never_repeat(self):
        ring = feed.TextRing()
        self.assertTrue(ring.empty())
        self.assertEqual(ring.nextSequence(), 1)
        self.assertEqual(ring.append(line("first")), 1)
        self.assertEqual(ring.append(value=line("second")), 2)
        self.assertFalse(ring.empty())
        ring.clear()
        self.assertTrue(ring.empty())
        self.assertEqual(ring.size(), 0)
        self.assertEqual(ring.nextSequence(), 3)
        self.assertEqual(ring.append(line("third")), 3)

    def test_the_oldest_rows_fall_off_at_capacity(self):
        ring = log(5, capacity=3)
        self.assertEqual((ring.size(), len(ring)), (3, 3))
        self.assertEqual([row.sequence for row in ring.rows()], [3, 4, 5])
        self.assertEqual(
            [row.value.text for row in ring.rows()], ["line 3", "line 4", "line 5"]
        )
        self.assertEqual(ring.nextSequence(), 6)

    def test_a_ring_takes_text_rows_and_nothing_else(self):
        ring = feed.TextRing()
        with self.assertRaises(TypeError):
            ring.append("bare words")
        with self.assertRaises(TypeError):
            ring.append()
        self.assertTrue(ring.empty())

    def test_rows_are_copies_taken_when_asked(self):
        ring = log(2)
        rows = ring.rows()
        rows[0].value.text = "rewritten"
        rows[0].sequence = 40
        rows.clear()
        ring.append(line("line 3"))
        self.assertEqual([row.sequence for row in ring.rows()], [1, 2, 3])
        self.assertEqual(ring.rows()[0].value, line("line 1"))

    def test_a_ring_is_indexed_from_either_end(self):
        ring = log(3)
        self.assertEqual(ring[0], feed.Row(sequence=1, value=line("line 1")))
        self.assertEqual(ring[2].sequence, 3)
        self.assertEqual(ring[-1].sequence, 3)
        self.assertEqual(ring[-3].sequence, 1)
        for index in (3, -4):
            with self.subTest(index=index), self.assertRaises(IndexError):
                ring[index]
        with self.assertRaises(IndexError):
            feed.TextRing()[0]

    def test_iteration_walks_the_rows_that_stood_when_it_began(self):
        ring = log(3)
        seen = []
        for row in ring:
            seen.append(row.sequence)
            ring.append(line("arrived meanwhile"))
        self.assertEqual(seen, [1, 2, 3])
        self.assertEqual(ring.size(), 6)
        self.assertEqual(list(ring), ring.rows())

    def test_a_copy_continues_the_sequence_on_its_own(self):
        ring = log(2)
        for duplicate in (ring.copy(), copy.copy(ring), copy.deepcopy(ring)):
            with self.subTest(duplicate=duplicate):
                self.assertEqual(duplicate.rows(), ring.rows())
                self.assertEqual(duplicate.append(line("line 3")), 3)
                self.assertEqual(ring.size(), 2)
                self.assertEqual(ring.nextSequence(), 3)


class Rings(unittest.TestCase):
    def test_a_ring_holds_whatever_it_is_given(self):
        class Reading:
            pass

        reading = Reading()
        ring = feed.Ring(capacity=4)
        self.assertEqual(ring.capacity(), 4)
        self.assertEqual(ring.append(reading), 1)
        self.assertEqual(ring.append(value=None), 2)
        self.assertEqual(ring.append(("FLUX", 0.25)), 3)
        self.assertIs(ring.rows()[0].value, reading)
        self.assertIs(ring[0].value, reading)
        self.assertIsNone(ring[1].value)
        self.assertEqual(ring[-1].value, ("FLUX", 0.25))

    def test_rows_compare_by_python_equality(self):
        ring = feed.Ring()
        ring.append({"level": "info", "words": "armed"})
        ring.append([1, 2, 3])
        self.assertEqual(
            ring.rows(),
            [
                feed.Row(sequence=1, value={"level": "info", "words": "armed"}),
                feed.Row(sequence=2, value=[1, 2, 3]),
            ],
        )
        self.assertNotEqual(ring[1], feed.Row(sequence=2, value=[1, 2]))

    def test_sequence_ids_survive_a_clear_and_the_window_slides(self):
        ring = feed.Ring(2)
        for value in ("a", "b", "c"):
            ring.append(value)
        self.assertEqual(
            [(row.sequence, row.value) for row in ring], [(2, "b"), (3, "c")]
        )
        self.assertEqual(len(ring), 2)
        ring.clear()
        self.assertTrue(ring.empty())
        self.assertEqual(ring.append("d"), 4)

    def test_a_ring_lets_go_of_what_it_drops(self):
        class Reading:
            pass

        def dropped(release):
            reading = Reading()
            watch = weakref.ref(reading)
            ring = feed.Ring(1)
            ring.append(reading)
            del reading
            gc.collect()
            self.assertIsNotNone(watch(), "a ring keeps the values it holds")
            release(ring)
            gc.collect()
            return watch()

        self.assertIsNone(dropped(lambda ring: ring.clear()))
        self.assertIsNone(dropped(lambda ring: ring.append("the next row")))

    def test_a_copy_shares_the_values_and_not_the_rows(self):
        value = {"shared": True}
        ring = feed.Ring()
        ring.append(value)
        duplicate = ring.copy()
        duplicate.append("only in the copy")
        self.assertEqual(ring.size(), 1)
        self.assertIs(duplicate[0].value, value)


class Columns(unittest.TestCase):
    def test_a_row_key_is_its_place_in_the_sequence(self):
        self.assertEqual(feed.rowKey(3), "row#3")
        self.assertEqual(feed.rowKey(sequence=512), "row#512")
        with self.assertRaises(TypeError):
            feed.rowKey(-1)

    def test_one_text_row_is_a_text_leaf(self):
        self.assertIsInstance(feed.textRow(line("first", "dim"), sheet()), compose.Text)
        self.assertIsInstance(
            feed.textRow(row=line("first", "unregistered"), styles=sheet()),
            compose.Text,
        )

    def test_a_text_feed_is_an_element_however_it_is_called(self):
        ring = log(3)
        self.assertIsInstance(feed.feed(ring, windowed(6)), compose.Element)
        self.assertIsInstance(
            feed.feed(ring=ring, options=windowed(6)), compose.Element
        )
        self.assertIsInstance(feed.feed(ring), compose.Element)
        self.assertIsInstance(feed.feed(ring=ring), compose.Element)
        # The column is an ordinary element: it takes the fluent verbs.
        self.assertIsInstance(feed.feed(ring, windowed(6)).flexGrow(1), compose.Element)

    def test_only_the_window_is_built(self):
        four = instances(feed.feed(log(4), windowed(4)))
        self.assertEqual(instances(feed.feed(log(10), windowed(4))), four)
        self.assertEqual(
            instances(feed.feed(log(400, capacity=600), windowed(4))), four
        )
        self.assertGreater(instances(feed.feed(log(10), windowed(8))), four)
        self.assertLess(instances(feed.feed(log(0), windowed(4))), four)

    def test_nothing_provided_means_the_default_options(self):
        ring = log(30)
        self.assertEqual(
            instances(feed.feed(ring)),
            instances(feed.feed(ring, feed.TextOptions())),
        )

    def test_a_row_function_is_called_once_for_each_visible_row(self):
        ring = feed.Ring()
        for value in ("a", "b", "c", "d", "e"):
            ring.append(value)
        seen = []

        def row(value):
            seen.append(value)
            return text(value)

        column = feed.feed(ring, feed.Options(visible=3), row)
        self.assertIsInstance(column, compose.Element)
        self.assertEqual(seen, ["c", "d", "e"])
        seen.clear()
        feed.feed(ring=ring, options=feed.Options(visible=1), row=row)
        self.assertEqual(seen, ["e"])
        seen.clear()
        feed.feed(feed.Ring(), feed.Options(), row)
        self.assertEqual(seen, [])

    def test_a_row_function_is_handed_the_value_that_was_appended(self):
        class Reading:
            def __init__(self, words):
                self.words = words

            def element(self):
                return text(self.words)

        readings = [Reading("armed"), Reading("sealed")]
        ring = feed.Ring()
        for reading in readings:
            ring.append(reading)
        seen = []

        def row(reading):
            seen.append(reading)
            return reading.element()

        feed.feed(ring, feed.Options(), row)
        self.assertEqual(len(seen), 2)
        for handed, appended in zip(seen, readings):
            self.assertIs(handed, appended)

    def test_a_text_ring_hands_its_row_function_a_text_row(self):
        ring = log(3)
        styles = sheet()
        seen = []

        def lit(row):
            seen.append(row)
            return feed.textRow(row, styles)

        column = feed.feed(ring, feed.Options(visible=2), lit)
        self.assertEqual(seen, [line("line 2"), line("line 3")])
        self.assertEqual(instances(column), instances(feed.feed(ring, windowed(2))))
        # The row handed over is a copy: writing to it leaves the ring alone.
        seen[0].text = "rewritten"
        self.assertEqual(ring[1].value.text, "line 2")

    def test_a_row_function_answers_an_element(self):
        ring = feed.Ring()
        ring.append("only")
        with self.assertRaisesRegex(TypeError, "returns a node"):
            feed.feed(ring, feed.Options(), lambda value: value)
        with self.assertRaisesRegex(TypeError, "returns a node"):
            feed.feed(ring, feed.Options(), lambda value: None)
        with self.assertRaises(TypeError):
            feed.feed(ring, feed.Options(), "not a function")

    def test_what_a_row_function_raises_is_what_the_caller_sees(self):
        ring = feed.Ring()
        for value in ("first", "second"):
            ring.append(value)
        calls = []

        def row(value):
            calls.append(value)
            raise LookupError("no element for " + value)

        with self.assertRaisesRegex(LookupError, "no element for first"):
            feed.feed(ring, feed.Options(), row)
        self.assertEqual(calls, ["first"])

    def test_a_feed_does_not_keep_its_row_function(self):
        class Builder:
            def row(self, value):
                return text(value)

        builder = Builder()
        watch = weakref.ref(builder)
        ring = feed.Ring()
        ring.append("only")
        column = feed.feed(ring, feed.Options(), builder.row)
        del builder
        gc.collect()
        self.assertIsNone(watch())
        self.assertIsInstance(column, compose.Element)


class Session(unittest.TestCase):
    """A scene rendered through the native host, which lends the canvas."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.results = {}
        builtins._sigil_feed_results = self.results
        self.addCleanup(delattr, builtins, "_sigil_feed_results")

    def render(self, drawing, *, setup="pass"):
        source = Path(self.directory.name) / "scene.py"
        output = Path(self.directory.name) / "scene.png"
        source.write_text(
            "import builtins\n"
            "import weakref\n"
            "from sigil import compose, weave\n"
            "from sigil.compose import Composer, box, feed, text\n"
            "from sigil.sketch import sketch\n"
            "results = builtins._sigil_feed_results\n"
            "def sheet():\n"
            "    base = weave.textStyle(weave.Type(size=8, color='#ffffff'))\n"
            "    return weave.StyleSheet(base)\n"
            "@sketch(size=(96, 96), capture_at=0)\n"
            "class Scene:\n"
            "    def setup(self, ctx):\n"
            + textwrap.indent(textwrap.dedent(setup).strip(), "        ")
            + "\n    def draw(self, pen):\n"
            + textwrap.indent(textwrap.dedent(drawing).strip(), "        ")
            + "\n"
        )
        render_file(source, output, at=0)


class Frames(Session):
    """What needs a layout, asked inside a frame that lends a canvas."""

    def test_rows_are_keyed_by_sequence_and_only_the_window_is_mounted(self):
        self.render("""
            pen.background('#000000')
            ring = feed.TextRing()
            for index in range(10):
                ring.append(feed.TextRow(text='line'))
            options = feed.TextOptions(window=feed.Options(visible=4), styles=sheet())
            probe = Composer()
            probe.setSize((96, 96))
            probe.render(box().children([feed.feed(ring, options)]))
            probe.draw(canvas=pen.canvas())
            for sequence in (1, 6, 7, 9, 10):
                results[sequence] = probe.bounds(feed.rowKey(sequence))
        """)
        self.assertIsNone(self.results[1])
        self.assertIsNone(self.results[6])
        self.assertIsNotNone(self.results[7])
        self.assertGreater(self.results[10].top(), self.results[9].top())
        self.assertGreater(self.results[9].top(), self.results[7].top())

    def test_the_sequence_key_stands_over_the_one_a_row_function_set(self):
        self.render("""
            pen.background('#000000')
            ring = feed.Ring()
            ring.append('only')
            probe = Composer()
            probe.setSize((96, 96))
            column = feed.feed(
                ring, feed.Options(), lambda value: text(value, 8).key('mine')
            )
            probe.render(box().children([column]))
            probe.draw(canvas=pen.canvas())
            results['mine'] = probe.bounds('mine')
            results['keyed'] = probe.bounds(feed.rowKey(1))
        """)
        self.assertIsNone(self.results["mine"])
        self.assertIsNotNone(self.results["keyed"])


class Teardown(Session):
    """What a ring that outlives its session still answers."""

    def test_a_ring_lets_go_of_its_values_when_its_session_closes(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                class Reading:
                    pass

                reading = Reading()
                results['reading'] = weakref.ref(reading)
                ring = feed.Ring()
                ring.append(reading)
                results['held'] = ring[0].value is reading
                results['ring'] = ring
                ctx.render(box())
            """,
        )
        self.assertTrue(self.results["held"])
        ring = self.results["ring"]
        gc.collect()
        self.assertIsNone(
            self.results["reading"](),
            "a retained ring must not keep its session's values alive",
        )
        # The ring still counts its rows; reading one says the value is gone.
        self.assertEqual(ring.size(), 1)
        self.assertEqual(ring.nextSequence(), 2)
        with self.assertRaisesRegex(RuntimeError, "lifetime has ended"):
            ring.rows()
        with self.assertRaisesRegex(RuntimeError, "lifetime has ended"):
            ring[0]
        calls = []
        with self.assertRaisesRegex(RuntimeError, "lifetime has ended"):
            feed.feed(ring, feed.Options(), lambda value: calls.append(value))
        self.assertEqual(calls, [])
        # A value appended with no session in force is the ring's alone.
        ring.clear()
        ring.append("after the session")
        self.assertEqual(ring[0], feed.Row(sequence=2, value="after the session"))

    def test_a_text_ring_is_a_value_that_outlives_its_session(self):
        self.render(
            "pen.background('#000000')",
            setup="""
                ring = feed.TextRing()
                ring.append(feed.TextRow(text='kept', style='dim'))
                results['ring'] = ring
                results['column'] = feed.feed(ring)
                ctx.render(box())
            """,
        )
        ring = self.results["ring"]
        self.assertEqual(ring[0].value, line("kept", "dim"))
        self.assertEqual(ring.append(line("after")), 2)
        self.assertIsInstance(self.results["column"], compose.Element)
        self.assertIsInstance(feed.feed(ring, windowed(4)), compose.Element)


if __name__ == "__main__":
    unittest.main()
