"""filter — the node's own rendered layer post-processed: the whole
subtree, its children included, goes through the filter.

The Python twin of filter_verb.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, material
from sigil.sketch import SketchContext, sketch

GROUND = "#0f1318"
PLATE = "#1d2730"
INK = "#9fe3d4"
ASH = "#8ea0ad"


def cell(caption: str, plate: compose.Element) -> compose.Element:
    """The same subtree — a plate, a word and a rule — under each
    filter."""
    return (
        compose.box(
            plate.width(compose.pct(100))
            .height(120)
            .borderRadius(10)
            .fill(PLATE)
            .column()
            .gap(8)
            .justifyContent("center")
            .alignItems("center")
            .children(
                compose.text("SIGNAL", size=26, color=INK).fontTrack(3),
                compose.box().width(58).height(4).fill(INK),
            ),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .flexBasis(0)
        .flexGrow(1)
        .alignItems("center")
    )


@sketch(size=(640, 250), background=GROUND, capture_at=0)
class FilterVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                cell("no filter", compose.box()),
                cell(
                    "filter(blur)", compose.box().filter(material.Effect.blur(3))
                ),
                cell(
                    "filter(glow)",
                    compose.box().filter(material.Effect.glow("#3fd6b0", 9)),
                ),
            )
            .row()
            .gap(16)
            .padding(24)
        )
