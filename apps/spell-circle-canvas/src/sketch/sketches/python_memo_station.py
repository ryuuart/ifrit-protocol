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
    marks = row(
        (
            box(width=3, height=5 if index % 5 else 10, fill="#425751")
            for index in range(36)
        ),
        gap=3,
        align_items="end",
    )
    return column(
        text(reading.label, style_class="label"),
        row(
            text(f"{reading.value:02d}", style_class="reading"),
            text("/ 100", style_class="unit"),
            gap=12,
            align_items="baseline",
        ),
        box(
            box(width=pct(reading.value), height=6, fill=reading.accent),
            width=212,
            height=6,
            fill="#253e36",
            corners=3,
            clip=True,
        ),
        marks,
        text("SIGNAL LOCKED", style_class="label", ink=reading.accent),
        width=272,
        gap=20,
        padding=30,
        fill="#10241e",
        corners=15,
        opacity=animate(from_(0).to(1), Transition(0.4, ease.outQuad)),
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
            column(
                row(
                    text("STATION / 04", style_class="label"),
                    text("LIVE READINGS", style_class="label"),
                    width=864,
                    justify="space_between",
                ),
                text("A quiet signal, held in place.", style_class="title"),
                row(
                    (memo(value, instrument, key=value.label) for value in values),
                    gap=24,
                ),
                row(
                    text("SENSOR ARRAY", style_class="label"),
                    text("NATIVE COMPOSITION / PYTHON MODELS", style_class="label"),
                    width=864,
                    justify="space_between",
                ),
                gap=28,
                width=864,
                absolute=True,
                left=48,
                top=46,
                ink="#e0ede5",
                style_sheet=self.sheet,
            )
        )
