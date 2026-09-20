---
kind: verb
library: SigilCompose
name: scaleX
qualified: sigil::compose::Element::scaleX
header: sigilcompose/core/verbs/Transform.h
group: Transform
python: sigil.compose.Element.scaleX
status: stable
---

# scaleX

Scale along X alone about the transform origin, multiplied INTO
`scale()`. Its vertical twin is `scaleY`.

## Syntax

```cpp
Element& scaleX(motion::Animatable<float> factor);
```

```python
def scale_x(self, factor: float | motion.Animatable) -> Element: ...
```

## Description

**Paint-only, like every transform lane.** Animating one never
relayouts: the content picture replays under the new transform, and a
scaled node takes exactly the room it took unscaled.

**Nearly every animated primitive a UI has is non-uniform.** Bars,
wipes, meters, cooldown sweeps, drain rings and "slide this piece into
its slot" are the common cases, and not one of them scales evenly.
Without a per-axis lane the idiom was a full-width fill inside a clip
translated by the remaining fraction of the width, which only survives
while the fill happens to be a gradient along the OTHER axis.

**Pin the growing edge with `transformOrigin`.**

```cpp
element.transformOrigin(0, 0.5f).scaleX(motion::bind(&fraction));
```

grows a bar rightward from its left edge.

## See also

`scale`, `scaleY`, `transformOrigin`, [`opacity`](opacity.md).
