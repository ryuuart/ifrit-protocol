"""Text effects: a title that decodes letter by letter, a word that flashes
in the page's own palette and settles to its ink, a line sung across by a
tint, a sequence that drops each letter in and then squashes it, and a wave
that never stops rolling."""

# TAGS: Compose/Typography, Compose/Motion, Python

from sigil import compose, motion, weave
from sigil.compose import textFx
from sigil.compose.textFx import GlyphModifier, GlyphModifierKeyframe
from sigil.motion import bind, ease, stagger
from sigil.sketch import Scene2DContext, sketch

NAME = "Text effects"
CATEGORY = "Python"

WIDTH, HEIGHT = 1280, 720
GROUND = "#0c0f14"
PANEL = "#141922"
RULE = "#262e3b"
INK = "#eef1f5"
QUIET = "#7d8899"
FLASH = "#ffd27a"
PHOSPHOR = "#5ce08a"
SUNG = "#7fe3c4"
PALE = "#39424f"
ACCENT = "#ff7a6b"
MONO = "Menlo, SF Mono, monospace"
CYCLE = 4.0  # seconds before each one-shot effect plays again
CIPHER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#%&*+=<>"


def label(content: str) -> compose.Element:
    return compose.text(content, size=12, color=QUIET).fontFamily(MONO).letterSpacing(2)


def panel(caption: str, *content: compose.Element) -> compose.Element:
    return (
        compose.box(label(caption), *content)
        .column()
        .justifyContent("center")
        .flexGrow(1)
        .gap(14)
        .padding(22, 26)
        .borderRadius(10)
        .fill(PANEL)
    )


@sketch(size=(WIDTH, HEIGHT), background=GROUND, capture_at=1.2)
class TextEffects:
    def setup(self, ctx: Scene2DContext) -> None:
        self.seconds = motion.animatable(0.0)
        ctx.engine.timer(lambda delta, elapsed: self.seconds.set(elapsed))
        ctx.render(self.page())

    def cycle(self, opens: float, closes: float) -> motion.Animatable:
        """A one-shot progress replayed every cycle: 0 until `opens` of the
        cycle, 1 from `closes` on."""
        phase = bind(self.seconds, to=(0, 1 / CYCLE), wrap=1)
        span = closes - opens
        return bind(phase, to=(-opens / span, (1 - opens) / span), clamp=(0, 1))

    def title(self) -> compose.Element:
        # Each letter churns through the cipher once its beat opens and has
        # landed by the beat's end; held, a letter waiting its turn paints
        # nothing rather than a wrong letter.
        return (
            compose.text("SIGIL DECODES", size=64, color=INK)
            .fontFamily(MONO)
            .letterSpacing(6)
            .textFx(
                textFx.hold(textFx.scramble(CIPHER, 16)),
                tween=motion.Tween(duration=0.7, delay=stagger(0.06)),
                progress=self.cycle(0.0, 0.55),
            )
        )

    def flash(self) -> compose.Element:
        # The stops name the page's custom property and the ink in force,
        # so restating either on the page recolours the flash.
        flare = textFx.tween(
            from_=GlyphModifier(alpha=0, colorScreen=compose.Fill.var("flash")),
            keyframes=[
                GlyphModifierKeyframe(
                    GlyphModifier(colorScreen=compose.Fill.var("flash"), dy=-6),
                    duration=0.3,
                ),
                GlyphModifierKeyframe(GlyphModifier(), duration=0.7),
            ],
            ease=ease.outQuad,
        )
        return (
            compose.text("PHOSPHOR", size=72, color=PHOSPHOR)
            .fontFamily(MONO)
            .letterSpacing(4)
            .textFx(
                flare,
                tween=motion.Tween(duration=0.9, delay=stagger(0.08)),
                progress=self.cycle(0.1, 0.75),
            )
        )

    def sung(self) -> compose.Element:
        # Set in the sung colour: the tint multiplies down from pale toward
        # it, word by word.
        return compose.text(
            "every word catches its colour", size=30, color=SUNG
        ).textFx(
            textFx.tint(from_=PALE),
            tween=motion.Tween(duration=0.5, delay=stagger(0.35)),
            unit=weave.Unit.Word,
            progress=self.cycle(0.05, 0.8),
        )

    def sequence(self) -> compose.Element:
        squash = textFx.tween(
            keyframes=[
                GlyphModifier(scaleX=1.3, scaleY=0.7, colorMultiplier=ACCENT),
                GlyphModifier(scaleX=0.9, scaleY=1.12),
                GlyphModifier(),
            ],
            ease=ease.inOutQuad,
        )
        drop = textFx.enter(textFx.rise(36))
        return (
            compose.text("FIRST, THEN", size=52, color=INK)
            .fontFamily(MONO)
            .textFx(
                textFx.sequence(drop.until(0.5), squash),
                tween=motion.Tween(duration=1.0, delay=stagger(0.07)),
                progress=self.cycle(0.0, 0.7),
            )
        )

    def wave(self) -> compose.Element:
        # A wrapping phase with no stagger: every letter reads one master
        # phase and the wave shifts it per letter.
        phase = bind(self.seconds, to=(0, 1 / 1.6), wrap=1)
        return compose.text("the wave keeps rolling", size=40, color=INK).textFx(
            textFx.waveLoop(0.14, 0.45), tween=motion.Tween(), progress=phase
        )

    def page(self) -> compose.Element:
        left = (
            compose.box(
                panel("SCRAMBLE · HOLD", self.title()),
                panel("TWEEN · STOPS NAMING var(--flash)", self.flash()),
            )
            .column()
            .gap(20)
            .width(700)
        )
        right = (
            compose.box(
                panel("TINT · BY WORD", self.sung()),
                panel("SEQUENCE · ENTER, THEN SQUASH", self.sequence()),
                panel("WAVE LOOP", self.wave()),
            )
            .column()
            .gap(20)
            .flexGrow(1)
        )
        return (
            compose.box(
                compose.text("Text effects", size=20, color=QUIET).fontFamily(MONO),
                compose.box().width("100%").height(1).fill(RULE),
                compose.box(left, right).row().gap(24).flexGrow(1),
            )
            .cover()
            .var("flash", FLASH)
            .padding(36, 48)
            .column()
            .gap(20)
            .fill(GROUND)
        )
