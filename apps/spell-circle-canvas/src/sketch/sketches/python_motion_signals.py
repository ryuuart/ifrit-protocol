"""A retained signal desk driven by one shared native clock value.

The tracks are native binding chains. The entrance is a native keyframe path;
Python assembles the components once and advances the source on the scene ticker.
TAGS: Motion/Animation, Drawing/Generative, Typography/Interface
"""

from sigil.compose import box, column, row, text
from sigil.motion import Output, Transition, animate, bind, ease, from_, through
from sigil.sketch import sketch

WIDTH = 860
TRACK = 610
INK = "#d9e3df"
MUTED = "#849894"


def signal_row(number, title, detail, signal, accent):
    marks = [
        box(
            width=1,
            height=22 if index % 2 == 0 else 12,
            fill="#334945",
            absolute=True,
            left=index * TRACK / 10,
            top=11,
        )
        for index in range(11)
    ]
    track = box(
        *marks,
        box(width=TRACK, height=1, fill="#435851", absolute=True, left=0, top=22),
        box(
            width=30,
            height=30,
            fill=accent,
            corners=8,
            absolute=True,
            left=0,
            top=7,
            translate_x=signal.copy().target(0, TRACK - 30),
            rotate=signal.copy().target(-20, 20),
        ),
        width=TRACK,
        height=48,
    )
    return row(
        column(
            text(f"0{number}", size=12, color=accent),
            text(title, size=17, color=INK),
            text(detail, size=11, color=MUTED),
            gap=5,
            width=168,
        ),
        track,
        gap=18,
        align_items="center",
        padding=(16, 20),
        width=WIDTH,
        fill="#162621" if number % 2 else "#13221e",
        corners=12,
    )


@sketch(size=(960, 800), background="#0b1713", capture_at=1.1)
class MotionSignals:
    def setup(self, ctx):
        seconds = Output(0)
        ctx.ticker.add(lambda dt, elapsed: seconds.set(elapsed))
        folded = Output(0)
        ctx.ticker.derive(folded, bind(seconds).scale(0.2).wrap(1))
        cycle = bind(seconds).source(0, 5)
        rows = [
            signal_row(
                1, "TRAVERSE", "source → triangle", cycle.copy().pingPong(), "#b7dd93"
            ),
            signal_row(
                2, "BREATHE", "source → cosine", cycle.copy().cosine(), "#77d6bc"
            ),
            signal_row(
                3,
                "POSTERIZE",
                "triangle → 8 levels",
                cycle.copy().pingPong().quantize(8),
                "#85c5e4",
            ),
            signal_row(
                4,
                "OVERSHOOT",
                "triangle → outBack",
                cycle.copy().pingPong().map(ease.outBack()),
                "#baa2e0",
            ),
            signal_row(
                5,
                "GATE",
                "fold → rise / hold / fall",
                bind(folded).trapezoid(0, 0.2, 0.7, 1),
                "#eab881",
            ),
            signal_row(
                6,
                "DRIFT",
                "triangle + seeded wiggle",
                cycle.copy().pingPong().wiggle(0.08, 4, 23),
                "#e78e87",
            ),
        ]
        entrance = animate(
            through([(0, 32), (0.45, -5), (0.75, 0)]), ease=ease.outCubic
        )
        header = row(
            column(
                text("MOTION / SIGNAL DESK", size=12, color="#96bd9e"),
                text("One clock. Six interpretations.", size=31, color=INK),
                gap=9,
            ),
            box(
                text("NATIVE\nCLOCK", size=13, color="#b7dd93"),
                padding=12,
                corners=8,
                fill="#1b3124",
            ),
            justify="space_between",
            align_items="center",
            width=WIDTH,
        )
        footer = row(
            text("RETAINED COMPONENTS", size=11, color=MUTED),
            text("OUTPUT → BIND → PROPERTY", size=11, color="#a8bcae"),
            text("PERIOD  5.0 s", size=11, color=MUTED),
            width=WIDTH,
            justify="space_between",
        )
        ctx.render(
            column(
                header,
                column(rows, gap=9, width=WIDTH),
                footer,
                width=WIDTH,
                gap=25,
                absolute=True,
                left=50,
                top=45,
                translate_y=entrance,
                opacity=animate(from_(0).to(1), Transition(0.5, ease.outQuad)),
            )
        )
