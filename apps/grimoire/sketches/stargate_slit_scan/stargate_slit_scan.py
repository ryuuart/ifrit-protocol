"""Study · Film — a camera passage written as luminous strip exposure.

TAGS: Studies/Film, Materials/Optics, Animation/Sequence, Runtime/Python

An original optical sequence studying slit-scan spatial stretching.
The source strips, camera path and typography are authored geometry.
"""

from pathlib import Path

from sigil.compose import Overflow, box, text
from sigil.material import BloomOptions, Filter, shader
from sigil.motion import animatable
from sigil.sketch import SketchContext, sketch
from sigil.weave import TextAlignment

WIDTH, HEIGHT, DURATION = 1800, 900, 28.0
ROOT = Path(__file__).parent


def title(words, top, size, tracking, weight=300):
    return (
        text(words, size=size, color="#E7E9DE")
        .fontFamily("Helvetica Neue")
        .fontWeight(weight)
        .letterSpacing(tracking)
        .width(WIDTH)
        .textAlign(TextAlignment.Center)
        .at(0, top)
    )


def ease(time, start, stop):
    x = max(0.0, min(1.0, (time - start) / (stop - start)))
    return x * x * (3.0 - 2.0 * x)


@sketch()
class StargateSlitScan:
    def setup(self, ctx: SketchContext):
        ctx.canvas(WIDTH, HEIGHT)
        ctx.background("#000000")
        ctx.captureAt(8.0)
        self.elapsed = animatable(0.0)
        self.travel = animatable(0.0)
        self.exposure = animatable(0.0)
        self.opening = animatable(0.0)
        self.closing = animatable(0.0)

        self.image = shader(
            ctx.assets.hub(),
            ctx.local("slit_scan.sksl"),
            {"resolution": (float(WIDTH), float(HEIGHT)), "seconds": 0.0},
        ).bind("seconds", self.travel)
        halation = Filter.bloom(
            BloomOptions(
                sigma=2.2,
                strength=0.19,
                spread=7.0,
                tail=0.16,
                threshold=0.65,
                knee=0.18,
            )
        )
        transfer = Filter.program(
            (ROOT / "emulsion.sksl").read_text(), {"seconds": 0.0}
        ).bind("seconds", self.elapsed)

        ctx.render(
            box()
            .inset(0)
            .fill("#000000")
            .overflow(Overflow.Clip)
            .children(
                [
                    box()
                    .inset(0)
                    .fill(self.image)
                    .filter(Filter.blur(0.65).then(halation).then(transfer))
                    .opacity(self.exposure),
                    box()
                    .inset(0)
                    .opacity(self.opening)
                    .children(
                        [
                            title("P A S S A G E", 368, 74, 8),
                            title("A CAMERA MOVEMENT WRITTEN IN LIGHT", 477, 15, 3.8),
                        ]
                    ),
                    box()
                    .inset(0)
                    .opacity(self.closing)
                    .children(
                        [
                            title("P A S S A G E", 398, 33, 6),
                            title("AN OPTICAL STUDY", 456, 12, 3.2),
                        ]
                    ),
                ]
            )
        )

    def update(self, elapsed, ctx):
        t = elapsed % DURATION
        self.elapsed.set(t)
        self.travel.set(max(0.0, min(24.0, t - 2.5)))
        self.exposure.set(ease(t, 1.8, 4.0) * (1.0 - ease(t, 25.0, 26.0)))
        self.opening.set(ease(t, 0.2, 1.2) * (1.0 - ease(t, 2.8, 4.2)))
        self.closing.set(ease(t, 26.0, 26.5) * (1.0 - ease(t, 27.3, 27.95)))
