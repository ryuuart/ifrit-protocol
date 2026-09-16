"""A retained instrument sheet built with ordinary Python components.

TAGS: Geometry/Layout, Typography/Interface, Motion/Animation
"""

from math import sin

from sigil.compose import box, column, row, text
from sigil.motion import entrance
from sigil.sketch import SketchContext, sketch

INK = "#e8eef2"
MUTED = "#8da1b6"
PANEL = "#1b2938"
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
    return (
        column()
        .padding(23)
        .gap(15)
        .grow(1)
        .corners(16)
        .fill(PANEL)
        .key(label)
        .opacity(entrance(0, 1, duration=0.6, delay=index * 0.1))
        .translateY(entrance(12, 0, duration=0.6, delay=index * 0.1))
        .children(
            [
                (
                    row()
                    .gap(8)
                    .alignItems("center")
                    .children(
                        [
                            (box().width(7).height(7).corners(4).fill(accent)),
                            text(label, size=12, color=MUTED),
                        ]
                    )
                ),
                text(value, size=46, color=INK),
                (
                    box()
                    .height(3)
                    .fill("#324153")
                    .children(
                        [
                            (
                                box()
                                .width(f"{level * 100}%")
                                .height(3)
                                .fill(accent)
                                .scaleX(
                                    entrance(0.02, 1, duration=0.9, delay=index * 0.12)
                                )
                            ),
                        ]
                    )
                ),
                text(detail, size=12, color=MUTED),
            ]
        )
    )


def signal_panel():
    samples = [
        0.26 + 0.52 * sin(i * 0.15 + 0.3) ** 2 + 0.13 * sin(i * 0.8) ** 2
        for i in range(48)
    ]
    return (
        column()
        .padding(25)
        .gap(22)
        .corners(16)
        .fill(PANEL)
        .children(
            [
                (
                    row()
                    .justify("space_between")
                    .alignItems("center")
                    .children(
                        [
                            (
                                column()
                                .gap(6)
                                .children(
                                    [
                                        text("Signal envelope", size=22, color=INK),
                                        text(
                                            "A composed view of 48 observations",
                                            size=12,
                                            color=MUTED,
                                        ),
                                    ]
                                )
                            ),
                            text("NORMALIZED  /  0—1", size=11, color=MUTED),
                        ]
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
                                .grow(1)
                                .corners(3)
                                .fill("#8bd0bd" if i < 32 else "#edbb83")
                                .key(f"sample.{i}")
                                .scaleY(
                                    entrance(
                                        0.03, 1, duration=0.7, delay=0.2 + i * 0.01
                                    )
                                )
                            )
                            for i, sample in enumerate(samples)
                        ]
                    )
                ),
                (
                    row()
                    .justify("space_between")
                    .children(
                        [
                            text("00:00", size=11, color=MUTED),
                            text("00:24", size=11, color=MUTED),
                            text("00:48", size=11, color=MUTED),
                        ]
                    )
                ),
            ]
        )
    )


@sketch(size=(1100, 740), background="#111b27", capture_at=2.0)
class Dashboard:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(
            column()
            .padding(44)
            .gap(24)
            .absolute()
            .inset(0)
            .children(
                [
                    (
                        row()
                        .justify("space_between")
                        .alignItems("center")
                        .children(
                            [
                                (
                                    column()
                                    .gap(10)
                                    .children(
                                        [
                                            text(
                                                "FIELD NOTES    /    002",
                                                size=12,
                                                color=MUTED,
                                            ),
                                            text(
                                                "A quiet instrument", size=36, color=INK
                                            ),
                                        ]
                                    )
                                ),
                                (
                                    row()
                                    .padding(14, 10)
                                    .gap(8)
                                    .corners(14)
                                    .fill("#203d3b")
                                    .alignItems("center")
                                    .children(
                                        [
                                            (
                                                box()
                                                .width(6)
                                                .height(6)
                                                .corners(3)
                                                .fill("#8bd0bd")
                                            ),
                                            text("OBSERVING", size=11, color="#8bd0bd"),
                                        ]
                                    )
                                ),
                            ]
                        )
                    ),
                    (box().height(1).fill("#2b3b4c")),
                    (
                        row()
                        .gap(18)
                        .children(
                            [
                                metric(**reading, index=i)
                                for i, reading in enumerate(READINGS)
                            ]
                        )
                    ),
                    signal_panel(),
                    (
                        row()
                        .justify("space_between")
                        .children(
                            [
                                text(
                                    "Three readings. One continuous field.",
                                    size=12,
                                    color=MUTED,
                                ),
                                text("OBSERVATION 002", size=11, color=MUTED),
                            ]
                        )
                    ),
                ]
            )
        )
