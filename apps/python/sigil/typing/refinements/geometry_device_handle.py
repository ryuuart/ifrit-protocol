"""the opaque, non-constructible device class both geometry-mesh/device-runtimes and world/device need exactly one registration of..

Input contracts for the erased signatures of the
geometry/device-handle package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
