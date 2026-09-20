---
kind: verb
library: SigilCompose
name: blend
qualified: sigil::compose::Element::blend
header: sigilcompose/core/verbs/Effects.h
group: Effects
python: sigil.compose.Element.blend
status: stable
example: blend_verb
---

# blend

How the node's paint meets what is already on the canvas. Source-over
unless it says otherwise, which is the ordinary "draw it on top".

<!-- example: blend_verb -->

## Syntax

```cpp
Element& blend(SkBlendMode mode);
```

```python
def blend(self, mode: skia.BlendMode) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `SkBlendMode` | The compositing operator — `kSrcOver`, `kMultiply`, `kScreen`, `kDifference`, and the rest of Skia's set. | Skia's own enumeration |

Python spells the members without the `k`: `skia.BlendMode.Multiply`.

## Description

**A blend that is not source-over composites the node as a GROUP.** The
node is drawn into a layer and that layer is blended, so its children
blend with what is under the node rather than with each other. That is
also a grouping property: such a node cannot host a shared 3D space, and
it flattens its children onto its own plane.

**What is beneath has to be there.** A blend resolves against the
destination, so a node blended inside a cached bake resolves against
transparent black instead of against the picture — which is why a
decoration that blends declares `blends()` and refuses its node, and
every ancestor, the automatic bake.

## Examples

- `reference/examples/blend_verb.cpp` — one disc over one bed in four
  modes.
- `reference/examples/blend_verb.py` — the same picture in Python.

## See also

[`opacity`](opacity.md) for the other group-forming verb,
[`filter`](filter.md), [`backdrop`](backdrop.md), and `cache`.
