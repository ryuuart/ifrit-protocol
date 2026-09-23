---
kind: type
library: SigilCompose
name: PaintBox
qualified: sigil::compose::PaintBox
group: Paint
status: stable
---

# PaintBox

Which rectangle a paint's unit square is stretched over: a box of the
element, a box of the tree, or each unit of a passage. A ramp authored in
the unit square is a statement about no box at all; the `PaintBox` it is
handed with says which one it lands on.

## Anatomy

| Value | The rectangle | Taken by |
| --- | --- | --- |
| `PaintBox::Element` | The element's own box — for a passage, its text box: across the widest line, from the first line's cap top to the last line's baseline. The default. | `fill`, `ink` |
| `PaintBox::Padding` | Inside the element's border — CSS's padding-box origin. A border here is a stroke dressing the boundary, so this is the element's own box until one exists. | `fill` |
| `PaintBox::Content` | Inside the border and the padding — CSS's content-box origin. The painted area does not move with it; only where the paint starts does. | `fill` |
| `PaintBox::Subtree` | The box of the element that STATED the paint; everything under it shows its own slice of one paint. | `ink` of a node or a rule |
| `PaintBox::Canvas` | The whole canvas; elements anywhere in the tree show slices of one field, and moving one moves which slice it shows. | `fill`, `ink` of a node or a rule |
| `PaintBox::Glyph`, `Cluster`, `Word`, `Line`, `Sentence` | Each such unit of a passage, the paint restarting on every one — Weave's own unit words. | `ink` on a text leaf, a rule landing on one, a span |

## What each verb does with the others

- **`fill`** refuses the five text units — a box has none — says so once
  on the diagnostic stream, and stretches the paint over `Element`. A
  fill does not inherit, so `Subtree` is `Element`, silently. The box is
  part of the fill's own statement: a later fill replaces it.
- **`ink`** reads `Padding` and `Content` as `Element`, and a text unit
  on a node that is no text leaf as `Element` too, each said once. A
  rule keeps a text unit until it lands, and drops it the same way on a
  node that is no text leaf. A span takes only the text units: any other
  box on a span lays the paint in the passage's own coordinates. A
  colour has no unit square to stretch, so a box handed with one is
  accepted and changes nothing.

## Make one

| Spelling | Language |
| --- | --- |
| `PaintBox::Glyph` | C++ |
| `compose.PaintBox.Glyph`, passed as `box=` | Python |

```cpp
text(u8"EMBER GLASS").ink(ramp, PaintBox::Word);
box().fill(ramp, PaintBox::Canvas);
```

```python
compose.text("EMBER GLASS").ink(ramp, box=compose.PaintBox.Word)
```

## Pass it to

[`fill`](../verbs/fill.md), [`ink`](../verbs/ink.md), and the `ink` of a
rule and of a [`span`](../verbs/span.md)'s `SpanStyle`.
