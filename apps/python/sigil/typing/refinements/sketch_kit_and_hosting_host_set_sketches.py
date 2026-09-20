"""A Python sketch that dresses a set.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/host-set-sketches package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
