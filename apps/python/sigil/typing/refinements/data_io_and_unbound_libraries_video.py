"""SigilMedia's video: the clip a hub opens, the decode pool and the movie encoder.

Input contracts for the erased signatures of the
data-io-and-unbound-libraries/video package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.returns("_sigil.media.Video", "size", "tuple[int, int]")
