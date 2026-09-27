"""Native Compose trees placed by a Draw pen and repeated as a custom brush tip.

TAGS: Drawing/Brushes, Geometry/Layout, Typography/Interface
"""

from sigil.compose import Element, box, text
from sigil.compose import document as doc
from sigil.draw import Pen, brush
from sigil.sketch import SketchContext, kit, sketch


def card(label: str, accent: str) -> Element:
    return (
        doc.article(
            doc.label("COMPOSE").fontSize(11),
            doc.h2(label).fontSize(22),
            doc.caption("Layout + type + paint").fontSize(11).ink("#ffffff"),
        )
        .gap(8)
        .padding(18)
        .borderRadius(12)
        .fill(accent)
        .ink("#ffffff")
    )


@sketch(size=(740, 370), capture_at=0)
class ComposeStamps:
    def setup(self, ctx: SketchContext) -> None:
        self.look = kit.feature_theme()
        with kit.provide(self.look):
            kit.stage(ctx, size=(740, 370), capture_at=0)
        self.cards = [
            card(label, accent).applyStyleSheet(self.look.styleSheet())
            for label, accent in (
                ("One tree", "#356c69"),
                ("Another", "#536b9c"),
                ("Transformed", "#aa674e"),
            )
        ]
        self.mark = (
            box()
            .row()
            .gap(4)
            .padding(6)
            .borderRadius(6)
            .fill(self.look.palette.figure)
            .ink(self.look.palette.ground)
            .font(self.look.font(self.look.type.captionLabel))
            .children(text("Aa", size=18), text("01", size=10))
        )
        self.brush = brush.marker(self.look.palette.figure, 78)
        self.brush.tip = brush.Tip.Custom
        self.brush.spacing = 92
        self.brush.markerTip = False
        self.brush.scatter = 0
        self.brush.rotation = brush.Rotation.Fixed
        self.brush.customTip = self.stamp

    def stamp(self, pen: Pen) -> None:
        pen.scale(1 / 64)
        pen.element(self.mark, -32, -18, 64, 36)

    def draw(self, pen: Pen) -> None:
        pen.background(self.look.palette.ground)
        pen.fill(self.look.palette.ink)
        pen.textFont(self.look.font(self.look.type.title))
        pen.text("Compose trees inside Draw", 28, 34)
        for i, tree in enumerate(self.cards):
            pen.push()
            pen.translate(30 + i * 237, 65)
            pen.rotate((i - 1) * 0.04)
            pen.element(tree, 0, 0, 200, 130, index=i)
            pen.pop()
        pen.fill(self.look.palette.ash)
        pen.textFont(self.look.font(self.look.type.captionNote))
        pen.text("The same bridge inside a custom brush tip", 28, 242)
        brush.line(pen, self.brush, (70, 302), (670, 302))
        pen.noLoop()
