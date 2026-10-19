"""The stamped and swept brush kinds: Scatter, Pattern, CornerArt, Ribbon, Art.

Input contracts for the erased signatures of the
compose-decorations/brush-marks package, and nothing else: a fragment is one
author's alone.
"""

from __future__ import annotations

from .table import Table


def register(table: Table) -> None:
    """Record what pybind11 erased from this package's signatures."""
    brush = "_sigil.compose.brush"
    # A ribbon's width is any width law and its paint any fill; a material
    # keeps its layers, surface and effects. A nib or a taper reads the
    # width as one number.
    table.declares(
        brush,
        "ribbon",
        """@typing.overload
def ribbon(width: _t.ProfileLike, paint: _t.FillLike) -> Ribbon: ...
@typing.overload
def ribbon(width: float, paint: _t.FillLike, *, nibAngleDegrees: float | None = None, nibContrast: float = 0.15, widthEnd: float | None = None) -> Ribbon: ...
""",
    )
    table.accessor(brush + ".Ribbon", "fill", "_sigil.compose.Fill", "_t.FillLike")
    table.accessor(
        brush + ".Ribbon", "width", "_sigil.geometry.path.Profile", "_t.ProfileLike"
    )
