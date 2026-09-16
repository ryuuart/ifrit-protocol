"""Gray-Scott simulation in NumPy, drawn through the native image and pen APIs.

Requires the optional ``studies`` package extra.
"""

# TAGS: Drawing/Generative, Patterns/Simulation

import numpy as np
from sigil.draw import Pen
from sigil.image import from_rgba
from sigil.sketch import SketchContext, sketch

REQUIRES = ("numpy",)


def laplace(field):
    return (
        -field[1:-1, 1:-1]
        + np.float32(0.2)
        * (field[1:-1, :-2] + field[1:-1, 2:] + field[:-2, 1:-1] + field[2:, 1:-1])
        + np.float32(0.05)
        * (field[:-2, :-2] + field[:-2, 2:] + field[2:, :-2] + field[2:, 2:])
    )


@sketch(size=(720, 720), capture_at=5)
class ReactionDiffusion:
    def setup(self, ctx: SketchContext) -> None:
        self.a = np.ones((144, 144), dtype=np.float32)
        self.b = np.zeros_like(self.a)
        self.b[56:88, 56:88] = 1
        self.b[30:42, 94:106] = 1
        self.pixels = np.full((144, 144, 4), 255, dtype=np.uint8)

    def advance(self):
        a, b = self.a[1:-1, 1:-1], self.b[1:-1, 1:-1]
        reaction = a * b * b
        next_a = np.clip(
            a + laplace(self.a) - reaction + np.float32(0.055) * (1 - a), 0, 1
        )
        next_b = np.clip(
            b + np.float32(0.5) * laplace(self.b) + reaction - np.float32(0.117) * b,
            0,
            1,
        )
        a[:] = next_a
        b[:] = next_b

    def draw(self, pen: Pen) -> None:
        for _ in range(5):
            self.advance()
        shade = np.clip((self.a - self.b) * np.float32(255), 0, 255).astype(np.uint8)
        self.pixels[:, :, :3] = shade[:, :, None]
        pen.background(0)
        pen.image(from_rgba(self.pixels, 144, 144), 0, 0, pen.width, pen.height)
