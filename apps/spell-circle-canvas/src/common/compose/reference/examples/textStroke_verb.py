"""textStroke — the letterforms thickened by a pass beneath their fill,
which is what keeps a caption legible over a busy ground.

The Python twin of textStroke_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
BUSY = "#6a8f7f"
INK = "#f4f7f9"
OUTLINE = "#121a1e"
ASH = "#8ea0ad"


def cell(caption: str, label: compose.Element) -> compose.Element:
    """The same word over the same ground, once plain and once
    engraved."""
    return (
        compose.box(
            compose.box(label)
            .width(compose.pct(100))
            .height(120)
            .borderRadius(10)
            .fill(BUSY)
            .justifyContent("center")
            .alignItems("center"),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .flexBasis(0)
        .flexGrow(1)
        .alignItems("center")
    )


@sketch(size=(600, 250), background=GROUND, capture_at=0)
class TextStrokeVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                cell(
                    "the letterforms alone",
                    compose.text("LEGIBLE", size=34, color=INK).fontTrack(1),
                ),
                cell(
                    "textStroke(4, outline)",
                    compose.text("LEGIBLE", size=34, color=INK)
                    .fontTrack(1)
                    .textStroke(4, OUTLINE),
                ),
            )
            .row()
            .gap(18)
            .padding(24)
        )
