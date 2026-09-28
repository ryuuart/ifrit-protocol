"""A first retained composition: Python components, a selector sheet, native kits.

EDIT THESE FIRST
  TITLE — the page heading.
  CARDS — the content and accent colors passed to one reusable component.
  LOOK — the selector sheet the whole deck reads its type and ink from.
  card() — the layout, paint and motion shared by every card.

TAGS: Runtime/Starter, Geometry/Layout, Materials/Gradients, Motion/Animation
"""

from sigil.compose import (
    Element,
    band,
    Overflow,
    StyleSheet,
    box,
    column,
    row,
    rule,
    stack,
    text,
    var,
)
from sigil.geometry.path import Formation
from sigil.material import Paint
from sigil.skia import PathBuilder
from sigil.motion import animate
from sigil.sketch import SketchContext, kit, sketch
from sigil.weave import Type

TITLE = "Spell Circle"
CARDS = [
    ("Describe", "A Python function returns a native element.", "#356c69"),
    ("Compose", "The kit supplies the page and its theme.", "#536b9c"),
    ("lamnimate", "The native runtime plays the entrance.", "#aa674e"),
]

# One sheet keyed by selectors, stated once and applied to the deck. A rule
# holds the static partials a node's own verbs write — type, ink and custom
# properties — so paint and motion stay verbs on the element below.
LOOK = StyleSheet(
    [
        rule(":root").var("quiet", "#dce6e9"),
        rule(".card").font(Type(size=14)).ink("#ffffff"),
        rule(".card > .title").font(Type(size=24)),
        rule(".card > .detail").ink(var("quiet")),
        rule(".card:first-child > .title").font(Type(size=32)),
        # Specificity, not order, decides: the leading card's title is
        # larger wherever both rules reach the same element.
    ]
)


def wash(accent: str) -> Paint:
    return Paint.linearGradient((0, 0), (1, 1), [(0, accent), (1, "#172b36")])


def card(title: str, detail: str, accent: str, delay: float = 0) -> Element:
    return (
        column()
        .styleClass("card")
        .overflow(Overflow.Clip)
        .gap(14)
        .padding(24)
        .height(172)
        .flexBasis(0)
        .flexGrow(1)
        .borderRadius(16)
        .fill(wash(accent))
        .opacity(animate(from_=0, to=1, duration=0.6, delay=delay))
        .translateY(animate(from_=16, to=0, duration=0.6, delay=delay))
        .children(
            text(title).styleClass("title"),
            text(detail).styleClass("detail"),
        )
    )


def deck() -> Element:
    # The applying node is inside its own sheet and is that sheet's `:root`,
    # and the sheet reaches nothing above this row.
    return box().children(
        row(
            card(title, detail, accent, delay=index * 0.12)
            for index, (title, detail, accent) in enumerate(CARDS)
        )
        .gap(18)
        .applyStyleSheet(LOOK),
        row(
            swatches().margin(24),
            stack().children(swatches()),
        ).margin(16),
    )


def swatches() -> Element:
    """The paint scratch from the previous pass. Nothing renders it."""
    return (
        box()
        .maxWidth("25%")
        .children(
            box()
            .fill(wash("skyblue"))
            .padding(16)
            .children(
                text("Potato").fill(wash("green")).ink("lightgray").padding(4),
                text("Dog").fill(wash("blue")).padding(4),
                row(
                    text("Dog").fill(wash("red")).padding(24),
                    text("Dog")
                    .fill(wash("cyan"))
                    .ink("white")
                    .padding(24)
                    .margin(8, 24),
                    text("cat").fill(wash("purple")).ink("white").padding(4),
                ),
            ),
            box().fill(wash("red")).children(text("Potato")),
            # A band is a leaf of its own: a ribbon swept along a spine at a
            # width across it, taking one side of the spine or straddling it.
            # band(
            #     lambda width, height: PathBuilder()
            #     .addCircle(width / 2, height / 2, min(width, height) / 3)
            #     .detach(),
            #     12,
            # )
            # .bandAlignment(Formation.Outer)
            # .height(120)
            # .fill(wash("gold")),
        )
    )


@sketch(size=(900, 560), capture_at=1.1, background= "transparent")
class SpellCircle:
    def setup(self, ctx: SketchContext) -> None:
        look = kit.house_theme()
        look.palette.ground = (0,0,0,0)
        look.palette.ink = "#273d41"
        look.palette.ash = "#627471"
        look.palette.rule = "#c8cec4"
        look.type.title.size = 38
        with kit.provide(look):
            ctx.render(
                kit.page(
                    deck(),
                    title=TITLE,
                    subtitle="One sheet states the type. Layout, paint and motion stay verbs.",
                    footer="Edit TITLE, CARDS, LOOK or card(), then save.",
                )
            )
