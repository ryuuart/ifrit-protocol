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
        pen.strokeWeight(1)
        turn = radians(25 + 3 * sin(pen.millis() / 1000 * 0.55))
        x, y, angle = pen.width / 3, pen.height, -pi / 2
        stack = []
        for index, symbol in enumerate(self.sentence):
            if symbol == "F":
                nx, ny = x + cos(angle) * 15.625, y + sin(angle) * 15.625
                pen.stroke(100 + 155 * index / len(self.sentence))
                pen.line(x, y, nx, ny)
                x, y = nx, ny
            elif symbol == "+":
                angle += turn
            elif symbol == "-":
                angle -= turn
            elif symbol == "[":
                stack.append((x, y, angle))
            elif symbol == "]":
                x, y, angle = stack.pop()
