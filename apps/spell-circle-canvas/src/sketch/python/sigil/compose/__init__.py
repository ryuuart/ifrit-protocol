"""Native composition with keyword properties and ordinary Python children."""

from collections.abc import Iterable, Mapping
from collections.abc import Set as AbstractSet
from difflib import get_close_matches
from importlib import import_module as _import_module

from _sigil.compose import *  # Native value types are shared with the fluent API.

from ..native import compose as _native

Element = _native.Element

_PROPERTIES = {
    name: name
    for name in (
        "width",
        "height",
        "gap",
        "padding",
        "margin",
        "corners",
        "fill",
        "ink",
        "key",
        "grow",
        "shrink",
        "inset",
        "left",
        "top",
        "right",
        "bottom",
        "opacity",
        "rotate",
        "scale",
        "justify",
        "aspect",
        "basis",
        "font",
        "block",
        "shape",
        "clip",
        "cache",
        "boundary",
        "threshold",
        "style",
        "effect",
        "backdrop",
        "appear",
        "blend",
        "travel",
        "area",
        "rect",
        "at",
        "thread",
        "region",
        "ellipsis",
        "sampling",
    )
}
_PROPERTIES.update(
    {
        "scale_x": "scaleX",
        "scale_y": "scaleY",
        "scale_z": "scaleZ",
        "translate_x": "translateX",
        "translate_y": "translateY",
        "translate_z": "translateZ",
        "rotate_x": "rotateX",
        "rotate_y": "rotateY",
        "rotate_z": "rotateZ",
        "skew_x": "skewX",
        "skew_y": "skewY",
        "font_size": "fontSize",
        "font_weight": "fontWeight",
        "font_track": "fontTrack",
        "align_items": "alignItems",
        "align_self": "alignSelf",
        "min_width": "minWidth",
        "min_height": "minHeight",
        "max_width": "maxWidth",
        "max_height": "maxHeight",
        "wrap_lines": "wrapLines",
        "style_sheet": "styleSheet",
        "style_class": "styleClass",
        "center_at": "centerAt",
        "hit_testable": "hitTestable",
        "z_index": "zIndex",
        "bake_scale": "bakeScale",
        "max_lines": "maxLines",
        "preserve_3d": "preserve3d",
        "backface": "backface",
        "perspective": "perspective",
        "transform_origin": "transformOrigin",
        "perspective_origin": "perspectiveOrigin",
    }
)


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
        if name in (
            "padding",
            "margin",
            "corners",
            "inset",
            "transform_origin",
            "perspective_origin",
        ) and isinstance(value, (tuple, list)):
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


def text(value, *, size=None, color=None, **properties):
    """Build native text; omitted size and color inherit from its container."""
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


def memo(properties, describe, *, key=None):
    """Defer native description until a copied model or captured environment changes.

    Models must support deepcopy and equality. The builder is a pure function
    of that model and the inherited environment; give state changes to the model.
    Styling belongs on the builder's returned element, not the memo shell.
    """
    element = _native.memo(properties, describe)
    return element if key is None else element.key(key)


def stack(*children, **properties):
    element = _style(_native.stack(), properties)
    return element.children(list(_children(children))) if children else element


def positioned(*children, **properties):
    element = _style(_native.positioned(), properties)
    return element.children(list(_children(children))) if children else element


def layout(scheme, *children, **properties):
    element = _style(_native.layout(scheme), properties)
    return element.children(list(_children(children))) if children else element


__all__ = [
    "Element",
    "box",
    "column",
    "graphics",
    "layout",
    "memo",
    "positioned",
    "row",
    "stack",
    "text",
]
__all__.extend(
    name for name in dir(_native) if not name.startswith("_") and name not in __all__
)
__all__.sort()

# Public child packages add keyword authoring over the same native records.
kit = _import_module(__name__ + ".kit")
layouts = _import_module(__name__ + ".layouts")
