"""Native composition: factories, fluent properties and explicit children."""

from collections.abc import Iterable as _Iterable

# Any node of the tree. A leaf is a class of its own rather than a subclass
# of the element, so the four are named wherever one child is taken.
_Node = Element | Text | Image | Band


def row(*children: _Node | _Iterable[_Node]) -> Element:
    """Create a native box with horizontal child flow."""
    return box(*children).row()


def column(*children: _Node | _Iterable[_Node]) -> Element:
    """Create a native box with vertical child flow."""
    return box(*children).column()
