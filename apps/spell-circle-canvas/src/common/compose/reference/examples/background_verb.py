"""background — a decoration painted beneath the fill, which is where a
shadow and anything else the surface sits on top of belongs.

The Python twin of background_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#1a2027"
PLATE = "#e9eef1"
SHADE = "#05080b8c"
ASH = "#8ea0ad"


def cell(caption: str, plate: compose.Element) -> compose.Element:
    return (
        compose.box(
            plate.width(160).height(100).borderRadius(12),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(12)
        .basis(0)
        .grow(1)
        .alignItems("center")
        .justifyContent("center")
    )


@sketch(size=(620, 250), background=GROUND, capture_at=0)
class BackgroundVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                cell("the fill alone", compose.box().fill(PLATE)),
                # Beneath the fill: the plate sits on the shadow.
                cell(
                    "background(shadow)",
                    compose.box()
                    .fill(PLATE)
                    .background(compose.shadow(SHADE, (0, 10), 18)),
                ),
                # The same mark with nothing over it — a background under
                # no fill is the whole of what the node paints.
                cell(
                    "background with no fill",
                    compose.box().background(compose.shadow(SHADE, (0, 10), 18)),
                ),
            )
            .row()
            .gap(18)
            .padding(28)
        )
