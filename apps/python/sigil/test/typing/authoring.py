"""Real component patterns checked through the installed public package."""

from dataclasses import dataclass
from pathlib import Path
from typing import assert_type

from sigil.compose import Element, box, column, graphics, layout, memo, row, text
from sigil.compose import kit as marks
from sigil.compose.layouts import Grid, fr
from sigil.draw import Pen
from sigil.material import skia
from sigil.motion import Output, bind, entrance
from sigil.native import compose as raw
from sigil.sketch import SketchContext, kit, render_file, sketch
from sigil.weave import Type, em, textStyle


@dataclass(frozen=True)
class Reading:
    title: str
    level: float


def wash(accent: str) -> skia.Paint:
    return skia.Paint.linearUnit((0, 0), (1, 1), [(0, accent), (1, "#172b36")])


def component(model: Reading) -> Element:
    return (
        column()
        .gap(12)
        .padding(20, 16)
        .corners(8, 8, 4, 4)
        .alignItems("start")
        .opacity(entrance(0, 1, duration=0.5))
        .children(
            [
                text(model.title, size=22),
                (
                    box()
                    .height(8)
                    .width("100%")
                    .fill(wash("#356c69"))
                    .scaleX(model.level)
                ),
                text("A native element"),
                (
                    row().children(
                        [
                            text("with ordinary children"),
                        ]
                    )
                ),
                *[(text(str(index)).key(str(index))) for index in range(3)],
            ]
        )
    )


def paint(pen: Pen) -> None:
    pen.fill("#abcdef")
    pen.circle(12, 12, 8)


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
                    [
                        (memo(self.model, component).key("reading")),
                        native,
                        (graphics("mark", paint).width(24).height(24)),
                    ]
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
assert_type(wash("#abcdef"), skia.Paint)
assert_type(
    (
        layout(Grid(columns=[fr(), fr()])).children(
            [
                text("Left"),
                text("Right"),
            ]
        )
    ),
    Element,
)
assert_type(marks.sheet(text("Body"), title_line=title_part), Element)
assert_type(render_file(Path("scene.py"), Path("preview.png"), at=0), str)

assert_type(text("Relative", size=em(1.2)), Element)
assert_type(text("Explicit", textStyle(Type(size=20))), Element)
