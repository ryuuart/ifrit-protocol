"""Give Skia, Image and Core each their own source of bound values.

Input contracts for the erased signatures of the
skia-image-core/value-split package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
