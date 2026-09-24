"""A typography atelier: named runs, inline objects, a threaded story and a curved baseline.

TAGS: Typography/Paragraphs, Typography/Lettering, Runtime/Python
"""

from sigil.compose import (
    Cache,
    Element,
    FontStyle,
    SpanStyle,
    StyleSheet,
    Text,
    TextPath,
    TextWrap,
    box,
    column,
    frame,
    row,
    rule,
    stroke,
    text,
    var,
)
from sigil.compose import document as doc
from sigil.compose import selectors as selected
from sigil.sketch import SketchContext, kit, sketch
from sigil.skia import PathBuilder
from sigil.weave import (
    Decoration,
    HangingEdge,
    HangingTable,
    InitialLetter,
    KeepOptions,
    Leading,
    ParagraphBlock,
    ParagraphStyle,
    RichText,
    Story,
    TextAlignment,
    Type,
    rich,
)

SIZE = (1100, 900)
WIDTH = 328
TEAL = "#8ccbbb"
HIGHLIGHT = "#31584f"
STORY_LEADING = Leading.absolute(24)
# The marks this passage ends its lines on stand partly past the measure,
# so a justified edge squares on the ink rather than on the advances. The
# stock Latin table is not reachable from Python, so the three are stated.
MARGIN_HANGS = HangingTable(
    entries=(
        HangingEdge(character="-", atEnd=0.55),
        HangingEdge(character=".", atEnd=0.35),
        HangingEdge(character=",", atEnd=0.35),
    )
)

# The soft hyphens (U+00AD) are the only places a word of the story may
# break: a text leaf's hyphenation reaches no pattern hyphenator from
# Python, so the breaker takes only the breaks typed here.
ARTICLE = (
    "A sen\u00adtence is more than a string of let\u00adters. It is a small "
    "jour\u00adney: the eye finds a be\u00adgin\u00adning, gath\u00aders a "
    "rhythm, and fol\u00adlows a thought across a mea\u00adsure. An "
    "ob\u00adject can join that jour\u00adney with\u00adout be\u00adcom\u00ading "
    "a sec\u00adond lay\u00adout. A change of voice can carry em\u00adpha\u00adsis "
    "with\u00adout split\u00adting the pas\u00adsage into boxes.\n"
    "When this frame runs out of room, the next takes up the same story. "
    "No one choos\u00ades the last word of the first col\u00adumn. The na\u00adtive "
    "break\u00ader finds it from the type, the mea\u00adsure, and the space "
    "that re\u00admains."
)

# The palette is stated once as custom properties at the root the sheet is
# applied on; every figure reads it by name. The face is one family list,
# inherited by every passage under the specimens.
SHEET = StyleSheet(
    [
        rule(":root")
        .var("paper", "#edf0e8")
        .var("ink", "#172b33")
        .var("teal", TEAL)
        .var("orange", "#e5a36c")
        .var("guide", "#355057"),
        rule(".specimen")
        .fontFamily("Hoefler Text, Baskerville, serif")
        .ink(var("paper")),
        rule(".passage").fontSize(29).lineHeight(Leading.absolute(39)),
        rule(".by-sheet .signal").fontStyle(FontStyle.Italic).ink(var("teal")),
        rule(".mark").borderRadius(12).fill(var("orange")),
        rule(".mark > *").absolute().inset(7).borderRadius(5).fill(var("ink")),
        rule(".story")
        .fontSize(17)
        .lineHeight(STORY_LEADING)
        .textAlign(TextAlignment.Justify)
        .textWrap(TextWrap.Pretty)
        .paragraph(ParagraphBlock(hanging=MARGIN_HANGS)),
        rule(".inscription")
        .fontSize(23)
        .fontStyle(FontStyle.Italic)
        .ink(var("orange")),
    ]
)


def passage(with_object: bool = False) -> RichText:
    # No base style: the passage is set in the face, size and ink in force
    # where its leaf lands, and the run named "signal" is a virtual child
    # of class `signal` that the sheet may style.
    value = rich().add("A small ").add("signal", name="signal")
    if with_object:
        value.add(" ").slot("signal-mark", (24, 24), baselineDrop=4)
    return value.add(" travels through the room.")


def specimen(content: Element | Text, height: float, padding: float = 18) -> Element:
    return kit.well(
        column(content).justifyContent("center").styleClass("specimen"),
        width=WIDTH,
        height=height,
        padding=padding,
        corners=4,
    )


def by_sheet() -> Element:
    return specimen(text(passage()).styleClass("passage by-sheet"), 194)


def with_object() -> Element:
    mark = box(box()).key("signal-mark").styleClass("mark")
    return specimen(text(passage(True)).styleClass("passage").children(mark), 194)


def by_selector() -> Element:
    highlight = Decoration(kind=Decoration.Kind.Highlight, color=HIGHLIGHT)
    emphasis = SpanStyle().font(Type(weight=700, decorations=(highlight,)))
    line = text(passage()).styleClass("passage")
    return specimen(line.span(selected.style("signal"), emphasis.ink(var("teal"))), 194)


def story(passage_frame: Text) -> Element:
    return specimen(
        passage_frame.styleClass("story").width(WIDTH - 36).height(208), 244
    )


def curved() -> Element:
    # workaround: Python binds no path construction outside `sigil.skia`,
    # so the one cubic this baseline follows is built with Skia's builder.
    curve = PathBuilder().moveTo(20, 150).cubicTo(84, 12, 223, 223, 306, 63).detach()
    guide = box().absolute().inset(0).shape(curve).stroke(stroke(1, var("guide")))
    inscription = (
        text("Follow the line.")
        .styleClass("inscription")
        .absolute()
        .inset(0)
        .textOnPath(
            TextPath(path=curve, at=0.5, align=TextPath.Align.Center, exactTangent=True)
        )
    )
    legend = row(doc.label("PATH"), doc.caption("one cubic / exact tangent"))
    legend = legend.gap(14).absolute().left(18).top(209)
    return specimen(box(guide, inscription, legend).height(244), 244, padding=0)


def part(eyebrow: str, *cases: tuple[str, str, Element, str]) -> Element:
    comparison = kit.comparison(
        [
            kit.ComparisonCase(title=title, control=control, figure=figure, note=note)
            for title, control, figure, note in cases
        ],
        measure=1020,
        gap=18,
    )
    return column(doc.eyebrow(eyebrow), comparison).gap(16).flexShrink(0)


@sketch(size=SIZE, capture_at=0.05)
class TypeAtelier:
    def setup(self, ctx: SketchContext) -> None:
        # The opening paragraph alone carries the initial letter and the
        # keeps; the second is set in the paragraph setting in force.
        # An initial letter's own style reaches its shaping and not its
        # paint, so the cap is drawn in the passage's ink, not in teal.
        opening = ParagraphStyle(
            leading=STORY_LEADING,
            initial=InitialLetter(lines=3, margin=6, style=Type(color=TEAL)),
            keep=KeepOptions(widowLines=2, orphanLines=2),
            spaceAfter=12,
        )
        article = Story(rich().add(ARTICLE)).paragraphs((opening,))
        with kit.provide(kit.feature_theme(kit.Density.Spacious)):
            kit.stage(ctx, size=SIZE, capture_at=0.05)
            runs = part(
                "01 / WHAT A RUN CAN CARRY",
                (
                    "A RUN THE SHEET NAMES",
                    ".by-sheet .signal",
                    by_sheet(),
                    "The named run is a virtual child; one rule sets it in italic.",
                ),
                (
                    "AN OBJECT IN THE LINE",
                    "24 × 24 px / baseline drop 4",
                    with_object(),
                    "The orange object reserves space and travels with the words.",
                ),
                (
                    "A RANGE A SELECTOR FINDS",
                    'selectors.style("signal")',
                    by_selector(),
                    "A span lays weight and a highlight over the range it selects.",
                ),
            )
            journeys = part(
                "02 / THE SAME ENGINE, TWO KINDS OF JOURNEY",
                (
                    "A STORY BEGINS",
                    "17 px / 24 px / justified, hyphenated",
                    story(
                        frame(article)
                        .key("opening")
                        .textThreadTo("continuation")
                        .textThreadBalance()
                    ),
                    "The initial letter belongs to the story's first paragraph.",
                ),
                (
                    "AND CONTINUES HERE",
                    "threaded / balanced / hanging stops",
                    story(frame(article).key("continuation")),
                    "Both frames share one story; the breaker chooses their join.",
                ),
                (
                    "A DIFFERENT BASELINE",
                    "23 px italic / one curved run",
                    curved(),
                    "The words follow one contour with their native glyph advances.",
                ),
            )
            # The page never moves, so the specimens bake once and replay.
            ctx.render(
                kit.page(
                    column(runs, journeys)
                    .gap(30)
                    .applyStyleSheet(SHEET)
                    .cache(Cache.Texture)
                    .key("atelier"),
                    title="Words, objects, and a place to go",
                    subtitle="A Python typography atelier / named runs, paragraph rules and geometry",
                    footer="Edit the words or the sheet. Runs, inline space and frame flow remain native.",
                )
            )
