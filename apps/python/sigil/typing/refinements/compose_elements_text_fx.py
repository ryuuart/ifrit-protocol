"""Text effects, tracks, beats and text units.

Input contracts for the erased signatures of the
compose-elements/text-fx package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    effects = "_sigil.compose.textFx"
    displaced = effects + ".Displaced"
    keyframe = effects + ".DisplacedKeyframe"
    tween = effects + ".DisplacedTween"
    times = "_t.DurationLike | _sigil.motion.Staggered"
    steps = f"collections.abc.Iterable[{keyframe} | {displaced}]"
    # A glyph's way home is a tween of displacements: each stop a
    # Displaced, its times the motion family's own.
    table.erased(keyframe, "__init__", displaced, "_t.EaseLike")
    table.accessor(keyframe, "to", displaced, displaced)
    table.accessor(keyframe, "ease", "_sigil.motion.Easing | None", "_t.EaseLike")
    table.erased(
        tween, "__init__", displaced, displaced, steps, times, times, "_t.EaseLike"
    )
    for name in ("from_", "to"):
        table.accessor(tween, name, displaced + " | None", displaced + " | None")
    table.accessor(tween, "keyframes", f"list[{keyframe}]", steps)
    for name in ("duration", "delay"):
        table.accessor(tween, name, "float | _sigil.motion.Staggered", times)
    table.accessor(tween, "ease", "_sigil.motion.Easing", "_t.EaseLike")
    table.returns(tween, "rest", displaced)
    # A track's master progress binds any motion value a number takes.
    table.accessor(
        "_sigil.compose.Track", "progress", "_sigil.motion.Animatable", "_t.ScalarLike"
    )
