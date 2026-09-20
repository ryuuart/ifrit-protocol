---
kind: verb
library: SigilCompose
name: foreground
qualified: sigil::compose::Element::foreground
header: sigilcompose/core/verbs/Decoration.h
group: Paint
python: sigil.compose.Element.foreground
status: stable
example: foreground_verb
---

# foreground

A decoration painted OVER the children — the top slot. A keyline that
has to survive whatever the node holds, a gloss lens over a whole
control, a grain that is meant to fall on the content too.

<!-- example: foreground_verb -->

## Syntax

```cpp
Element& foreground(Decoration d, std::string name = {});
```

```python
def foreground(self, decoration: DecorationLike, name: str = '') -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Decoration` | The mark. | [`Decoration`](../../VALUES.md#the-marks) |
| `name` | A LOCAL label, for `parts::named` and `spans::rest(name)`. | Any string |

## Description

**Above the children is the whole point, and the whole risk.** A texture
here greys out the node's own label; a keyline here survives a
full-bleed child that would have covered it. Choose this slot when the
mark is ABOUT the node as a whole, and [`overlay`](overlay.md) when it
is about the surface the content sits on.

**The unqualified strokes share this list.** `stroke(brush)` appends
here, which is why the unqualified strokes always paint before the
span-qualified passes: they are foregrounds, and the span passes are
their own list after it.

**Repeated calls APPEND**, in declaration order.

**Decorations dress the outline, and `clip` does not clip them**, so an
outer keyline on a clipped node keeps its reach.

## Examples

- `reference/examples/foreground_verb.cpp` — a keyline under a
  full-bleed child and the same keyline over it.
- `reference/examples/foreground_verb.py` — the same picture in Python.

## See also

[`overlay`](overlay.md) for the slot under the content,
[`background`](background.md) for the one under the fill,
[`stroke`](stroke.md), and `style` for a bundle that fills both halves
at once.
