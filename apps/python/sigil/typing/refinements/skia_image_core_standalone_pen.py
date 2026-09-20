"""A pen over a Python-owned canvas, draw.Frame, Pen.retained, and the declared pen forms.

Input contracts for the erased signatures of the
skia-image-core/standalone-pen package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
