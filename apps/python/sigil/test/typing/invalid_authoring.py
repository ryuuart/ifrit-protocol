"""Each marked line must be rejected by the public authoring contracts."""

from sigil.compose import Element, box, graphics, memo, row, text
from sigil.compose import document as doc
from sigil.draw import Pen
from sigil.sketch import SketchContext, kit, sketch


def component(model: int) -> Element:
    return text(str(model))


def wrong_paint(pen: str) -> None:
    print(pen)


box(wdith=20)  # error: reportCallIssue
box().width(object())  # error: reportArgumentType
row().children([42])  # error: reportCallIssue,reportArgumentType
text(12)  # error: reportCallIssue,reportArgumentType
box().fill(object())  # error: reportArgumentType
box().alignItems("middle")  # error: reportArgumentType
row(text("valid"), [text("nested")])  # error: reportArgumentType
row([42])  # error: reportCallIssue,reportArgumentType
row().children(["implicit text"])  # error: reportCallIssue,reportArgumentType
row().children([None])  # error: reportCallIssue,reportArgumentType
row().children((text("valid"), None))  # error: reportCallIssue,reportArgumentType
row().children(text("valid"), "implicit text")  # error: reportArgumentType
row().children(text("valid"), [text("nested")])  # error: reportArgumentType
graphics("bad", wrong_paint)  # error: reportCallIssue,reportArgumentType
memo("wrong model", component)  # error: reportArgumentType
kit.page(box(), titlle="typo")  # error: reportCallIssue
kit.well(width=120, content="center")  # error: reportArgumentType
doc.article("Unclassified text")  # error: reportCallIssue,reportArgumentType
doc.list((doc.item("One"), None))  # error: reportCallIssue,reportArgumentType
doc.section(doc.h2("Title"), [doc.paragraph("Nested")])  # error: reportArgumentType
doc.paragraph(42)  # error: reportCallIssue,reportArgumentType
doc.heading(level="second", words="Title")  # error: reportArgumentType
doc.figure(body="A figure")  # error: reportArgumentType
doc.h1(wrods="Typo")  # error: reportCallIssue
box().role(42)  # error: reportCallIssue,reportArgumentType
box().varDefaults({doc.measure: object()})  # error: reportArgumentType
sketch(size=(200, "wide"))  # error: reportArgumentType


def draw(pen: Pen, ctx: SketchContext) -> None:
    pen.circle("left", 20, 10)  # error: reportArgumentType
    pen.line(x1=0, y1=0, x2=12, yy2=12)  # error: reportCallIssue
    ctx.render("a scene")  # error: reportArgumentType
