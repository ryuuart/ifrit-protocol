"""Each marked line must be rejected by the public authoring contracts."""

from sigil.compose import Element, box, graphics, memo, row, text
from sigil.draw import Pen
from sigil.sketch import SketchContext, kit, sketch


def component(model: int) -> Element:
    return text(str(model))


def wrong_paint(pen: str) -> None:
    print(pen)


box(wdith=20)  # error: reportCallIssue
box().width(object())  # error: reportArgumentType
row().children([42])  # error: reportArgumentType
text(12)  # error: reportArgumentType
box().fill(object())  # error: reportArgumentType
box().alignItems("middle")  # error: reportArgumentType
row(text("implicit child"))  # error: reportCallIssue
row().children(["implicit text"])  # error: reportArgumentType
row().children([None])  # error: reportArgumentType
graphics("bad", wrong_paint)  # error: reportArgumentType
memo("wrong model", component)  # error: reportArgumentType
kit.page(box(), titlle="typo")  # error: reportCallIssue
kit.well(width=120, content="center")  # error: reportArgumentType
sketch(size=(200, "wide"))  # error: reportArgumentType


def draw(pen: Pen, ctx: SketchContext) -> None:
    pen.circle("left", 20, 10)  # error: reportArgumentType
    ctx.render("a scene")  # error: reportArgumentType
