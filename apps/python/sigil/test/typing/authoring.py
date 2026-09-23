"""Real component patterns checked through the installed public package."""

from dataclasses import dataclass
from pathlib import Path
from typing import assert_type

from sigil import compose as raw
from sigil.compose import (
    Band,
    Element,
    Image,
    Text,
    box,
    column,
    graphics,
    image,
    layout,
    memo,
    row,
    text,
)
from sigil.compose import document as doc
from sigil.compose import kit as marks
from sigil.compose.layouts import Grid, fr
from sigil.draw import Pen
from sigil.image import load
from sigil.material import Paint
from sigil.motion import Output, bind, entrance
from sigil.sketch import SketchContext, kit, render_file, sketch
from sigil.weave import Type, em, rich, textStyle

document_items = [doc.item("One"), doc.item(body=doc.paragraph("Two"), marker="2.")]
assert_type(doc.article(doc.h1(words="Title"), doc.paragraph("A passage")), Element)
assert_type(doc.section(tuple(document_items)), Element)
assert_type(doc.list(item for item in document_items), Element)
assert_type(doc.quote(words="A quotation"), Element)
assert_type(
    doc.quote(doc.paragraph("A quotation"), doc.caption("Attribution")), Element
)
assert_type(doc.paragraph(words=rich().add("A mixed passage")), Text)
assert_type(doc.heading(level=3, words="Section"), Text)
assert_type(doc.figure(body=box(), note="A figure"), Element)
assert_type(doc.article().var(doc.measure, em(36)), Element)
assert_type(box().role("notice", font=Type(weight=600)), Element)
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
        .padding(horizontal=20, vertical=16)
        .borderRadius(topLeft=8, topRight=8, bottomRight=4, bottomLeft=4)
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


def title_part(words: str, props: marks.Sheet) -> Text:
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

assert_type(text("Relative", size=em(1.2)), Text)
assert_type(text("Explicit", textStyle(Type(size=20))), Text)

children: list[Text] = [text("One"), text("Two")]
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


# A leaf is a node wherever a node is taken: as one child, as a container's
# argument, and in the content slot of a kit piece or a document component.
# The factory that makes it still answers the leaf's own class, so the verbs
# only that leaf states stay in reach along the chain.
picture = image(load("swatch.png"))
assert_type(picture, Image)
assert_type(picture.imageRegion((0, 0, 8, 8)).width(120), Image)
assert_type(text("Leaf").textStroke(1, "#000").fontSize(11), Text)

assert_type(row(text("Leaf"), picture), Element)
assert_type(column(text("Leaf"), picture), Element)
assert_type(box([text("Leaf"), picture]), Element)
assert_type(box().children(text("Leaf"), picture), Element)
assert_type(box().children([text("Leaf"), picture]), Element)
assert_type(layout(Grid(columns=[fr()]), text("Leaf"), picture), Element)
assert_type(doc.article(doc.h2("Title"), text("Leaf"), picture), Element)
assert_type(doc.figure(body=text("Leaf"), note="A leaf under a note"), Element)
assert_type(marks.well(picture), Element)
assert_type(marks.sheet(text("Leaf")), Element)
assert_type(kit.page(picture, title="A leaf on a page"), Element)
assert_type(memo(Reading("A leaf", 1.0), lambda model: text(model.title)), Element)


def banded(leaf: Band) -> Element:
    """A band is a node like the others, wherever one is taken."""
    return marks.sheet(row(leaf).children(leaf))
