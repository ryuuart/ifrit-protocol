"""Two directories of plates compared.

Input contracts for the erased signatures of the sketch plate comparison,
and nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # Either directory is any path a filesystem call takes.
    table.parameters("_sigil.sketch.compare", first=PATH_INPUT, second=PATH_INPUT)
