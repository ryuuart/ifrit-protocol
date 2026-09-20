"""fill — the three forms of the verb on one sheet: a colour, a paint,
and the ink in force.

The Python twin of fill_verb.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, material
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
LABEL = "#8ea0ad"
ACCENT = "#e2714b"


def swatch(square: compose.Element, spelling: str) -> compose.Element:
    """One labelled swatch: the square, and the spelling under it."""
    return (
        compose.box(
            square.width(120).height(120).corners(12),
            compose.text(spelling, size=13, color=LABEL),
        )
        .column()
        .gap(10)
        .alignItems("center")
    )


@sketch(size=(600, 220), background=GROUND, capture_at=0)
class FillVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                swatch(compose.box().fill(ACCENT), "fill(colour)"),
                swatch(
                    compose.box().fill(
                        material.skia.Paint.linearUnit(
                            (0, 0), (1, 1), [(0.0, "#2f6f8f"), (1.0, "#8f2f4f")]
                        )
                    ),
                    "fill(paint)",
                ),
                swatch(
                    compose.box().fill(compose.Fill.currentInk()),
                    "fill(the ink in force)",
                ),
            )
            .row()
            .gap(28)
            .padding(24)
            .justify("center")
            .alignItems("center")
            .ink(ACCENT)
        )
