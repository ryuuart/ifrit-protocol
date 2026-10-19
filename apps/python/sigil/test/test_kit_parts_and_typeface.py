"""The kit parts and the typeface door answer Python in the shape of their
C++ twins.

A ruler's tick line is a Python callable offered the value and the ruler;
a channel follows one number of a connection as a live value; the
specimen sheet is a theme; the compose kit's table and bars take their
records as keywords with their parts as callables; and `weave.typeface`
answers the face class every other font door answers.
"""

import builtins
import tempfile
import unittest
from pathlib import Path

from sigil import data, io, media, motion, weave
from sigil.compose import Element, text
from sigil.compose import kit as neutral
from sigil.sketch import kit, renderFile


def drawn(tree):
    """The tree drawn on a white page, as RGBA bytes."""
    with tempfile.TemporaryDirectory() as directory:
        builtins._sigil_kit_part = tree
        try:
            source = Path(directory) / "part.py"
            output = Path(directory) / "part.png"
            source.write_text(
                "import builtins\n"
                "from sigil.compose import box\n"
                "from sigil.sketch import kit, sketch\n"
                "@sketch(size=(320, 200), background='#ffffff')\n"
                "class Scene:\n"
                "    def setup(self, ctx):\n"
                "        ctx.render(box().applyStyleSheet(kit.houseTheme().styleSheet())\n"
                "                   .children([builtins._sigil_kit_part]))\n"
            )
            renderFile(source, output)
            return media.decode(media.Image, output.read_bytes()).frameAt(0).image.rgba()
        finally:
            del builtins._sigil_kit_part


class TickLine(unittest.TestCase):
    def test_a_rulers_tick_line_is_a_python_callable_offered_the_value(self):
        asked = []

        def tick(value, ruler):
            asked.append((value, ruler.of))
            return text(f"{value:.1f}")

        ruler = kit.Ruler(of=kit.Axis.Y, ticks=[0, 0.5, 1], tickLine=tick)
        self.assertIsInstance(ruler.tickLine(0.25, ruler), Element)
        self.assertEqual(asked, [(0.25, kit.Axis.Y)])
        asked.clear()

        frame = kit.Plot(pad=6)
        frame.x.domain = data.Interval(0, 1)
        frame.y.domain = data.Interval(0, 1)
        plotted = kit.plot("ticks", frame, [kit.axis(ruler)]).width(200).height(120)
        drawn(plotted)
        self.assertEqual(sorted(value for value, _ in asked), [0, 0.5, 1])

        ruler.tickLine = None
        self.assertIsNone(ruler.tickLine)
        with self.assertRaises(TypeError):
            ruler.tickLine = 3


class Channel(unittest.TestCase):
    def test_a_channel_follows_one_number_of_its_connection(self):
        hub = io.Hub()
        door = data.connect(hub, "fixture://desk")
        wind = kit.Channel(hub, door, "Wind", "value")
        self.assertEqual(wind.name(), "Wind")
        self.assertEqual(wind.value(), 0)
        self.assertIsNone(wind.lastRead())
        bound = wind.live()

        inlet = io.testing.inletOf(door)
        inlet.deliver(b'{"kind": "Wind", "value": 0.75}')
        hub.advance(0.0)
        self.assertAlmostEqual(wind.value(), 0.75)
        self.assertAlmostEqual(wind.lastRead(), 0.75)
        # The live value read before the message moves with it.
        self.assertAlmostEqual(bound.value, 0.75)
        self.assertIsInstance(bound, motion.Animatable)

        # A message under another name leaves it standing.
        inlet.deliver(b'{"kind": "Rain", "value": 0.1}')
        hub.advance(0.0)
        self.assertAlmostEqual(wind.value(), 0.75)

        self.assertEqual(kit.Channel().value(), 0)
        with self.assertRaises(TypeError):
            kit.Channel(hub, door, "Wind", True)


class SpecimenTheme(unittest.TestCase):
    def test_the_specimen_sheet_is_a_theme(self):
        theme = kit.specimenTheme()
        self.assertIsInstance(theme, kit.Theme)
        self.assertIsNotNone(theme.styleSheet())


class Table(unittest.TestCase):
    def test_a_table_takes_its_record_as_keywords_and_its_cells_as_a_part(self):
        offered = []

        def cell(words, table, column, row, cells):
            offered.append((words, column, row, cells, len(table.columns)))
            return text(words)

        columns = [neutral.Column(head="KEY", width=60), neutral.Column(head="COST", figure=True)]
        rows = [["alpha", "1"], ["beta", "2"]]
        built = neutral.table(rows, columns=columns, keys=["a", "b"], cellLine=cell)
        self.assertIsInstance(built, Element)
        drawn(built)
        self.assertIn(("beta", 0, 1, ["beta", "2"], 2), offered)
        self.assertIn(("1", 1, 0, ["alpha", "1"], 2), offered)

        # One row-major run of words is the rectangular table.
        self.assertEqual(
            drawn(neutral.table(["alpha", "1", "beta", "2"], columns=columns)),
            drawn(neutral.table(rows, columns=columns)),
        )

        how = neutral.Table(keys=["a"], swatches=["#ff0000"], headRuled=True)
        copied = how.copy()
        self.assertEqual((copied.keys, copied.headRuled), (["a"], True))
        self.assertIsNone(neutral.Table().cellLine)
        self.assertIsInstance(neutral.Table().headLine("HEAD", how), Element)
        with self.assertRaises(TypeError):
            neutral.table(rows, colums=columns)


class Bars(unittest.TestCase):
    def test_bars_take_their_record_as_keywords_and_the_figure_as_a_part(self):
        figures = []

        def figure(value):
            figures.append(value)
            return text(f"{value:.0f}")

        built = neutral.bars(
            ["north", "south"], [3, 9], length=120, inks=["#ff0000"], figureLine=figure
        )
        self.assertIsInstance(built, Element)
        drawn(built)
        self.assertEqual(sorted(figures), [3, 9])
        how = neutral.Bars(inks=["#00ff00", "#0000ff"])
        self.assertEqual(len(how.copy().inks), 2)
        self.assertNotEqual(
            drawn(neutral.bars(["north"], [3], inks=["#ff0000"])),
            drawn(neutral.bars(["north"], [3])),
        )


class Typeface(unittest.TestCase):
    def test_the_typeface_door_answers_a_face(self):
        face = weave.typeface("Helvetica Neue", weight=700)
        self.assertIsInstance(face, weave.Face)
        self.assertEqual(face.familyName(), "Helvetica Neue")
        self.assertGreaterEqual(face.style().weight, 600)
        italic = weave.typeface("Helvetica Neue", italic=True)
        self.assertEqual(italic.style().slant, weave.FaceSlant.Italic)


if __name__ == "__main__":
    unittest.main()
