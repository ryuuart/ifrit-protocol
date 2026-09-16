"""Six native arrangements on one inherited specimen sheet.

TAGS: Geometry/Layout, Typography/Interface
"""

from sigil.compose import box, column, layout, memo, row, text
from sigil.compose import kit as marks
from sigil.compose.layouts import AlongPath, BaselineGrid, Diagonal, Jittered, Radial
from sigil.motion import entrance
from sigil.native import compose as raw
from sigil.sketch import SketchContext, kit, sketch
from sigil.skia import PathBuilder

WIDTH, HEIGHT = 352, 214
TEAL, RUST, GOLD = "#356c69", "#bd704c", "#bd994f"


def sheet_theme():
    look = kit.house_theme()
    look.palette.ground = "#f4f0e6"
    look.palette.cellGround = "#e8e3d7"
    look.palette.ink = "#30372f"
    look.palette.ash = "#72766c"
    look.palette.rule = "#c6c6b5"
    look.palette.figure = TEAL
    look.type.title.size = 30
    look.type.title.track = -0.5
    look.type.subtitle.size = 12
    look.type.subtitle.track = 0.4
    look.type.captionLabel.size = 11
    look.type.captionNote.size = 11
    look.type.captionNote.track = 0
    look.spacing.marginX = 48
    look.spacing.marginTop = 38
    look.spacing.marginBottom = 28
    look.spacing.subtitleGap = 10
    look.spacing.contentGap = 28
    look.spacing.captionGap = 10
    look.spacing.cellGap = 24
    return look


def board(*children):
    return marks.board(*children, size=(WIDTH, HEIGHT))


def bead(index, diameter=12):
    colors = (TEAL, TEAL, TEAL, RUST, GOLD)
    return box(
        width=diameter,
        height=diameter,
        corners=diameter / 2,
        fill=colors[index % len(colors)],
        shrink=0,
    )


def radial():
    orbit = marks.ring((WIDTH / 2, HEIGHT / 2), 73, raw.stroke(0.8, "#b9bead"))
    numbers = layout(
        Radial(radiusFraction=0.71),
        (
            marks.centred(text(f"{i + 1:02}", size=10, color=TEAL))
            .width(24)
            .height(24)
            .shrink(0)
            for i in range(12)
        ),
        width=WIDTH,
        height=HEIGHT,
    )
    core = marks.centred(text("360°", size=27, color=TEAL)).width(WIDTH).height(HEIGHT)
    return board(orbit, numbers, core)


def diagonal():
    steps = layout(
        Diagonal(skewDeg=-14, gap=8),
        (
            row(
                box(width=4, height=19, fill=RUST),
                text(word, size=11, color="#f7f1e5"),
                gap=12,
                width=194,
                height=22,
                align_items="center",
                fill=TEAL,
            )
            for word in ("ORIGIN", "MEASURE", "OFFSET", "REPEAT", "RESOLVE")
        ),
        width=234,
        height=150,
    )
    return board(marks.at(steps, 62, 29, 234, 150))


def along_path():
    curve = PathBuilder().moveTo(26, 160).cubicTo(94, -13, 226, 231, 328, 51).detach()
    guide = (
        box(width=WIDTH, height=HEIGHT)
        .shape(curve)
        .fill(None)
        .stroke(raw.stroke(1.2, "#a5b4a2"))
    )
    samples = layout(
        AlongPath(path=curve), (bead(i) for i in range(22)), width=WIDTH, height=HEIGHT
    )
    return board(guide, samples)


def baseline():
    guides = marks.ladder(count=5, pitch=28, fill="#c4c6b6").width(296)
    verse = layout(
        BaselineGrid(rhythm=28, gap=4),
        text("Light", size=30, color=TEAL),
        text("through the", size=16, color="#72766c"),
        text("leaves.", size=29, color=RUST),
        width=296,
        height=154,
    )
    return board(marks.at(guides, 28, 28, 296, 140), marks.at(verse, 28, 28, 296, 154))


def jittered():
    seeds = layout(
        Jittered(seed=37, jitter=0.82),
        (bead(i, 8 + (i % 4) * 4) for i in range(35)),
        width=302,
        height=164,
    )
    return board(marks.at(seeds, 25, 25, 302, 164))


def captured_palette(properties):
    look = kit.theme()
    colors = (look.palette.ink, look.palette.figure, look.palette.rule)
    swatches = row(
        (box(width=72, height=84, fill=color, corners=3) for color in colors), gap=10
    )
    return (
        marks.centred(
            column(
                swatches,
                text(properties[0], size=11, color=look.palette.ink),
                gap=18,
            )
        )
        .width(WIDTH)
        .height(HEIGHT)
        .fill(look.palette.cellGround)
    )


def specimen(picture, index, label, note):
    surface = kit.well(width=WIDTH, height=HEIGHT, corners=6).children([picture])
    return kit.caption(
        surface, measure=WIDTH, label=f"{index:02} / {label}", note=note
    ).opacity(entrance(0, 1, duration=0.45, delay=index * 0.05))


@sketch(size=(1200, 760), capture_at=1.1)
class OrderSpecimen:
    def setup(self, ctx: SketchContext) -> None:
        look = sheet_theme()
        with kit.provide(look):
            kit.stage(ctx, size=(1200, 760), capture_at=1.1)
            inset = look.copy()
            inset.palette.ink = "#e9efda"
            inset.palette.figure = "#dab26a"
            inset.palette.rule = "#84a493"
            inset.palette.cellGround = "#254a48"
            with kit.provide(inset):
                palette = memo(("A theme held beyond its scope",), captured_palette)
            first = kit.cells(
                specimen(
                    radial(),
                    1,
                    "Radial",
                    "Twelve labels share one native circular layout.",
                ),
                specimen(
                    diagonal(),
                    2,
                    "Diagonal",
                    "Measured rows follow a single oblique axis.",
                ),
                specimen(
                    along_path(),
                    3,
                    "AlongPath",
                    "Equal arc-length steps along a cubic contour.",
                ),
            )
            second = kit.cells(
                specimen(
                    baseline(),
                    4,
                    "BaselineGrid",
                    "Different type sizes land on the same rhythm.",
                ),
                specimen(
                    jittered(),
                    5,
                    "Jittered",
                    "A seeded field keeps every specimen inside its cell.",
                ),
                specimen(
                    palette,
                    6,
                    "Memo + Provide",
                    "A deferred component restores its native environment.",
                ),
            )
            sheet = kit.page(
                kit.cells(first, second, column=True, gap=30),
                title="Six ways to place a thought",
                subtitle="FORM STUDIES   /   Native arrangement · inherited theme · ordinary Python",
                footer="SIGIL / COMPOSITION LABORATORY                                                  One renderer. Six native arrangements.",
            )
        ctx.render(sheet)
