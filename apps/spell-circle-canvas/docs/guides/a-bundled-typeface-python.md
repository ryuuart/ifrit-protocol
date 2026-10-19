# A bundled typeface in Python

A word set in a font file the sketch carries, beside the same word in an
installed family, and the bundled family read back as a specimen line.
The whole file is
`apps/grimoire/sketches/python_bundled_type/python_bundled_type.py`, with
its font under `data/` beside it; open it with
`Grimoire --sketch python_bundled_type` and edit it live.

## Two lines

A font file is a resource, like an image: the session's hub reads it,
keeps the face it holds and watches the file. Loading it is also what
makes its family known, because the session's font context adopts every
face its hub reads under the family the file names. So a bundled family is
two lines — a load, once, and a family name, anywhere:

```python
        ctx.assets.hub().load(weave.Face, ctx.local("data/MachineType.ttf"))
```

```python
    word = compose.text(WORD, size=64, color=INK).fontFamily(family)
```

`family` is `"Muthur Study"` for one word and an installed list for the
other; the element cannot tell the two apart. There is no registration
step and no directory the host scans: the load is the registration. The
same family answers Weave directly — `ctx.fonts.familyTypeface("Muthur
Study")` is the held face — and the face's Skia typeface is
`face.typeface()`.

## Editing the font

Save a new `MachineType.ttf` over the old one while the sketch runs. The
hub reads the file again, the new face takes the family over in the
font context, and the declaration that re-runs is set in it. Nothing in
the sketch names the face it held before.

## Reading the family back

`weave.fonts` is the catalogue: family names, a family's styles — weight
and CSS's name for it, width, slant, the font's own name for the style —
and its variation axes. Handed the session's font context it lists the
bundled families first, marked bundled, each style with the URI it was
read from:

```python
        bundled = weave.fonts.family(BUNDLED, ctx.fonts)
```

```python
def style_line(style: weave.fonts.Style) -> str:
    slant = style.slant.name.lower()
    return f"{style.name} · {style.weight} {style.weightName} · {slant} · {style.uri}"
```

At a Python prompt, `print(weave.fonts.family("Avenir Next"))` prints the
family as one specimen line and each of its `styles` as another.

## The whole sketch

```python
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
```

## Where to go next

- **[Write a sketch in Python](guide:a-sketch-in-python)** — install,
  render a still, open it live.
- **[The verbs](/reference/SigilCompose/verbs/index.html)** — `fontFamily`
  among the cascade's verbs: the family list CSS's way, and a face stated
  directly.
