"""Live values and bindings: one number from an oscillator, followed by a dial,
a meter, a peak lamp, a stepped readout and three shaped tracks, each through
its own binding.

The binding's stages, one each: a range and a seeded wiggle (the dial), a
clamp (the meter), an envelope (the lamp), quantize (the steps), and on the
tracks a fold that runs out and back, a cosine envelope and an overshooting
ease.

EDIT THESE FIRST: SOURCE, SWEEP, SEGMENTS, SHAPES."""

# TAGS: Motion/Bindings, Compose/Motion, Python

from sigil import compose, motion
from sigil.motion import Oscillator, Range, Wave, Wiggle, bind, ease, envelope
from sigil.sketch import Scene2DContext, sketch

NAME = "Python bindings"
CATEGORY = "Python"

WIDTH, HEIGHT = 1280, 720
GROUND = "#101214"
FACE = "#1b1f24"
RULE = "#343b44"
INK = "#eceae4"
QUIET = "#8b929b"
NEEDLE = "#ff6a3d"
SIGNAL = "#7fd1ae"
PEAK = "#ff4d4d"
MONO = "Menlo, SF Mono, monospace"

DIAL = 360
SWEEP = (-135, 135)  # the needle's travel, in degrees
FULL_SCALE = Range(0.0, 1.0)  # what the source reads from empty to full
SEGMENTS = 12
METER = 520
PUCK = 22

# The remaining stages, each a track whose puck reads the same live number:
# the label, then the binding's keywords. A fold runs the lower half out and
# back; a cosine envelope rises and falls once over the source's range; an
# overshooting ease passes its stop and settles.
SHAPES = [
    ("FOLD · ALTERNATE", dict(from_=(0.0, 0.5), alternate=True)),
    ("COSINE · ENVELOPE", dict(from_=FULL_SCALE, envelope=envelope.cosine())),
    ("OVERSHOOT · EASE", dict(from_=FULL_SCALE, ease=ease.outBack())),
]

SOURCE = Oscillator(wave=Wave.Sine, hertz=0.17, phase=0.6, amplitude=0.46, centre=0.5)


def label(content: str, size: float = 12, color: str = QUIET) -> compose.Element:
    return (
        compose.text(content, size=size, color=color).fontFamily(MONO).letterSpacing(2)
    )


def spoke(degrees: float, child: compose.Element) -> compose.Element:
    """A dial-sized layer turned about the dial's centre, carrying one mark."""
    return compose.box(child).absolute().inset(0).alignItems("center").rotate(degrees)


def dial(level: motion.Animatable) -> compose.Element:
    ticks = [
        spoke(
            SWEEP[0] + (SWEEP[1] - SWEEP[0]) * step / 10,
            compose.box()
            .width(3 if step % 5 == 0 else 2)
            .height(26 if step % 5 == 0 else 14)
            .fill(PEAK if step >= 8 else INK)
            .borderRadius(1),
        )
        for step in range(11)
    ]
    needle = (
        compose.box(
            compose.box().width(4).height(DIAL / 2 - 30).borderRadius(2).fill(NEEDLE)
        )
        .absolute()
        .inset(0)
        .alignItems("center")
        .padding(30, 0, 0, 0)
        .rotate(
            bind(
                level,
                from_=FULL_SCALE,
                to=SWEEP,
                wiggle=Wiggle(amount=1.6, frequency=9, seed=4),
            )
        )
    )
    hub = (
        compose.box()
        .width(22)
        .height(22)
        .borderRadius(11)
        .fill(INK)
        .absolute()
        .left(DIAL / 2 - 11)
        .top(DIAL / 2 - 11)
    )
    return (
        compose.box(*ticks, needle, hub)
        .width(DIAL)
        .height(DIAL)
        .padding(18)
        .borderRadius(DIAL / 2)
        .fill(FACE)
    )


def meter(level: motion.Animatable) -> compose.Element:
    bar = (
        compose.box()
        .width(METER)
        .height(18)
        .borderRadius(3)
        .fill(SIGNAL)
        .transformOrigin("0%", "50%")
        .scaleX(bind(level, from_=FULL_SCALE, clamp=(0, 1)))
    )
    lamp = (
        compose.box()
        .width(18)
        .height(18)
        .borderRadius(9)
        .fill(PEAK)
        .opacity(bind(level, envelope=envelope.trapezoid(0.8, 0.86, 1.0, 1.0)))
    )
    return (
        compose.box(
            compose.box(bar).width(METER).height(18).borderRadius(3).fill(FACE),
            lamp,
        )
        .row()
        .gap(16)
        .alignItems("center")
    )


def stepped(level: motion.Animatable) -> compose.Element:
    lights = [
        compose.box()
        .width(METER / SEGMENTS - 6)
        .height(30)
        .borderRadius(2)
        .fill(PEAK if index >= SEGMENTS - 2 else SIGNAL)
        .opacity(
            bind(
                level,
                from_=(index / SEGMENTS, (index + 1) / SEGMENTS),
                clampFrom=True,
                quantize=2,
                to=(0.12, 1.0),
            )
        )
        for index in range(SEGMENTS)
    ]
    return compose.box(*lights).row().gap(6)


def shaped(level: motion.Animatable, title: str, stages: dict) -> compose.Element:
    """A track and a puck carried along it by one shaped binding."""
    puck = (
        compose.box()
        .width(PUCK)
        .height(PUCK)
        .borderRadius(6)
        .fill(NEEDLE)
        .absolute()
        .left(0)
        .top(0)
        .translateX(bind(level, to=(0, METER - PUCK), **stages))
    )
    rail = compose.box().width(METER).height(1).fill(RULE).absolute().left(0).top(PUCK / 2)
    return (
        compose.box(
            label(title, 10),
            compose.box(rail, puck).width(METER).height(PUCK),
        )
        .column()
        .gap(6)
    )


@sketch(size=(WIDTH, HEIGHT), background=GROUND, captureAt=3.4)
class Bindings:
    def setup(self, ctx: Scene2DContext) -> None:
        self.level = motion.animatable(SOURCE.at(0))
        ctx.engine.timer(lambda delta, elapsed: self.level.set(SOURCE.at(elapsed)))
        ctx.render(self.page())

    def page(self) -> compose.Element:
        readings = (
            compose.box(
                label("METER · RANGE · CLAMP"),
                meter(self.level),
                compose.box().height(18),
                label("STEPPED · TWELVE SEGMENTS"),
                stepped(self.level),
                compose.box().height(18),
                *(shaped(self.level, title, stages) for title, stages in SHAPES),
                compose.box().height(18),
                label("ONE SOURCE · OSCILLATOR 0.17 HZ"),
                compose.text(
                    "Every mark reads the same live number.", size=22, color=INK
                ),
            )
            .column()
            .gap(12)
        )
        heading = (
            compose.box(
                compose.text("LIVE VALUES", size=40, color=INK).letterSpacing(6),
                label("ANIMATABLE → BIND → PROPERTY"),
            )
            .column()
            .gap(8)
        )
        body = (
            compose.box(dial(self.level), readings).row().gap(72).alignItems("center")
        )
        return (
            compose.box(heading, body)
            .cover()
            .padding(40, 72)
            .column()
            .gap(32)
            .fill(GROUND)
        )
