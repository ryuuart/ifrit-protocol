"""Native composition with keyword properties and ordinary Python children."""

from collections.abc import Iterable, Mapping
from collections.abc import Set as AbstractSet
from difflib import get_close_matches

from .native import compose as _native

Element = _native.Element

__all__ = ["Element", "box", "column", "graphics", "row", "text"]

_PROPERTIES = {
    "width": "width",
    "height": "height",
    "gap": "gap",
    "padding": "padding",
    "corners": "corners",
    "fill": "fill",
    "ink": "ink",
    "key": "key",
    "grow": "grow",
    "inset": "inset",
    "left": "left",
    "top": "top",
    "opacity": "opacity",
    "rotate": "rotate",
    "scale": "scale",
    "scale_x": "scaleX",
    "scale_y": "scaleY",
    "translate_x": "translateX",
    "translate_y": "translateY",
    "font_size": "fontSize",
    "font_weight": "fontWeight",
    "align_items": "alignItems",
    "justify": "justify",
}


def _style(element, properties):
    for name, value in properties.items():
        if name == "absolute":
            if not isinstance(value, bool):
                raise TypeError("absolute must be True or False")
            if value:
                element.absolute()
            continue
        method = _PROPERTIES.get(name)
        if method is None:
            suggestion = get_close_matches(name, [*_PROPERTIES, "absolute"], n=1)
            hint = f"; did you mean {suggestion[0]!r}?" if suggestion else ""
            raise TypeError(f"unknown element property {name!r}{hint}")
        if name in ("padding", "corners") and isinstance(value, (tuple, list)):
            getattr(element, method)(*value)
        else:
            getattr(element, method)(value)
    return element


def _children(values, ancestors=None):
    ancestors = set() if ancestors is None else ancestors
    for value in values:
        if value is None:
            continue
        if isinstance(value, Element):
            yield value
        elif isinstance(value, str):
            yield _native.text(value)
        elif isinstance(value, Iterable) and not isinstance(
            value, (bytes, bytearray, Mapping, AbstractSet)
        ):
            identity = id(value)
            if identity in ancestors:
                raise ValueError("children contain a recursive collection")
            ancestors.add(identity)
            yield from _children(value, ancestors)
            ancestors.remove(identity)
        else:
            raise TypeError(
                "children must be elements, strings, None, or ordered iterables; "
                f"got {type(value).__name__}"
            )


def box(*children, **properties):
    """Build one native box; flatten nested child lists and generators."""
    element = _style(_native.box(), properties)
    if children:
        element.children(list(_children(children)))
    return element


def row(*children, **properties):
    """Build a box whose children flow horizontally."""
    return box(*children, **properties).row()


def column(*children, **properties):
    """Build a box whose children flow vertically."""
    return box(*children, **properties).column()


def text(value, *, size=16.0, color="#ffffff", **properties):
    """Build native text with its initial font size and color."""
    if not isinstance(value, str):
        raise TypeError("text content must be a string")
    return _style(_native.text(value, size=size, color=color), properties)


def graphics(program, *, key, **properties):
    """Keep a canvas painted by program(pen); equal keys promise equal programs."""
    if not callable(program):
        raise TypeError("graphics program must be callable")
    if not isinstance(key, str) or not key:
        raise ValueError("graphics key must be a nonempty string")
    return _style(_native.graphics(key, program), properties)
