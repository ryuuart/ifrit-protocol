"""Rows, table, bars and the bordered feed plate.

Input contracts for the erased signatures of the
compose-kit/rows-plate package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    board = "_sigil.compose.kit.Board"
    table.parameters(board + ".__init__", size="_t.Vec2Like")
    table.accessor(board, "size", "_t.Vec2", "_t.Vec2Like")
    kit = "_sigil.compose.kit"
    # A part is called with as many of the values it is offered as it
    # names, in order, and answers the element drawn there.
    def part(*offered: str) -> str:
        forms = [
            f"collections.abc.Callable[[{', '.join(offered[:count])}], _t.NodeLike]"
            for count in range(len(offered) + 1)
        ]
        return " | ".join(forms) + " | None"

    def whole(*offered: str) -> str:
        return f"collections.abc.Callable[[{', '.join(offered)}], _sigil.compose.Element]"

    for owner, name, offered in (
        ("Table", "headLine", ("str", "Table")),
        ("Table", "cellLine", ("str", "Table", "int", "int", "list[str]")),
        ("Bars", "labelLine", ("str", "Bars")),
        ("Bars", "figureLine", ("float", "Bars")),
    ):
        table.parameters(f"{kit}.{owner}.__init__", **{name: part(*offered)})
        table.accessor(
            f"{kit}.{owner}", name, f"{whole(*offered)} | None", part(*offered)
        )
    table.parameters(
        f"{kit}.table",
        rows="collections.abc.Sequence[str] | collections.abc.Sequence[collections.abc.Sequence[str]]",
    )
