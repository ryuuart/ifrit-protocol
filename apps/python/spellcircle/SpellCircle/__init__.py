"""Public scene-authoring and transport API for the SpellCircle protocol."""

from .builder import SceneBuilder
from .canvas import SpellCircleCanvas
from .codec import decode_scene, encode_scene
from .model import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
)
from .network import SceneSender, send_once

__all__ = [
    "BoxDefinition",
    "CircleDefinition",
    "EdgeDefinition",
    "PointReference",
    "SceneBuilder",
    "SceneDefinition",
    "SceneSender",
    "SpellCircleCanvas",
    "decode_scene",
    "encode_scene",
    "send_once",
]
