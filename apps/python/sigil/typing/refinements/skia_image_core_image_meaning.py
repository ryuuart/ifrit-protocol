"""Decode options, probes, channel planes, pixmap and layer encoding, asset frames, coverage masks and distance fields.

Input contracts for the erased signatures of the
skia-image-core/image-meaning package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
