"""ink — the colour text under a node is set in, and the colour every
mark that names none is painted in.

The Python twin of ink_verb.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
PLATE = "#1b2229"
PALE = "#d9e3ea"
AMBER = "#e0a03c"


def panel(caption: str) -> compose.Element:
    """One panel. Nothing inside it names a colour: the text takes the
    ink, the stroke takes the ink because it names none, and the square
    takes it by asking for the ink in force outright."""
    return (
        compose.box(
            compose.text(caption, size=15),
            compose.box()
            .height(30)
            .width(30)
            .corners(4)
            .fill(compose.Fill.currentInk()),
        )
        .column()
        .gap(12)
        .padding(18)
        .basis(0)
        .grow(1)
        .corners(10)
        .fill(PLATE)
        .stroke(compose.stroke(1.5))
    )


@sketch(size=(600, 260), background=GROUND, capture_at=0)
class InkVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                panel("The ink inherited"),
                # …and a subtree that sets its own.
                panel("The ink set here").ink(AMBER),
            )
            .row()
            .gap(18)
            .padding(24)
            .alignItems("stretch")
            # The ink for everything under this node…
            .ink(PALE)
        )
