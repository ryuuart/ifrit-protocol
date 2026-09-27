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
    # One engine runs every animated value: a number or a colour with the
    # tween of its own family, and a list of numbers as siblings a stagger
    # resolves over.
    position = f"position: {MOTION}.Position = ..."
    table.declares(
        MOTION + ".Timeline",
        "add",
        "".join(
            "@typing.overload\n"
            f"def add(self, {lead}, tween: {MOTION}.{family}Tween, {position}) "
            f"-> {MOTION}.Timeline: ...\n"
            for lead, family in (
                (f"target: {MOTION}.Animatable", ""),
                (f"target: {MOTION}.ColorAnimatable", "Color"),
                (f"targets: list[{MOTION}.Animatable]", ""),
            )
        ),
    )
    table.parameters(MOTION + ".Timeline.call", callback=NOTIFY)
    table.declares(
        MOTION + ".Engine",
        "animate",
        "@typing.overload\n"
        f"def animate(self, target: {MOTION}.Animatable, tween: {MOTION}.Tween) "
        f"-> {MOTION}.Animation: ...\n"
        "@typing.overload\n"
        f"def animate(self, target: {MOTION}.ColorAnimatable, "
        f"tween: {MOTION}.ColorTween) -> {MOTION}.Animation: ...\n"
        "@typing.overload\n"
        f"def animate(self, targets: list[{MOTION}.Animatable], "
        f"tween: {MOTION}.Tween) -> {MOTION}.Timeline: ...\n",
    )
    table.parameters(MOTION + ".Engine.timer", callback="_t.TimerCallback")
