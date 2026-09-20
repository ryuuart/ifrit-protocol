"""A Python sketch's own schema, and a settled page.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/host-schema-and-scry package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
