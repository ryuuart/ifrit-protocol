"""A schema-driven FlatBuffer value, its hub decoder, and the .bfbs a Python sketch can read.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/data-schema-and-flatbuffer package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
