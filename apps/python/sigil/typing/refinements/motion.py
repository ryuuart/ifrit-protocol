"""Conversion seams of motion values, tickers and animation descriptions.

Motion factory dispatch preserves the corresponding value family.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    table.erased("_sigil.motion.Animatable", "__init__", "_t.ScalarLike")
    table.erased("_sigil.motion.Bound", "map wave", "_t.EaseLike")
    table.erased("_sigil.motion.Transition", "__init__ ease", "_t.EaseLike")
    table.erased("_sigil.motion", "ramp", "_t.EaseLike")
    table.parameters("_sigil.motion.Ticker.add", function="_t.TickCallback")
    table.parameters(
        "_sigil.motion.Ticker.addFixed",
        function="collections.abc.Callable[[], bool | None]",
    )
    for prefix, input_type, value_type in (
        ("", "_t.FloatLike", "float"),
        ("Color", "_t.ColorLike", "_sigil.material.Color"),
        ("Fill", "_t.FillLike", "_sigil.compose.Fill"),
    ):
        table.erased("_sigil.motion." + prefix + "From", "to", input_type)
        table.returns("_sigil.motion." + prefix + "From", "to", prefix + "FromTo")
        table.erased(
            "_sigil.motion." + prefix + "Output", "__init__ set value", input_type
        )
        table.returns(
            "_sigil.motion." + prefix + "Output",
            "__call__ endValue get value",
            value_type,
        )
        table.returns("_sigil.motion." + prefix + "Transitioned", "value", value_type)
        table.returns(
            "_sigil.motion." + prefix + "Transitioned",
            "from_value",
            value_type + " | None",
        )
        table.returns(
            "_sigil.motion." + prefix + "Transitioned",
            "waypoints",
            "list[tuple[float, " + value_type + "]]",
        )
    for name in ("from_", "to", "entrance", "transition", "through", "animate"):
        declarations = []
        for prefix, value in (
            ("", "float"),
            ("Color", "_t.ColorLike"),
            ("Fill", "_sigil.compose.Fill"),
        ):
            timing = "duration: _t.FloatLike = 0.25, delay: _t.FloatLike = 0.0, ease: _t.EaseLike = None"
            if name == "from_":
                args, result = f"value: {value}", prefix + "From"
            elif name == "to":
                args, result = f"value: {value}", prefix + "To"
            elif name == "entrance":
                stop = "_t.FillLike" if prefix == "Fill" else value
                args, result = (
                    f"start: {value}, stop: {stop}, {timing}",
                    prefix + "Transitioned",
                )
            elif name == "transition":
                args, result = f"target: {value}, {timing}", prefix + "Transitioned"
            elif name == "through":
                args, result = (
                    f"frames: collections.abc.Iterable[tuple[_t.FloatLike, {value}]]",
                    prefix + "Waypoints",
                )
            else:
                args = f"path: {prefix}FromTo | {prefix}To | {prefix}Waypoints, spec: Transition | None = None, *, ease: _t.EaseLike = None"
                result = prefix + "Transitioned"
            declarations.append(
                f"@typing.overload\ndef {name}({args}) -> {result}: ..."
            )
        table.declares("_sigil.motion", name, "\n".join(declarations))
