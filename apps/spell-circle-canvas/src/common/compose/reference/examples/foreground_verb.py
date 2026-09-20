"""foreground — a decoration painted over the children, which is what a
keyline that must survive the content it sits on needs.

The Python twin of foreground_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
PLATE = "#223039"
INK = "#e8eef2"
KEY = "#f0f4f7"
ASH = "#8ea0ad"


def keyline() -> compose.PathFormat:
    return compose.stroke(3, KEY, compose.PathFormat.Align.Inner)


def tile(caption: str, plate: compose.Element) -> compose.Element:
    """A tile whose child fills the whole box, so a mark under the
    children would be covered and a mark over them is not."""
    return (
        compose.box(
            plate.width(compose.pct(100))
            .height(120)
            .corners(10)
            .children(
                compose.box(
                    compose.text("A FULL-BLEED CHILD", size=14, color=INK).fontTrack(2)
                )
                .cover()
                .corners(10)
                .fill(PLATE)
                .justify("center")
                .alignItems("center")
            ),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .basis(0)
        .grow(1)
        .alignItems("center")
    )


@sketch(size=(620, 250), background=GROUND, capture_at=0)
class ForegroundVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                # Under the fill and under the children: the child covers it.
                tile("background(keyline)", compose.box().background(keyline())),
                # Over the children: the keyline stands.
                tile("foreground(keyline)", compose.box().foreground(keyline())),
            )
            .row()
            .gap(18)
            .padding(24)
        )
