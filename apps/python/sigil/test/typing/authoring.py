"""Real component patterns checked through the installed public package."""

from dataclasses import dataclass
from pathlib import Path
from typing import assert_type

from sigil import compose as raw
from sigil.compose import Element, box, column, graphics, layout, memo, row, text
from sigil.compose import document as doc
from sigil.compose import kit as marks
from sigil.compose.layouts import Grid, fr
from sigil.draw import Pen
from sigil.material import Paint
from sigil.motion import Output, bind, entrance
from sigil.sketch import SketchContext, kit, render_file, sketch
from sigil.weave import Type, em, rich, rule, textStyle

document_items = [doc.item("One"), doc.item(body=doc.paragraph("Two"), marker="2.")]
assert_type(doc.article(doc.h1(words="Title"), doc.paragraph("A passage")), Element)
assert_type(doc.section(tuple(document_items)), Element)
assert_type(doc.list(item for item in document_items), Element)
assert_type(doc.quote(words="A quotation"), Element)
assert_type(
    doc.quote(doc.paragraph("A quotation"), doc.caption("Attribution")), Element
)
assert_type(doc.paragraph(words=rich().add("A mixed passage")), Element)
assert_type(doc.heading(level=3, words="Section"), Element)
assert_type(doc.figure(body=box(), note="A figure"), Element)
assert_type(doc.article().var(doc.measure, em(36)), Element)
assert_type(box().role(rule("notice").font(Type(weight=600))), Element)
assert_type(box().role("notice"), Element)
assert_type(box().varDefaults({doc.measure: em(30), "accent": "#123456"}), Element)


@dataclass(frozen=True)
class Reading:
    title: str
    level: float


def wash(accent: str) -> Paint:
    return Paint.linearUnit(
        start=(0, 0), end=(1, 1), stops=((0, accent), (1, "#172b36"))
    )


def component(model: Reading) -> Element:
    return (
        column()
        .gap(12)
        .padding(20, 16)
        .corners(topLeft=8, topRight=8, bottomRight=4, bottomLeft=4)
        .alignItems(alignment="start")
        .opacity(entrance(0, 1, duration=0.5))
        .children(
            text(model.title, size=22),
            (box().height(8).width("100%").fill(wash("#356c69")).scaleX(model.level)),
            text("A native element"),
            (
                row().children(
                    text("with ordinary children"),
                )
            ),
            *[(text(str(index)).key(str(index))) for index in range(3)],
        )
    )


def paint(pen: Pen) -> None:
    pen.fill("#abcdef")
    pen.circle(x=12, y=12, diameter=8)
    pen.line(start=(0, 0), end=(24, 24))


def title_part(words: str, props: marks.Sheet) -> Element:
    return text(words, size=props.marginX)


@sketch(size=(720, 420), capture_at=1)
class TypedSketch:
    model = Reading("One native scene", 0.7)

    def setup(self, ctx: SketchContext) -> None:
        look = kit.house_theme()
        look.palette.ground = "#13252e"
        look.type.title.size = 34
        progress = Output(1)
        native: Element = raw.box().width(24).fill("#abcdef")
        with kit.provide(look):
            body = (
                row()
                .opacity(bind(progress))
                .children(
                    (memo(self.model, component).key("reading")),
                    native,
                    (graphics("mark", paint).width(24).height(24)),
                )
            )
            panel = kit.well(body, width=620, height=220, corners=12)
            ctx.render(kit.page(panel, title="Typed Python"))

    def update(self, elapsed: float, ctx: SketchContext) -> None:
        if elapsed > 2:
            ctx.render(component(self.model))


assert_type(TypedSketch(), TypedSketch)
assert_type(TypedSketch().model, Reading)
assert_type(component(Reading("A", 1)), Element)
assert_type(memo(Reading("B", 0.2), component), Element)
assert_type(wash("#abcdef"), Paint)
assert_type(
    (
        layout(Grid(columns=[fr(), fr()])).children(
            text("Left"),
            text("Right"),
        )
    ),
    Element,
)
assert_type(marks.sheet(text("Body"), title_line=title_part), Element)
assert_type(render_file(Path("scene.py"), Path("preview.png"), at=0), str)

assert_type(text("Relative", size=em(1.2)), Element)
assert_type(text("Explicit", textStyle(Type(size=20))), Element)

children: list[Element] = [text("One"), text("Two")]
assert_type(row().children(children), Element)
assert_type(row().children(tuple(children)), Element)
assert_type(row().children(iter(children)), Element)
assert_type(row().children(child for child in children), Element)
assert_type(row().children((text("One"), text("Two"))), Element)
assert_type(row().children(()), Element)
assert_type(row().children(*children), Element)
assert_type(row().children(), Element)

assert_type(row(text("One"), text("Two")), Element)
assert_type(column(tuple(children)), Element)
assert_type(box(child for child in children), Element)
assert_type(layout(Grid(columns=[fr(), fr()]), children), Element)
