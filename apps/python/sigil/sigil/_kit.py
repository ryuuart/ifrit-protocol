"""Property records and ordered children shared by the native kit libraries."""

from collections.abc import Iterable, Mapping
from collections.abc import Set as AbstractSet

from _sigil.compose import Band, Element, Image, Rule, StyleSheet, Text

# Every kind of node. A typed leaf is a class of its own rather than a
# subclass of the element, and converts into one where it lands, so asking
# after the element alone would answer no for a text, an image or a band.
NODES = (Element, Text, Image, Band)


def specification(record_type, value, properties):
    """Copy a native props value and apply keyword fields by their native names."""
    if value is None:
        result = record_type()
    elif isinstance(value, record_type):
        result = value.copy()
    else:
        raise TypeError(f"expected {record_type.__name__}, got {type(value).__name__}")
    for name, item in properties.items():
        descriptor = getattr(record_type, name, None)
        if not isinstance(descriptor, property):
            raise TypeError(unknown(record_type, name))
        setattr(result, name, item)
    return result


def unknown(record_type, name):
    """Name the field a keyword missed, and its spelling when only the case differs.

    A record's fields are spelled as the native record spells them, so
    ``padding_vertical`` is not a field of a record that has ``paddingVertical``.
    """
    message = f"unknown {record_type.__name__} property {name!r}"
    first, *rest = name.split("_")
    field = first + "".join(part[:1].upper() + part[1:] for part in rest)
    if rest and isinstance(getattr(record_type, field, None), property):
        message += f"; the field is spelled {field!r}"
    return message


def children(values, ancestors=None):
    from _sigil.compose import text

    ancestors = set() if ancestors is None else ancestors
    for value in values:
        if value is None:
            continue
        if isinstance(value, NODES):
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
                "children must be nodes, strings, None, or ordered iterables; "
                f"got {type(value).__name__}"
            )


def stagedStyleSheet(styleSheet=None, theme=None, sheet=None):
    """The one stylesheet a stage applies: ``styleSheet``, after the
    stylesheet a deprecated ``theme`` builder makes and before the
    deprecated ``sheet`` of rules joined after it. A rule stands for a
    stylesheet of that one rule; nothing named is None."""
    whole = None
    for part in (
        None if theme is None else theme.styleSheet(),
        styleSheet,
        sheet,
    ):
        if part is None:
            continue
        if isinstance(part, Rule):
            part = StyleSheet([part])
        whole = part if whole is None else whole + part
    return whole
