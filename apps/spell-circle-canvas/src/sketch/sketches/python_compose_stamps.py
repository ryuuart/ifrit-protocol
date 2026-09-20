"""Native Compose trees placed by a Draw pen and repeated as a custom brush tip.

TAGS: Drawing/Brushes, Geometry/Layout, Typography/Interface
"""

from sigil.compose import Element, box, text
from sigil.draw import Pen, brush
from sigil.sketch import SketchContext, sketch


def card(label: str, accent: str) -> Element:
    return (
        box()
        .column()
        .gap(8)
        .padding(18)
        .borderRadius(12)
        .fill(accent)
        .ink("#ffffff")
        .children(
            text("COMPOSE", size=11),
            text(label, size=27),
            text("Layout + type + paint", size=11),
        )
    )


@sketch(size=(740, 370), background="#f1efe8", capture_at=0)
class ComposeStamps:
    def setup(self, ctx: SketchContext) -> None:
        self.cards = [
            card("One tree", "#356c69"),
            card("Another", "#536b9c"),
            card("Transformed", "#aa674e"),
        ]
        self.mark = (
            box()
            .row()
            .gap(4)
            .padding(6)
            .borderRadius(6)
            .fill("#273d41")
            .ink("#ffffff")
            .children(text("Aa", size=18), text("01", size=10))
        )
        self.brush = brush.marker("#273d41", 78)
        self.brush.tip = brush.Tip.Custom
        self.brush.spacing = 92
        self.brush.markerTip = False
        self.brush.scatter = 0
        self.brush.rotation = brush.Rotation.Fixed
        self.brush.customTip = self.stamp

    def stamp(self, pen: Pen) -> None:
        pen.scale(1 / 64)
        pen.element(self.mark, (-32, -18, 64, 36))

    def draw(self, pen: Pen) -> None:
        pen.background("#f1efe8")
        pen.fill("#273d41")
        pen.textSize(20)
        pen.text("Compose trees inside Draw", 28, 34)
        for i, tree in enumerate(self.cards):
            pen.push()
            pen.translate(30 + i * 237, 65)
            pen.rotate((i - 1) * 0.04)
            pen.element(tree, (0, 0, 200, 130), index=i)
            pen.pop()
        pen.textSize(12)
        pen.text("The same bridge inside a custom brush tip", 28, 242)
        brush.line(pen, self.brush, (70, 302), (670, 302))
        pen.noLoop()
