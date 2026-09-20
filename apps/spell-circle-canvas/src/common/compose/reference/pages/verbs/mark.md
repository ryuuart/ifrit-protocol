---
kind: verb
library: SigilCompose
name: mark
qualified: sigil::compose::Element::mark
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# mark

A sibling anchored to a unit of the text: a caret, a callout, a tick, a
rule standing at a word's edge.

## Description

```cpp
text(line, style)
    .mark(weave::selectors::word(3), box().left(0).top(pct(100))
                                          .width(pct(100)).height(2)
                                          .fill(Fill::color(ink)));
```

**`what` becomes a child of this text node whose PARENT BOX is the rect
the selector resolves to**, so it is written in exactly the placement
longhand a `positioned()` child takes — px or pct `left`, `top`,
`right`, `bottom`, `width`, `height`, measured inside that rect and free
to sit outside it. With no dimensions at all the mark simply IS the
rect.

**A mark is not a reserved slot.** A `weave::rich().slot()` reserves
space INSIDE the flow: the line breaks around it, it moves the line's
height, and the type after it starts further along. A mark reserves
nothing — the text is laid out as though it were not there and the mark
is placed on the result, so it may overlap the letters, straddle
several, or hang outside the node's box entirely. Reserve a box for
content that is part of the sentence; mark the type that is already
there.

**A selector resolving several units gives ONE rect**, the union of
every glyph it addressed, so `weave::selectors::each` anchors a mark to
the whole paragraph — a rect, and rarely the intent. One mark is one
element with one identity and one box; to mark each of several units,
write one mark per unit. A selector resolving nothing places nothing and
warns once.

**The rect is the REST rect** — where the layout put those glyphs, not
where an `fx()` track has thrown them this frame. The mark therefore
follows a reflow, a restyle and a resize exactly as the letters do, and
stands still while a cascade deviates them. A deviation is per glyph and
per track and several compose, so there is no one place a moving unit
"is", and a mark re-placed at paint would make the layout depend on the
frame. For a mark that must RIDE the motion, read `Composer::beatsOf`
and drive the mark's own transform from it.

**A mark carrying no key is given one from its declaration order**, so
it prunes; one that carries a key keeps it, and that key is what
`Composer::bounds` and `hitTest` answer for.

**It needs no reach.** A track declares one because the glyphs it throws
are painted by the text node itself; a mark is a CHILD, and the
recording cull already grows by the union of its children.

**On a path run the rect is on the CURVE**: the axis-aligned bound of
the advance boxes where the baseline placed them, the same placement
`beatsOf` reports.

## See also

`fx`, [`annotate`](annotate.md), `Composer::beatsOf`,
`weave::selectors::`.
