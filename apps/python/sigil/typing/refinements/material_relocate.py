"""Splitting the material bindings into one file per subject.

Input contracts for the erased signatures of the
material/relocate package, and nothing else: a fragment is one
author's alone.

The split moves registrations between files and adds no signature, so
this package has nothing to refine: every material contract already
recorded stays recorded, under the same raw path, because a path names
the module a value is registered on and not the file that registered
it. The unions the split does add — the ramp stops, the uniform input,
the tile program, the texture producer, the radiance function, the
image decoder and the bank maker — are declared in `typing/_types.pyi`
and spoken for by the packages whose signatures name them.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
