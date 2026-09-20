"""Give Skia, Image and Core each their own source of bound values.

Input contracts for the erased signatures of the
skia-image-core/value-split package, and nothing else: a fragment is one
author's alone.

The table stays empty because these signatures erase nothing: every
parameter of the chance stream and of the image doors reaches its
declaration through a spelling pybind11 writes itself — a buffer, bytes,
a number, an enumeration or a registered class.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
