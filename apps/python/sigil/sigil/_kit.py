"""Property records and ordered children shared by the native kit libraries."""

from collections.abc import Iterable, Mapping
from collections.abc import Set as AbstractSet

from _sigil.compose import Element


def specification(record_type, value, properties):
    """Copy a native props value and apply snake_case keyword fields."""
    if value is None:
        result = record_type()
    elif isinstance(value, record_type):
        result = value.copy()
    else:
        raise TypeError(f"expected {record_type.__name__}, got {type(value).__name__}")
    for name, item in properties.items():
        first, *rest = name.split("_")
        field = first + "".join(part.title() for part in rest)
        descriptor = getattr(record_type, field, None)
        if not isinstance(descriptor, property):
            raise TypeError(f"unknown {record_type.__name__} property {name!r}")
        setattr(result, field, item)
    return result


def children(values, ancestors=None):
    from _sigil.compose import text

    ancestors = set() if ancestors is None else ancestors
    for value in values:
        if value is None:
            continue
        if isinstance(value, Element):
            yield value
        elif isinstance(value, str):
            yield text(value)
        elif isinstance(value, Iterable) and not isinstance(
            value, (bytes, bytearray, Mapping, AbstractSet)
        ):
            identity = id(value)
            if identity in ancestors:
                raise ValueError("children contain a recursive collection")
            ancestors.add(identity)
            yield from children(value, ancestors)
            ancestors.remove(identity)
        else:
            raise TypeError(
                "children must be elements, strings, None, or ordered iterables; "
                f"got {type(value).__name__}"
            )
