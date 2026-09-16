"""Immutable scene values shared by authoring, codecs and application hosts."""

from __future__ import annotations

import math
import operator
from dataclasses import dataclass
from numbers import Real
from typing import TypeVar

_FLOAT32_MAX = float.fromhex("0x1.fffffep+127")
_Value = TypeVar("_Value")


def _number(name: str, value: float, minimum: float | None = None) -> float:
    if isinstance(value, bool) or not isinstance(value, Real):
        raise TypeError(f"{name} must be a real number")
    try:
        result = float(value)
    except OverflowError as error:
        raise ValueError(
            f"{name} must be finite and representable as float32"
        ) from error
    if not math.isfinite(result) or abs(result) > _FLOAT32_MAX:
        raise ValueError(f"{name} must be finite and representable as float32")
    if minimum is not None and result < minimum:
        raise ValueError(f"{name} must be at least {minimum}")
    return result


def _active(value: float) -> float:
    result = _number("active", value, 0)
    if result > 1:
        raise ValueError("active must be between zero and one")
    return result


def _text(name: str, value: str) -> str:
    if not isinstance(value, str):
        raise TypeError(f"{name} must be text")
    value.encode("utf-8")
    return value


def _values(
    name: str, values: tuple[_Value, ...], kind: type[_Value]
) -> tuple[_Value, ...]:
    result = tuple(values)
    if any(not isinstance(value, kind) for value in result):
        raise TypeError(f"{name} must contain {kind.__name__} values")
    return result


@dataclass(frozen=True, slots=True)
class CircleDefinition:
    """A visible circle, or a radius-zero anchor used only by its points."""

    name: str
    center_x: float
    center_y: float
    radius: int
    text_start: float = 0.0
    active: float = 0.0

    def __post_init__(self) -> None:
        _text("name", self.name)
        object.__setattr__(self, "center_x", _number("center_x", self.center_x))
        object.__setattr__(self, "center_y", _number("center_y", self.center_y))
        if isinstance(self.radius, bool):
            raise TypeError("radius must be an integer")
        radius = operator.index(self.radius)
        if not 0 <= radius <= 0xFFFFFFFF:
            raise ValueError("radius must fit an unsigned 32-bit integer")
        object.__setattr__(self, "radius", radius)
        object.__setattr__(self, "text_start", _number("text_start", self.text_start))
        object.__setattr__(self, "active", _active(self.active))


@dataclass(frozen=True, slots=True)
class PointReference:
    """A point on a circle, measured clockwise from twelve o'clock in turns."""

    circle: CircleDefinition
    position: float = 0.0
    value: str = ""

    def __post_init__(self) -> None:
        if not isinstance(self.circle, CircleDefinition):
            raise TypeError("circle must be a CircleDefinition")
        object.__setattr__(self, "position", _number("position", self.position))
        _text("value", self.value)


@dataclass(frozen=True, slots=True)
class EdgeDefinition:
    """A connection between two shared point values."""

    first: PointReference
    second: PointReference

    def __post_init__(self) -> None:
        if not isinstance(self.first, PointReference) or not isinstance(
            self.second, PointReference
        ):
            raise TypeError("edge endpoints must be PointReference values")


@dataclass(frozen=True, slots=True)
class BoxDefinition:
    """A text box attached to a point, with an optional background intensity."""

    value: str
    point: PointReference
    active: float = 0.0

    def __post_init__(self) -> None:
        _text("value", self.value)
        if not isinstance(self.point, PointReference):
            raise TypeError("point must be a PointReference")
        object.__setattr__(self, "active", _active(self.active))


@dataclass(frozen=True, slots=True)
class SceneDefinition:
    """A scene independent of its renderer or transport.

    Zero canvas dimensions use the receiving host's native coordinate space.
    Tuples preserve draw order; referenced anchors need not be visible circles.
    Floating-point values are encoded as float32 by the wire codec.
    """

    circles: tuple[CircleDefinition, ...] = ()
    edges: tuple[EdgeDefinition, ...] = ()
    boxes: tuple[BoxDefinition, ...] = ()
    width: float = 0.0
    height: float = 0.0

    def __post_init__(self) -> None:
        object.__setattr__(
            self, "circles", _values("circles", self.circles, CircleDefinition)
        )
        object.__setattr__(self, "edges", _values("edges", self.edges, EdgeDefinition))
        object.__setattr__(self, "boxes", _values("boxes", self.boxes, BoxDefinition))
        object.__setattr__(self, "width", _number("width", self.width, 0))
        object.__setattr__(self, "height", _number("height", self.height, 0))
