"""Native composition: factories, fluent properties and explicit children."""

from collections.abc import Iterable as _Iterable


def row(*children: Element | _Iterable[Element]) -> Element:
    """Create a native box with horizontal child flow."""
    return box(*children).row()


def column(*children: Element | _Iterable[Element]) -> Element:
    """Create a native box with vertical child flow."""
    return box(*children).column()
