"""The Engine, its options and clock policy, the playbacks and positions.

Input contracts for the erased signatures of the
motion/clock package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

MOTION = "_sigil.motion"
NOTIFY = "collections.abc.Callable[[], None]"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # Every playback states its own verbs, each answering the playback it
    # was called on, and a completion report is told nothing.
    for kind in ("Playback", "Animation", "Timer", "Timeline"):
        table.parameters(f"{MOTION}.{kind}.onComplete", callback=NOTIFY)
    table.erased(MOTION + ".Timeline", "add", MOTION + ".Tween")
    table.parameters(MOTION + ".Timeline.call", callback=NOTIFY)
    table.erased(MOTION + ".Engine", "animate", MOTION + ".Tween")
    table.parameters(MOTION + ".Engine.timer", callback="_t.TimerCallback")
