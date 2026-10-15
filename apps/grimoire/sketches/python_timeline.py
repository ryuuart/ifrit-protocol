"""A title sequence on one timeline: labels, positions, calls that swap the
credit, and a transport timer that holds a frame and plays it again."""

# TAGS: Motion/Animation, Compose/Motion, Python

from sigil import compose, motion
from sigil.motion import Tween, afterPrevious, at, atLabel, ease, stagger, withPrevious
from sigil.sketch import Scene2DContext, sketch

NAME = "Python timeline"
CATEGORY = "Python"

WIDTH, HEIGHT = 1280, 720
NIGHT = "#07080d"
INK = "#f4efe6"
EMBER = "#e9a15a"
QUIET = "#7d8496"
RULE = "#262a36"
MONO = "Menlo, SF Mono, monospace"

OPEN = 0.4  # the title starts
CREDITS = 2.6  # the first credit is swapped in
OUT = 6.8  # everything leaves
LENGTH = OUT + 0.95  # the last item ends
FREEZE = 4.6  # the transport holds this frame
HOLD = 0.8  # for this long, and the finished sequence as long again
TRACK = 1060

CREDIT_LINES = [
    ("WRITTEN AND DIRECTED BY", "Ines Marlowe"),
    ("DIRECTOR OF PHOTOGRAPHY", "Tomas Achterberg"),
    ("MUSIC BY", "Noor Halvorsen"),
]


def label(content: str, size: float = 12, color: str = QUIET) -> compose.Element:
    return (
        compose.text(content, size=size, color=color).fontFamily(MONO).letterSpacing(2)
    )


def credit(index: int) -> compose.Element:
    role, name = CREDIT_LINES[index]
    return (
        compose.box(label(role, 13, EMBER), compose.text(name, size=30, color=INK))
        .column()
        .gap(8)
        .alignItems("center")
        .key(f"credit{index}")
        .opacity(motion.animate(from_=0, to=1, duration=0.6))
        .translateY(motion.animate(from_=14, to=0, duration=0.6, ease=ease.outCubic))
    )


@sketch(size=(WIDTH, HEIGHT), background=NIGHT, captureAt=1.4)
class TitleSequence:
    def setup(self, ctx: Scene2DContext) -> None:
        self.composer = ctx.composer
        self.title = motion.animatable(0.0)
        self.rule = motion.animatable(0.0)
        self.subtitle = motion.animatable(0.0)
        self.playhead = motion.animatable(0.0)
        self.waited = 0.0
        self.frozen = False
        self.sequence = (
            ctx.engine.timeline()
            .label("open", at(OPEN))
            .add(
                self.title,
                Tween(from_=0, to=1, duration=1.4, ease=ease.outCubic),
                atLabel("open"),
            )
            .add(
                self.rule,
                Tween(from_=0, to=1, duration=1.0, ease=ease.inOutCubic),
                withPrevious(0.5),
            )
            .add(self.subtitle, Tween(from_=0, to=1, duration=0.7), afterPrevious())
            .label("credits", at(CREDITS))
            .call(lambda: self.show_credit(0), atLabel("credits"))
            .call(lambda: self.show_credit(1), atLabel("credits", 1.4))
            .call(lambda: self.show_credit(2), atLabel("credits", 2.8))
            .add(
                [self.title, self.rule, self.subtitle],
                Tween(to=0, duration=0.65, delay=stagger(0.15)),
                at(OUT),
            )
        )
        ctx.engine.timer(self.transport)
        ctx.render(self.page())

    def show_credit(self, index: int) -> None:
        self.composer.renderSlot("credit", credit(index))

    def transport(self, delta: float, elapsed: float) -> None:
        self.playhead.set(self.sequence.progress())
        if self.sequence.isPaused() or self.sequence.isCompleted():
            self.waited += delta
            if self.waited < HOLD:
                return
            self.waited = 0.0
            if self.sequence.isCompleted():
                self.frozen = False
                self.composer.renderSlot("credit", compose.box())
                self.sequence.seek(0).play()
            else:
                self.sequence.resume()
        elif not self.frozen and self.sequence.currentTime() >= FREEZE:
            self.frozen = True
            self.sequence.pause()

    def page(self) -> compose.Element:
        title = (
            compose.text("THE NORTHERN LINE", size=64, color=INK)
            .letterSpacing(10)
            .opacity(self.title)
            .translateY(motion.bind(self.title, to=(28, 0)))
        )
        rule = (
            compose.box()
            .width(520)
            .height(2)
            .fill(EMBER)
            .transformOrigin("50%", "50%")
            .scaleX(self.rule)
        )
        subtitle = label("A FILM IN FOUR STATIONS", 15, QUIET).opacity(self.subtitle)
        credits = compose.box(compose.slot("credit")).height(120).justifyContent("end")
        card = (
            compose.box(title, rule, subtitle, credits)
            .column()
            .gap(22)
            .alignItems("center")
            .flexGrow(1)
            .justifyContent("center")
        )
        return (
            compose.box(card, self.strip())
            .cover()
            .padding(40, 110)
            .column()
            .fill(NIGHT)
        )

    def strip(self) -> compose.Element:
        marks = [
            compose.box(label(name, 10), compose.box().width(1).height(10).fill(QUIET))
            .column()
            .gap(4)
            .absolute()
            .left(TRACK * moment / LENGTH)
            .top(0)
            for name, moment in [
                ("open", OPEN),
                ("credits", CREDITS),
                ("hold", FREEZE),
                ("out", OUT),
            ]
        ]
        played = (
            compose.box()
            .width(TRACK)
            .height(2)
            .fill(EMBER)
            .transformOrigin("0%", "50%")
            .scaleX(self.playhead)
        )
        head = (
            compose.box()
            .width(10)
            .height(10)
            .borderRadius(5)
            .fill(INK)
            .absolute()
            .top(26)
            .left(-5)
            .translateX(motion.bind(self.playhead, to=(0, TRACK)))
        )
        track = compose.box(played).width(TRACK).height(2).fill(RULE).absolute().top(30)
        return compose.box(*marks, track, head).width(TRACK).height(40)
