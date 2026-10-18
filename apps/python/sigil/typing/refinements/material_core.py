"""The value model: Material, Recipe authoring, Schema, UniformBlock, Bank, over and masking stacks.

Input contracts for the erased signatures of the
material/core package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    # A field reads back as the kind it was declared: a number, a colour,
    # or the numbers of a wider field.
    table.returns(
        "_sigil.material.Material", "get", "float | _sigil.material.Color | list[float]"
    )
    # A layer's mask is a Mask, or the mask's own fields in order: the
    # source and its channel, then optionally the range and the inversion.
    table.parameters(
        "_sigil.material.Material.layer",
        mask=(
            "Mask"
            " | tuple[Material | _t.ColorLike, ColorChannel]"
            " | tuple[Material | _t.ColorLike, ColorChannel,"
            " typing.SupportsFloat, typing.SupportsFloat]"
            " | tuple[Material | _t.ColorLike, ColorChannel,"
            " typing.SupportsFloat, typing.SupportsFloat, bool]"
            " | None"
        ),
    )
