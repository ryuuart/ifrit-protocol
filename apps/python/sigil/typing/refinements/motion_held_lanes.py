"""Held-motion resolution and the lane retargets.

Input contracts for the erased signatures of the
motion/held-lanes package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import DURATION, Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A held number is any animatable a property takes: a number, a live
    # value or a described motion.
    for member in ("enter", "isRunning", "valueOf"):
        table.parameters(f"_sigil.motion.{member}", value="_t.ScalarLike")
    table.parameters(
        "_sigil.motion.retarget",
        previous="_t.ScalarLike",
        next="_t.ScalarLike",
        transition=f"{DURATION} | _sigil.motion.Tween | None",
    )
