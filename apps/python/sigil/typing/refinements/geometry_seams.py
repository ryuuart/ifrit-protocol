"""the edge, corner, profile, shaper and crossing-rule readings the decoration packages name as 'geometry/seams' belong to no package the reports define..

Input contracts for the erased signatures of the
geometry/seams package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
