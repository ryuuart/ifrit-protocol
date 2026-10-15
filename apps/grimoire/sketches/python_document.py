"""One native document tree with two inherited editorial themes.

TAGS: Typography/Documents, Typography/Styles, Runtime/Python
"""

from sigil.compose import Element, StyleSheet, document, row, rule
from sigil.sketch import kit, sketch
from sigil.skia import Typeface
from sigil.weave import Leading, ParagraphBlock, Type


def passage() -> Element:
    return (
        document.article(
            document.eyebrow("FIELD JOURNAL  /  07"),
            document.h1("A field worth noticing."),
            document.lead(
                "A small observation becomes a story when its parts find a rhythm."
            ),
            document.rule().opacity(0.25),
            document.section(
                document.h2("Begin with the ordinary"),
                document.paragraph(
                    "The light crosses the table. A leaf turns toward the window. "
                    "The words stay the same in both columns; their voice belongs "
                    "to the document that holds them."
                ),
            ),
            document.list(
                document.item("Give each thought a place."),
                document.item("Let supporting details stay close."),
                document.item("Leave the measure comfortable to read."),
            ),
            document.quote(
                "A theme changes the voice. The document keeps its meaning."
            ),
            document.footer("Same content · native roles · two inherited stylesheets"),
        )
        .width(498)
        .padding(28)
        .borderRadius(5)
    )


def voice(
    ink: str, muted: str, accent: str, *, size: float, face: Typeface
) -> StyleSheet:
    return StyleSheet(
        [
            rule("article")
            .font(Type(face=face, size=size, color=ink))
            .paragraph(ParagraphBlock(leading=Leading.multiple(1.45))),
            rule("h1").font(
                Type(face=face, size=34, weight=600, track=0, color=accent)
            ),
            rule("h2").font(Type(face=face, size=19, weight=600, track=0)),
            rule("lead").font(Type(face=face, size=17, color=muted)),
            rule("eyebrow").font(Type(size=10, track=1.1, color=muted)),
            rule("quote, .quote").font(Type(color=accent)),
            rule("footer").font(Type(size=10, color=muted)),
        ]
    )


@sketch(size=(1100, 800), captureAt=0.05, theme=kit.featureTheme(kit.Density.Spacious))
class DocumentStudy:
    def setup(self) -> Element:
        first, second = passage(), passage()
        paper = first.fill("#f6f1e7").applyStyleSheet(
            voice(
                "#2a4037",
                "#66786b",
                "#8c492d",
                size=15,
                face=kit.houseFace(kit.Voice.Book),
            )
        )
        night = second.fill("#132a2a").applyStyleSheet(
            voice(
                "#d7e6dd",
                "#9fb7aa",
                "#e5bd78",
                size=15,
                face=kit.houseFace(kit.Voice.Interface),
            )
        )
        return kit.page(
            row(paper, night).gap(24).alignItems("stretch"),
            title="One document, two voices",
            subtitle="Python components carry meaning. A native stylesheet sets the reading voice.",
            footer="Edit voice() to restyle a subtree; edit passage() to change both documents.",
        )
