"""Typed authoring through the native typography vocabulary."""

from sigil.compose import Declarations, Element, Text, TextPath, box, frame, text
from sigil.compose import selectors as compose_selectors
from sigil.motion import Output
from sigil.skia import Paint, Path
from sigil.weave import (
    Block,
    Decoration,
    FrameOptions,
    HyphenationOptions,
    InitialLetter,
    JustificationOptions,
    KeepOptions,
    Leading,
    PaintLayer,
    ParagraphStyle,
    Story,
    TabStop,
    TabStopOptions,
    Type,
    TypeSheet,
    Unit,
    rich,
    selectors,
)

sheet = TypeSheet().set("accent", Type(color="#e7a466"))
passage = (
    rich()
    .styles(sheet)
    .add("Title ")
    .add("detail", name="accent")
    .slot("marker", (12, 12))
    .add(" text", type=Type(size=18))
)
layout = Block(
    leading=Leading.multiple(1.3),
    hyphenation=HyphenationOptions(consecutiveLimit=2),
    justification=JustificationOptions(spaceStretch=0.6),
    tabStops=TabStopOptions(stops=(TabStop(position=160),)),
)
heading = ParagraphStyle(
    keep=KeepOptions(withNext=True),
    initial=InitialLetter(lines=3, style=Type(weight=700)),
)
paint = Paint()
paint.setColor("#314052")
voice = Type(
    decorations=(Decoration(thickness=2),),
    underlays=(PaintLayer(paint=paint, offset=(2, 2)),),
)
selection = selectors.each(Unit.Word).take(1) | compose_selectors.style("accent")
node: Text = (
    text(passage)
    .font(voice)
    .block(layout)
    .paragraphStyles((heading,))
    .span(selection, Declarations().fontWeight(700))
    .textFirstBaseline(FrameOptions.FirstBaseline.CapHeight)
    .children(box().key("marker").fill("#ffffff"))
)
progress = Output(0.75)
curved: Text = text(passage).textOnPath(
    TextPath(path=Path.Circle(60, 60, 50), at=progress)
)
article = Story(passage).paragraphs((heading,))
page: Element = (
    box()
    .row()
    .children(frame(article).key("a").textThreadTo("b"), frame(article).key("b"))
)
