"""A retained signal desk driven by one shared native clock value.

The tracks are native bindings of one live value. The entrance is a native
keyframe path; Python assembles the components once and writes the source from
a timer on the scene engine.
TAGS: Motion/Animation, Drawing/Generative, Typography/Interface
"""

from sigil.compose import box, column, text
from sigil.compose import document as doc
from sigil.motion import Keyframe, Wiggle, animatable, animate, bind, ease, envelope
from sigil.sketch import SketchContext, kit, sketch

WIDTH = 860
TRACK = 610


def signal_row(number, title, detail, signal, accent):
    look = kit.theme()
    marks = [
        (
            box()
            .width(1)
            .height(22 if index % 2 == 0 else 12)
            .fill(look.palette.rule)
            .absolute()
            .left(index * TRACK / 10)
            .top(11)
        )
        for index in range(11)
    ]
    track = (
        box()
        .width(TRACK)
        .height(48)
        .children(
            *marks,
            (
                box()
                .width(TRACK)
                .height(1)
                .fill(look.palette.rule)
                .absolute()
                .left(0)
                .top(22)
            ),
            (
                box()
                .width(30)
                .height(30)
                .fill(accent)
                .borderRadius(8)
                .absolute()
                .left(0)
                .top(7)
                .translateX(bind(signal, to=(0, TRACK - 30)))
                .rotate(bind(signal, to=(-20, 20)))
            ),
        )
    )
    return (
        kit.well(padding=16, paddingY=20, corners=6)
        .row()
        .gap(18)
        .alignItems("center")
        .width(WIDTH)
        .children(
            (
                column()
                .gap(5)
                .width(168)
                .children(
                    text(f"0{number}").styleClass("control").ink(accent),
                    doc.label(title),
                    doc.caption(detail),
                )
            ),
            track,
        )
    )


@sketch(size=(960, 800), capture_at=1.1)
class MotionSignals:
    def setup(self, ctx: SketchContext) -> None:
        seconds = animatable(0)
        ctx.engine.timer(lambda delta, elapsed: seconds.set(elapsed))
        folded = bind(seconds, from_=(0, 5), wrap=1)
        cycle = (0, 5)
        with kit.provide(kit.feature_theme(kit.Density.Spacious)):
            kit.stage(ctx, size=(960, 800), capture_at=1.1)
            rows = [
                signal_row(
                    1,
                    "TRAVERSE",
                    "source → triangle",
                    bind(seconds, from_=cycle, alternate=True),
                    "#b7dd93",
                ),
                signal_row(
                    2,
                    "BREATHE",
                    "source → cosine",
                    bind(seconds, from_=cycle, envelope=envelope.cosine()),
                    "#77d6bc",
                ),
                signal_row(
                    3,
                    "POSTERIZE",
                    "triangle → 8 levels",
                    bind(seconds, from_=cycle, alternate=True, quantize=8),
                    "#85c5e4",
                ),
                signal_row(
                    4,
                    "OVERSHOOT",
                    "triangle → outBack",
                    bind(seconds, from_=cycle, alternate=True, ease=ease.outBack()),
                    "#baa2e0",
                ),
                signal_row(
                    5,
                    "GATE",
                    "fold → rise / hold / fall",
                    bind(folded, envelope=envelope.trapezoid(0, 0.2, 0.7, 1)),
                    "#eab881",
                ),
                signal_row(
                    6,
                    "DRIFT",
                    "triangle + seeded wiggle",
                    bind(
                        seconds,
                        from_=cycle,
                        alternate=True,
                        wiggle=Wiggle(amount=0.08, frequency=4, seed=23),
                    ),
                    "#e78e87",
                ),
            ]
            entrance = animate(
                from_=32,
                keyframes=[Keyframe(-5, duration=0.45), Keyframe(0, duration=0.3)],
                ease=ease.outCubic,
            )
            ctx.render(
                kit.page(
                    column(*rows)
                    .gap(9)
                    .width(WIDTH)
                    .translateY(entrance)
                    .opacity(animate(from_=0, to=1, duration=0.5, ease=ease.outQuad)),
                    title="One clock. Six interpretations.",
                    subtitle="One live value / native bindings / keyframe entrance",
                    footer="animatable → bind → property · period 5.0 s · retained components",
                )
            )
