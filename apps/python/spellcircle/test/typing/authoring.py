"""Public scene authoring stays typed without generated FlatBuffers imports."""

from dataclasses import replace
from typing import assert_type

from SpellCircle import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneBuilder,
    SceneDefinition,
    SceneSender,
    SpellCircleCanvas,
    decode_scene,
    encode_scene,
)


def diagram(label: str) -> SceneDefinition:
    canvas = SpellCircleCanvas(width=640, height=480)
    ring = canvas.circle(label, 320, 240, 100, active=0.5)
    north = canvas.point(ring, 0, value="north")
    east = canvas.point(ring, 0.25)
    edge = canvas.edge(north, east)
    box = canvas.box(label, east)
    assert_type(ring, CircleDefinition)
    assert_type(north, PointReference)
    assert_type(edge, EdgeDefinition)
    assert_type(box, BoxDefinition)
    assert_type(canvas.circles, tuple[CircleDefinition, ...])
    assert_type(canvas.edges, tuple[EdgeDefinition, ...])
    assert_type(canvas.boxes, tuple[BoxDefinition, ...])
    assert_type(canvas.to_bytes(), bytes)
    return canvas.snapshot()


def revise(scene: SceneDefinition) -> SceneDefinition:
    return replace(scene, width=800, height=600)


def serialized_copy(scene: SceneDefinition) -> SceneDefinition:
    payload = encode_scene(scene)
    assert_type(payload, bytes)
    assert_type(decode_scene(bytearray(payload)), SceneDefinition)
    return decode_scene(memoryview(payload))


def transmit(scene: SceneDefinition, sender: SceneSender) -> None:
    sender.send(encode_scene(scene))


def primitive_scene(circle: CircleDefinition) -> bytes:
    builder = SceneBuilder()
    offset = builder.build_circle(circle)
    assert_type(offset, int)
    return builder.build_scene_bytes([offset], [], [], 640, 480)


ring = CircleDefinition("ring", 320, 240, 100)
point = PointReference(ring, 0.5, "south")
scene = SceneDefinition(
    circles=(ring,),
    edges=(EdgeDefinition(point, PointReference(ring)),),
    boxes=(BoxDefinition("readout", point),),
    width=640,
    height=480,
)
assert_type(scene, SceneDefinition)
