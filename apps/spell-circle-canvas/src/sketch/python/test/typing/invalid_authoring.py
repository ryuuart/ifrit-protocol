"""Each marked line must be rejected by the public authoring contracts."""

from sigil.compose import Element, box, graphics, memo, row, text
from sigil.draw import Pen
from sigil.sketch import SketchContext, kit, sketch


def component(model: int) -> Element:
    return text(str(model))


def wrong_paint(pen: str) -> None:
    print(pen)


box(wdith=20)  # error: reportCallIssue
box(width=object())  # error: reportArgumentType
row(42)  # error: reportArgumentType
text(12)  # error: reportArgumentType
box(fill=object())  # error: reportArgumentType
box(align_items="middle")  # error: reportArgumentType
graphics(wrong_paint, key="bad")  # error: reportArgumentType
memo("wrong model", component)  # error: reportArgumentType
kit.page(box(), titlle="typo")  # error: reportCallIssue
kit.well(width=120, content="center")  # error: reportArgumentType
sketch(size=(200, "wide"))  # error: reportArgumentType


def draw(pen: Pen, ctx: SketchContext) -> None:
    pen.circle("left", 20, 10)  # error: reportArgumentType
    ctx.render("a scene")  # error: reportArgumentType
