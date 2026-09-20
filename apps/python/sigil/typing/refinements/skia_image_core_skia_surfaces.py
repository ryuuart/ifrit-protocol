"""Owned raster surfaces, the canvas seam, image info and pixmaps, the missing canvas verbs, SigilSkia direct draws and pixel helpers.

Input contracts for the erased signatures of the
skia-image-core/skia-surfaces package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
