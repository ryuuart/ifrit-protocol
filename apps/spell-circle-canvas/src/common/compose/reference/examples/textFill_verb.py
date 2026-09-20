"""textFill — the glyphs painted with a material mapped to text-metric
space, so one ramp authored in the unit square crosses the capitals at
any size.

The Python twin of textFill_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, material
from sigil.sketch import SketchContext, sketch

GROUND = "#101418"
ASH = "#7e8f9c"


def chrome() -> material.Paint:
    """The chrome ramp, authored once in the unit square: the horizon
    sits where the ramp's middle stops meet, and lands on the capitals
    at whatever size the word is set."""
    return material.Paint.linearUnit(
        (0, 0),
        (0, 1),
        [
            (0.00, "#f2f6f8"),
            (0.46, "#8fa6b4"),
            (0.52, "#2b3d4a"),
            (0.58, "#cfe0e8"),
            (1.00, "#6d8593"),
        ],
    )


@sketch(size=(620, 260), background=GROUND, capture_at=0)
class TextFillVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                compose.text("CHROME", size=64).fontTrack(2).textFill(chrome()),
                compose.text("SET SMALLER", size=26).fontTrack(2).textFill(chrome()),
                compose.text(
                    "One ramp, two sizes, the same horizon.", size=13, color=ASH
                ),
            )
            .column()
            .gap(18)
            .padding(28)
            .justify("center")
        )
