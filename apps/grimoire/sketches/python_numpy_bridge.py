"""The NumPy bridge: arrays computed in NumPy handed to the native drawing in
one call each, with no per-element Python on the drawing side.

Left, a sine-composed angle field read at 14,000 seeded samples: NumPy turns
the field into one packed array of line segments and `drawPoints` strokes
the whole batch. Right, a Gray–Scott reaction stepped in NumPy at a fixed
rate on the session's clock, its shade written into an RGBA array and drawn
through `media.fromRgba`, sampled nearest so every cell stays a cell.

Requires the optional ``studies`` package extra.

EDIT THESE FIRST: SAMPLES, REACH, FEED, KILL, STEPS_PER_SECOND.

TAGS: Drawing/Generative, Patterns/Simulation, Python
"""

from math import tau

import numpy as np
from sigil import compose
from sigil.core import chance
from sigil.draw import SQUARE, Pen, PointMode
from sigil.media import fromRgba
from sigil.sketch import Scene2DContext, sketch

REQUIRES = ("numpy",)

WIDTH, HEIGHT = 1280, 720
FIELD = 520  # each panel's side, in pixels
GROUND = "#0b0d10"
INK = "#e9ecef"
QUIET = "#88909a"
MONO = "Menlo, SF Mono, monospace"

# The flow field: how many samples, and how far each segment reaches.
SAMPLES = 14_000
REACH = 20

# The reaction: a grid of cells a quarter of the panel's side, the feed and
# kill rates, and how many steps the clock takes each second, so a still at
# a moment is the same picture at any frame rate.
CELLS = FIELD // 4
FEED = np.float32(0.055)
KILL = np.float32(0.117)
STEPS_PER_SECOND = 240


def laplace(field: np.ndarray) -> np.ndarray:
    """The nine-point Laplacian of a field's interior."""
    return (
        -field[1:-1, 1:-1]
        + np.float32(0.2)
        * (field[1:-1, :-2] + field[1:-1, 2:] + field[:-2, 1:-1] + field[2:, 1:-1])
        + np.float32(0.05)
        * (field[:-2, :-2] + field[:-2, 2:] + field[2:, :-2] + field[2:, 2:])
    )


def label(content: str, size: float = 12, color: str = QUIET) -> compose.Text:
    return compose.text(content, size=size, color=color).fontFamily(MONO).letterSpacing(1.5)


def panel(title: str, detail: str, leaf: compose.Element) -> compose.Element:
    return compose.box(label(title, 12, INK), leaf, label(detail, 11)).column().gap(10)


@sketch(size=(WIDTH, HEIGHT), background=GROUND, captureAt=5.0)
class NumpyBridge:
    def setup(self, ctx: Scene2DContext) -> compose.Element:
        # THE FLOW FIELD'S SAMPLES, seeded so every run places them alike,
        # and the segment array the field writes into: every even row a
        # sample, every odd row where its segment ends.
        self.samples = np.array(
            [
                (
                    chance.Stream.mix64(index * 2 + 0x6BCECD).unit() * FIELD,
                    chance.Stream.mix64(index * 2 + 0x883A00).unit() * FIELD,
                )
                for index in range(SAMPLES)
            ]
        )
        self.segments = np.empty((SAMPLES * 2, 2), dtype=np.float32)
        self.segments[::2] = self.samples

        # THE REACTION: two concentrations, two seeded squares of the second,
        # and the RGBA array the shade is written into.
        self.first = np.ones((CELLS, CELLS), dtype=np.float32)
        self.second = np.zeros_like(self.first)
        self.second[50:80, 50:80] = 1
        self.second[27:38, 85:96] = 1
        self.pixels = np.full((CELLS, CELLS, 4), 255, dtype=np.uint8)
        ctx.engine.timer(self.step, stepRate=STEPS_PER_SECOND)

        return (
            compose.box(
                compose.text("NUMPY IN, NATIVE OUT", size=30, color=INK).letterSpacing(5),
                compose.box(
                    panel(
                        f"LINES · {SAMPLES:,} SEGMENTS · ONE drawPoints",
                        "an angle field evaluated as arrays every frame",
                        compose.pen("numpy_bridge.flow", self.flow)
                        .width(FIELD)
                        .height(FIELD)
                        .overflow(compose.Overflow.Clip),
                    ),
                    panel(
                        f"PIXELS · {CELLS} × {CELLS} CELLS · ONE fromRgba",
                        f"Gray–Scott at {STEPS_PER_SECOND} steps a second, sampled nearest",
                        compose.pen("numpy_bridge.reaction", self.reaction)
                        .width(FIELD)
                        .height(FIELD),
                    ),
                )
                .row()
                .gap(48),
            )
            .cover()
            .padding(36, 96)
            .column()
            .gap(24)
            .fill(GROUND)
        )

    def step(self) -> None:
        """One step of the reaction, on the session's clock."""
        first, second = self.first[1:-1, 1:-1], self.second[1:-1, 1:-1]
        reaction = first * second * second
        next_first = np.clip(
            first + laplace(self.first) - reaction + FEED * (1 - first), 0, 1
        )
        next_second = np.clip(
            second + np.float32(0.5) * laplace(self.second) + reaction - KILL * second,
            0,
            1,
        )
        first[:] = next_first
        second[:] = next_second

    def flow(self, pen: Pen) -> None:
        pen.background(0)
        pen.noFill()
        pen.strokeCap(SQUARE)
        pen.stroke(255, 190)
        pen.strokeWeight(0.55)
        seconds = pen.millis() / 1000
        x, y = self.samples.T
        angle = (np.sin(x * 0.01 + seconds * 0.22) + np.sin(y * 0.01 - seconds * 0.19)) * tau
        self.segments[1::2, 0] = x + np.cos(angle) * REACH
        self.segments[1::2, 1] = y + np.sin(angle) * REACH
        pen.canvas().drawPoints(PointMode.Lines, self.segments, pen.strokePaint())

    def reaction(self, pen: Pen) -> None:
        shade = np.clip((self.first - self.second) * np.float32(255), 0, 255).astype(np.uint8)
        self.pixels[:, :, :3] = shade[:, :, None]
        pen.noSmooth()
        pen.image(fromRgba(self.pixels, CELLS, CELLS), 0, 0, pen.width, pen.height)
