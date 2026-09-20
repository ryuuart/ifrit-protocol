"""stroke — the node's boundary dressed by a brush: the whole of it, two
rings stacked on it, and one run of it claimed by a span.

The Python twin of stroke_verb.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
PLATE = "#1c232b"
ASH = "#8ea0ad"
EDGE = "#6fb3a6"
HALO = "#2f5f6a"


def cell(caption: str, plate: compose.Element) -> compose.Element:
    return (
        compose.box(
            plate.width(compose.pct(100)).height(120).borderRadius(10).fill(PLATE),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .flexBasis(0)
        .flexGrow(1)
        .alignItems("center")
    )


@sketch(size=(620, 240), background=GROUND, capture_at=0)
class StrokeVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                cell("stroke(brush)", compose.box().stroke(compose.stroke(2, EDGE))),
                # Repeated calls append: two calls are two rings.
                cell(
                    "two calls, two rings",
                    compose.box()
                    .stroke(compose.stroke(6, HALO))
                    .stroke(compose.stroke(2, EDGE)),
                ),
                # A span-qualified pass claims the run it resolves to.
                # The plate's corners are rounded, and a fillet turns by
                # less at each step than the 30 degrees a break defaults
                # to, so the angle comes down to what this silhouette
                # actually turns.
                cell(
                    "stroke(where, what)",
                    compose.box().stroke(
                        compose.spans.corners(22, 7), compose.stroke(2, EDGE)
                    ),
                ),
            )
            .row()
            .gap(18)
            .padding(24)
        )
