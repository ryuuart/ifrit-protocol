#!/usr/bin/env python3
"""
Send a SpellCircle FlatBuffer with 10 circles via UDP.

Usage:
    python send_spell_circles.py [--host HOST] [--port PORT]

Requires the flatbuffers package:
    pip install flatbuffers
"""

import argparse
import os
import random
import socket
import sys

sys.path.insert(0, os.path.dirname(__file__))

import flatbuffers
from SpellCircle import Circle as Circ
from SpellCircle import Scene as SC
from SpellCircle.Vec2 import CreateVec2

HOST = "127.0.0.1"
PORT = 27015

NAMES = ["Aurum", "Ignis", "Aqua", "Terra", "Ventus", "Umbra", "Lux", "Fulmen", "Glacies", "Arcanum"]


def random_circles():
    return [
        (name, random.uniform(50.0, 550.0), random.uniform(50.0, 500.0), random.randint(25, 65))
        for name in NAMES
    ]


def build_packet(circles):
    builder = flatbuffers.Builder(1024)

    circle_offsets = []
    for name, x, y, radius in circles:
        name_off = builder.CreateString(name)

        Circ.CircleStart(builder)
        Circ.CircleAddPos(builder, CreateVec2(builder, x, y))
        Circ.CircleAddName(builder, name_off)
        Circ.CircleAddRadius(builder, radius)
        circle_offsets.append(Circ.CircleEnd(builder))

    SC.SceneStartCirclesVector(builder, len(circle_offsets))
    for off in reversed(circle_offsets):
        builder.PrependUOffsetTRelative(off)
    circles_vec = builder.EndVector()

    SC.SceneStart(builder)
    SC.SceneAddCircles(builder, circles_vec)
    root = SC.SceneEnd(builder)

    builder.Finish(root)
    return bytes(builder.Output())


def main():
    parser = argparse.ArgumentParser(description="Send SpellCircle FlatBuffer via UDP")
    parser.add_argument("--host", default=HOST)
    parser.add_argument("--port", default=PORT, type=int)
    args = parser.parse_args()

    circles = random_circles()
    buf = build_packet(circles)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(buf, (args.host, args.port))

    print(
        f"Sent SpellCircle ({len(circles)} circles, {len(buf)} bytes) -> {args.host}:{args.port}"
    )
    for name, x, y, radius in circles:
        print(f"  {name:<10s}  ({x:6.1f}, {y:6.1f})  r={radius}")


if __name__ == "__main__":
    main()
