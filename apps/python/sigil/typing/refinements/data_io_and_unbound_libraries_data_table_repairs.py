"""Typed column spans, Flag/Instant comparisons, deepcopy, and four FINDINGS data defects.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/data-table-repairs package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
