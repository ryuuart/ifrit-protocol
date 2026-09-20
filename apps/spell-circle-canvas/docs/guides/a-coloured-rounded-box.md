# Put a coloured rounded box on the page

The smallest complete thing: a canvas, one box, a fill and a corner
radius. Everything else in the composition library is more of the same
shape — an element, and verbs said to it.

## The whole file, in C++

Save this anywhere on disk as `first_box.cpp`.

```cpp
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {
constexpr SkSize kCanvas = {480, 320};
constexpr SkColor4f kGround = hexColor(0x11151a);
constexpr SkColor4f kAccent = hexColor(0xe2714b);
}  // namespace

struct FirstBox {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 0});
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box()
        .cover()
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .children({box().width(220).height(140).borderRadius({16}).fill(kAccent)});
  }
};

SIGIL_SKETCH(FirstBox, "Guide", "a coloured rounded box")
```

Open it:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook first_box.cpp
```

It compiles in a couple of seconds and opens on the picture. Edit the
file and save: the window swaps to the recompiled sketch without
restarting.

## The whole file, in Python

```python
from sigil import compose
from sigil.sketch import SketchContext, sketch

GROUND = "#11151a"
ACCENT = "#e2714b"


@sketch(size=(480, 320), background=GROUND, capture_at=0)
class FirstBox:
    def setup(self, ctx: SketchContext) -> None:
        ctx.render(self.describe())

    def describe(self) -> compose.Element:
        return (
            compose.box(
                compose.box().width(220).height(140).borderRadius(16).fill(ACCENT)
            )
            .cover()
            .justify("center")
            .alignItems("center")
        )
```

Render a still, with no window and no source checkout:

```sh
sigil render first_box.py --output first_box.png
```

## What each line is

**`box()`** is an **element**: a free function that returns an `Element`
and starts a tree. An element is a cheap value, built fresh every frame
and thrown away — you do not keep one and mutate it. The other factories
are on [the element index](/reference/SigilCompose/elements/index.html).

**`.width`, `.borderRadius`, `.fill`, `.justify`** are **verbs**: members that
return the element by reference, so they chain. The order you say them
in does not matter. They are grouped by concern on
[the verb index](/reference/SigilCompose/verbs/index.html).

**`.children({...})`** takes the whole block of children at the end,
after everything said about the node itself. In Python the children are
the positional arguments to the factory, which reads the same way.

**`.cover()`** makes the outer box fill the canvas, so the centring has
something to centre inside.

## The two values you just passed

`fill` takes a colour here, but that is the narrowest of what it
accepts — it takes a gradient, a shader, a moving colour and a whole
material just as happily. The value is a
[`Fill`](value:sigil::compose::Fill), and the widest form of the same
argument is a [`SurfacePaint`](value:sigil::compose::SurfacePaint). If
you are unsure which of the four colour-ish things to reach for, read
the colour chapter on the [SigilCompose](doxygen:SigilCompose) site once
and you will not have to ask again.

`borderRadius` takes a [`Corners`](value:sigil::compose::Corners) — one
number rounds all four, four numbers dress each corner. The radius is a
plain number and not a length, because a radius changes what is drawn
and never what is measured.

The sizes are [`Dimension`](value:sigil::compose::Dimension)s.
A bare number is pixels, which is why `width(220)` reads as a number;
`width(50_pct)` is half the parent, `width(6_pw)` is six percent of the
canvas whatever the parent is, and `width(2_rem)` follows the root's
font.

## Next

- Give it a border: `.stroke(stroke(1.5f, Fill::color(kAccent)))`, whose
  value is a [`PathFormat`](value:sigil::compose::PathFormat).
- Give it a shadow: `.background(shadow(kGround, {0, 6}, 18))`, whose
  value is a [`Shadow`](value:sigil::compose::Shadow) — and note it goes
  on `background`, which paints beneath the fill rather than over it.
- Make the colour move: [Animate a fill](guide:animate-a-fill).
