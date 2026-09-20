---
kind: verb
library: SigilCompose
name: effect
qualified: sigil::compose::Element::effect
header: sigilcompose/core/verbs/Effects.h
group: Effects
python: sigil.compose.Element.effect
status: stable
example: effect_verb
---

# effect

Post-processes the node's own rendered layer: the fill, the content, the
children and the decorations are drawn, and the filter runs over the
result.

<!-- example: effect_verb -->

## Syntax

```cpp
Element& effect(material::skia::Effect e);
```

```python
def effect(self, effect: material.skia.Effect) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `material::skia::Effect` | A filter over pixels: blurs, glows, colour programs, whole recipes. | [`material::skia::Effect`](../../VALUES.md#the-layer) — `Effect::blur`, `Effect::glow`, `Effect::filter`, `Effect::recipe` |

## Description

**It forces a stacking context.** The subtree is drawn into a layer of
its own so the filter has something to read, which means the node is no
longer flattened into its parent's plane: a node with an effect cannot
host a shared 3D space, and its children are projected onto its plane
one by one.

**It is baked once under a texture cache.** A node held as a texture
runs the filter when the bake is taken, not per frame, so a static
subtree pays for the effect once.

**It reads the node, not the canvas.** What is beneath the node is not
in the layer; filtering THAT is [`backdrop`](backdrop.md).

## Examples

- `reference/examples/effect_verb.cpp` — the same subtree plain,
  blurred, and glowing.
- `reference/examples/effect_verb.py` — the same picture in Python.

## See also

[`backdrop`](backdrop.md), [`blend`](blend.md),
[`opacity`](opacity.md), and `cache` for how the layer is held.
