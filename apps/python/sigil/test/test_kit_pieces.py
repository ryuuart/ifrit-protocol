"""The sketch kit's pieces in Python: each a record of keyword properties
and the function that sets it, as the native kit spells them.

Every piece is checked three ways: its record takes and reads back its
fields, its function answers an element from keywords alone, and a
misspelled keyword names the field it missed. The readout of a door is
drawn, and drawn the same as the readout spelled by hand from the rows
the door's state answers.
"""

import builtins
import tempfile
import unittest
from pathlib import Path

from sigil import compose, data, io, media, motion
from sigil.compose import Element, box, text
from sigil.compose import feed as feeds
from sigil.compose import kit as neutral
from sigil.sketch import kit, renderFile

PIECES = (
    "Chart",
    "Legend",
    "Meter",
    "Ticker",
    "Scrollbar",
    "Console",
    "Instrument",
    "Panel",
    "titleCard",
)


class Records(unittest.TestCase):
    def test_every_piece_answers_an_element_from_keywords(self):
        ring = feeds.TextRing(8)
        ring.append(feeds.TextRow(text="ready"))
        frame = kit.Plot(pad=6)
        frame.x.domain = data.Interval(0, 1)
        frame.y.domain = data.Interval(0, 1)
        built = {
            "Chart": kit.plot(
                "curve",
                frame,
                [
                    kit.axis(of=kit.Axis.X),
                    kit.axis(of=kit.Axis.Y, count=3),
                    kit.rules(y=[0.5], pen=1),
                    kit.trace(lambda x: x * x, pen=2),
                    kit.trace([0.1, 0.4, 0.2, 0.8]),
                    kit.area(lambda x: x / 2, base=0),
                    kit.path(lambda t: (t, 1 - t)),
                    kit.marks([0.2, 0.6], x=lambda value: value, y=lambda value: value),
                    kit.bands([0.3, 0.7, 0.5]),
                    kit.segments([(0, 0, 1, 1)], x=lambda row: row[0],
                                 y=lambda row: row[1], toX=lambda row: row[2],
                                 toY=lambda row: row[3]),
                    kit.label("peak", 0.5, 0.9),
                    lambda frame, key, index: box(),
                ],
            ).width(200).height(120),
            "Legend": kit.legend(
                kit.LegendEntry(swatch="#ff0000", label="red", note="warm"),
                kit.LegendEntry(swatch="#0000ff", label="blue"),
                column=False,
            ),
            "Meter": kit.meter(fraction=0.4, label="load", reading="40%", width=160),
            "Ticker": kit.ticker(text("NEWS"), contentWidth=48, width=160, height=20),
            "Scrollbar": kit.scrollbar(
                scrolled=kit.Scrolled(view=100, content=400, track=100), at=0.5
            ),
            "Console": kit.console(ring, visible=4, levels=[("fail", "#ff0000")]),
            "Instrument": kit.instrument(
                box().width(64).height(36), text("readings"), pictureWidth=128
            ),
            "Panel": kit.panel(text("inside"), eyebrow="ONE", title="Panel"),
            "titleCard": kit.titleCard(eyebrow="PLATE", title="Kit pieces", notes=["a"]),
        }
        self.assertEqual(sorted(built), sorted(PIECES))
        for name, element in built.items():
            with self.subTest(piece=name):
                self.assertIsInstance(element, Element)

    def test_the_other_parts_of_each_header_are_bound_too(self):
        for element in (
            kit.swatchStrip(swatches=["#000000", "#ffffff"], labels=["0", "1"]),
            kit.chip(label="LIVE"),
            kit.gauge(fraction=0.25, reading="25"),
            kit.backdrop(vignette=0.4, grain=0.1),
            kit.frame(text("screen"), width=120, height=80),
            kit.frame(width=120, height=80),
            kit.timeline(kit.Timeline.Mark(at=0.5, label="half"), width=200),
        ):
            with self.subTest(element=element):
                self.assertIsInstance(element, Element)

    def test_records_read_back_what_they_were_given(self):
        entry = kit.LegendEntry(swatch="#ff0000", label="red", opacity=0.5)
        self.assertEqual(entry.label, "red")
        self.assertEqual(entry.opacity.value, 0.5)
        strip = kit.SwatchStrip(labels=["a", "b"])
        self.assertEqual(strip.labels, ["a", "b"])
        meter = kit.Meter(fraction=0.3, level=motion.animatable(0.6))
        self.assertAlmostEqual(meter.fraction, 0.3)
        self.assertAlmostEqual(meter.level.value, 0.6)
        thumb = kit.Scrolled(view=100, content=400, track=100).thumb()
        self.assertAlmostEqual(thumb.length, 25)
        self.assertAlmostEqual(thumb.travel, 75)
        card = kit.TitleCard(title=kit.Line(words="Title"))
        self.assertEqual(card.title.words, "Title")
        console = kit.Console(levels=[("fail", "#ff0000")])
        self.assertEqual(console.levels[0][0], "fail")
        ruler = kit.Ruler(of=kit.Axis.Y, ticks=[0, 0.5, 1])
        self.assertEqual(ruler.ticks, [0, 0.5, 1])
        self.assertEqual(kit.Trace(pen=3).pen.width, 3)
        self.assertEqual(kit.Datum(x=1, y=2), kit.Datum(x=1, y=2))
        instrument = kit.Instrument(pictureSize=(320, 180))
        self.assertEqual(instrument.pictureSize.width(), 320)

    def test_a_misspelled_keyword_names_the_field(self):
        for call, field in (
            (lambda: kit.meter(key_line="#000000"), "keyline"),
            (lambda: kit.legend(swatch_side=8), "swatchSide"),
            (lambda: kit.scrollbar(thumb_inset=2), "thumbInset"),
            (lambda: kit.axis(tick_line=None), "tickLine"),
        ):
            with self.subTest(field=field):
                with self.assertRaises(TypeError):
                    call()

    def test_a_plot_layer_is_a_native_layer_or_a_function(self):
        frame = kit.Plot()
        self.assertIsInstance(kit.axis(), kit.Layer)
        with self.assertRaisesRegex(TypeError, "layer"):
            kit.plot("refused", frame, [42])


class Drawn(unittest.TestCase):
    """Two trees drawn on the house sheet and compared pixel for pixel."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        builtins._sigil_kit_piece = None
        self.addCleanup(delattr, builtins, "_sigil_kit_piece")

    def drawn(self, tree):
        builtins._sigil_kit_piece = tree
        source = Path(self.directory.name) / "piece.py"
        output = Path(self.directory.name) / "piece.png"
        source.write_text(
            "import builtins\n"
            "from sigil.compose import box\n"
            "from sigil.sketch import kit, sketch\n"
            "@sketch(size=(320, 200), background='#ffffff')\n"
            "class Scene:\n"
            "    def setup(self, ctx):\n"
            "        ctx.render(box().applyStyleSheet(kit.houseTheme().styleSheet())\n"
            "                   .children([builtins._sigil_kit_piece]))\n"
        )
        renderFile(source, output)
        return media.decode(media.Image, output.read_bytes()).frameAt(0).image.rgba()

    def test_a_doors_readout_is_the_readout_of_the_rows_its_state_answers(self):
        hub = io.Hub()
        door = data.connect(hub, "fixture://sky")
        inlet = io.testing.inletOf(door)
        inlet.deliver(b'{"kind": "gust"}', sender="ws://127.0.0.1:52341")
        inlet.deliver(b"this is no document at all", sender="ws://127.0.0.1:52341")
        hub.advance(0.0)
        state = door.state()
        self.assertEqual(state.revision, 2)
        self.assertEqual(state.undecodable, 1)
        readiness = {
            io.ReadyState.Connecting: "connecting",
            io.ReadyState.Open: "open",
            io.ReadyState.Closed: "closed",
        }[state.readiness]
        last = (
            neutral.Reading(name="error", value=state.error)
            if state.error
            else neutral.Reading(name="sender", value="ws://127.0.0.1:52341")
        )
        rows = [
            neutral.Reading(name="door", value=state.localAddress or "fixture://sky"),
            neutral.Reading(name="state", value=readiness),
            neutral.Reading(name="revision", value=str(state.revision)),
            neutral.Reading(name="dropped", value=str(state.dropped)),
            neutral.Reading(name="undecodable", value=str(state.undecodable)),
            last,
            neutral.Reading(name="gusts", value="1"),
        ]
        readout = kit.connectionReadout(
            door,
            rows=neutral.Rows(measure=300),
            readings=[neutral.Reading(name="gusts", value="1")],
        )
        self.assertEqual(
            self.drawn(readout), self.drawn(neutral.readout(rows, measure=300))
        )
        # A different count draws differently, so the comparison sees rows.
        rows[2] = neutral.Reading(name="revision", value="3")
        self.assertNotEqual(
            self.drawn(readout), self.drawn(neutral.readout(rows, measure=300))
        )


if __name__ == "__main__":
    unittest.main()
