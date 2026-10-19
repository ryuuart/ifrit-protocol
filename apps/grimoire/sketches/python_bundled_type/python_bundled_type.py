"""Bundled type: a font file this sketch carries, loaded once through the
session's hub and then named by its family, beside an installed family.

The face is Muthur Study, square machine capitals drawn for this
catalogue, so it carries no third-party licence; it covers capitals,
figures and a little punctuation. Under the two words, `weave.fonts` reads
the bundled family back as a specimen line per style.
"""

# TAGS: Python, Typography/Styles

from sigil import compose, weave
from sigil.sketch import Scene2DContext, sketch

NAME = "Bundled type"
CATEGORY = "Python"

BUNDLED = "Muthur Study"
INSTALLED = "Avenir Next"
WORD = "SIGNAL 42"
GROUND = "#101318"
INK = "#eef1f5"
QUIET = "#8a96a8"
ACCENT = "#f2b35e"
MONO = "Menlo, monospace"


def caption(content: str, color: str = QUIET) -> compose.Element:
    return compose.text(content, size=13, color=color).fontFamily(MONO)


def specimen(family: str, note: str) -> compose.Element:
    word = compose.text(WORD, size=64, color=INK).fontFamily(family)
    return compose.box(word, caption(note)).column().gap(14).width(470)


def style_line(style: weave.fonts.Style) -> str:
    slant = style.slant.name.lower()
    return f"{style.name} · {style.weight} {style.weightName} · {slant} · {style.uri}"


@sketch(size=(1040, 320), background=GROUND, captureAt=0)
class BundledType:
    def setup(self, ctx: Scene2DContext) -> None:
        # Loading the file is the whole registration: the session's font
        # context adopts the face under the family the file names.
        ctx.assets.hub().load(weave.Face, ctx.local("data/MachineType.ttf"))
        bundled = weave.fonts.family(BUNDLED, ctx.fonts)
        installed = weave.fonts.family(INSTALLED, ctx.fonts)
        words = (
            compose.box(
                specimen(BUNDLED, f"bundled · {BUNDLED} · data/MachineType.ttf"),
                specimen(
                    f"{INSTALLED}, Helvetica Neue, sans-serif",
                    f"installed · {INSTALLED} · {len(installed.styles)} styles",
                ),
            )
            .row()
            .gap(40)
            .alignItems("end")
        )
        styles = (
            compose.box(
                caption("weave.fonts.family(BUNDLED, ctx.fonts)", ACCENT),
                *(caption(style_line(style), INK) for style in bundled.styles),
            )
            .column()
            .gap(8)
        )
        ctx.render(compose.box(words, styles).column().gap(56).padding(48))
