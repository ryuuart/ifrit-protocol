"""Conversion seams of the values that change over time.

A tween, a keyframe and an animatable come in three value families — a
number, a colour and a fill — and each family reads its values the way
that family's slots do. The factories dispatch on the value named, so
each is declared once per family.
"""

from __future__ import annotations

from .table import Table

MOTION = "_sigil.motion"
STAGGERED = MOTION + ".Staggered"
TIMES = f"_t.DurationLike | {STAGGERED}"
TIME_READING = f"float | {STAGGERED}"
EASING = MOTION + ".Easing"


# (prefix, the value a slot takes, the value one reads back, the value a
# tween field takes)
FAMILIES = (
    ("", "_t.FloatLike", "float", f"_t.FloatLike | {STAGGERED}"),
    ("Color", "_t.ColorLike", "_sigil.material.Color", "_t.ColorLike"),
    ("Fill", "_t.FillLike", "_sigil.compose.Fill", "_t.FillLike"),
)


def register(table: Table) -> None:
    """Record what pybind11 erased from the motion values' signatures."""
    for prefix, value, reading, field_type in FAMILIES:
        keyframe = f"{MOTION}.{prefix}Keyframe"
        tween = f"{MOTION}.{prefix}Tween"
        animatable = f"{MOTION}.{prefix}Animatable"
        steps = f"collections.abc.Iterable[{keyframe} | {value}]"
        field_reading = TIME_READING if prefix == "" else reading

        table.erased(keyframe, "__init__", value, "_t.EaseLike")
        table.accessor(keyframe, "to", reading, value)
        table.accessor(keyframe, "ease", EASING + " | None", "_t.EaseLike")

        table.erased(
            tween, "__init__", field_type, field_type, steps, TIMES, TIMES,
            "_t.EaseLike",
        )
        for name in ("from_", "to"):
            table.accessor(tween, name, field_reading + " | None",
                  field_type + " | None")
        table.accessor(tween, "keyframes", f"list[{keyframe}]", steps)
        for name in ("duration", "delay"):
            table.accessor(tween, name, TIME_READING, TIMES)
        table.accessor(tween, "ease", EASING, "_t.EaseLike")
        table.returns(tween, "rest", reading)

        table.accessor(animatable, "value", reading, value)
        table.erased(animatable, "set", value)
        table.returns(animatable, "__call__", reading)
        table.returns(animatable, "described", tween + " | None")

    table.erased(MOTION + ".Animatable", "__init__", "_t.ScalarLike")
    table.erased(
        MOTION + ".ColorAnimatable",
        "__init__",
        f"_t.ColorLike | {MOTION}.ColorAnimatable | {MOTION}.ColorTween",
    )
    table.erased(
        MOTION + ".FillAnimatable",
        "__init__",
        f"_t.FillLike | {MOTION}.FillAnimatable | {MOTION}.FillTween"
        f" | {MOTION}.ColorTween",
    )
    table.returns(MOTION + ".Animatable", "binding", MOTION + ".Binding | None")
    table.returns(MOTION + ".Animatable", "source", MOTION + ".Animatable | None")

    table.declares(
        MOTION,
        "animatable",
        "@typing.overload\n"
        f"def animatable(value: _t.FloatLike) -> {MOTION}.Animatable: ...\n"
        "@typing.overload\n"
        f"def animatable(value: _sigil.compose.Fill) -> {MOTION}.FillAnimatable: ...\n"
        "@typing.overload\n"
        f"def animatable(value: _t.ColorLike) -> {MOTION}.ColorAnimatable: ...\n",
    )
    declarations = []
    for prefix, _, _, _ in FAMILIES:
        declarations.append(
            "@typing.overload\n"
            f"def animate(tween: {MOTION}.{prefix}Tween) -> "
            f"{MOTION}.{prefix}Animatable: ...\n"
        )
    # The keyword form reads the family off the first value named, so each
    # family is declared three times — led by `from_`, by `to` alone, or by
    # `keyframes` alone — and no two families accept the same leading
    # value: the number comes first, then the fill, and a colour is what
    # is left.
    timing = (
        f"duration: {TIMES} | None = None, delay: {TIMES} | None = None, "
        "ease: _t.EaseLike = None, loop: typing.SupportsInt = 0, "
        "loopDelay: _t.DurationLike = 0.0, "
        f"alternate: bool = False, composition: {MOTION}.Composition = ..."
    )
    for prefix, value, _, field_type in (FAMILIES[0], FAMILIES[2], FAMILIES[1]):
        if prefix == "Fill":
            value = field_type = "_sigil.compose.Fill"
        keyframe = f"{MOTION}.{prefix}Keyframe"
        steps = f"collections.abc.Iterable[{keyframe} | {value}]"
        result = f"{MOTION}.{prefix}Animatable"
        for lead in (
            f"from_: {field_type}, to: {field_type} | None = None, "
            f"keyframes: {steps} | None = None",
            f"from_: None = None, to: {field_type}, keyframes: {steps} | None = None",
            f"from_: None = None, to: None = None, keyframes: {steps}",
        ):
            declarations.append(
                f"@typing.overload\ndef animate(*, {lead}, {timing}) -> {result}: ...\n"
            )
    table.declares(MOTION, "animate", "".join(declarations))
