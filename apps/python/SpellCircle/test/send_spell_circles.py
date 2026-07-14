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
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from SpellCircle import SpellCircleCanvas, send_once

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


def _scaled(value: float, canvas_size: float) -> float:
    """Scale a value defined against the default 4000-unit canvas to `canvas`."""
    return value * canvas_size / 4000.0


def build_scene(
    canvas_size: float = CANVAS,
    small_circle_count: int = 5,
    box_count: int = 3,
) -> tuple[bytes, int, int, int]:
    """Builds one randomized scene and returns bytes plus primitive counts."""
    scene_canvas = SpellCircleCanvas(canvas_size, canvas_size)

    canvas_center = canvas_size / 2.0
    central_circle = scene_canvas.circle(
        "FLARE SIGIL CIRCLE",
        canvas_center,
        canvas_center,
        round(_scaled(1150, canvas_size)),
        text_start=0.62,
    )

    # Smaller circles placed at random positions/radii, kept inside the canvas
    # and clear of the central circle.
    margin = _scaled(120, canvas_size)
    small_circles = []
    selected_names = random.sample(
        SMALL_NAMES, min(small_circle_count, len(SMALL_NAMES))
    )
    for name in selected_names:
        radius = _scaled(random.uniform(220, 440), canvas_size)
        angle = random.uniform(0, 2 * math.pi)
        center_distance = random.uniform(
            central_circle.radius + radius * 0.4,
            canvas_center - radius - margin,
        )
        x_position = min(
            max(
                canvas_center + center_distance * math.cos(angle),
                radius + margin,
            ),
            canvas_size - radius - margin,
        )
        y_position = min(
            max(
                canvas_center + center_distance * math.sin(angle),
                radius + margin,
            ),
            canvas_size - radius - margin,
        )
        small_circles.append(scene_canvas.circle(
            name, x_position, y_position, round(radius),
            active=random.random(),
        ))

    # Star web on the big circle. Points here are pure edge endpoints with
    # nothing worth labelling, so they carry no value.
    ring = 12
    for point_index in range(ring):
        for skip in (5, 7):
            first_point = scene_canvas.point(
                central_circle, point_index / ring
            )
            second_point = scene_canvas.point(
                central_circle, ((point_index + skip) % ring) / ring
            )
            scene_canvas.edge(first_point, second_point)

    # Spoke from each small circle to its nearest point on the big circle.
    for small_circle in small_circles:
        angle_fraction = math.atan2(
            small_circle.center_y - canvas_center,
            small_circle.center_x - canvas_center,
        ) / (2 * math.pi)
        central_circle_position = (angle_fraction + 1.0 + 0.25) % 1.0
        central_point = scene_canvas.point(
            central_circle, central_circle_position
        )
        small_circle_point = scene_canvas.point(small_circle, 0.5)
        scene_canvas.edge(central_point, small_circle_point)

    # Boxes at random positions on the big circle. The anchor point carries
    # no value of its own — the box already draws `name` — to avoid stacking
    # an identical label on top of the box.
    selected_box_names = random.sample(
        BOX_NAMES, min(box_count, len(BOX_NAMES))
    )
    for name in selected_box_names:
        anchor_point = scene_canvas.point(central_circle, random.random())
        scene_canvas.box(name, anchor_point, active=random.random())

    scene_bytes = scene_canvas.to_bytes()
    return (
        scene_bytes,
        len(scene_canvas.circles),
        len(scene_canvas.edges),
        len(scene_canvas.boxes),
    )


def main() -> None:
    """Parses command-line options, builds a scene, and sends it once."""
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
    arguments = parser.parse_args()

    if arguments.seed is not None:
        random.seed(arguments.seed)
    scene_bytes, circle_count, edge_count, box_count = build_scene(
        canvas_size=arguments.canvas,
        small_circle_count=arguments.circles,
        box_count=arguments.boxes,
    )

    send_once(scene_bytes, arguments.host, arguments.port)

    print(
        f"Sent SpellCircle ({circle_count} circles, {edge_count} edges, "
        f"{box_count} boxes, {len(scene_bytes)} bytes) -> "
        f"{arguments.host}:{arguments.port}"
    )


if __name__ == "__main__":
    main()
