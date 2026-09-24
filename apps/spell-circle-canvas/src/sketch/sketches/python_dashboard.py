"""A retained instrument sheet built with ordinary Python components.

TAGS: Geometry/Layout, Typography/Interface, Motion/Animation
"""

from math import sin

from sigil.compose import box, column, row, text
from sigil.compose import document as doc
from sigil.motion import entrance
from sigil.sketch import SketchContext, kit, sketch

READINGS = [
    dict(
        label="RESONANCE",
        value="0.86",
        detail="Within the expected range",
        accent="#edbb83",
        level=0.86,
    ),
    dict(
        label="COHERENCE",
        value="94.2%",
        detail="Stable across all channels",
        accent="#8bd0bd",
        level=0.942,
    ),
    dict(
        label="AMPLITUDE",
        value="12.8",
        detail="A gentle rise over baseline",
        accent="#92b4e4",
        level=0.64,
    ),
]


def metric(label, value, detail, accent, level, index):
    look = kit.theme()
    return (
        kit.well(padding=23, corners=6)
        .column()
        .gap(15)
        .flexGrow(1)
        .key(label)
        .opacity(entrance(0, 1, duration=0.6, delay=index * 0.1))
        .translateY(entrance(12, 0, duration=0.6, delay=index * 0.1))
        .children(
            (
                row()
                .gap(8)
                .alignItems("center")
                .children(
                    (box().width(7).height(7).borderRadius(4).fill(accent)),
                    doc.label(label),
                )
            ),
            text(value).styleClass("readout").fontSize(46),
            (
                box()
                .height(3)
                .fill(look.palette.rule)
                .children(
                    (
                        box()
                        .width(f"{level * 100}%")
                        .height(3)
                        .fill(accent)
                        .scaleX(entrance(0.02, 1, duration=0.9, delay=index * 0.12))
                    ),
                )
            ),
            doc.caption(detail),
        )
    )


def signal_panel():
    samples = [
        0.26 + 0.52 * sin(i * 0.15 + 0.3) ** 2 + 0.13 * sin(i * 0.8) ** 2
        for i in range(48)
    ]
    return (
        kit.well(padding=25, corners=6)
        .column()
        .gap(22)
        .children(
            (
                row()
                .justifyContent("space_between")
                .alignItems("center")
                .children(
                    (
                        column()
                        .gap(6)
                        .children(
                            doc.h2("Signal envelope"),
                            doc.caption("A composed view of 48 observations"),
                        )
                    ),
                    text("NORMALIZED  /  0—1").styleClass("control"),
                )
            ),
            (
                row()
                .height(136)
                .gap(7)
                .alignItems("end")
                .children(
                    [
                        (
                            box()
                            .height(124 * sample + 8)
                            .flexGrow(1)
                            .borderRadius(3)
                            .fill("#8bd0bd" if i < 32 else "#edbb83")
                            .key(f"sample.{i}")
                            .scaleY(
                                entrance(0.03, 1, duration=0.7, delay=0.2 + i * 0.01)
                            )
                        )
                        for i, sample in enumerate(samples)
                    ]
                )
            ),
            (
                row()
                .justifyContent("space_between")
                .children(
                    text("00:00").styleClass("control"),
                    text("00:24").styleClass("control"),
                    text("00:48").styleClass("control"),
                )
            ),
        )
    )


@sketch(size=(1100, 660), capture_at=2.0)
class Dashboard:
    def setup(self, ctx: SketchContext) -> None:
        with kit.provide(kit.feature_theme(kit.Density.Spacious)):
            kit.stage(ctx, size=(1100, 660), capture_at=2.0)
            ctx.render(
                kit.page(
                    column(
                        row(
                            metric(**reading, index=i)
                            for i, reading in enumerate(READINGS)
                        ).gap(18),
                        signal_panel(),
                    ).gap(24),
                    title="A quiet instrument",
                    subtitle="Retained components / three readings and a continuous signal envelope",
                    footer="Observation 002 · ordinary Python descriptions · native entrance motion",
                )
            )
