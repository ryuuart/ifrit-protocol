---
kind: verb
library: SigilCompose
name: ink
qualified: sigil::compose::Element::ink
header: sigilcompose/core/verbs/Cascade.h
group: Paint
python: sigil.compose.Element.ink
status: stable
example: ink_verb
---

# ink

The colour text under this node is set in, and the colour every mark
that names none is painted in. It is CSS's `color` spelled alone, and
like CSS's it INHERITS: a node that sets it hands it to everything under
it, wherever the code that built a child ran.

<!-- example: ink_verb -->

## Syntax

```cpp
Element& ink(SkColor4f colour);
Element& ink(VarRef reference);
```

```python
def ink(self, value: ElementInkLike) -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `SkColor4f` | The colour outright. | `hexColor(0xRRGGBB)`, or the four channels |
| `VarRef` | The custom property to read it from: `ink(var("accent"))`. | [`VarRef`](../../VALUES.md#the-custom-properties), through `compose::var` |

In Python the parameter is `ElementInkLike`: a colour in any of its
written forms, or a `compose.var` reference.

## Description

**Three things flow down the tree**: the font, its colour — this — and
the custom properties. Everything else a node says about itself stays on
that node, which is CSS's own split between the properties that inherit
and the ones that do not. A node that leaves the ink unset takes the
nearest ancestor's, and the root's is the composer's inherited default.

**What reads it.** Every text leaf that names no colour of its own; every
decoration whose paint is unnamed, which is what makes
`stroke(2)` a stroke in the colour in force; and `Fill::currentInk()`,
wherever a slot demands a fill and the answer is "whatever the ink is".
A paint program reads the same value off `PaintContext::ink`.

**A change eases and does not relayout.** A node whose ink changes under
a `transition` eases it, and everything under it follows — repainted
while the colour moves, cached again once it settles. A BOUND ink is not
offered: a live value inherited from above would make the whole subtree
under it volatile.

**A property that was never set leaves the inherited ink standing** and
says so once, as every silent no-op in this library does.

## Examples

- `reference/examples/ink_verb.cpp` — one panel taking the inherited
  ink and one setting its own, with the text, the stroke and the swatch
  in each following it.
- `reference/examples/ink_verb.py` — the same picture in Python.

## See also

[`fill`](fill.md) for the node's own box, `font` and `block` for the
other two inherited lanes, `var` and `varDefaults` for the properties an
ink can be read from, and the *cascade* group on [the verb
index](../../VERBS.md).
