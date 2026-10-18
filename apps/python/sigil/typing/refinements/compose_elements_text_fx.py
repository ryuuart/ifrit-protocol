"""Text effects, tracks, beats and text units.

Input contracts for the erased signatures of the
compose-elements/text-fx package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # The unit's range is answered as the paragraph's own range class,
    # registered with the layout after the composer that reads units.
    table.returns("_sigil.compose.TextUnit", "range", "_sigil.weave.CharRange")
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
    registerEffects(table, effects, times)


def registerEffects(table: Table, effects: str, times: str) -> None:
    """The effects a track runs, and the verbs a text leaf runs them by."""
    modifier = effects + ".GlyphModifier"
    keyframe = effects + ".GlyphModifierKeyframe"
    effect = effects + ".TextEffect"
    # Each colour term is a colour, the ink in force or a custom property,
    # read back as the fill of that colour kind.
    term = "_t.ColorLike | _sigil.compose.Fill | _sigil.compose.VarRef"
    for name in ("colorMultiplier", "colorAdd", "colorScreen"):
        table.accessor(modifier, name, "_sigil.compose.Fill", term)
    table.erased(keyframe, "__init__", "_t.EaseLike")
    table.accessor(keyframe, "ease", "_sigil.motion.Easing | None", "_t.EaseLike")
    table.parameters(
        effects + ".tween",
        keyframes=f"collections.abc.Iterable[{keyframe} | {modifier}]",
        duration=times,
        ease="_t.EaseLike",
    )
    table.parameters(
        effects + ".tint",
        from_="_t.ColorLike",
        to="_t.ColorLike",
        keyframes="collections.abc.Iterable[_sigil.motion.ColorKeyframe | _t.ColorLike]",
        duration=times,
        ease="_t.EaseLike",
    )
    table.declares(
        effects,
        "sequence",
        f"def sequence(*phases: {effects}.Phase | {effect}) -> {effect}:\n"
        '    """Phases in local time, each seeing its own window as 0 to 1."""\n',
    )
    table.declares(
        effects,
        "mix",
        f"def mix(*effects: {effect}) -> {effect}:\n"
        '    """Every effect at the same local time, composed as stacked tracks."""\n',
    )
    # The keyword form of a track: every field a track names, the effect
    # beside them.
    text = "_sigil.compose.Text"
    table.parameters(
        text + ".textFx",
        where="_sigil.weave.Selector",
        tween="_sigil.motion.Tween",
        unit="_sigil.weave.Unit",
        innerUnit="_sigil.weave.Unit",
        beatsOver="_sigil.compose.Beats",
        progress="_t.ScalarLike",
        reach="_t.FloatLike",
        continuous="bool",
    )
    table.parameters(text + ".variationDrive", value="_t.ScalarLike")
    table.parameters(text + ".textAttach", what="_t.NodeLike")
