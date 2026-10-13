"""Springs and oscillators: a panel that snaps between two places on a spring
stepped from a timer, a colour that settles, a flash and a decay for each hit,
and the three words for stillness."""

# TAGS: Motion/Physics, Compose/Motion, Python

import math

from sigil import compose, material, motion
from sigil.motion import Oscillator, Spring, SpringParameters, Wave, bind, decay, flash
from sigil.sketch import Scene2DContext, sketch

NAME = "Python springs"
CATEGORY = "Python"

WIDTH, HEIGHT = 1280, 720
GROUND = "#0f1115"
TRACK_INK = "#22262e"
INK = "#f1efe9"
QUIET = "#8a909a"
COOL = "#4d7cff"
WARM = "#ff8a3d"
STRIKE = "#fff1c2"
MONO = "Menlo, SF Mono, monospace"

TRACK = 980
PANEL = 220
PLACES = (120.0, TRACK - PANEL - 120.0)  # the two places the panel snaps between
DWELL = 3.0  # seconds at each place before the target moves
HIT_SPEED = 150.0  # a crossing faster than this, in pixels a second, is a hit
SNAP = SpringParameters(period=0.6, damping=0.45)  # rings, then rests
SETTLE = SpringParameters(period=0.9, damping=1.0)  # arrives without crossing
WAVES = [
    Oscillator(wave=Wave.Sine, hertz=0.6, amplitude=1.0),
    Oscillator(wave=Wave.Triangle, hertz=0.6, amplitude=1.0),
    Oscillator(wave=Wave.Square, hertz=0.6, amplitude=1.0, duty=0.3),
]


def label(content: str, size: float = 12, color: str = QUIET) -> compose.Element:
    return (
        compose.text(content, size=size, color=color).fontFamily(MONO).letterSpacing(2)
    )


def answer(word: str, question: str, yes: bool) -> compose.Element:
    return (
        compose.box(
            label(word.upper(), 12, INK).width(110),
            label(question, 12).width(330),
            label("YES" if yes else "NO", 12, WARM if yes else QUIET),
        )
        .row()
        .gap(16)
    )


@sketch(size=(WIDTH, HEIGHT), background=GROUND, capture_at=0.3)
class Springs:
    def setup(self, ctx: Scene2DContext) -> None:
        self.composer = ctx.composer
        self.engine = ctx.engine
        self.travel = Spring(value=PLACES[0])  # the place, with its own velocity
        self.warmth = Spring()  # 0 cool, 1 warm
        self.place = motion.animatable(PLACES[0])
        self.tint = motion.animatable(compose.Fill.color(COOL))
        self.strikes = [motion.animatable(0.0), motion.animatable(0.0)]
        self.shake = motion.animatable(0.0)
        self.swings = [motion.animatable(0.0) for _ in WAVES]
        self.hit_at = -10.0
        self.answers = None
        ctx.engine.timer(self.step)
        ctx.render(self.page())

    def step(self, delta: float, elapsed: float) -> None:
        toward = 1 - int(elapsed / DWELL) % 2  # which place the panel is sent to
        target = PLACES[toward]
        before = self.travel.value - target
        self.travel = self.travel.step(target, delta, SNAP)
        crossed = before * (self.travel.value - target) < 0
        if crossed and abs(self.travel.velocity) > HIT_SPEED:
            self.hit_at = elapsed
        self.warmth = self.warmth.step(toward, delta, SETTLE)

        self.place.set(self.travel.value)
        self.tint.set(
            compose.Fill.color(material.lerpOklab(COOL, WARM, self.warmth.value))
        )
        age = elapsed - self.hit_at
        self.strikes[toward].set(flash(age, 0.03, 0.3, rest=0.2))
        self.strikes[1 - toward].set(0.2)
        self.shake.set(5 * decay(age, 0.2) * math.sin(age * 40))
        for swing, oscillator in zip(self.swings, WAVES):
            swing.set(oscillator.at(elapsed))
        self.show_answers(target)

    def show_answers(self, target: float) -> None:
        answers = (
            self.place.isRunning(),
            self.engine.isRunning(),
            self.travel.isSettled(target),
        )
        if answers == self.answers:
            return
        self.answers = answers
        declared, running, settled = answers
        self.composer.renderSlot(
            "stillness",
            compose.box(
                answer("declared", "could the place move?", declared),
                answer("running", "is the engine moving anything?", running),
                answer("settled", "has the spring come to rest?", settled),
            )
            .column()
            .gap(10),
        )

    def page(self) -> compose.Element:
        panel = (
            compose.box(
                label("PANEL", 12, INK),
                compose.text("Snap", size=34, color=INK),
            )
            .column()
            .gap(8)
            .padding(18)
            .width(PANEL)
            .height(150)
            .borderRadius(14)
            .fill(self.tint)
            .absolute()
            .left(0)
            .top(0)
            .translateX(self.place)
            .rotate(self.shake)
        )
        marks = [
            compose.box()
            .width(PANEL)
            .height(150)
            .borderRadius(14)
            .stroke(material.from_(STRIKE), 3)
            .opacity(strike)
            .absolute()
            .left(left)
            .top(0)
            for left, strike in zip(PLACES, self.strikes)
        ]
        stage = (
            compose.box(*marks, panel)
            .width(TRACK)
            .height(150)
            .borderRadius(14)
            .fill(TRACK_INK)
        )
        heading = (
            compose.box(
                compose.text("SPRINGS", size=40, color=INK).letterSpacing(6),
                label("A POSITION AND A VELOCITY, STEPPED TOWARDS A TARGET"),
            )
            .column()
            .gap(8)
        )
        return (
            compose.box(
                heading,
                stage,
                compose.box(compose.slot("stillness"), self.oscillators())
                .row()
                .gap(80),
            )
            .cover()
            .padding(56, 150)
            .column()
            .gap(56)
            .fill(GROUND)
        )

    def oscillators(self) -> compose.Element:
        lanes = [
            compose.box(
                compose.box()
                .width(18)
                .height(18)
                .borderRadius(9)
                .fill(WARM)
                .translateY(bind(swing, from_=(-1, 1), to=(36, -36))),
                label(oscillator.wave.name.upper(), 10),
            )
            .column()
            .gap(48)
            .alignItems("center")
            .padding(40, 0, 0, 0)
            .width(90)
            for swing, oscillator in zip(self.swings, WAVES)
        ]
        return compose.box(*lanes).row().gap(8)
