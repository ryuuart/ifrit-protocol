"""text — the content forms: the inheriting leaf, the leaf set in a whole
style, and mixed text as a comparable value.

The Python twin of text_element.cpp. A reference example: it is rendered
with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, weave
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
INK = "#e2e9ee"
ASH = "#8ea0ad"
ACCENT = "#e0a03c"


def stated() -> weave.TextStyle:
    """A total style: a ``weave.TextStyle`` states every field itself, so
    a leaf set in one inherits nothing from the tree above it."""
    return weave.textStyle(weave.Type(size=26, color=ACCENT))


def row(caption: str, leaf: compose.Element) -> compose.Element:
    return compose.box(compose.text(caption, size=12, color=ASH), leaf).column().gap(6)


@sketch(size=(620, 300), background=GROUND, capture_at=0)
class TextElement:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                row("text(utf8)", compose.text("Set in the font and ink in force.")),
                row(
                    "text(utf8, style)",
                    compose.text("Set in a style of its own.", stated()),
                ),
                row(
                    "text(rich)",
                    compose.text(
                        weave.rich()
                        .add("Mixed text as ")
                        .add("one comparable value", weave.Type(color=ACCENT))
                    ),
                ),
            )
            .column()
            .gap(26)
            .padding(28)
            # The font and the ink everything under this node is set in.
            .fontSize(20)
            .ink(INK)
        )
