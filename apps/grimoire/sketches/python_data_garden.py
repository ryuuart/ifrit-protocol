"""An invented greenhouse trial ledger, plotted from a small native table.

The measurements are illustrative, not a published botanical dataset. Native
CSV parsing, missing-value handling, sorting, band and square-root scales place
all marks; Python supplies the page's subject and composition.
TAGS: Data/Visualization, Drawing/Charts, Typography/Editorial
"""

from sigil.compose import box, column, graphics, row, text, var
from sigil.compose import document
from sigil.data import Order, Scale, Transform, decodeCsv
from sigil.draw import CENTER, LEFT, RIGHT
from sigil.sketch import Scene2DContext, kit, sketch
from sigil.weave import Type

CSV = """species,light,height,water,room
Maidenhair,36,24,84,A
Bird's nest,44,38,72,A
Staghorn,67,45,44,B
Button fern,28,19,61,A
Boston fern,52,54,92,B
Hart's tongue,39,33,68,A
Blue star,62,48,57,B
Rabbit's foot,57,29,49,B
Holly fern,72,41,53,C
Autumn fern,81,58,66,C
Silver lace,47,27,70,A
Royal fern,88,68,95,C
"""
ROOMS = {"A": "#6c9981", "B": "#c59e68", "C": "#ab7868"}


def register(context):
    """The face, size and tracking of the register the program's node is set
    in, with the colour left to the pen's fill."""
    font = context.font
    return Type(face=font.face, size=font.size, letterSpacing=font.letterSpacing)


@sketch(size=(1000, 780), captureAt=0, theme=kit.featureTheme(kit.Density.Spacious))
class DataGarden:
    def setup(self, ctx: Scene2DContext) -> None:
        self.table = decodeCsv(CSV)
        self.ranking = self.table.sort("height", Order.Descending)
        self.light = Scale(domain=(20, 100), range=(52, 530))
        self.height = Scale(domain=(0, 80), range=(365, 32))
        self.radius = Scale(domain=(0, 100), range=(0, 19), transform=Transform.Sqrt)
        self.rank = Scale(
            range=(12, 361), transform=Transform.Band, steps=12, padding=0.42
        )
        self.bar = Scale(domain=(0, 80), range=(0, 130))
        ctx.render(self.redraw())

    def redraw(self):
        summary = (
            row()
            .width(892)
            .justifyContent("space-between")
            .padding(horizontal=19, vertical=0)
            .children(
                (
                    column()
                    .gap(5)
                    .children(
                        document.label("SPECIMENS"),
                        text("12").styleClass("readout").fontSize(33),
                    )
                ),
                (
                    column()
                    .gap(5)
                    .children(
                        document.label("ROOMS"),
                        text("03").styleClass("readout").fontSize(33),
                    )
                ),
                (
                    column()
                    .gap(9)
                    .children(
                        document.label("OBSERVATION"),
                        text("Week 08").styleClass("readout").fontSize(25),
                    )
                ),
                (
                    column()
                    .gap(12)
                    .children(
                        document.label("BUBBLE AREA"),
                        text("Water / ml").styleClass("readout").fontSize(19),
                    )
                ),
            )
        )
        legend = (
            row()
            .gap(19)
            .children(
                [
                    (
                        row()
                        .gap(7)
                        .children(
                            (box().width(8).height(8).fill(ink).borderRadius(4)),
                            document.label(f"ROOM {room}").ink(var("palette-ash")),
                        )
                    )
                    for room, ink in ROOMS.items()
                ]
            )
        )
        return kit.page(
            column(
                summary,
                (
                    row()
                    .gap(27)
                    .children(
                        (
                            column()
                            .width(560)
                            .gap(15)
                            .children(
                                (
                                    row()
                                    .width(560)
                                    .justifyContent("space-between")
                                    .children(
                                        document.label("LIGHT × HEIGHT"),
                                        legend,
                                    )
                                ),
                                (
                                    graphics("greenhouse-scatter", self.scatter)
                                    .styleClass("control")
                                    .width(560)
                                    .height(416)
                                ),
                                document.caption(
                                    "Illustrative measurements  ·  relative light / final height in cm"
                                )
                                .fontSize(11)
                                .ink(var("palette-ash")),
                            )
                        ),
                        (box().width(1).height(448).fill(var("palette-rule"))),
                        (
                            column()
                            .width(276)
                            .gap(15)
                            .children(
                                document.label("HEIGHT / SORTED"),
                                (
                                    graphics("greenhouse-ranking", self.bars)
                                    .styleClass("control")
                                    .width(276)
                                    .height(416)
                                ),
                                document.footer("NATIVE TABLE → SORT → BAND SCALE")
                                .fontSize(9)
                                .ink(var("palette-ash")),
                            )
                        ),
                    )
                ),
            )
            .gap(24)
            .width(892),
            title="Under glass",
            subtitle="Native tables and scales / an illustrative trial of light, water and growth",
            footer="CSV → table → sort → scale · measurements are illustrative",
        )

    def scatter(self, pen, context):
        ground, ink, ash, rule = (
            context.vars.find(name)
            for name in ("palette-ground", "palette-ink", "palette-ash", "palette-rule")
        )
        pen.background(ground)
        pen.textFont(register(context))
        pen.textSize(10)
        for tick in self.height.ticks(4):
            y = self.height(tick)
            pen.stroke(rule)
            pen.strokeWeight(1)
            pen.line(52, y, 530, y)
            pen.noStroke()
            pen.fill(ash)
            pen.textAlign(RIGHT, CENTER)
            pen.text(str(int(tick)), 40, y)
        for tick in self.light.ticks(4):
            x = self.light(tick)
            pen.stroke(rule)
            pen.line(x, 32, x, 365)
            pen.noStroke()
            pen.fill(ash)
            pen.textAlign(CENTER, CENTER)
            pen.text(str(int(tick)), x, 388)
        for index in range(len(self.table)):
            sample = self.table.row(index)
            x, y = self.light(sample["light"]), self.height(sample["height"])
            radius = self.radius(sample["water"])
            pen.stroke(ground)
            pen.strokeWeight(2)
            pen.fill(ROOMS[sample["room"]])
            pen.circle(x, y, radius * 2)
            pen.noStroke()
            pen.fill("#ffffff")
            pen.circle(x, y, 3)
        pen.noStroke()
        pen.fill(ink)
        pen.textAlign(LEFT, CENTER)
        pen.textSize(11)
        pen.text("FINAL HEIGHT / cm", 52, 11)

    def bars(self, pen, context):
        ground, ink, ash, rule = (
            context.vars.find(name)
            for name in ("palette-ground", "palette-ink", "palette-ash", "palette-rule")
        )
        pen.background(ground)
        pen.textFont(register(context))
        for index in range(len(self.ranking)):
            sample = self.ranking.row(index)
            y = self.rank(index)
            height = self.rank.bandwidth()
            pen.noStroke()
            pen.fill(rule)
            pen.rect(130, y, 130, height)
            pen.fill(ROOMS[sample["room"]])
            pen.rect(130, y, self.bar(sample["height"]), height)
            pen.fill(ink)
            pen.textAlign(LEFT, CENTER)
            pen.textSize(10)
            pen.text(sample["species"], 0, y + height / 2)
            pen.textAlign(RIGHT, CENTER)
            pen.text(str(int(sample["height"])), 275, y + height / 2)
        pen.fill(ash)
        pen.textAlign(LEFT, CENTER)
        pen.textSize(10)
        pen.text("0", 130, 388)
        pen.textAlign(RIGHT, CENTER)
        pen.text("80 cm", 260, 388)
