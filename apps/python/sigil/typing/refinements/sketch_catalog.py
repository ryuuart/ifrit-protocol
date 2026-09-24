"""The registry's rows and a catalog of sketch files.

Input contracts for the erased signatures of the sketch catalog, and
nothing else: a fragment is one author's alone.
"""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # Every file and both directories are any path a filesystem call takes.
    table.parameters(
        "_sigil.sketch.catalog",
        files=f"collections.abc.Sequence[{PATH_INPUT}]",
        sketchDirectory=PATH_INPUT + " | None",
        workspace=PATH_INPUT + " | None",
    )
