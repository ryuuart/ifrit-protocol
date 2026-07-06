#!/usr/bin/env python3
"""
Send a single SpellCircle FlatBuffer scene via UDP.

Builds a screen-filling sigil: a large central circle whose perimeter points
are webbed together by edges, several randomly placed smaller circles around
the canvas, edges connecting them, and labelled boxes anchored to points.

The scene is authored in a logical canvas of `--canvas` units (square). The
app records this size and scales the geometry up to its native 4K texture, so
a small authoring canvas (e.g. 100) still renders at full resolution.

Usage:
    python send_spell_circles.py [--host HOST] [--port PORT] [--seed N]
                                 [--canvas UNITS] [--circles N] [--boxes N]

Requires: pip install flatbuffers
"""

import argparse
import math
import random

from sc_canvas import SCCanvas
from sc_network import send_once

HOST = "127.0.0.1"
PORT = 27015
CANVAS = 4000

SMALL_NAMES = [
    "IGNIS", "AQUA", "TERRA", "VENTUS", "UMBRA", "LUX", "GLACIES", "FULGUR",
    "SILVA", "MARE", "AETHER", "CHAOS", "ORDO", "NOX", "AURORA",
]
BOX_NAMES = [
    "ANGER", "FAITH", "SADNESS", "JOY", "FEAR", "HOPE", "WRATH", "CALM",
    "GREED", "VALOR",
]


def _scaled(value, canvas):
    """Scale a value defined against the default 4000-unit canvas to `canvas`."""
    return value * canvas / 4000.0


def build_scene(canvas=CANVAS, n_smalls=5, n_boxes=3):
    sc = SCCanvas(canvas, canvas)

    c = canvas / 2.0
    big = sc.circle(
        "FLARE SIGIL CIRCLE",
        c, c,
        round(_scaled(1150, canvas)),
        text_start=0.62,
    )

    # Smaller circles placed at random positions/radii, kept inside the canvas
    # and clear of the central circle.
    margin = _scaled(120, canvas)
    smalls = []
    for name in random.sample(SMALL_NAMES, min(n_smalls, len(SMALL_NAMES))):
        radius = _scaled(random.uniform(220, 440), canvas)
        ang = random.uniform(0, 2 * math.pi)
        dist = random.uniform(big.radius + radius * 0.4, c - radius - margin)
        x = min(max(c + dist * math.cos(ang), radius + margin), canvas - radius - margin)
        y = min(max(c + dist * math.sin(ang), radius + margin), canvas - radius - margin)
        smalls.append(sc.circle(
            name, x, y, round(radius),
            active=random.random(),
        ))

    # Star web on the big circle. Points here are pure edge endpoints with
    # nothing worth labelling, so they carry no value.
    ring = 12
    for i in range(ring):
        for skip in (5, 7):
            a = sc.point(big, i / ring)
            b = sc.point(big, ((i+skip)%ring)/ring)
            sc.edge(a, b)

    # Spoke from each small circle to its nearest point on the big circle.
    for s in smalls:
        ang = math.atan2(s.y - c, s.x - c) / (2 * math.pi)
        big_pos = (ang + 1.0 + 0.25) % 1.0
        a = sc.point(big, big_pos)
        b = sc.point(s, 0.5)
        sc.edge(a, b)

    # Boxes at random positions on the big circle. The anchor point carries
    # no value of its own — the box already draws `name` — to avoid stacking
    # an identical label on top of the box.
    for name in random.sample(BOX_NAMES, min(n_boxes, len(BOX_NAMES))):
        pt = sc.point(big, random.random())
        sc.box(name, pt, active=random.random())

    buf = sc.to_bytes()
    return buf, len(sc.circles), len(sc.edges), len(sc.boxes)


def main():
    parser = argparse.ArgumentParser(description="Send a SpellCircle FlatBuffer via UDP")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", default=PORT, type=int)
    parser.add_argument("--seed", default=None, type=int,
                        help="RNG seed for reproducible layouts (default: random)")
    parser.add_argument("--canvas", default=CANVAS, type=float,
                        help="authoring canvas size in logical units (default: 4000)")
    parser.add_argument("--circles", default=5, type=int,
                        help="number of randomly placed small circles")
    parser.add_argument("--boxes", default=3, type=int,
                        help="number of randomly placed labelled boxes")
    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
    buf, n_circles, n_edges, n_boxes = build_scene(
        canvas=args.canvas, n_smalls=args.circles, n_boxes=args.boxes
    )

    send_once(buf, args.host, args.port)

    print(
        f"Sent SpellCircle ({n_circles} circles, {n_edges} edges, {n_boxes} boxes, "
        f"{len(buf)} bytes) -> {args.host}:{args.port}"
    )


if __name__ == "__main__":
    main()
