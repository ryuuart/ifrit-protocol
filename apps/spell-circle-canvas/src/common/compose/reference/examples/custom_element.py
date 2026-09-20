"""custom — a box whose content is one paint program.

The C++ `custom` leaf has no Python binding. The pen door is the Python
spelling for a node that draws its own content: ``compose.pen`` takes a
key and a program exactly as `custom` does, and hands it the pen instead
of the canvas.

The Python twin of custom_element.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose
from sigil.draw import Pen
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
CELL = "#1b2229"
ASH = "#8ea0ad"


def rings(pen: Pen) -> None:
    """The program: rings measured off the box the node was laid out at,
    which the pen reports as its own width and height."""
    centre_x, centre_y = pen.width / 2, pen.height / 2
    pen.noFill()
    for ring in range(1, 8):
        t = ring / 7
        pen.strokeWeight(1.0 + 2.0 * t)
        pen.stroke(f"#6fb3a6{round(255 * (1.0 - 0.9 * t)):02x}")
        pen.circle(centre_x, centre_y, 28.0 * ring)


@sketch(size=(520, 280), background=GROUND, capture_at=0)
class CustomElement:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                # The key is the program's identity: one key, one
                # drawing. A pen leaf sizes like an empty box, so this
                # one states its own dimensions.
                compose.pen("reference/rings", rings)
                .width(compose.pct(100))
                .height(190)
                .borderRadius(10)
                .fill(CELL)
                .clip(),
                compose.text("compose.pen(key, program)", size=12, color=ASH),
            )
            .column()
            .gap(10)
            .padding(24)
            .alignItems("center")
        )
