"""SigilVideo decode, playback, encode and the Compose video leaf.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/video package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
