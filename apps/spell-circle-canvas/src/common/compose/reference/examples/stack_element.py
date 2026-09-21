"""stack — the overlap container: every child shares the box, painted in
zIndex then declaration order.

The Python twin of stack_element.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
PLATE = "#223039"
INK = "#e8eef2"
BADGE = "#d8603f"
WASH = "#0d11168c"


@sketch(size=(520, 300), background=GROUND, capture_at=0)
class StackElement:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return compose.box(
            compose.stack(
                # The plate's own ground, filling the box it stands in.
                compose.box().cover().fill(PLATE),
                # A scrim over it: a covering child painted after the
                # ground and before the corner pieces.
                compose.box().cover().fill(WASH),
                # A pinned corner piece — an absolute child keeps its
                # insets, so two edges place it and its content sizes it.
                compose.box(compose.text("NEW", size=12, color=INK))
                .top(14)
                .right(14)
                .padding(8, 5)
                .borderRadius(4)
                .fill(BADGE),
                # A caption pinned to the other three edges.
                compose.text("Every child shares the box.", size=15, color=INK)
                .left(18)
                .right(18)
                .bottom(16),
            )
            .flexGrow(1)
            .borderRadius(12)
            .fill(PLATE)
            .overflow(compose.Overflow.Clip)
        ).padding(26)
