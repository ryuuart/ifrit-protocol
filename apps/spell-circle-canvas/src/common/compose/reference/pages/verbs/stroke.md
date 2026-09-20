---
kind: verb
library: SigilCompose
name: stroke
qualified: sigil::compose::Element::stroke
header: sigilcompose/core/Element.h
group: Paint
python: sigil.compose.Element.stroke
status: stable
example: stroke_verb
---

# stroke

Dresses the node's BOUNDARY with a brush — fill's peer. The whole
boundary, or the runs a span claims: a rule, a keyline, a reticle's
corner brackets, a wire drawing itself along an edge.

This is the node's outline, not its letterforms. The verb that thickens
glyphs is [`textStroke`](textStroke.md).

<!-- example: stroke_verb -->

## Syntax

```cpp
Element& stroke(Decoration brush, std::string name = {});
Element& stroke(Spans where, Decoration what, std::string name = {});
```

```python
def stroke(self, decoration: DecorationLike, name: str = '') -> Element: ...
def stroke(self, spans: Spans, decoration: DecorationLike,
           name: str = '') -> Element: ...
```

## Parameters

| Value | What it is | Where one comes from |
|---|---|---|
| `Decoration` | The mark: anything answering `paint(canvas, PaintContext)`. | [`Decoration`](../../VALUES.md#the-marks), usually a [`PathFormat`](../types/PathFormat.md) |
| `Spans` | Which runs of the boundary this pass claims. | [`Spans`](../../VALUES.md#the-marks): `spans::corners`, `spans::edges`, `spans::upTo`, `spans::every`, and `\|` between them |
| `name` | A LOCAL label for this mark, for `parts::named` and `spans::rest(name)`. | Any string; it is not a query key |

In Python a decoration is a `compose.Decoration`, a `compose.PathFormat`
or a `compose.Shadow`: `compose.stroke(width, paint, align)` builds the
usual one, and a scheme written by hand is C++ only.

## Description

**Repeated calls APPEND.** Two calls are two rings, stacked in
declaration order — the Photoshop model, not a replaced property.

**The unqualified form does not CLAIM.** It overlays the whole boundary,
so stacking is free and nothing can collide. Naming a `where` is what
turns a pass into a claim on part of the boundary, and two claims that
overlap are reported out loud, naming both passes and the run. One
boundary, one mark: layering two marks on one run is a composite brush
rather than two passes.

Two exceptions are deliberate: bare `spans::rest()` claims whatever the
other passes left over, so a rule and its bracket corners are two calls
and no arithmetic, and `spans::rest(name)` is the complement of one
named pass and may overlay the others.

**Call order does not decide paint order.** The unqualified strokes
paint FIRST — they are foregrounds and share that list — and the span
passes follow in their own declaration order. Interleaving the two by
call order is not expressible; if a span pass must sit under a
whole-boundary one, make the whole-boundary one a span pass too with
`spans::every(1)`.

**The span form is exactly the mask spelling.**

```cpp
element.stroke(where, what, name);
element.stroke(what, name).mask(parts::named(name), by::spans(where));
```

Identical pixels, and the same value under the same intersection rule —
a further mask over the marks cuts this pass to the intersection, which
is how reticle brackets light up as a sweep reaches them. The one thing
the pass form does that the mask spelling does not is claim its run and
join the overlap check.

**Decorations dress the outline, and `clip` does not clip them.** A clip
cuts the fill, the content and the children; an outer stroke, a glow and
a shadow keep their reach on a clipped node.

A mark that names no colour is painted in the [`ink`](ink.md) in force.

## Examples

- `reference/examples/stroke_verb.cpp` — the boundary dressed whole,
  dressed twice, and dressed on the runs `spans::corners` claims.
- `reference/examples/stroke_verb.py` — the same picture in Python.

## See also

[`background`](background.md) for the same grammar in the other z-half,
[`foreground`](foreground.md) and [`overlay`](overlay.md) for the slots,
`boundary` for WHICH outline is dressed, `mask` for gating what is
painted, and [`fill`](fill.md) for the region inside the line.
