"""A retained station board whose immutable readings control native memo nodes.

TAGS: Typography/Interface, Motion/Animation, Drawing/Generative
"""

from dataclasses import dataclass
from math import sin

from sigil.compose import box, column, memo, pct, row, stroke, text
from sigil.motion import Transition, animate, ease, from_
from sigil.sketch import SketchContext, sketch
from sigil.weave import StyleSheet, Type, rule


@dataclass(frozen=True)
class Reading:
    label: str
    value: int
    accent: str


def instrument(reading):
    marks = (
        row()
        .gap(3)
        .alignItems("end")
        .children(
            [
                (box().width(3).height(5 if index % 5 else 10).fill("#425751"))
                for index in range(36)
            ]
        )
    )
    return (
        column()
        .width(272)
        .gap(20)
        .padding(30)
        .fill("#10241e")
        .corners(15)
        .opacity(animate(from_(0).to(1), Transition(0.4, ease.outQuad)))
        .children(
            (text(reading.label).styleClass("label")),
            (
                row()
                .gap(12)
                .alignItems("baseline")
                .children(
                    (text(f"{reading.value:02d}").styleClass("reading")),
                    (text("/ 100").styleClass("unit")),
                )
            ),
            (
                box()
                .width(212)
                .height(6)
                .fill("#253e36")
                .corners(3)
                .clip(True)
                .children(
                    (box().width(pct(reading.value)).height(6).fill(reading.accent)),
                )
            ),
            marks,
            (text("SIGNAL LOCKED").styleClass("label").ink(reading.accent)),
        )
    ).stroke(stroke(1, "#2b463d"))


@sketch(size=(960, 580), background="#071811", capture_at=2.4)
class MemoStation:
    def setup(self, ctx: SketchContext) -> None:
        self.last = None
        self.sheet = StyleSheet(
            [
                rule("title").font(Type(size=42, weight=600)),
                rule("label").font(Type(size=11, track=1.3, color="#789f8e")),
                rule("reading").font(Type(size=66, track=-2)),
                rule("unit").font(Type(size=13, color="#789f8e")),
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
        ctx.render(
            column()
            .gap(28)
            .width(864)
            .absolute()
            .left(48)
            .top(46)
            .ink("#e0ede5")
            .styleSheet(self.sheet)
            .children(
                (
                    row()
                    .width(864)
                    .justify("space_between")
                    .children(
                        (text("STATION / 04").styleClass("label")),
                        (text("LIVE READINGS").styleClass("label")),
                    )
                ),
                (text("A quiet signal, held in place.").styleClass("title")),
                (
                    row()
                    .gap(24)
                    .children(
                        [(memo(value, instrument).key(value.label)) for value in values]
                    )
                ),
                (
                    row()
                    .width(864)
                    .justify("space_between")
                    .children(
                        (text("SENSOR ARRAY").styleClass("label")),
                        (
                            text("NATIVE COMPOSITION / PYTHON MODELS").styleClass(
                                "label"
                            )
                        ),
                    )
                ),
            )
        )
