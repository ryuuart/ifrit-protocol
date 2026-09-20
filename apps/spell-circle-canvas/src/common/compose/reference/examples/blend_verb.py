"""blend — how a node's paint meets what is already on the canvas.

The Python twin of blend_verb.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, skia
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
BED = "#c4763c"
DISC = "#4f8fd8"
ASH = "#8ea0ad"


def cell(caption: str, mode: skia.BlendMode) -> compose.Element:
    """One bed with one disc over it, the disc blended as named."""
    return (
        compose.box(
            compose.stack(
                compose.box().cover().fill(BED),
                compose.box()
                .width(84)
                .height(84)
                .corners(42)
                .fill(DISC)
                .blend(mode)
                .left(compose.pct(30))
                .top(18),
            )
            .width(compose.pct(100))
            .height(120)
            .corners(10)
            .clip(),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .basis(0)
        .grow(1)
        .alignItems("center")
    )


@sketch(size=(640, 250), background=GROUND, capture_at=0)
class BlendVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                cell("SrcOver", skia.BlendMode.SrcOver),
                cell("Multiply", skia.BlendMode.Multiply),
                cell("Screen", skia.BlendMode.Screen),
                cell("Difference", skia.BlendMode.Difference),
            )
            .row()
            .gap(16)
            .padding(24)
        )
