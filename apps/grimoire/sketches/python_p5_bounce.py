"""Bouncing balls written the way p5.js writes a sketch: functions, no class.

Click anywhere to drop another ball. Each one falls, bounces off the floor
and the walls, and loses a little speed at every bounce.

EDIT THESE FIRST
  GRAVITY — how fast a ball falls, in canvas units per second squared.
  BOUNCE — how much speed a ball keeps when it meets an edge.
  COLORS — the inks the balls take in turn.

TAGS: Runtime/Starter, Motion/Simulation
"""

import random

from sigil.draw import Pen
from sigil.sketch.globals import *

GRAVITY = 900
BOUNCE = 0.82
COLORS = ["#f1bb7b", "#8bd0bd", "#e58f84", "#9fb6e8"]
STEP = 1 / 240

size(640, 360)
captureAt(2.0)

balls = []
simulated = 0.0


def addBall(x, y):
    balls.append(
        {
            "x": x,
            "y": y,
            "speedX": random.uniform(-260, 260),
            "speedY": random.uniform(-420, -120),
            "diameter": random.uniform(18, 46),
            "color": COLORS[len(balls) % len(COLORS)],
        }
    )


def setup():
    random.seed(7)
    for index in range(6):
        addBall(width * (index + 1) / 7, height / 3)


def move(ball):
    radius = ball["diameter"] / 2
    ball["speedY"] += GRAVITY * STEP
    ball["x"] += ball["speedX"] * STEP
    ball["y"] += ball["speedY"] * STEP
    if ball["y"] > height - radius:
        ball["y"] = height - radius
        ball["speedY"] *= -BOUNCE
    if ball["x"] < radius or ball["x"] > width - radius:
        ball["x"] = min(max(ball["x"], radius), width - radius)
        ball["speedX"] *= -BOUNCE


def draw(pen: Pen):
    global simulated
    while simulated < seconds:
        for ball in balls:
            move(ball)
        simulated += STEP
    pen.background("#121720")
    pen.noStroke()
    for ball in balls:
        pen.fill(ball["color"])
        pen.circle(ball["x"], ball["y"], ball["diameter"])


def mousePressed():
    addBall(mouseX, mouseY)
