"""Everything a canvas sketch is handed.

Input contracts for the erased signatures of the
sketch-kit-and-hosting/host-context-surface package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

PATH_INPUT = "os.PathLike[str] | os.PathLike[bytes] | str | bytes"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A declared canvas reads a size the way every other size-taking slot
    # does, because the record's conversion is the shared one.
    table.erased("_sigil.sketch.CanvasSpecification", "size", "_t.SizeLike")
    # A bake answers nothing when the raster surface cannot be taken.
    table.returns("_sigil.Context", "bakeSet", "_sigil.skia.Image | None")
    # A guest reads the canvas off the pen the drawing is landing on, and
    # answers nothing while no publication is standing.
    table.erased("_sigil.sketch.Guest", "frame", "_sigil.draw.Pen")
    table.returns("_sigil.sketch.Guest", "frame", "_sigil.skia.Image | None")
    # The probe answers whether every URL is here and, when one is not,
    # which one stood the sketch down.
    table.returns("_sigil.sketch", "requireCached", "tuple[bool, str]")
    table.parameters(
        "_sigil.sketch.requireCached", cacheDirectory=PATH_INPUT + " | None"
    )
