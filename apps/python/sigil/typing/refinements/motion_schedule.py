"""Place, stagger, cues, Staggered, Timing, timingOf, Schedule and Beat.

Input contracts for the erased signatures of the
motion/schedule package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table

MOTION = "_sigil.motion"
STAGGERED = MOTION + ".Staggered"
TIMES = f"_t.DurationLike | {STAGGERED}"


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased(STAGGERED, "__init__", "_t.DurationLike")
    table.erased(
        MOTION,
        "stagger",
        "_t.DurationLike | tuple[_t.DurationLike, _t.DurationLike]",
        MOTION + ".StaggerFrom | typing.SupportsIndex",
        "_t.EaseLike",
        "_t.DurationLike",
    )
    table.erased(MOTION, "cues", "collections.abc.Iterable[_t.DurationLike]")
    table.erased(MOTION + ".Timing", "__init__", TIMES, TIMES)
    table.accessor(MOTION + ".Timing", "delay", f"float | {STAGGERED}", TIMES)
    table.accessor(
        MOTION + ".Timing", "within", f"float | {STAGGERED} | None",
        TIMES + " | None",
    )
    # A tween as a collective's schedule reads it: the number family's
    # tween, and a second stagger inside every beat when one is nested.
    table.declares(
        MOTION,
        "timingOf",
        f"def timingOf(tween: {MOTION}.Tween, within: {TIMES} | None = None) "
        f"-> {MOTION}.Timing: ...\n",
    )
