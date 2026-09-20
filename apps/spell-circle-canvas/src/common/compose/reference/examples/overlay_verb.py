"""overlay — the middle slot: over the fill, under the content and the
children. The same mark in the foreground slot is drawn beside it, so
the difference is the picture rather than a sentence.

The Python twin of overlay_verb.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
PLATE = "#d9a441"
INK = "#1a1206"
VIGNETTE = "#1a120673"
ASH = "#8ea0ad"


def band() -> compose.PathFormat:
    """A band 56 px wide inside the node's own boundary — wide enough to
    cross the digit standing in the middle of it, so which slot it is in
    is visible rather than described."""
    return compose.stroke(56, VIGNETTE, compose.PathFormat.Align.Inner)


def cell(caption: str, plate: compose.Element) -> compose.Element:
    return (
        compose.box(
            plate.width(compose.pct(100))
            .height(120)
            .corners(10)
            .fill(PLATE)
            .clip()
            .justify("center")
            .alignItems("center")
            .children(compose.text("47", size=52, color=INK)),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(10)
        .basis(0)
        .grow(1)
        .alignItems("center")
    )


@sketch(size=(600, 250), background=GROUND, capture_at=0)
class OverlayVerb:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                cell(
                    "overlay — over the fill, under the digit",
                    compose.box().overlay(band()),
                ),
                cell(
                    "foreground — over the digit too",
                    compose.box().foreground(band()),
                ),
            )
            .row()
            .gap(18)
            .padding(24)
        )
