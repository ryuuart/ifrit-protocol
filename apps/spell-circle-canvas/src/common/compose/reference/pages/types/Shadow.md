---
kind: type
library: SigilCompose
name: Shadow
qualified: sigil::compose::Shadow
group: Shape and edge
status: stable
---

# Shadow

CSS's box shadow as a comparable value — so a statically shadowed node
prunes with no memo. It is a decoration, not a paint: `Element::boxShadow`
lays it where CSS paints it, an outer shadow BENEATH the fill however late
in the chain it is said and an inset one over the fill and under the
content.

## Anatomy

The fields are CSS's `box-shadow` words, the same on `TextShadow` and
`material::ShadowOptions`. `Shadow::color` is the `Fill` the silhouette
is re-stamped in, `Shadow::offset` is how far it is moved,
`Shadow::blur` is how soft it is (CSS's blur radius, a Gaussian of half
of it), and `Shadow::spread` is how far the outline grows first — or
shrinks, where it is negative — its corners kept square on a square box.
`Shadow::inset` turns it inward: a band hugging the inner edge, cast
along the offset, so a downward offset shades the top inner edge.

`Shadow::knockout` is CSS's box-shadow semantics: the shape's own
footprint is cut OUT of the shadow, so nothing paints under the node. A
translucent node over a knocked-out shadow stays clear instead of
sampling its own shadow through itself.

`Shadow::bindOffsetX` and `Shadow::bindOffsetY` replace an axis of the
offset with what an animatable reads as now, and declare the decoration
animated — the hover-lift shadow slides without re-describing.
`Shadow::maximumBind` reserves the cull reach a bound range needs, because
`Shadow::bleed` cannot read a future value and a shadow that escapes the
reserved rect is simply cut off.

`Shadow::isRunning` is the volatility declaration; `Shadow::paint` is
the decoration entry point the seam calls.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `.boxShadow({.offset = {0, 4}, .blur = 12, .color = Fill::color(colour)})` | C++ | CSS's verb, one shadow or a list with the first on top |
| `.boxShadow(offset=(0, 4), blur=12, spread=0, color="#0006", inset=False)` | Python | the same, by keyword, or one `Shadow` or a list |
| `Shadow{.blur = 18.0f, .color = Fill::color(colour)}` | C++ | designated initialisers |
| `compose::shadow(color, offset, blur)` | C++ | the three-argument decoration, with `knockout` and `maximumBind` beside it |
| `compose.shadow(color, offset, blur)` | Python | the same, taking any colour spelling, a material or a fill |

The colour is a `Fill`, as every other mark's ink is: `compose::shadow` takes a
colour — a braced literal names the type, `material::Color{0, 0, 0, 0.5f}`
— or a material, which converts, and `Fill::currentInk()` and
`var(name)` follow the node's cascade where the shadow is painted.
Unstated, it is the ink in force, as CSS's `box-shadow` colour is. A
shadow cast by a node's own fill is
also the material's effect `material::Filter::shadow`, which rides the
fill rather than standing beside it.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::boxShadow` | verb | SigilCompose — where CSS paints it |
| `Element::background` | verb | SigilCompose — under the fill, by hand |
| `Element::foreground` | verb | SigilCompose — over the content, for an inner shadow drawn deliberately on top |
| `Element::overlay` | verb | SigilCompose |
| `Decoration` | type | SigilCompose — the implicit constructor every decoration slot takes |
| `decorations::paintOn` | function | SigilCompose — against geometry you built yourself |

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `compose::shadow` | function | SigilCompose |

## Description

A shadow is a value rather than a program because the reconciler can then
prove two frames asked for the same one. `Decoration` keeps a comparator
for any scheme that is equality-comparable, and this one is, so a static
shadowed node records once and replays.

Bind an offset and that stops: the decoration answers `Shadow::isRunning`
truthfully, the node is declared volatile, and the picture cache lets go
of it for as long as the binding is attached. That is the trade a moving
shadow makes, and the reason `Shadow::maximumBind` exists — the cull rect is
computed once from what the value declares, and a bound offset is the one
number the value cannot read ahead.

## See also

- `core/Shadow.h` — the header: `Shadow`, `shadow`
- `brush/Decorations.h` — the other value decorations: `PathFormat`,
  `stroke`, `Wash`, `Slice`, `ContourWalk`, `paintOn`
- [PathFormat](value:sigil::compose::PathFormat) — the other value
  decoration, and the one that formats a stroke
- [Fill](value:sigil::compose::Fill) — what the node's own surface is
  painted with, over the shadow
