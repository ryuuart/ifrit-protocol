"""A bracketed Lindenmayer tree with a slowly moving branch angle.

TAGS: Drawing/Generative, Patterns/Ornament
"""

from math import cos, pi, radians, sin

from sigil.draw import ROUND, Pen
from sigil.sketch import SketchContext, sketch


@sketch(size=(800, 800), capture_at=0.05)
class LSystemTree:
    def setup(self, ctx: SketchContext) -> None:
        self.sentence = "F"
        for _ in range(4):
            self.sentence = self.sentence.replace("F", "FF+[+F-F-F]-[-F+F+F]")

    def draw(self, pen: Pen) -> None:
        pen.background(51)
        pen.noFill()
        pen.strokeCap(ROUND)
        turn = radians(25 + 3 * sin(pen.millis() / 1000 * 0.55))
        x, y, angle = 0.0, 0.0, -pi / 2
        stack: list[tuple[float, float, float]] = []
        branches: list[tuple[float, float, float, float, float]] = []
        left = top = right = bottom = 0.0
        for index, symbol in enumerate(self.sentence):
            if symbol == "F":
                nx, ny = x + cos(angle), y + sin(angle)
                branches.append((x, y, nx, ny, 100 + 155 * index / len(self.sentence)))
                left, top = min(left, nx), min(top, ny)
                right, bottom = max(right, nx), max(bottom, ny)
                x, y = nx, ny
            elif symbol == "+":
                angle += turn
            elif symbol == "-":
                angle -= turn
            elif symbol == "[":
                stack.append((x, y, angle))
            elif symbol == "]":
                x, y, angle = stack.pop()
        scale = min(
            (pen.width - 64) / (right - left), (pen.height - 64) / (bottom - top)
        )
        pen.push()
        pen.translate(
            (pen.width - (left + right) * scale) / 2,
            (pen.height - (top + bottom) * scale) / 2,
        )
        pen.scale(scale)
        pen.strokeWeight(1 / scale)
        for x, y, nx, ny, shade in branches:
            pen.stroke(shade)
            pen.line(x, y, nx, ny)
        pen.pop()
