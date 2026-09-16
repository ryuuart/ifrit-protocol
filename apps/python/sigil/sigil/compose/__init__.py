"""Native composition: factories, fluent properties and explicit children."""

from importlib import import_module as _import_module

from _sigil.compose import *

from ..native import compose as _native


def row() -> Element:
    """Create a native box with horizontal child flow."""
    return _native.box().row()


def column() -> Element:
    """Create a native box with vertical child flow."""
    return _native.box().column()


__all__ = ["column", "row"]
__all__.extend(name for name in dir(_native) if not name.startswith("_"))
__all__.sort()

# Kit records and layout schemes have their own domain-specific fields.
kit = _import_module(__name__ + ".kit")
layouts = _import_module(__name__ + ".layouts")
