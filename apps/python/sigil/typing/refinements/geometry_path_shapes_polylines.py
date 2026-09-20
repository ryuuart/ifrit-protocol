"""Polylines, contours, poses, stride, the numeric leaf and sections, with the shared point-batch conversions.

Input contracts for the erased signatures of the
geometry-path-shapes/polylines package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
