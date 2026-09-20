"""box — the flex container, and the same factory as a leaf.

The Python twin of box_element.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
CARD = "#1e252c"
INK = "#dde6ec"
ASH = "#8ea0ad"
ACCENT = "#5fb0a4"


def card(title: str, body: str) -> compose.Element:
    """A card: a column of boxes, one of which is a bare leaf standing in
    as a rule."""
    return (
        compose.box(
            compose.text(title, size=17, color=INK),
            compose.box().height(2).width(36).fill(ACCENT),
            compose.text(body, size=13, color=ASH),
        )
        .column()
        .gap(10)
        .padding(18)
        .flexBasis(0)
        .flexGrow(1)
        .borderRadius(10)
        .fill(CARD)
    )


@sketch(size=(640, 260), background=GROUND, capture_at=0)
class BoxElement:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                card("Describe", "A box with no children is a leaf."),
                card("Contain", "A box with children is a flex line."),
                card("Space", "Gap and padding are the air between."),
            )
            .row()
            .gap(18)
            .padding(22)
            .alignItems("stretch")
        )
