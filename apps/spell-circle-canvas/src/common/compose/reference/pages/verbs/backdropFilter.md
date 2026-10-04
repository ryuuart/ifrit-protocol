---
kind: verb
library: SigilCompose
name: backdropFilter
qualified: sigil::compose::Element::backdropFilter
header: sigilcompose/core/verbs/Effects.h
group: Effects
status: stable
example: backdropFilter_verb
---

# backdropFilter

Filters what is ALREADY PAINTED beneath the node's bounds, before the
node paints — CSS's `backdrop-filter`. The frosted panel, the blurred
sheet over a photograph, the tinted glass over a lattice.

<!-- example: backdropFilter_verb -->

## Syntax

```cpp
Element& backdropFilter(material::Filter e);
```

```python
def backdropFilter(self, effect: material.Effect) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `material::Filter` | A filter over pixels: blurs, glows, colour programs, whole recipes. | [`material::Filter`](../../VALUES.md#the-layer) — `Filter::blur` is the frosted one |

## Description

**The node's own paint goes down after the filter has run**, so a
translucent fill over a blurred backdrop is the whole frosted-glass
idiom in two verbs.

**It is incompatible with a texture cache**, because the backdrop
depends on the live destination rather than on anything the node holds:
such a node falls back to picture caching. That is the cost of the
effect, and it is worth stating out loud when a whole panel wears one.

**The result is clipped to the node's shape.** Input sampling follows the
filter's bounded reach and can read the backdrop outside that shape.

## Examples

- `reference/examples/backdropFilter_verb.cpp` — two translucent panels over
  one lattice, one of them filtering what is under it.
- `reference/examples/backdropFilter_verb.py` — the same picture in Python.

## See also

[`filter`](filter.md) for filtering the node itself,
[`blendMode`](blendMode.md) for how its paint MEETS what is beneath,
[`opacity`](opacity.md), and `cache`.
