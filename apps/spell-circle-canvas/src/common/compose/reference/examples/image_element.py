"""image — a picture as a leaf, and the three ways it meets its box.

The picture is built here rather than loaded, so the example depends on
no asset: an eight-by-four checker, sampled nearest, whose proportions
differ from the cells it is shown in.

The Python twin of image_element.cpp. A reference example: it is
rendered with ``sigil render`` and belongs to no sketch registry.
"""

from sigil import compose, image, skia
from sigil.sketch import SketchContext, sketch

GROUND = "#14181d"
CELL = "#1c232a"
ASH = "#8ea0ad"
LIGHT = (0x6F, 0xB3, 0xA6, 0xFF)
DARK = (0x25, 0x30, 0x3A, 0xFF)


def checker() -> skia.Image:
    """Eight by four pixels of checker, so a square cell shows what each
    fit does with the proportions."""
    pixels = bytearray()
    for row in range(4):
        for column in range(8):
            pixels.extend(LIGHT if (row + column) % 2 == 0 else DARK)
    return image.from_rgba(bytes(pixels), 8, 4)


def cell(caption: str, leaf: compose.Element) -> compose.Element:
    return (
        compose.box(
            compose.box(leaf)
            .height(140)
            .width(compose.pct(100))
            .corners(8)
            .fill(CELL)
            .clip(),
            compose.text(caption, size=12, color=ASH),
        )
        .column()
        .gap(8)
        .basis(0)
        .grow(1)
        .alignItems("center")
    )


@sketch(size=(620, 260), background=GROUND, capture_at=0)
class ImageElement:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        picture = checker()
        # Nearest sampling on the container reaches every image leaf
        # under it, which is what a grid of pixels wants and a photograph
        # does not.
        return (
            compose.box(
                cell("Fit.Contain", compose.image(picture, compose.Fit.Contain).cover()),
                cell("Fit.Cover", compose.image(picture, compose.Fit.Cover).cover()),
                cell("Fit.Stretch", compose.image(picture, compose.Fit.Stretch).cover()),
            )
            .row()
            .gap(16)
            .padding(22)
            .sampling(skia.SamplingOptions(skia.FilterMode.Nearest))
        )
