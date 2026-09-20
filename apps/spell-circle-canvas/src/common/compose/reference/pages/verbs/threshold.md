---
kind: verb
library: SigilCompose
name: threshold
qualified: sigil::compose::Element::threshold
header: sigilcompose/core/verbs/Decoration.h
group: Paint
python: sigil.compose.Element.threshold
status: stable
---

# threshold

How much paint counts as ink under `Boundary::Coverage` — the tolerance
the silhouette is cut at, as a fraction of full opacity.

## Syntax

```cpp
Element& threshold(float coverage);
```

```python
def threshold(self, coverage: float) -> Element: ...
```

## Description

```cpp
image(photo).key("fig").boundary(Boundary::Coverage).threshold(0.35f);
text(body, bodyStyle).flowAround("fig", 12);
```

**The default is the rule an unantialiased rasteriser uses** — the
paint reached at least half the pixel — so the traced edge is where the
drawn edge is.

**It is the dial a soft edge needs.** Lower it and a wash, a feathered
cut-out or a glow becomes silhouette; raise it and only the solid core
does. The value is clamped to [0, 1].

**Everything that asks this node for its coverage reads the same
number** — its own decorations, and any text flowing around it.

## See also

[`boundary`](boundary.md), `flowAround`, `Boundary`.
