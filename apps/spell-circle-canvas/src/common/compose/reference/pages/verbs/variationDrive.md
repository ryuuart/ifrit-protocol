---
kind: verb
library: SigilCompose
name: variationDrive
qualified: sigil::compose::Text::variationDrive
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# variationDrive

Drive a variable-font axis from a live value at DRAW time —
paint-only volatility, with no reshape and no relayout.

## Syntax

```cpp
Element& variationDrive(const char (&tag)[5],
                        motion::Animatable<float> value);
```

## Description

**An advance-variant axis is REFUSED.** The paint phase probes the
node's fonts once per axis; a weight axis that moves advances on most
faces is declined with a debug warning and the text draws at its shaped
coordinates. Drive GRAD — the advance-invariant weight — or re-render
discretely instead.

**It is sugar over `textFx()`**, and deliberately so: it appends a
whole-text track whose deviation is a glyph modifier's axis, so a driven
axis composes with entrances, loops and every other track instead of
being a second text path they would hide. Being a track, it also draws
through the batched glyph path, so a span's band stands at its rest
placement while the letters move.

**It takes an animatable, and a live one is what makes it a drive.** A
constant sits the axis at that coordinate and never moves, which is what
the style's own variations already say; a `motion::animatable` or a
`motion::bind` over one moves it every frame. The effect's identity is
keyed on WHICH live value feeds it — the cell it reads, never the number
behind it — so two drives of one axis from two live values cannot prune
onto each other.

## See also

`textFx`, [`span`](span.md), `weave::FontVariation`.
