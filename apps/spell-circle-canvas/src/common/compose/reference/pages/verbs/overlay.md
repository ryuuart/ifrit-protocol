---
kind: verb
library: SigilCompose
name: overlay
qualified: sigil::compose::Element::overlay
header: sigilcompose/core/Element.h
group: Paint
python: sigil.compose.Element.overlay
status: stable
example: overlay_verb
---

# overlay

A decoration painted OVER the fill and UNDER the content and children —
the middle slot. Hazard stripes over a surface but under the digit,
scanlines over a panel but under its readout, bevelled chrome over the
ground it dresses.

<!-- example: overlay_verb -->

## Syntax

```cpp
Element& overlay(Decoration d, std::string name = {});
```

```python
def overlay(self, decoration: DecorationLike, name: str = '') -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Decoration` | The mark: anything answering `paint(canvas, PaintContext)`. | [`Decoration`](../../VALUES.md#the-marks) |
| `name` | A LOCAL label, so `mask(parts::named(name), …)` can address this mark and nothing else. | Any string; it is not a query key |

## Description

**The stacking order is a contract, not a hint**, and picking the wrong
slot is the commonest way to draw nothing visible.
[`background`](background.md) sits beneath the FILL, so an opaque fill
covers it. [`foreground`](foreground.md) paints above the CHILDREN, so a
texture put there greys out the node's own label. This middle slot is
what everything between the two wants.

The alternative to it is a sibling stack, which costs a node and loses
the shared outline: a decoration in this slot is dressed by the node's
own shape, corners and boundary, and moves and caches with it.

**Repeated calls APPEND**, in declaration order.

**A decoration is any value that paints.** `PathFormat`, `Shadow` and
`Slice` are the ones in the box; a struct of your own with
`paint(canvas, PaintContext)` is a decoration too, and one that also
answers `operator==` prunes with no memo around it. A scheme that
repaints differently from frame to frame must say so with
`isAnimated()`, or its node is treated as static and the mark freezes.
Nothing introspects on your behalf.

## Examples

- `reference/examples/overlay_verb.cpp` — one band in this slot and the
  same band in the foreground slot, over the same digit.
- `reference/examples/overlay_verb.py` — the same picture in Python.

## See also

[`background`](background.md), [`foreground`](foreground.md),
[`fill`](fill.md), [`stroke`](stroke.md), and `mask` for gating one
named mark.
