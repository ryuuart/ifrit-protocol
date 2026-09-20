"""backdrop — what is already painted beneath a node, filtered before
the node paints: the frosted panel over a lattice.

The Python twin of backdrop_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, material
from sigil.sketch import SketchContext, sketch

GROUND = "#10161b"
INK = "#f0f5f8"
VEIL = "#dcecf42e"
LIGHT = "#26414d"
DARK = "#14242c"
TILE = 28


def lattice() -> compose.Element:
    """The ground the panels stand on: a lattice of real elements, so a
    filtered backdrop is visibly a filter rather than a wash."""
    return compose.box(
        compose.box(
            compose.box()
            .width(TILE)
            .height(TILE)
            .fill(LIGHT if (row + column) % 2 == 0 else DARK)
            for column in range(23)
        )
        .row()
        .height(TILE)
        for row in range(10)
    ).column()


def panel(caption: str, plate: compose.Element) -> compose.Element:
    return (
        plate.width(210)
        .height(120)
        .borderRadius(14)
        .fill(VEIL)
        .justify("center")
        .alignItems("center")
        .children(compose.text(caption, size=14, color=INK))
    )


@sketch(size=(620, 280), background=GROUND, capture_at=0)
class BackdropVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return compose.stack(
            lattice().cover(),
            compose.box(
                panel("the veil alone", compose.box()),
                # The lattice under this panel is blurred before the
                # panel's own translucent fill goes down.
                panel(
                    "backdrop(blur)",
                    compose.box().backdrop(material.Effect.blur(7)),
                ),
            )
            .cover()
            .row()
            .gap(20)
            .padding(30)
            .justify("center")
            .alignItems("center"),
        )
