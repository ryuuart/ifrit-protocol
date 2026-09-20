# Write a sketch in Python

Python is a full authoring language here, not a wrapper. A saved `.py`
file is imported into a fresh sketch instance with no C++ compile and no
link, and composition, layout, text, motion and drawing run through the
same native libraries a C++ sketch uses. There is one element type and
no second node model to keep in sync.

## Install

The distribution is `sigil-sketch`; the import package is `sigil`.

```sh
uv venv .venv --python 3.14
uv pip install --python .venv/bin/python dist/sigil_sketch-*.whl
```

The installed command renders through the native canvas session. It
needs no Sketchbook window, no display server, no `PYTHONPATH` and no
source checkout.

## The whole file

```python
"""A first sketch: a ring of chips, and a caption under them."""

from sigil import compose, material, motion
from sigil.sketch import SketchContext, sketch

GROUND = "#11151a"
INK = "#e8eef2"
ACCENT = "#e2714b"


def chip(index: int, total: int) -> compose.Element:
    """One chip, tinted along a ramp by where it sits in the run."""
    tint = material.lerpOklab("#2f6f8f", ACCENT, index / (total - 1))
    return compose.box().width(54).height(54).corners(10).fill(tint)


@sketch(size=(620, 260), background=GROUND, capture_at=1.0)
class FirstPythonSketch:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        chips = compose.box(*[chip(i, 7) for i in range(7)]).row().gap(12)
        caption = compose.text("seven chips along a ramp", size=14, color=INK)
        return (
            compose.box(chips, caption)
            .cover()
            .column()
            .gap(22)
            .justify("center")
            .alignItems("center")
        )
```

## Run it

```sh
# one still, headless — no window, no display server
sigil render first_python_sketch.py --output preview.png

# the same, at a chosen moment on the scene clock
sigil render first_python_sketch.py --output preview.png --at 2

# live, hot-swapped on every save, in the native host
sigil open first_python_sketch.py
```

Omitting `--at` uses the capture moment the sketch declared —
`capture_at` above — or 1.5 seconds if none is declared. From Python
itself the same renderer is `render_file`:

```python
from sigil.sketch import render_file

render_file("first_python_sketch.py", "preview.png", at=2.0)
```

## How the Python spelling maps to the C++ one

Same names, same argument order, with three differences you will notice
immediately.

**Children are positional.** `compose.box(a, b)` is C++'s
`box().children({a, b})`. Unpack a list with `*`, as above.

**Enumerations are strings.** `.justify("center")` is
`Justify::Center`, `.alignItems("center")` is `Align::Center`. The
native enumeration values work too where you prefer them.

**Values have literal forms.** A colour is a CSS string, a 3- or
4-sequence of unit floats, or a `material.Color`; a dimension is a
number, `"50%"` or `"auto"`; a corner radius is a number. These are not
conveniences bolted on the side — they are the declared unions
`ColorLike`, `FillLike`, `SurfacePaintLike` and `DimensionLike`, which
the shipped type declarations name, so an editor completes and checks
them.

## Which module a name comes from

| Module | What is in it |
| --- | --- |
| `sigil.compose` | the elements, the verbs, and the composition values |
| `sigil.draw` | the pen, its verbs and the brushes |
| `sigil.material` | `Color`, `Material`, `Ramp`, `Palette` and the colour arithmetic |
| `sigil.material.skia` | `Paint` and `Effect` — the paint model, and the filter over a rendered layer |
| `sigil.material.kit` | the stock recipes, and Python's only door to a `Material`: `unlit`, `surface` |
| `sigil.motion` | `animate`, `bind`, `to`, `from_`, `Transition`, the outputs |
| `sigil.weave` | the text styles, the paragraph vocabulary |
| `sigil.skia` | the raw Skia values a canvas program uses: paths, images, `Paint` as Skia means it |
| `sigil.sketch` | `sketch`, `SketchContext`, `render_file`, and the sketch kit |

One name exists in two modules and it is worth fixing in your head once.
`sigil.skia.Paint` is Skia's own draw paint — a style, a stroke width, a
blend mode for one draw. `sigil.material.skia.Paint` is what a surface
is shaded WITH, and it is the one `fill` takes. The last word is the
same in both and the module in front is the whole of the difference.
Colour has no such split: `sigil.material.Color` is the one colour
class.

## The other authoring path

Everything above is the retained, declarative path. The immediate one is
a pen, and a sketch may use both:

```python
from sigil.draw import Pen

@sketch(size=(620, 260), background=GROUND)
class PenSketch:
    def draw(self, pen: Pen) -> None:
        pen.background(GROUND)
        pen.fill(ACCENT)
        pen.circle(310, 130, 80)
```

A `setup` that renders a tree and a `draw` that takes the pen are two
methods of the same sketch class, and a pen can place a composed element
where it stands.

## Next

- The colour chapter on the [SigilCompose](doxygen:SigilCompose) site —
  the values these calls are passing, in both languages.
- [Animate a fill](guide:animate-a-fill) — the Python spelling is
  beside each C++ one.
- The Python package's own README is the canon for installation, the
  wheel, live sketches and what is not yet bound.
