"""Split the material bindings into per-subject files and link the missing feature libraries.

Input contracts for the erased signatures of the
material/relocate package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
