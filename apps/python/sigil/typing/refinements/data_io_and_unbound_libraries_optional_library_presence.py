"""How a Python process learns which optional SDK modules this build carries.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/optional-library-presence package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
