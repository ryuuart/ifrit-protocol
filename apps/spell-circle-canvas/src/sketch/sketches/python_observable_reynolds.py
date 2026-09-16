"""Fifty boids combine separation, alignment and cohesion, with retained trails.

TAGS: Drawing/Generative, Motion/Physics
"""

from cmath import phase
from math import cos, pi

from sigil.draw import CLOSE, HSB, Pen
from sigil.sketch import SketchContext, sketch


def limited(vector, maximum):
    return vector * min(1, maximum / abs(vector)) if vector else vector


def steer(desired, velocity):
    direction = desired / abs(desired) if abs(desired) > 0.0001 else 0j
    return limited(direction * 8 - velocity, 0.05)


@sketch(size=(900, 720), capture_at=5)
class ReynoldsSteering:
    def setup(self, ctx: SketchContext) -> None:
        self.boids = []

    def advance(self, width, height):
        forces = []
        for index, (position, velocity) in enumerate(self.boids):
            separation = alignment = centre = 0j
            close = nearby = 0
            for other, (other_position, other_velocity) in enumerate(self.boids):
                if index == other:
                    continue
                delta = position - other_position
                distance = abs(delta)
                if 0 < distance < 25:
                    separation += delta / (distance * distance)
                    close += 1
                if distance < 50:
                    alignment += other_velocity
                    centre += other_position
                    nearby += 1
            force = steer(separation / close, velocity) if close else 0j
            if nearby:
                force += steer(alignment / nearby, velocity)
                force += steer(centre / nearby - position, velocity)
            forces.append(force)
        updated = []
        for (position, velocity), force in zip(self.boids, forces):
            velocity = limited(velocity + force, 8)
            position += velocity
            x, y = position.real, position.imag
            x = width + 20 if x < -20 else -20 if x > width + 20 else x
            y = height + 20 if y < -20 else -20 if y > height + 20 else y
            updated.append((complex(x, y), velocity))
        self.boids = updated

    def draw(self, pen: Pen) -> None:
        if pen.frameCount == 1:
            pen.randomSeed(0xC2A16)
            pen.colorMode(HSB, 360, 100, 100, 255)
            self.boids = [
                (
                    complex(pen.width / 2, pen.height / 2),
                    complex(pen.random(-1, 1), pen.random(-1, 1)),
                )
                for _ in range(50)
            ]
            pen.background(0)
        pen.background(0, 1)
        self.advance(pen.width, pen.height)
        pen.noFill()
        pen.stroke((180 + cos(pen.millis() / 1000 * 0.06) * 180 + 360) % 360, 100, 100)
        pen.strokeWeight(1)
        for position, velocity in self.boids:
            pen.push()
            pen.translate(position.real, position.imag)
            pen.rotate(phase(velocity) + pi / 2)
            pen.beginShape()
            for x, y in ((0, -40), (-20, 40), (20, 40)):
                pen.vertex(x, y)
            pen.endShape(CLOSE)
            pen.pop()
