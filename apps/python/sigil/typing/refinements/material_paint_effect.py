"""The revisioned pixel buffer, raw shader interop, bound pans, live paint uniforms and Effect::filter.

Input contracts for the erased signatures of the
material/paint-effect package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
