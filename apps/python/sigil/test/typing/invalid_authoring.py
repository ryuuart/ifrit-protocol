"""Each marked line must be rejected by the public authoring contracts."""

from sigil.compose import Text, box, graphics, layout, memo, row, text
from sigil.compose import document as doc
from sigil.compose import kit as marks
from sigil.compose.layouts import Grid, fr
from sigil.draw import Pen
from sigil.sketch import SketchContext, kit, sketch


def component(model: int) -> Text:
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
box().role(42)  # error: reportArgumentType
box().varDefaults({doc.measure: object()})  # error: reportArgumentType
sketch(size=(200, "wide"))  # error: reportArgumentType


def draw(pen: Pen, ctx: SketchContext) -> None:
    pen.circle("left", 20, 10)  # error: reportArgumentType
    pen.line(x1=0, y1=0, x2=12, yy2=12)  # error: reportCallIssue
    ctx.render("a scene")  # error: reportArgumentType


# A non-node is refused in every slot that takes a node, however the slot
# spells it: one child, a container's argument, a kit piece's content, a
# document component's body, and the tree a memo builder answers.
row().children(3.5)  # error: reportCallIssue,reportArgumentType
box().children(text("valid"), 42)  # error: reportArgumentType
layout(Grid(columns=[fr()]), 42)  # error: reportCallIssue,reportArgumentType
marks.sheet(42)  # error: reportArgumentType
marks.well("not a node")  # error: reportArgumentType
kit.page(42)  # error: reportArgumentType
doc.figure(body=42)  # error: reportArgumentType
memo(3, lambda model: model)  # error: reportArgumentType
