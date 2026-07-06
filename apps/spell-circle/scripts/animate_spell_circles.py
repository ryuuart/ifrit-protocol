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
import time

from sc_canvas import SCCanvas
from sc_network import SCSender

HOST = "127.0.0.1"
PORT = 27015
CANVAS = 1000.0
CX = CY = CANVAS / 2.0

# Shrinks the whole layout relative to the fixed CANVAS (rather than shrinking
# CANVAS itself, which wouldn't change anything — the app scales authored
# coordinates up to a fixed native texture size, so only the ratio of element
# size to CANVAS affects how much margin is left at the edges). More margin
# here means boxes, which anchor outward from the big circle, have room to
# sit fully on-canvas instead of spilling past its border.
FOOTPRINT_SCALE = 0.68
BIG_R = 380.0 * FOOTPRINT_SCALE

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


def _racetrack(t, period, margin=80.0, phase=0.0, reverse=False):
    """Position moving at constant speed around a square perimeter inset
    `margin` from the canvas edges, visiting all four corners every lap."""
    x0 = y0 = margin
    side = CANVAS - 2.0 * margin
    perimeter = 4.0 * side

    progress = t / period + phase
    if reverse:
        progress = -progress
    d = (progress * perimeter) % perimeter

    if d < side:
        return x0 + d, y0
    d -= side
    if d < side:
        return x0 + side, y0 + d
    d -= side
    if d < side:
        return x0 + side - d, y0 + side
    d -= side
    return x0, y0 + side - d


def _pos_on_big(cx, cy):
    """Return the arc-fraction [0,1] on the big circle closest to (cx,cy)."""
    ang = math.atan2(cy - CY, cx - CX)
    return (ang / (2.0 * math.pi) + 0.25 + 1.0) % 1.0


def _ramp(value, threshold):
    """Map a sine `value` in [-1, 1] to a fill intensity [0, 1]: 0 below
    `threshold`, ramping up to 1 at the peak. Used for the one flashing
    circle (UMBRA) so there's still something quick to contrast against the
    held transitions everything else uses."""
    return max(0.0, (value - threshold) / (1.0 - threshold))


def _hold_pulse(t, phase=0.0, hold=1.0, transition=0.3):
    """Trapezoidal wave in [0, 1]: eases up over `transition` seconds, holds
    at 1 for `hold` seconds, eases down over `transition` seconds, then holds
    at 0 for `hold` seconds — long enough to actually see each state instead
    of flickering between them. `phase` (seconds) offsets where in the cycle
    `t` starts, so multiple objects using this don't pulse in lockstep."""
    period = 2.0 * (hold + transition)
    x = (t + phase) % period
    if x < transition:
        u = x / transition
        return u * u * (3.0 - 2.0 * u)
    x -= transition
    if x < hold:
        return 1.0
    x -= hold
    if x < transition:
        u = x / transition
        return 1.0 - u * u * (3.0 - 2.0 * u)
    return 0.0


def build_frame(t):
    sc = SCCanvas(CANVAS, CANVAS)

    # Central circle — label drifts slowly.
    big = sc.circle(
        "FLARE SIGIL CIRCLE", CX, CY, round(BIG_R),
        text_start=(0.62 + t * 0.04) % 1.0,
    )

    # Orbiting circles.
    n = len(ORBITERS)
    smalls = []
    for i, (orb_r, speed, name, base_r) in enumerate(ORBITERS):
        phase = i * (2.0 * math.pi / n)
        ang = t * speed + phase
        cx = CX + orb_r * math.cos(ang)
        cy = CY + orb_r * math.sin(ang)
        # Radius breathes on a per-circle frequency.
        r = base_r + 14.0 * FOOTPRINT_SCALE * math.sin(t * 1.4 + i * 1.1)
        # Every orbiter but the last holds its active/inactive state for a
        # beat so the transition reads clearly instead of flickering; the
        # last one (UMBRA) keeps the quick sine-driven flash for contrast.
        if i == n - 1:
            active = _ramp(math.sin(t * 2.2 + i * 1.3), 0.68)
        else:
            active = _hold_pulse(t, phase=i * 0.9)
        smalls.append(sc.circle(
            name, cx, cy, max(1, round(r)),
            text_start=(0.75 + t * 0.09 + i * 0.13) % 1.0,
            active=active,
        ))

    # Star web on the big circle. Points here are pure edge endpoints with
    # nothing worth labelling, so they carry no value.
    for i in range(RING):
        for skip in RING_SKIPS:
            a = sc.point(big, i / RING)
            b = sc.point(big, ((i+skip)%RING)/RING)
            sc.edge(a, b)

    # Spoke from each orbiter to the big circle. Points are kept in
    # `spoke_points` so a box can later be assigned the same Point as the
    # edge's endpoint, rather than creating a fresh one. The first spoke's
    # big-circle endpoint demonstrates a Point's own value label (rendered
    # like a box) by tracking its live fractional position on the ring; the
    # rest carry no value.
    spoke_points = []
    for i, s in enumerate(smalls):
        pos = _pos_on_big(s.x, s.y)
        value = f"{pos:.2f}" if i == 0 else ""
        a = sc.point(big, pos, value)
        b = sc.point(s, 0.5)
        sc.edge(a, b)
        spoke_points.append((a, b))

    # Ring connecting adjacent orbiters. Points kept in `ring_points` for the
    # same reason as `spoke_points` above.
    ring_points = []
    for i in range(n):
        sa, sb = smalls[i], smalls[(i + 1) % n]
        a = sc.point(sa, 0.25)
        b = sc.point(sb, 0.75)
        sc.edge(a, b)
        ring_points.append((a, b))

    tracks = {"spoke": spoke_points[0], "ring": ring_points[0]}

    # Boxes drifting around the big circle. Each holds its active/inactive
    # state for a beat (see _hold_pulse) rather than flickering, staggered by
    # phase so they don't all transition in lockstep.
    for i, (name, drift, phase) in enumerate(BOX_SPECS):
        pos = (t * drift + phase) % 1.0
        active = _hold_pulse(t, phase=i * 0.9 + 0.4)
        pt = sc.point(big, pos)
        sc.box(name, pt, active=active)

    # Boxes pinned to one of the points an edge is already assigned to, so
    # they follow that point wherever the edge's endpoint goes.
    for i, (name, track, end) in enumerate(EDGE_RIDERS):
        pt = tracks[track][end]
        active = _hold_pulse(t, phase=i * 0.9 + 1.3)
        sc.box(name, pt, active=active)

    # DRIFTER: a box anchored to a radius-0 circle lapping a rectangular
    # racetrack, unconnected to any visible circle's perimeter.
    drifter_x, drifter_y = _racetrack(t, **DRIFTER_TRACK)
    drifter_anchor = sc.anchor(drifter_x, drifter_y)
    drifter_pt = sc.point(drifter_anchor)
    sc.box("DRIFTER", drifter_pt, active=_hold_pulse(t, phase=2.1))

    # STRAY: a lone point (no box) lapping its own racetrack, labelled with
    # its live coordinates to make the arbitrary positioning visible. Points
    # only enter the scene's registry when referenced by an Edge or a Box
    # (see PointComponent's doc comment), so it rides a self-looped edge —
    # both ends the same Point — purely to register it; the edge is
    # zero-length and draws nothing.
    stray_x, stray_y = _racetrack(t, **STRAY_TRACK)
    stray_anchor = sc.anchor(stray_x, stray_y)
    stray_pt = sc.point(stray_anchor, value=f"STRAY {stray_x:.0f},{stray_y:.0f}")
    sc.edge(stray_pt, stray_pt)

    return sc.to_bytes()


def main():
    parser = argparse.ArgumentParser(description="Stream animated SpellCircle via UDP")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", default=PORT, type=int)
    parser.add_argument("--fps", default=60, type=int, help="target frames per second")
    args = parser.parse_args()

    frame_time = 1.0 / args.fps

    with SCSender(args.host, args.port) as sender:
        t0 = time.monotonic()
        frame = 0
        print(f"Streaming to {args.host}:{args.port} at {args.fps} fps  (Ctrl-C to stop)")
        try:
            while True:
                t = time.monotonic() - t0
                sender.send(build_frame(t))
                frame += 1

                # Print actual FPS every 5 seconds.
                if frame % (args.fps * 5) == 0:
                    fps_actual = frame / t if t > 0 else 0
                    print(f"  t={t:6.1f}s  frame={frame:6d}  fps={fps_actual:.1f}", end="\r")

                # Sleep until the next frame deadline.
                deadline = t0 + frame * frame_time
                slack = deadline - time.monotonic()
                if slack > 0:
                    time.sleep(slack)

        except KeyboardInterrupt:
            elapsed = time.monotonic() - t0
            print(f"\nStopped: {frame} frames in {elapsed:.1f}s "
                  f"({frame/elapsed:.1f} fps average)")


if __name__ == "__main__":
    main()
