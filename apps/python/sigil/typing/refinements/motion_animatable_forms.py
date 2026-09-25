"""Binding, Range, Envelope and its factories, Wiggle and bind.

Input contracts for the erased signatures of the
motion/animatable-forms package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

MOTION = "_sigil.motion"
BINDING = MOTION + ".Binding"
RANGE = f"{MOTION}.Range | tuple[_t.FloatLike, _t.FloatLike]"
# Every stage keyword, in the order the stages run; None keeps the stage.
STAGES = (
    RANGE,
    "bool",
    "bool",
    MOTION + ".Envelope",
    "_t.EaseLike",
    "typing.SupportsInt",
    "bool",
    RANGE,
    "_t.FloatLike",
    MOTION + ".Wiggle",
    RANGE,
)


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased(BINDING, "__init__", *STAGES)
    for name in ("from_", "to", "clamp"):
        table.accessor(BINDING, name, MOTION + ".Range", RANGE)
    table.accessor(BINDING, "ease", MOTION + ".Easing | None", "_t.EaseLike")
    table.erased(MOTION, "bind", "_t.ScalarLike", *STAGES)
    table.erased(MOTION + ".envelope", "shaped", "_t.EaseLike")
