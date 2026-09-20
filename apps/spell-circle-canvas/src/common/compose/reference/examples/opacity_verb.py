"""opacity — the node and everything under it faded as ONE group, which
is why the overlapping children inside a faded card do not show through
each other.

The Python twin of opacity_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
PLATE = "#6fb3a6"
DISC = "#27343d"
ASH = "#8ea0ad"


def card(value: float, caption: str) -> compose.Element:
    """A card with an overlapping child, at one opacity."""
    return (
        compose.box(
            compose.stack(
                compose.box().cover().borderRadius(10).fill(PLATE),
                compose.box()
                .width(64)
                .height(64)
                .borderRadius(32)
                .fill(DISC)
                .left(20)
                .top(28),
            )
            .width(compose.pct(100))
            .height(120)
            .opacity(value),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .basis(0)
        .grow(1)
        .alignItems("center")
    )


@sketch(size=(620, 240), background=GROUND, capture_at=0)
class OpacityVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                card(1.0, "opacity(1)"),
                card(0.55, "opacity(0.55)"),
                card(0.2, "opacity(0.2)"),
            )
            .row()
            .gap(18)
            .padding(24)
        )
