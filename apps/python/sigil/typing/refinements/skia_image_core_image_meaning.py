"""SigilMedia's images: the decoded document and its frames, decode and encode, the pixel difference.

Input contracts for the erased signatures of the
skia-image-core/image-meaning package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.returns("_sigil.media.Image", "size", "tuple[int, int]")
    table.parameters("_sigil.media.decode", type="type[_sigil.media.Image]")
