---
kind: verb
library: SigilCompose
name: background
qualified: sigil::compose::Element::background
header: sigilcompose/core/verbs/Decoration.h
group: Paint
status: stable
example: background_verb
---

# background

A decoration painted BENEATH the fill — the CSS box-shadow ordering.
Shadows, ground textures, anything the surface sits on top of.

<!-- example: background_verb -->

## Syntax

```cpp
Element& background(Decoration d, std::string name = {});
Element& background(Spans where, Decoration what, std::string name = {});
```

```python
def background(self, decoration: DecorationLike, name: str = '') -> Element: ...
def background(self, spans: Spans, decoration: DecorationLike,
               name: str = '') -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Decoration` | The mark. | [`Decoration`](../../VALUES.md#the-marks), usually [`Shadow`](../types/Shadow.md) or [`PathFormat`](../types/PathFormat.md) |
| `Spans` | Which runs of the boundary this pass claims. | [`Spans`](../../VALUES.md#the-marks) |
| `name` | A LOCAL label, so a mask can address this mark alone. | Any string |

## Description

**Under the fill means under the fill.** An opaque fill covers a
background completely — a bevel put here renders as a flat slab. That is
the commonest way a decoration draws nothing visible, and the fix is
[`overlay`](overlay.md), which paints over the fill and under the
content.

**Repeated calls APPEND**, in declaration order, like every decoration
slot.

**The span form is `stroke(where, what)`'s twin in the other z-half.**
Everything about the two is shared deliberately: the passes append into
ONE list in declaration order, one claim record covers both halves, the
no-overlap rule reads across both, and `spans::rest()` complements both.
A boundary does not have two of itself, so a background pass and a
stroke pass claiming the same run is the same conflict as two stroke
passes doing it.

```cpp
element.background(spans::edges(14), stroke(3, shadowInk))  // under the fill
       .stroke(spans::corners(18), stroke(2, ink));         // over the kids
```

**A background under no fill is the whole of what the node paints**,
which is how a bare drop shadow, a glow or a ground texture is written.

## Examples

- `reference/examples/background_verb.cpp` — a plate on a shadow, the
  plate alone, and the shadow alone.
- `reference/examples/background_verb.py` — the same picture in Python.

## See also

[`fill`](fill.md), [`overlay`](overlay.md),
[`foreground`](foreground.md) and [`stroke`](stroke.md) for the rest of
the stacking order, and `layerStyle` for a whole bundle of decorations at
once.
