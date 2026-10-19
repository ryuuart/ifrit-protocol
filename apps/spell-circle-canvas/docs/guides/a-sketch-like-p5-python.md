# A sketch like p5.js, in Python

A p5.js sketch is a file of functions: `setup` runs once, `draw` runs
every frame, and `width`, `mouseX` or `frameCount` are simply there to
read. A Python sketch can be written the same way — no class, no
decorator, no context to thread through — and an HTML-shaped page is the
same idea turned around: one function that returns the whole tree.

This guide reads two files: `apps/grimoire/sketches/python_p5_bounce.py`,
balls that bounce and multiply where you click, and
`apps/grimoire/sketches/python_p5_page.py`, a page described as a
function of time.

## The whole bouncing file

```python
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
```

## Run it

```sh
sigil open apps/grimoire/sketches/python_p5_bounce.py
sigil render apps/grimoire/sketches/python_p5_bounce.py -o bounce.png
```

The first opens a window that reloads on every save; the second takes the
still at the moment the file declared. Neither needs the file to name
itself: with no `NAME`, it is listed under its stem.

## What made it a sketch

The file defines `setup` and `draw` at its top level. That is the whole
declaration: the loader wraps the module's functions into the same class
the `@sketch` decorator makes, so the host runs it exactly as it runs a
class. The four functions it looks for are `setup()`, `draw(pen)`,
`update(elapsed)` and `describe(seconds)`; the file needs at least one of
`setup`, `draw` and `describe`.

`size(640, 360)` and `captureAt(2.0)` declare the canvas and the moment a
still is taken. They can stand at the top of the file, as here, or inside
`setup`, where p5 calls `createCanvas`. `background(color)` and
`theme(look)` declare the rest. Inside `draw`, the pen paints the ground
instead — `pen.background("#121720")` — and a declaration there is
refused, because the canvas is already drawing.

## The names that are simply there

`from sigil.sketch.globals import *` brings in the declarations and the
readings. Before every hook, the host writes the readings into the
sketch's own module, so `draw` reads `seconds` and `mousePressed` reads
`mouseX` as plain names:

| Name | What it holds |
| --- | --- |
| `width`, `height` | the canvas size |
| `frameCount` | frames drawn since the sketch began |
| `seconds` | scene seconds since the sketch began |
| `deltaTime` | milliseconds the last frame took |
| `mouseX`, `mouseY` | where the pointer stands |
| `mouseIsPressed` | whether its button is down |
| `key`, `keyCode` | the last key pressed |
| `keyIsPressed` | whether any key is down |

The balls move by `seconds`, not by frames: `draw` steps the simulation
in fixed slices until it has caught up with the scene clock. A window
that drops a frame and a still taken at two seconds therefore show the
same balls in the same places.

## Events

`mousePressed()` is called before the `draw` of the first frame the
button is down; `mouseReleased()`, `mouseMoved()`, `mouseDragged()`,
`keyPressed()` and `keyReleased()` are called the same way, from what
changed since the frame before. The pointer and the keys are read from
the pen `draw` is handed, so a sketch has to draw to hear them.

## The page: HTML's half

A web page has no `draw` loop; it is a tree that says what is there. The
second file is that shape — `describe(seconds)` returns the page at a
scene time, and the host draws it again at every step, patching only what
changed:

```python
"""A page written the way HTML writes one: a function that returns the tree.

The file has no class and no decorator. describe(seconds) returns the page
at a scene time, and the host draws it again at every step, patching only
what changed.

EDIT THESE FIRST
  STEPS — the list the page reads out.
  describe() — the tree itself.

TAGS: Runtime/Starter, Typography/Documents
"""

from sigil.compose import Element, box, document
from sigil.sketch import kit
from sigil.sketch.globals import *

STEPS = [
    "Write a function that returns a tree.",
    "Declare the canvas with size() and a theme().",
    "Save the file; the page redraws.",
]

size(900, 460)
theme(kit.featureTheme(kit.Density.Spacious))
captureAt(1.0)


def describe(seconds: float) -> Element:
    filled = min(seconds / 1.5, 1.0)
    return kit.page(
        document.article(
            document.h2("A page is a function of time."),
            document.paragraph(
                "Like a document in a browser, the tree is the whole program: "
                "boxes, text and lists, laid out and drawn natively."
            ),
            document.list(*(document.item(step) for step in STEPS)),
            box(box().width(f"{filled * 100}%").height(8).fill("#8bd0bd"))
            .height(8)
            .borderRadius(4)
            .fill("#ffffff22")
            .overflow("clip"),
        ).gap(14),
        title="Hello, page.",
        subtitle="describe(seconds) returns the page at a scene time.",
    )
```

`theme(...)` is provided around every hook and stages the canvas with the
theme's ground, so the kit's page reads its type and colours from it. The
bar's width is the only thing that moves; the rest of the tree is the
same at every step, and is kept.

A `describe` annotated `-> world.Frame` instead of `-> Element` makes the
file a scene3d sketch, drawn on the device; the kind is read from that
annotation in the text, before the file is ever run.

## When to reach for a class

The file of functions is the first tier. A class decorated with
`@sketch(size=..., captureAt=..., theme=...)` holds the same hooks as
methods, keeps its state on `self` rather than in module names, and is
what to write when one file holds helpers that would otherwise be read as
hooks. A file that has both runs its class, and the loader warns that the
functions are not hooks. [Write a sketch in Python](guide:a-sketch-in-python)
starts from the class form.
