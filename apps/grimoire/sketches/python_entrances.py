"""Entrances and staggers: a grid arriving from its centre, a queue arriving
row by row and leaving in reverse, a wipe by columns and a seeded scatter."""

# TAGS: Motion/Animation, Compose/Motion, Python

from sigil import compose, motion
from sigil.compose import textFx
from sigil.motion import StaggerAxis, StaggerFrom, ease, stagger
from sigil.sketch import Scene2DContext, sketch

NAME = "Python entrances"
CATEGORY = "Python"

WIDTH, HEIGHT = 1280, 720
GROUND = "#0d1016"
PANEL = "#161b24"
RULE = "#2a3342"
INK = "#eef1f5"
QUIET = "#8a96a8"
ACCENTS = ["#f2b35e", "#e8806b", "#c77dba", "#7e9de0", "#5fc3c0", "#9ccf6f"]
MONO = "Menlo, SF Mono, monospace"

COLUMNS, ROWS = 6, 4
QUEUE = ["intake", "sorting", "review", "approval", "dispatch", "archive"]


def label(content: str, size: float = 12, color: str = QUIET) -> compose.Element:
    return compose.text(content, size=size, color=color).fontFamily(MONO)


def card(index: int) -> compose.Element:
    from_centre = stagger(
        0.12,
        grid=(COLUMNS, ROWS),
        from_=StaggerFrom.Center,
        ease=ease.inQuad,
        start=0.2,
    )
    return (
        compose.box(label(f"{index + 1:02d}", 11, INK))
        .width(104)
        .height(72)
        .padding(10)
        .borderRadius(8)
        .fill(ACCENTS[(index % COLUMNS + index // COLUMNS) % len(ACCENTS)])
        .opacity(motion.animate(from_=0, to=1, duration=0.6, delay=from_centre))
        .scale(
            motion.animate(
                from_=0.4, to=1, duration=0.8, delay=from_centre, ease=ease.outBack()
            )
        )
    )


def card_grid() -> compose.Element:
    cards = [card(index) for index in range(COLUMNS * ROWS)]
    return (
        compose.box(*cards)
        .row()
        .flexWrap(compose.FlexWrap.Wrap)
        .gap(12)
        .width(COLUMNS * 104 + (COLUMNS - 1) * 12)
    )


def wipe() -> compose.Element:
    by_columns = stagger(
        0.05,
        grid=(16, 3),
        from_=StaggerFrom.First,
        axis=StaggerAxis.Columns,
        start=0.25,
    )
    cells = [
        compose.box()
        .width(32)
        .height(12)
        .borderRadius(2)
        .fill(ACCENTS[4])
        .opacity(
            motion.animate(
                from_=0, to=1, duration=0.3, delay=by_columns, ease=ease.outQuad
            )
        )
        for _ in range(16 * 3)
    ]
    return (
        compose.box(*cells)
        .row()
        .flexWrap(compose.FlexWrap.Wrap)
        .gap(4)
        .width(16 * 36 - 4)
    )


def scatter() -> compose.Element:
    seeded = stagger(0.04, from_=StaggerFrom.Random, seed=11, start=0.35)
    dots = [
        compose.box()
        .width(14)
        .height(14)
        .borderRadius(7)
        .fill(ACCENTS[0])
        .translateY(motion.animate(from_=-18, to=0, duration=0.4, delay=seeded))
        .opacity(motion.animate(from_=0, to=1, duration=0.25, delay=seeded))
        for _ in range(24)
    ]
    return compose.box(*dots).row().gap(10)


def cursor(stop: int) -> compose.Element:
    stops = [compose.box().width(8).height(8).borderRadius(4).fill(RULE) for _ in QUEUE]
    pill = (
        compose.box()
        .width(24)
        .height(8)
        .borderRadius(4)
        .fill(INK)
        .absolute()
        .left(-8)
        .top(0)
        .key("pill")
        .translateX(motion.animate(to=stop * 56, duration=0.4, ease=ease.outCubic))
    )
    return compose.box(*stops, pill).row().gap(48).padding(0, 8)


def queue_row(name: str, index: int) -> compose.Element:
    one_by_one = stagger(0.12, start=0.2)
    return (
        compose.box(
            compose.box().width(6).height(28).borderRadius(3).fill(ACCENTS[index]),
            label(f"{index + 1}", 12, QUIET),
            compose.text(name, size=17, color=INK),
        )
        .row()
        .gap(14)
        .alignItems("center")
        .padding(10, 14)
        .borderRadius(6)
        .fill(PANEL)
        .key(name)
        .opacity(
            motion.animate(
                from_=0, to=1, duration=0.45, delay=one_by_one, ease=ease.outCubic
            )
        )
        .translateX(
            motion.animate(
                from_=48, to=0, duration=0.45, delay=one_by_one, ease=ease.outCubic
            )
        )
        .exit()
    )


def queue(present: bool) -> compose.Element:
    rows = [queue_row(name, index) for index, name in enumerate(QUEUE)]
    return (
        compose.box(*(rows if present else []))
        .column()
        .gap(10)
        .height(len(QUEUE) * 48 + (len(QUEUE) - 1) * 10)
    )


@sketch(size=(WIDTH, HEIGHT), background=GROUND, capture_at=0.8)
class Entrances:
    def setup(self, ctx: Scene2DContext) -> None:
        self.composer = ctx.composer
        self.present = True
        ctx.engine.timer(self.turn, stepRate=1 / 3.5)
        self.stop = 0
        ctx.engine.timer(self.move_cursor, stepRate=1 / 0.7)
        ctx.render(self.page())
        self.composer.renderSlot("cursor", cursor(self.stop))

    def turn(self) -> None:
        self.present = not self.present
        self.composer.render(self.page())

    def move_cursor(self) -> None:
        self.stop = (self.stop + 1) % len(QUEUE)
        self.composer.renderSlot("cursor", cursor(self.stop))

    def page(self) -> compose.Element:
        title = (
            compose.text("ARRIVALS", size=44, color=INK)
            .letterSpacing(8)
            .textFx(textFx.entrance(textFx.rise(30)))
        )
        left = (
            compose.box(
                label("GRID · FROM THE CENTRE · EASED"),
                card_grid(),
                label("WIPE · BY COLUMNS"),
                wipe(),
                label("SCATTER · SEED 11"),
                scatter(),
            )
            .column()
            .gap(14)
        )
        right = (
            compose.box(
                label("QUEUE · IN, THEN OUT IN REVERSE"),
                queue(self.present),
                compose.box().height(18),
                label("CURSOR · EASES ON CHANGE"),
                compose.slot("cursor"),
            )
            .column()
            .gap(10)
            .width(340)
        )
        body = compose.box(left, right).row().gap(56)
        return (
            compose.box(title, compose.box().width("100%").height(1).fill(RULE), body)
            .cover()
            .padding(40, 56)
            .column()
            .gap(22)
            .fill(GROUND)
        )
