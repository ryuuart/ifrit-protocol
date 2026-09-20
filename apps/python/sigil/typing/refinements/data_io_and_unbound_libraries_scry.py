"""SigilScry engine, views, image slots, the Compose web leaf and the sketch settling seam.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/scry package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
