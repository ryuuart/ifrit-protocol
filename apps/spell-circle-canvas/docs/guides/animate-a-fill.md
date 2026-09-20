# Animate a fill

Three ways, and they differ in WHO OWNS THE MOTION. Pick by that, not by
which one is shortest.

| Way | Who drives it | Reach for it when |
| --- | --- | --- |
| Describe a new colour under a transition | the runtime | the colour changes when your state changes — a hover, a selection, an arriving value |
| Bind a live output | you, from a ticker | the colour is a continuous function of time or of something you are stepping yourself |
| Put the motion inside a material paint | the paint | the mark is a shader and the thing moving is inside it |

## 1. Describe the new colour; the runtime ramps it

The description is rebuilt every frame anyway. Say what the colour IS
now, wrap it in `animate(to(...))`, and the reconciler notices the value
moved and starts a motion that retargets from wherever the colour
currently is.

```cpp
using namespace std::chrono_literals;
namespace motion = sigil::motion;

Element swatch(bool hot) {
  return box()
      .width(220)
      .height(140)
      .corners({16})
      .fill(animate(motion::to(Fill::color(hot ? kHot : kCool)), {220ms}));
}
```

```python
from sigil import motion

def swatch(hot: bool) -> compose.Element:
    return (
        compose.box()
        .width(220)
        .height(140)
        .corners(16)
        .fill(motion.animate(motion.to(HOT if hot else COOL),
                             motion.Transition(duration=0.22)))
    )
```

Two variants of the same verb:

- `animate(motion::from(a).to(b), spec)` adds an ENTRANCE: the path
  plays once, when the node first mounts, and afterwards behaves exactly
  like the plain form. Re-describing it does not restart the entrance.
- `Element::transition` sets a node default, so a plain constant on that
  node ramps instead of jumping, with no `animate` at the call site.

Nothing is stepped by hand here, and nothing is volatile: between
changes the node is a static node and caches like one.

## 2. Bind a live output

When the colour is a function of the clock, hold the value yourself and
hand the node a pointer to it. A bound fill compares by the output's
IDENTITY, never by the colour behind it, so re-describing does not
defeat the prune while still painting live.

```cpp
struct Pulse {
  choreograph::Output<Fill> tint{Fill::color(kCool)};

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = kCanvas, .background = kGround, .captureSeconds = 1.5});
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const float t = motion::phase(ticker.elapsed(), 2.0);
      tint = Fill::color(
          material::skia::toSkColor(material::lerpOklab(kCool, kHot, t)));
    });
    ctx.composer.render(describe());
  }

  Element describe() const {
    return box().cover().justify(Justify::Center).alignItems(Align::Center)
        .children({box().width(220).height(140).corners({16}).fill(&tint)});
  }
};
```

Two things in there are worth naming. The mix runs in OKLab, through
`material::lerpOklab`, because a straight sRGB mix between two saturated
colours passes through a dark band. And the ticker lambda is the only
thing that writes the output: the description reads it by pointer.

## 3. Put the motion inside the paint

When the mark is a shader rather than a colour, the fill is a
`material::skia::Paint` and the moving part is a uniform inside it. A
bound uniform makes the paint LIVE, which makes its node volatile, which
is what stops a cache freezing the first frame.

```cpp
namespace skia = sigil::material::skia;

box().width(220).height(140).corners({16})
    .fill(skia::Paint::sksl(effect).uniform("uPhase", &phase));
```

The same rule reaches the whole paint tree: a blend inherits the tier of
its layers, and a slot's source inherits it to its parent. You do not
declare volatility anywhere — the value answers `isAnimated` from how it
was built.

## The one thing you must declare

If you write a decoration or a paint program OF YOUR OWN that repaints
differently from one frame to the next, say so with
`bool isAnimated() const`. Nothing introspects on your behalf: by the
time the composer holds it, a decoration is a type-erased value with one
paint entry point, and there is no way to look inside a lambda and see
that it read the clock. Say nothing and the node is treated as static —
its first frame is recorded and replayed forever, with no error and no
warning.

Every value in the tree spells this the same way:
[`Shadow`](../../src/common/compose/reference/pages/types/Shadow.md),
[`PathFormat`](../../src/common/compose/reference/pages/types/PathFormat.md),
[`Paint`](../../src/common/material/reference/pages/types/Paint.md),
[`Effect`](../../src/common/material/reference/pages/types/Effect.md) and
[`Material`](../../src/common/material/reference/pages/types/Material.md)
all answer `isAnimated`, and it is always derived from how the value was
constructed, never a setter.

## What a fill accepts, once it moves

`Element::fill` takes a `motion::Animatable<Fill>`, and that one type
covers all of: a plain fill, a transition produced by `animate`, a bare
output pointer, and a shaped `motion::bind` chain. Beside it the same
verb takes a material paint, and the widest form of the argument is a
[`SurfacePaint`](../../src/common/compose/reference/pages/types/SurfacePaint.md).

In Python the whole union is `SurfacePaintLike`, which is what a
colouring parameter is annotated with unless it has a stated reason to
be narrower. One such reason is worth knowing: `Element::ink` takes a
colour and NOT an animatable, because the ink inherits, and a bound ink
would make every node under it volatile.

## Next

- [Colour, fill, paint and material](../../src/common/compose/reference/COLOURING.md)
  — the lattice all four of these values live in.
- [Write a sketch in Python](a-sketch-in-python.md).
