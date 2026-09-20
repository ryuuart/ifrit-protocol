"""The composer surface: queries, dials, profiling and a standalone composer.

Input contracts for the erased signatures of the
compose-elements/composer package, and nothing else: a fragment is one
author's alone.

Most of the composer states itself: its reports are registered ahead of
the calls that answer in them, so a profile is a list of rows, a plane's
counts are bytes and an inherited ink is a colour without a word here.
What is left is the three inputs read through a shared conversion, and
the one pointer parameter whose absence is a value. The point a hit test
is asked about is named by the compose fragment, which spoke for it
while the sketch adapter owned the class.
"""

from __future__ import annotations

from .table import Table

COMPOSER = "_sigil.compose.Composer"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # The viewport reads a size the way every other size-taking slot does.
    table.erased(COMPOSER, "setSize", "_t.SizeLike")
    # Where the pointer stands is a point, however it is spelled; whether
    # its button is down is a plain truth value pybind11 already names.
    table.erased(COMPOSER, "setPointer", "_t.PointLike")
    # No clock freezes paint time at zero, and a pointer parameter that
    # admits None is written without it.
    table.declares(
        COMPOSER,
        "setClock",
        "def setClock(self, clock: _sigil.motion.FrameClock | None) -> None: ...",
    )
