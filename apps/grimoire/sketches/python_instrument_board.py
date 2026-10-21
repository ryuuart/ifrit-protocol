"""An instrument board: three readings reported at an instrument's cadence,
each drawn through a memo, beside a signal envelope whose 48 bars enter on
one stagger.

A reading is an immutable value. Twice a second the board reports all three
again; `memo` compares each with the one it drew last and describes only the
wells whose reading changed, so COHERENCE, which never changes, is drawn
once. The envelope is described once and its bars rise in order on a single
`stagger`.

EDIT THESE FIRST
  CADENCE — reports a second.
  report() — what each reading says at a report.
  ENVELOPE — the 48 samples the bars stand at.

TAGS: Geometry/Layout, Typography/Interface, Motion/Animation, Python
"""

from dataclasses import dataclass
from math import sin

from sigil import compose
from sigil.compose import box, column, document, memo, row, text
from sigil.motion import animate, ease, stagger
from sigil.sketch import Scene2DContext, kit, sketch

CADENCE = 2.0
ENVELOPE = [
    0.26 + 0.52 * sin(index * 0.15 + 0.3) ** 2 + 0.13 * sin(index * 0.8) ** 2
    for index in range(48)
]
MINT, AMBER, SKY = "#8bd0bd", "#edbb83", "#92b4e4"


@dataclass(frozen=True)
class Reading:
    label: str
    value: int
    detail: str
    accent: str


def report(count: int) -> list[Reading]:
    """The three readings at the count-th report."""
    return [
        Reading("RESONANCE", 72 + round(8 * sin(count * 0.5)), "Within the expected range", AMBER),
        Reading("COHERENCE", 94, "Stable across all channels", MINT),
        Reading("AMPLITUDE", 40 + round(14 * sin(count * 0.3)), "A gentle rise over baseline", SKY),
    ]


def instrument(reading: Reading) -> compose.Element:
    """One reading's well. The board enters once: the wells fade and rise on
    a stagger the first time they mount, and keep their place after."""
    arrive = stagger(0.1)
    return (
        kit.well(padding=23, corners=6)
        .column()
        .gap(15)
        .flexGrow(1)
        .flexBasis(0)
        .opacity(animate(from_=0, to=1, duration=0.6, delay=arrive))
        .translateY(animate(from_=12, to=0, duration=0.6, delay=arrive, ease=ease.outCubic))
        .children(
            row(box().width(7).height(7).borderRadius(4).fill(reading.accent), document.label(reading.label))
            .gap(8)
            .alignItems("center"),
            row(text(f"{reading.value:02d}").styleClass("readout").fontSize(46), text("/ 100").styleClass("control"))
            .gap(12)
            .alignItems("baseline"),
            kit.meter(fraction=reading.value / 100, bar=reading.accent, width="100%", height=4),
            document.caption(reading.detail),
        )
    )


def envelope() -> compose.Element:
    rise = stagger(0.01, start=0.2)
    bars = [
        box()
        .height(124 * sample + 8)
        .flexGrow(1)
        .borderRadius(3)
        .fill(MINT if index < 32 else AMBER)
        .transformOrigin("50%", "100%")
        .scaleY(animate(from_=0.03, to=1, duration=0.7, delay=rise, ease=ease.outCubic))
        for index, sample in enumerate(ENVELOPE)
    ]
    return (
        kit.well(padding=25, corners=6)
        .column()
        .gap(22)
        .children(
            row(
                column(document.h2("Signal envelope"), document.caption("A composed view of 48 observations")).gap(6),
                text("NORMALIZED  /  0—1").styleClass("control"),
            )
            .justifyContent("space_between")
            .alignItems("center"),
            row(*bars).height(136).gap(7).alignItems("end"),
            row(text("00:00").styleClass("control"), text("00:24").styleClass("control"), text("00:48").styleClass("control"))
            .justifyContent("space_between"),
        )
    )


@sketch(size=(1100, 680), captureAt=2.4, theme=kit.featureTheme(kit.Density.Spacious))
class InstrumentBoard:
    def setup(self, ctx: Scene2DContext) -> None:
        self.composer = ctx.composer
        self.count = 0
        ctx.render(
            kit.page(
                column(compose.slot("readings"), envelope()).gap(24),
                title="A quiet instrument",
                subtitle="Three readings reported twice a second through memo / a signal envelope entering on one stagger",
                footer="memo · keyed wells · kit.meter · stagger",
            )
        )
        # The slot exists once the page is rendered: the first report fills
        # it now, and the timer reports again at the instrument's cadence.
        self.report()
        ctx.engine.timer(self.report, stepRate=CADENCE)

    def report(self) -> None:
        """Describe the readings again; memo keeps every well whose reading is
        the one it drew."""
        self.composer.renderSlot(
            "readings",
            row(*(memo(reading, instrument).key(reading.label) for reading in report(self.count))).gap(18),
        )
        self.count += 1
