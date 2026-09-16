"""A first retained composition: Python components, native kits and material paints.

EDIT THESE FIRST
  TITLE — the page heading.
  CARDS — the content and accent colors passed to one reusable component.
  card() — the layout shared by every card.

TAGS: Runtime/Starter, Geometry/Layout, Materials/Gradients, Motion/Animation
"""

from sigil.compose import column, row, text
from sigil.material import skia
from sigil.motion import entrance
from sigil.sketch import kit, sketch

TITLE = "Hello, Compose."
CARDS = [
    ("Describe", "A Python function returns a native element.", "#356c69"),
    ("Compose", "The kit supplies the page and its theme.", "#536b9c"),
    ("Animate", "The native runtime plays the entrance.", "#aa674e"),
]


def wash(accent):
    return skia.Paint.linearUnit((0, 0), (1, 1), [(0, accent), (1, "#172b36")])


def card(title, detail, accent, delay=0):
    return column(
        text(title, size=26),
        text(detail, size=14, color="#dce6e9"),
        gap=14,
        padding=24,
        height=172,
        basis=0,
        grow=1,
        corners=16,
        ink="#ffffff",
        fill=wash(accent),
        opacity=entrance(0, 1, duration=0.6, delay=delay),
        translate_y=entrance(16, 0, duration=0.6, delay=delay),
    )


@sketch(size=(900, 360), capture_at=1.1)
class HelloCompose:
    def setup(self, ctx):
        look = kit.house_theme()
        look.palette.ground = "#f4f0e6"
        look.palette.ink = "#273d41"
        look.palette.ash = "#627471"
        look.palette.rule = "#c8cec4"
        look.type.title.size = 38
        with kit.provide(look):
            ctx.render(
                kit.page(
                    row(
                        (card(*data, delay=i * 0.1) for i, data in enumerate(CARDS)),
                        gap=18,
                    ),
                    title=TITLE,
                    subtitle="One description. Layout, type, paint and motion stay native.",
                    footer="Edit TITLE, CARDS or card(), then save.",
                )
            )
