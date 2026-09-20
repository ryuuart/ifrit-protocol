"""The field parameter structs and recipe accessors, plus the CRT screen and its kit preset.

Input contracts for the erased signatures of the
material/field-crt package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
