"""SigilUSD writer and readers over the values a scene is made of.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/usd package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
