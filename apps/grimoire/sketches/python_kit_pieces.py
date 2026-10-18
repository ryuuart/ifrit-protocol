"""Nine pieces of the sketch kit on one sheet, each set from keywords alone:
a plot with its layers, a key, meters and a dial, a ticker, a scrollbar, a
console over a ring of rows, an instrument, a panel and a title card.

EDIT THESE FIRST
  WAVE — the curve the chart traces and marks.
  KEY — the legend's colours and what they stand for.
  LOG — the console's rows and the level each is set in.

TAGS: Python, Data/Charts, Typography/Interface
"""

import math

from sigil import data, motion
from sigil.compose import Element, box, column, row, text
from sigil.compose import feed as feeds
from sigil.sketch import kit, sketch

NAME = "Kit pieces"
CATEGORY = "Python"

WIDTH, HEIGHT = 1280, 900
CELL = 368
TEAL, RUST, GOLD, SLATE = "#356c69", "#bd704c", "#bd994f", "#536b9c"
KEY = (
    (TEAL, "settled", "at rest"),
    (GOLD, "running", "a motion writes it"),
    (RUST, "declared", "live, may move"),
    (SLATE, "described", "a tween to come"),
)
LOG = (
    ("door open on udp 27025", ""),
    ("revision 12 from 127.0.0.1", ""),
    ("one arrival was no cue", "warn"),
    ("schema missing: build it", "fail"),
    ("replaying the recording", ""),
)


def wave(x: float) -> float:
    """A damped swing across the chart's x domain."""
    return math.exp(-2.2 * x) * math.cos(9 * x)


def chart() -> Element:
    frame = kit.Plot(pad=10)
    frame.x.domain = data.Interval(0, 1)
    frame.y.domain = data.Interval(-1, 1)
    samples = [index / 8 for index in range(9)]
    return kit.plot(
        "wave",
        frame,
        [
            kit.rules(y=[0], pen=1),
            kit.axis(of=kit.Axis.X, at=-1, ticks=[0, 0.5, 1]),
            kit.axis(of=kit.Axis.Y, ticks=[-1, 0, 1], line=False),
            kit.area(wave, base=0),
            kit.trace(wave, pen=2),
            kit.marks(
                samples,
                lambda sample: box().width(6).height(6).borderRadius(3).fill(RUST),
                x=lambda sample: sample,
                y=wave,
            ),
            kit.label("e^-2.2x cos 9x", 0.62, 0.8),
        ],
    ).width(CELL - 24).height(170)


def meters(seconds: float) -> Element:
    level = 0.5 + 0.4 * math.sin(seconds * 1.7)
    return (
        column()
        .gap(14)
        .children(
            kit.meter(fraction=level, label="load", reading=f"{level * 100:.0f}%", width=220),
            kit.meter(fraction=0.82, label="queue", reading="82%", width=220, bar=RUST),
            kit.gauge(fraction=level, diameter=72, reading=f"{level:.2f}"),
        )
    )


def ticker(seconds: float) -> Element:
    words = row().gap(18).children(
        text("DOOR OPEN").styleClass("control"),
        text("REVISION 12").styleClass("control"),
        text("ONE ARRIVAL WAS NO CUE").styleClass("control"),
    )
    return column().gap(10).children(
        kit.ticker(words, contentWidth=330, phase=seconds * 0.25, width=CELL - 48, height=22),
        kit.timeline(
            kit.Timeline.Mark(at=0.0, label="0s"),
            kit.Timeline.Mark(at=0.5, label="cue"),
            kit.Timeline.Mark(at=1.0, label="4s"),
            width=CELL - 48,
        ),
    )


def scrolling(seconds: float) -> Element:
    lines = column().gap(6).flexGrow(1).children(
        *(text(f"row {index:02}").styleClass("control") for index in range(7))
    )
    bar = kit.scrollbar(
        scrolled=kit.Scrolled(view=150, content=600, track=150, minLength=16),
        at=0.5 + 0.5 * math.sin(seconds),
        track="#2a3038",
    ).width(12).height(150).padding(2)
    return row().gap(12).height(150).children(lines, bar)


def console() -> Element:
    ring = feeds.TextRing(16)
    for line, level in LOG:
        ring.append(feeds.TextRow(text=line, style=level))
    return kit.console(
        ring, visible=5, levels=[("warn", GOLD), ("fail", RUST)]
    ).width(CELL - 24).height(120)


def instrument() -> Element:
    picture = box().width(320).height(180).fill(TEAL).children(
        box().absolute().left(120).top(50).width(80).height(80).borderRadius(40).fill(GOLD)
    )
    readings = column().gap(4).children(
        text("revision 12").styleClass("control"),
        text("dropped 0").styleClass("control"),
    )
    return kit.instrument(
        picture,
        readings,
        pictureSize=(320, 180),
        pictureWidth=160,
        pictureLabel="PICTURE",
        readingsLabel="READINGS",
    )


def panel() -> Element:
    return kit.panel(
        text("A region of the sheet, in its own well."),
        eyebrow="REGION",
        title="Panel",
        note="eyebrow, title and note over a well",
    )


def title_card(seconds: float) -> Element:
    return kit.titleCard(
        eyebrow=kit.Line(words="PLATE 9", opacity=motion.animatable(1.0)),
        title="Kit pieces",
        subtitle="nine pieces, one sheet",
        notes=["set from keywords alone", f"at {seconds:.1f} s"],
        ruled=True,
    )


def piece(label: str, note: str, picture: Element) -> Element:
    return kit.cell(
        picture, label=label, note=note, plate=kit.Well(width=CELL, height=196, padding=12)
    )


@sketch(size=(WIDTH, HEIGHT), captureAt=1.2, theme=kit.featureTheme(kit.Density.Compact))
class KitPieces:
    def describe(self, seconds: float) -> Element:
        return kit.page(
            kit.panelGrid(
                piece("CHART", "plot, axes, rules, area, trace, marks, label", chart()),
                piece(
                    "LEGEND",
                    "the key to a picture",
                    kit.legend(
                        *(
                            kit.LegendEntry(swatch=colour, label=label, note=note)
                            for colour, label, note in KEY
                        )
                    ),
                ),
                piece("METER", "two bars and a dial", meters(seconds)),
                piece("TICKER", "a strip that runs past, over a timeline", ticker(seconds)),
                piece("SCROLLBAR", "a thumb sized from what it measures", scrolling(seconds)),
                piece("CONSOLE", "a ring of rows in one voice", console()),
                piece("INSTRUMENT", "a picture beside its readings", instrument()),
                piece("PANEL", "a region with its heading", panel()),
                piece("TITLE CARD", "eyebrow, title, subtitle, notes", title_card(seconds)),
                columns=3,
            ),
            title="Kit pieces.",
            subtitle="The sketch kit's pieces in Python, each a record of keyword properties",
            footer="sigil.sketch.kit · chart, legend, meter, ticker, scrollbar, console, instrument, panel, titleCard",
        )
