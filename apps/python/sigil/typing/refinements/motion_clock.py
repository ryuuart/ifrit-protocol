"""FrameClock, a constructible Ticker and the choreograph timeline.

Input contracts for the erased signatures of the
motion/clock package, and nothing else: a fragment is one
author's alone.

The Ticker's own two callables are named by the motion fragment, which
speaks for every ticker whoever owns it; what this fragment states is
the easing a ramp is shaped by and the three callables the timeline
stores on its own behalf.
"""

from __future__ import annotations

from .table import Table

TIMELINE = "_sigil.motion.Timeline"
NOTIFY = "collections.abc.Callable[[], None]"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased("_sigil.motion", "rampTo", "_t.EaseLike")
    # A cue and the timeline's two reports are told nothing and answer
    # nothing: choreograph's own callback carries no value.
    table.parameters(TIMELINE + ".cue", function=NOTIFY)
    table.parameters(TIMELINE + ".setFinishFn", function=NOTIFY)
    table.parameters(TIMELINE + ".setClearedFn", function=NOTIFY)
