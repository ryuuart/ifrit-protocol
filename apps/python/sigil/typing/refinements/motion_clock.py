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
# A motion is written the same way whether it replaces what was driving
# the output or is added after it, so the two spell one signature. The
# reports are optional and stand behind the keyword-only bar, where
# pybind11 writes them as a bare callable; the whole declaration is
# stated here so each report says which arities it accepts.
MOTION = (
    "self, output: Output,"
    " phrases: collections.abc.Sequence[Phrase] = [], *,"
    " removeOnFinish: bool | None = None,"
    " playbackSpeed: typing.SupportsFloat | None = None,"
    " startTime: typing.SupportsFloat | None = None,"
    " onStart: _t.MotionCallback | None = None,"
    " onUpdate: _t.MotionCallback | None = None,"
    " onFinish: _t.MotionCallback | None = None"
)


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    table.erased("_sigil.motion", "rampTo", "_t.EaseLike")
    # A cue and the timeline's two reports are told nothing and answer
    # nothing: choreograph's own callback carries no value.
    table.parameters(TIMELINE + ".cue", function=NOTIFY)
    table.parameters(TIMELINE + ".setFinishFn", function=NOTIFY)
    table.parameters(TIMELINE + ".setClearedFn", function=NOTIFY)
    table.declares(
        TIMELINE,
        "apply",
        f"def apply({MOTION}) -> None:\n"
        '    """Writes a motion over this output, replacing whatever was '
        'driving it."""\n',
    )
    table.declares(
        TIMELINE,
        "append",
        f"def append({MOTION}) -> None:\n"
        '    """Adds to the end of the motion already driving this output, '
        'and writes a new one where there is none."""\n',
    )
