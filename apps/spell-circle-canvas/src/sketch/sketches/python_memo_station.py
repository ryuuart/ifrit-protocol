"""A retained station board whose immutable readings control native memo nodes.

TAGS: Typography/Interface, Motion/Animation, Drawing/Generative
"""

from dataclasses import dataclass
from math import sin

from sigil.compose import (
    Overflow,
    StyleSheet,
    box,
    memo,
    pct,
    row,
    rule,
    text,
)
from sigil.compose import document as doc
from sigil.motion import Transition, animate, ease, from_
from sigil.sketch import SketchContext, kit, sketch
from sigil.weave import Type


@dataclass(frozen=True)
class Reading:
    label: str
    value: int
    accent: str


def instrument(reading):
    look = kit.theme()
    marks = (
        row()
        .gap(3)
        .alignItems("end")
        .children(
            [
                (box().width(3).height(5 if index % 5 else 10).fill(look.palette.rule))
                for index in range(36)
            ]
        )
    )
    return (
        kit.well(padding=30, corners=6, keyline=look.palette.rule)
        .column()
        .width(272)
        .gap(20)
        .opacity(animate(from_(0).to(1), Transition(0.4, ease.outQuad)))
        .children(
            doc.label(reading.label),
            (
                row()
                .gap(12)
                .alignItems("baseline")
                .children(
                    (text(f"{reading.value:02d}").styleClass("readout")),
                    (text("/ 100").styleClass("control")),
                )
            ),
            (
                box()
                .width(212)
                .height(6)
                .fill(look.palette.rule)
                .borderRadius(3)
                .overflow(Overflow.Clip)
                .children(
                    (box().width(pct(reading.value)).height(6).fill(reading.accent)),
                )
            ),
            marks,
            doc.label("SIGNAL LOCKED").ink(reading.accent),
        )
    )


@sketch(size=(960, 480), capture_at=2.4)
class MemoStation:
    def setup(self, ctx: SketchContext) -> None:
        self.last = None
        self.look = kit.feature_theme(kit.Density.Spacious)
        with kit.provide(self.look):
            kit.stage(ctx, size=(960, 480), capture_at=2.4)
        self.sheet = StyleSheet(
            [
                rule(".readout").font(Type(size=66, track=-2)),
            ]
        )

    def update(self, elapsed: float, ctx: SketchContext) -> None:
        # The input model changes at an instrument's reporting cadence.
        tick = int(elapsed * 2)
        if tick == self.last:
            return
        self.last = tick
        values = (
            Reading("01 / RESONANCE", 72 + round(8 * sin(tick * 0.5)), "#bbe991"),
            Reading("02 / COHERENCE", 94, "#79d9b1"),
            Reading("03 / AMPLITUDE", 40 + round(14 * sin(tick * 0.3)), "#e6bd7b"),
        )
        with kit.provide(self.look):
            ctx.render(
                kit.page(
                    row(memo(value, instrument).key(value.label) for value in values)
                    .gap(24)
                    .applyStyleSheet(self.sheet),
                    title="A quiet signal, held in place",
                    subtitle="Station 04 / immutable readings govern retained memo descriptions",
                    footer="Native composition · Python models · sensor array",
                )
            )
