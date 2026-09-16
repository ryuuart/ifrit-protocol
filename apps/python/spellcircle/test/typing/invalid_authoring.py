"""Each marked misuse must produce its stated public-contract diagnostic."""

from SpellCircle import (
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
    SpellCircleCanvas,
)

ring = CircleDefinition("ring", 320, 240, 100)
point = PointReference(ring)
canvas = SpellCircleCanvas()

CircleDefinition("ring", "left", 240, 100)  # error: reportArgumentType
CircleDefinition("ring", 320, 240, 1.5)  # error: reportArgumentType
CircleDefinition("ring", 320, 240, 100, tint="red")  # error: reportCallIssue
PointReference(ring, value=42)  # error: reportArgumentType
SceneDefinition(circles=(point,))  # error: reportArgumentType
canvas.edge(ring, point)  # error: reportArgumentType
EdgeDefinition(point, "east")  # error: reportArgumentType
ring.radius = 120  # error: reportAttributeAccessIssue
point.value = "changed"  # error: reportAttributeAccessIssue
scene = SceneDefinition()
scene.width = 200  # error: reportAttributeAccessIssue
