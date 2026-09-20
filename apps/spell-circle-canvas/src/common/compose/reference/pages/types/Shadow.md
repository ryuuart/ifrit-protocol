---
kind: type
library: SigilCompose
name: Shadow
qualified: sigil::compose::Shadow
header: sigilcompose/brush/Decorations.h
group: Shape and edge
python: sigil.compose.Shadow
status: stable
---

# Shadow

A soft drop shadow behind the node's outline, as a comparable value — so
a statically shadowed node prunes with no memo. It is a decoration, not a
paint: you attach it with `Element::background`, and you attach it BEFORE
the fill, so the fill paints over it.

## Anatomy

`Shadow::color` is the colour the silhouette is re-stamped in,
`Shadow::offset` is how far it is moved, and `Shadow::blur` is how soft
it is. Those three are the whole of the still shadow.

`Shadow::knockout` is CSS's box-shadow semantics: the shape's own
footprint is cut OUT of the shadow, so nothing paints under the node. A
translucent node over a knocked-out shadow stays clear instead of
sampling its own shadow through itself.

`Shadow::bindOffsetX` and `Shadow::bindOffsetY` replace an axis of the
offset with what an animatable reads as now, and declare the decoration
animated — the hover-lift shadow slides without re-describing.
`Shadow::maxBind` reserves the cull reach a bound range needs, because
`Shadow::bleed` cannot read a future value and a shadow that escapes the
reserved rect is simply cut off.

`Shadow::isAnimated` is the volatility declaration; `Shadow::paint` is
the decoration entry point the seam calls.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `compose::shadow(colour, offset, blur)` | C++ | the three-argument spelling |
| `Shadow{.color = colour, .blur = 18.0f}` | C++ | designated initialisers, for the fields the three-argument form does not reach |
| `compose.shadow(color, offset, blur)` | Python | the same, taking any colour spelling |

The colour is a colour and not a fill: a shadow is one flat re-stamp of
the silhouette, so there is nothing in it for a gradient or a tree
reference to mean.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `Element::background` | verb | SigilCompose — the usual place, under the fill |
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

Bind an offset and that stops: the decoration answers `Shadow::isAnimated`
truthfully, the node is declared volatile, and the picture cache lets go
of it for as long as the binding is attached. That is the trade a moving
shadow makes, and the reason `Shadow::maxBind` exists — the cull rect is
computed once from what the value declares, and a bound offset is the one
number the value cannot read ahead.

## See also

- `brush/Decorations.h` — the header: `Shadow`, `shadow`, `PathFormat`,
  `stroke`, `Wash`, `Slice`, `ContourWalk`, `paintOn`
- [PathFormat](PathFormat.md) — the other value decoration, and the one
  that formats a stroke
- [Fill](Fill.md) — what the node's own surface is painted with, over the
  shadow
