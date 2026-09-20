"""copy.deepcopy on every bound record.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/value-copy-protocol package, and nothing else: a fragment is one
author's alone.

This package adds `__copy__` and `__deepcopy__` to every bound record, and
pybind11 writes both signatures whole: `__copy__` takes nothing, `__deepcopy__`
takes the memo dictionary the `copy` module keeps its cycles in, and each
answers the class it is defined on. Nothing is erased, so the table stays
empty; `copy.copy` and `copy.deepcopy` are already declared to answer what
they were given.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
