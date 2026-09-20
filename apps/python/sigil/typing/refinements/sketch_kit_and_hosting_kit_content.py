"""Theme extras, passage, Document and Instrument.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/kit-content package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
