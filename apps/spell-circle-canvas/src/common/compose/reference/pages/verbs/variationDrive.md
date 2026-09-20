---
kind: verb
library: SigilCompose
name: variationDrive
qualified: sigil::compose::Element::variationDrive
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# variationDrive

Drive a variable-font axis from a bound output at DRAW time —
paint-only volatility, with no reshape and no relayout.

## Syntax

```cpp
Element& variationDrive(const char (&tag)[5],
                        const choreograph::Output<float>* value);
```

## Description

**An advance-variant axis is REFUSED.** The paint phase probes the
node's fonts once per axis; a weight axis that moves advances on most
faces is declined with a debug warning and the text draws at its shaped
coordinates. Drive GRAD — the advance-invariant weight — or re-render
discretely instead.

**It is sugar over `fx()`**, and deliberately so: it appends a
whole-text track whose deviation is a glyph modifier's axis, so a driven
axis composes with entrances, loops and every other track instead of
being a second text path they would hide. Being a track, it also draws
through the batched glyph path, so a span's band stands at its rest
placement while the letters move.

**It takes a bare OUTPUT and not an animatable**, deliberately: a drive
IS a live binding — a constant axis coordinate is the style's own
variations, not this — and the effect's identity is keyed on WHICH
output feeds it, so two drives of one axis from two outputs cannot prune
onto each other.

## See also

`fx`, [`spanStyle`](spanStyle.md), `weave::FontVariation`.
