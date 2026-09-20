"""A typography atelier: mixed runs, inline objects and a threaded story.

TAGS: Typography/Paragraphs, Typography/Lettering, Runtime/Python
"""

from sigil.compose import Element, TextPath, box, column, frame, row, stroke, text
from sigil.compose import document as doc
from sigil.compose import selectors as selected
from sigil.sketch import SketchContext, kit, sketch
from sigil.skia import PathBuilder
from sigil.weave import (
    Block,
    Decoration,
    InitialLetter,
    KeepOptions,
    Leading,
    ParagraphStyle,
    RichText,
    Story,
    StyleSheet,
    Type,
    rich,
    textStyle,
)

WIDTH, GUTTER, MEASURE = 328, 18, 1020
PAPER, INK, MUTED = "#edf0e8", "#172b33", "#667b80"
TEAL, ORANGE, GUIDE = "#8ccbbb", "#e5a36c", "#355057"

ARTICLE = (
    "A sentence is more than a string of letters. It is a small journey: "
    "the eye finds a beginning, gathers a rhythm, and follows a thought "
    "across a measure. An object can join that journey without becoming "
    "a second layout. A change of voice can carry emphasis without "
    "splitting the passage into boxes.\n"
    "When this frame runs out of room, the next takes up the same story. "
    "No one chooses the last word of the first column. The native breaker "
    "finds it from the type, the measure, and the space that remains."
)


def passage(with_object: bool = False) -> RichText:
    value = rich().add("A small ").add("signal", name="signal")
    if with_object:
        value.add(" ").slot("signal-mark", (24, 24), baselineDrop=4)
    return value.add(" travels through the room.")


def specimen(content: Element, height: float) -> Element:
    return kit.well(content, width=WIDTH, height=height, padding=18, corners=4)


def run_figure(mode: int) -> Element:
    line = text(passage(mode == 1)).font(Type(size=29, color=PAPER))
    line.block(Block(leading=Leading.absolute(39)))
    if mode == 1:
        line.children(
            box()
            .key("signal-mark")
            .borderRadius(12)
            .fill(ORANGE)
            .children(box().absolute().inset(7).borderRadius(5).fill(INK))
        )
    elif mode == 2:
        highlight = Decoration(kind=Decoration.Kind.Highlight, color="#31584f")
        line.spanStyle(
            selected.style("signal"),
            Type(color=TEAL, weight=700, decorations=(highlight,)),
        )
    return specimen(column(line).justifyContent("center"), 194)


def curved_figure() -> Element:
    curve = PathBuilder().moveTo(20, 150).cubicTo(84, 12, 223, 223, 306, 63).detach()
    guide = box().absolute().inset(0).shape(curve).stroke(stroke(1, GUIDE))
    inscription = (
        text("Follow the line.")
        .font(Type(size=23, color=ORANGE, track=0))
        .absolute()
        .inset(0)
        .onPath(
            TextPath(path=curve, at=0.5, align=TextPath.Align.Center, exactTangent=True)
        )
    )
    key = (
        row(
            doc.label("PATH").fontSize(10).ink(MUTED),
            doc.caption("one cubic / exact tangent").fontSize(10).ink(MUTED),
        )
        .gap(14)
        .absolute()
        .left(18)
        .top(209)
    )
    return kit.well(
        box(guide, inscription, key), width=WIDTH, height=244, padding=0, corners=4
    )


def case(title: str, control: str, figure: Element, note: str) -> kit.ComparisonCase:
    return kit.ComparisonCase(title=title, control=control, figure=figure, note=note)


@sketch(size=(1100, 900), capture_at=0.05)
class TypeAtelier:
    def setup(self, ctx: SketchContext) -> None:
        look = kit.study_theme()
        look.type.title.size = 34
        look.type.title.track = -0.5
        look.type.captionNote.track = 0
        book = kit.house_face(kit.Voice.Book)
        body_type = Type(face=book, size=17, track=0, color=PAPER)
        article = Story(rich(textStyle(body_type)).add(ARTICLE)).paragraphs(
            (
                ParagraphStyle(
                    leading=Leading.absolute(24),
                    initial=InitialLetter(lines=2, margin=5, style=Type(color=TEAL)),
                    keep=KeepOptions(widowLines=2, orphanLines=2),
                    spaceAfter=12,
                ),
                ParagraphStyle(leading=Leading.absolute(24)),
            )
        )
        first = frame(article).key("opening").thread("continuation").balanceChain()
        second = frame(article).key("continuation")
        sheet = StyleSheet().set("signal", Type(color=PAPER))
        with kit.provide(look):
            kit.stage(ctx, size=(1100, 900), capture_at=0.05)
            content = (
                column(
                    column(
                        doc.eyebrow("01 / WHAT A RUN CAN CARRY"),
                        kit.comparison(
                            (
                                case(
                                    "THE PASSAGE",
                                    "29 px / mixed runs",
                                    run_figure(0),
                                    "One named run shares the passage's face and measure.",
                                ),
                                case(
                                    "AN OBJECT IN THE LINE",
                                    "24 × 24 px / baseline drop 4",
                                    run_figure(1),
                                    "The orange object reserves space and travels with the words.",
                                ),
                                case(
                                    "EMPHASIS BY NAME",
                                    'selectors.style("signal")',
                                    run_figure(2),
                                    "A named run takes a highlight without becoming a child box.",
                                ),
                            ),
                            measure=MEASURE,
                            gap=GUTTER,
                        ),
                    )
                    .gap(16)
                    .shrink(0),
                    column(
                        doc.eyebrow("02 / THE SAME ENGINE, TWO KINDS OF JOURNEY"),
                        kit.comparison(
                            (
                                case(
                                    "A STORY BEGINS",
                                    "17 px / 24 px leading",
                                    specimen(
                                        column(first.width(WIDTH - 36).height(208)), 244
                                    ),
                                    "The opening letter belongs to the story's first paragraph.",
                                ),
                                case(
                                    "AND CONTINUES HERE",
                                    "threaded / balanced columns",
                                    specimen(
                                        column(second.width(WIDTH - 36).height(208)),
                                        244,
                                    ),
                                    "Both frames share content; the breaker chooses their join.",
                                ),
                                case(
                                    "A DIFFERENT BASELINE",
                                    "23 px / one curved run",
                                    curved_figure(),
                                    "The words follow one contour with their native glyph advances.",
                                ),
                            ),
                            measure=MEASURE,
                            gap=GUTTER,
                        ),
                    )
                    .gap(16)
                    .shrink(0),
                )
                .gap(30)
                .font(Type(face=book))
                .styleSheet(sheet)
            )
            ctx.render(
                kit.page(
                    content,
                    title="Words, objects, and a place to go",
                    subtitle="A Python typography atelier / native runs, paragraph rules and geometry",
                    footer="Edit the words, type or measures. Runs, inline space and frame flow remain native.",
                )
            )
