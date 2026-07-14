#!/usr/bin/env python3
"""
Stream an animated SpellCircle scene over UDP at ~60 fps.

Five circles orbit a central sigil at varying speeds (some clockwise, some
counter-clockwise). Their radii pulse, and a star web of edges connects
everything while labelled boxes travel along the perimeter of the central
circle. A couple of extra boxes are pinned to one of the two points already
assigned to a chosen edge, so they follow that point wherever the edge's
endpoint goes. Every circle and box holds its active/inactive fill state for
about a second between smooth transitions so the change reads clearly instead
of flickering, except the last orbiter (UMBRA), which keeps a fast
sine-driven flash for contrast. The first orbiter's spoke endpoint on the big
circle also carries its own value label (drawn like a box) tracking its live
fractional position [0, 1] on the ring.

A box (DRIFTER) and a point (STRAY) each lap a rectangular racetrack near the
canvas edges — straight sides and sharp corners, deliberately unlike anything
circular in the rest of the scene — anchored to their own radius-0 "anchor"
circle (see the Circle.radius doc in SpellCircle.fbs) rather than to any
visible circle's perimeter, demonstrating that boxes/points can be placed at
an arbitrary (x, y).

The scene is authored in a 1000×1000 coordinate space so coordinates are easy
to reason about; the app scales it up to the native 4K texture automatically.

Usage:
    python animate_spell_circles.py [--host HOST] [--port PORT] [--fps N]

Requires: pip install flatbuffers
"""

import argparse
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from SpellCircle import SceneSender, SpellCircleCanvas

HOST = "127.0.0.1"
PORT = 27015
CANVAS = 1000.0
CANVAS_CENTER_X = CANVAS / 2.0
CANVAS_CENTER_Y = CANVAS / 2.0

# Shrinks the whole layout relative to the fixed CANVAS (rather than shrinking
# CANVAS itself, which wouldn't change anything — the app scales authored
# coordinates up to a fixed native texture size, so only the ratio of element
# size to CANVAS affects how much margin is left at the edges). More margin
# here means boxes, which anchor outward from the big circle, have room to
# sit fully on-canvas instead of spilling past its border.
FOOTPRINT_SCALE = 0.68
BIG_CIRCLE_RADIUS = 380.0 * FOOTPRINT_SCALE

# (orbit_radius, angular_speed rad/s, name, base_radius)
# Negative speed = counter-clockwise.
ORBITERS = [
    (220.0 * FOOTPRINT_SCALE,  0.80, "IGNIS",  58.0 * FOOTPRINT_SCALE),
    (280.0 * FOOTPRINT_SCALE,  0.50, "AQUA",   52.0 * FOOTPRINT_SCALE),
    (190.0 * FOOTPRINT_SCALE, -0.35, "TERRA",  62.0 * FOOTPRINT_SCALE),
    (260.0 * FOOTPRINT_SCALE,  0.65, "VENTUS", 48.0 * FOOTPRINT_SCALE),
    (235.0 * FOOTPRINT_SCALE, -0.55, "UMBRA",  55.0 * FOOTPRINT_SCALE),
]

# Boxes that drift along the big circle at different rates.
BOX_SPECS = [
    ("NEXUS",  0.13, 0.00),   # (name, drift_speed rad/s, initial_phase)
    ("ANCHOR", 0.08, 0.33),
    ("VOID",   0.11, 0.67),
]

# Star-web ring on the big circle.
RING = 10
RING_SKIPS = (4, 6)

# Boxes pinned to one of the two points already assigned to a chosen edge, so
# each box follows that same point (name, edge track, which end: 0 or 1).
# See build_frame() for how the "spoke" and "ring" tracks are resolved.
EDGE_RIDERS = [
    ("COURIER", "spoke", 0),
    ("HERALD",  "ring",  1),
]

# Free-roaming box/point: each laps its own rectangular racetrack
# (period seconds per lap, margin inset from the canvas edges, phase in
# [0, 1) of a lap, reverse direction). Sharp corners and straight edges read
# as unmistakably not-a-circle, unlike a Lissajous blob that still hovers
# near the sigil; different margins/periods/directions keep the two tracks
# visually distinct from each other too.
DRIFTER_TRACK = dict(period=14.0, margin=90.0, phase=0.0, reverse=False)
STRAY_TRACK = dict(period=19.0, margin=40.0, phase=0.5, reverse=True)


def _racetrack(
    elapsed_seconds: float,
    period: float,
    margin: float = 80.0,
    phase: float = 0.0,
    reverse: bool = False,
) -> tuple[float, float]:
    """Position moving at constant speed around a square perimeter inset
    `margin` from the canvas edges, visiting all four corners every lap."""
    minimum_x = margin
    minimum_y = margin
    side_length = CANVAS - 2.0 * margin
    perimeter = 4.0 * side_length

    progress = elapsed_seconds / period + phase
    if reverse:
        progress = -progress
    distance = (progress * perimeter) % perimeter

    if distance < side_length:
        return minimum_x + distance, minimum_y
    distance -= side_length
    if distance < side_length:
        return minimum_x + side_length, minimum_y + distance
    distance -= side_length
    if distance < side_length:
        return minimum_x + side_length - distance, minimum_y + side_length
    distance -= side_length
    return minimum_x, minimum_y + side_length - distance


def _position_on_central_circle(x_position: float, y_position: float) -> float:
    """Returns the central-circle arc fraction nearest a canvas position."""
    angle = math.atan2(
        y_position - CANVAS_CENTER_Y,
        x_position - CANVAS_CENTER_X,
    )
    return (angle / (2.0 * math.pi) + 0.25 + 1.0) % 1.0


def _ramp(value: float, threshold: float) -> float:
    """Map a sine `value` in [-1, 1] to a fill intensity [0, 1]: 0 below
    `threshold`, ramping up to 1 at the peak. Used for the one flashing
    circle (UMBRA) so there's still something quick to contrast against the
    held transitions everything else uses."""
    return max(0.0, (value - threshold) / (1.0 - threshold))


def _hold_pulse(
    elapsed_seconds: float,
    phase: float = 0.0,
    hold: float = 1.0,
    transition: float = 0.3,
) -> float:
    """Trapezoidal wave in [0, 1]: eases up over `transition` seconds, holds
    at 1 for `hold` seconds, eases down over `transition` seconds, then holds
    at 0 for `hold` seconds — long enough to actually see each state instead
    of flickering between them. `phase` (seconds) offsets where in the cycle
    `t` starts, so multiple objects using this don't pulse in lockstep."""
    period = 2.0 * (hold + transition)
    cycle_position = (elapsed_seconds + phase) % period
    if cycle_position < transition:
        fraction = cycle_position / transition
        return fraction * fraction * (3.0 - 2.0 * fraction)
    cycle_position -= transition
    if cycle_position < hold:
        return 1.0
    cycle_position -= hold
    if cycle_position < transition:
        fraction = cycle_position / transition
        return 1.0 - fraction * fraction * (3.0 - 2.0 * fraction)
    return 0.0


def build_frame(elapsed_seconds: float) -> bytes:
    """Builds the animated scene at ``elapsed_seconds``."""
    scene_canvas = SpellCircleCanvas(CANVAS, CANVAS)

    # Central circle — label drifts slowly.
    central_circle = scene_canvas.circle(
        "FLARE SIGIL CIRCLE",
        CANVAS_CENTER_X,
        CANVAS_CENTER_Y,
        round(BIG_CIRCLE_RADIUS),
        text_start=(0.62 + elapsed_seconds * 0.04) % 1.0,
    )

    # Orbiting circles.
    orbiter_count = len(ORBITERS)
    orbiting_circles = []
    for orbiter_index, (
        orbit_radius,
        angular_speed,
        name,
        base_radius,
    ) in enumerate(ORBITERS):
        phase = orbiter_index * (2.0 * math.pi / orbiter_count)
        angle = elapsed_seconds * angular_speed + phase
        center_x = CANVAS_CENTER_X + orbit_radius * math.cos(angle)
        center_y = CANVAS_CENTER_Y + orbit_radius * math.sin(angle)
        # Radius breathes on a per-circle frequency.
        radius = base_radius + 14.0 * FOOTPRINT_SCALE * math.sin(
            elapsed_seconds * 1.4 + orbiter_index * 1.1
        )
        # Every orbiter but the last holds its active/inactive state for a
        # beat so the transition reads clearly instead of flickering; the
        # last one (UMBRA) keeps the quick sine-driven flash for contrast.
        if orbiter_index == orbiter_count - 1:
            active = _ramp(
                math.sin(elapsed_seconds * 2.2 + orbiter_index * 1.3),
                0.68,
            )
        else:
            active = _hold_pulse(
                elapsed_seconds, phase=orbiter_index * 0.9
            )
        orbiting_circles.append(
            scene_canvas.circle(
                name,
                center_x,
                center_y,
                max(1, round(radius)),
                text_start=(
                    0.75 + elapsed_seconds * 0.09 + orbiter_index * 0.13
                )
                % 1.0,
                active=active,
            )
        )

    # Star web on the big circle. Points here are pure edge endpoints with
    # nothing worth labelling, so they carry no value.
    for point_index in range(RING):
        for skip in RING_SKIPS:
            first_point = scene_canvas.point(
                central_circle, point_index / RING
            )
            second_point = scene_canvas.point(
                central_circle, ((point_index + skip) % RING) / RING
            )
            scene_canvas.edge(first_point, second_point)

    # Spoke from each orbiter to the big circle. Points are kept in
    # `spoke_points` so a box can later be assigned the same Point as the
    # edge's endpoint, rather than creating a fresh one. The first spoke's
    # big-circle endpoint demonstrates a Point's own value label (rendered
    # like a box) by tracking its live fractional position on the ring; the
    # rest carry no value.
    spoke_points = []
    for orbiter_index, orbiting_circle in enumerate(orbiting_circles):
        central_position = _position_on_central_circle(
            orbiting_circle.center_x, orbiting_circle.center_y
        )
        value = f"{central_position:.2f}" if orbiter_index == 0 else ""
        central_point = scene_canvas.point(
            central_circle, central_position, value
        )
        orbiting_point = scene_canvas.point(orbiting_circle, 0.5)
        scene_canvas.edge(central_point, orbiting_point)
        spoke_points.append((central_point, orbiting_point))

    # Ring connecting adjacent orbiters. Points kept in `ring_points` for the
    # same reason as `spoke_points` above.
    ring_points = []
    for orbiter_index in range(orbiter_count):
        first_circle = orbiting_circles[orbiter_index]
        second_circle = orbiting_circles[
            (orbiter_index + 1) % orbiter_count
        ]
        first_point = scene_canvas.point(first_circle, 0.25)
        second_point = scene_canvas.point(second_circle, 0.75)
        scene_canvas.edge(first_point, second_point)
        ring_points.append((first_point, second_point))

    tracks = {"spoke": spoke_points[0], "ring": ring_points[0]}

    # Boxes drifting around the big circle. Each holds its active/inactive
    # state for a beat (see _hold_pulse) rather than flickering, staggered by
    # phase so they don't all transition in lockstep.
    for box_index, (name, drift_speed, phase) in enumerate(BOX_SPECS):
        position = (elapsed_seconds * drift_speed + phase) % 1.0
        active = _hold_pulse(
            elapsed_seconds, phase=box_index * 0.9 + 0.4
        )
        anchor_point = scene_canvas.point(central_circle, position)
        scene_canvas.box(name, anchor_point, active=active)

    # Boxes pinned to one of the points an edge is already assigned to, so
    # they follow that point wherever the edge's endpoint goes.
    for rider_index, (name, track_name, endpoint_index) in enumerate(
        EDGE_RIDERS
    ):
        anchor_point = tracks[track_name][endpoint_index]
        active = _hold_pulse(
            elapsed_seconds, phase=rider_index * 0.9 + 1.3
        )
        scene_canvas.box(name, anchor_point, active=active)

    # DRIFTER: a box anchored to a radius-0 circle lapping a rectangular
    # racetrack, unconnected to any visible circle's perimeter.
    drifter_x, drifter_y = _racetrack(elapsed_seconds, **DRIFTER_TRACK)
    drifter_anchor = scene_canvas.anchor(drifter_x, drifter_y)
    drifter_point = scene_canvas.point(drifter_anchor)
    scene_canvas.box(
        "DRIFTER",
        drifter_point,
        active=_hold_pulse(elapsed_seconds, phase=2.1),
    )

    # STRAY: a lone point (no box) lapping its own racetrack, labelled with
    # its live coordinates to make the arbitrary positioning visible. Points
    # only enter the scene's registry when referenced by an Edge or a Box
    # (see PointComponent's doc comment), so it rides a self-looped edge —
    # both ends the same Point — purely to register it; the edge is
    # zero-length and draws nothing.
    stray_x, stray_y = _racetrack(elapsed_seconds, **STRAY_TRACK)
    stray_anchor = scene_canvas.anchor(stray_x, stray_y)
    stray_point = scene_canvas.point(
        stray_anchor, value=f"STRAY {stray_x:.0f},{stray_y:.0f}"
    )
    scene_canvas.edge(stray_point, stray_point)

    return scene_canvas.to_bytes()


def main() -> None:
    """Parses command-line options and streams animation frames."""
    parser = argparse.ArgumentParser(description="Stream animated SpellCircle via UDP")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", default=PORT, type=int)
    parser.add_argument("--fps", default=60, type=int, help="target frames per second")
    arguments = parser.parse_args()

    frame_duration = 1.0 / arguments.fps

    with SceneSender(arguments.host, arguments.port) as sender:
        start_time = time.monotonic()
        frame_number = 0
        print(
            f"Streaming to {arguments.host}:{arguments.port} at "
            f"{arguments.fps} fps  (Ctrl-C to stop)"
        )
        try:
            while True:
                elapsed_seconds = time.monotonic() - start_time
                sender.send(build_frame(elapsed_seconds))
                frame_number += 1

                # Print actual FPS every 5 seconds.
                if frame_number % (arguments.fps * 5) == 0:
                    actual_fps = (
                        frame_number / elapsed_seconds
                        if elapsed_seconds > 0
                        else 0
                    )
                    print(
                        f"  t={elapsed_seconds:6.1f}s  "
                        f"frame={frame_number:6d}  fps={actual_fps:.1f}",
                        end="\r",
                    )

                # Sleep until the next frame deadline.
                deadline = start_time + frame_number * frame_duration
                slack = deadline - time.monotonic()
                if slack > 0:
                    time.sleep(slack)

        except KeyboardInterrupt:
            elapsed_seconds = time.monotonic() - start_time
            print(
                f"\nStopped: {frame_number} frames in {elapsed_seconds:.1f}s "
                f"({frame_number / elapsed_seconds:.1f} fps average)"
            )


if __name__ == "__main__":
    main()
