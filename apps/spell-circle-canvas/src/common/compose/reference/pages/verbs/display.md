---
kind: verb
library: SigilCompose
name: display
qualified: sigil::compose::Element::display
header: sigilcompose/core/verbs/Box.h
group: Size and spacing
python: sigil.compose.Element.display
status: stable
---

# display

Whether the node has a box in the layout at all. CSS `display`, as far
as a flex tree has one: `Display::Flex`, which every node is unless it
says otherwise, `Display::None` and `Display::Contents`. There is no
inline value; text set inline belongs to the paragraph a text leaf
holds.

## Description

**`Display::None` removes the node and everything under it** from the
layout, the picture and the hit test. Its siblings close the gap, a
`layout()` scheme hands it no cell, and `Composer::bounds` answers an
empty rect for it. The DESCRIPTION keeps the node, which is the
difference from leaving it out of `children()`: its instance, its keyed
state and the motions running on it survive, so saying `Display::Flex`
again puts back the node that was there rather than mounting a new one.
Nothing replays an entrance when it returns.

**It is not `opacity(0)`.** A transparent node still takes its room on
the line and is still laid out; a node with no box takes none. A
transparent node is skipped by the hit test as well, so the room is the
whole difference.

**`Display::Contents` removes only the node's own box.** Its children
are laid out as items of ITS parent's line, in its place: they take that
parent's direction, wrap, gap and justification, and a wrapper that
exists to group children in code stops being a flex item. What was said
to the wrapper about a box has nothing to apply to: a size, padding, a
fill, a shape, a clip. What it says that is not about a box still
reaches the children: its cascade (`font`, `ink`, `var`), its `key` for
the hit test, its opacity. Give a transform to the children rather than
to the wrapper, whose pivot has no box to be a fraction of.

**A `layout()` scheme places its direct children and nothing deeper.**
A `Display::None` child is skipped, so the cells close up as a flex line
does. A `Display::Contents` child is still one child of the scheme: it
is handed a cell it has no box to take, and its own children fall back
into the container's flex flow. Hand a scheme the children themselves.

**Python spells the first keyword `Display.None_`**, since `None` is the
language's own word.

## See also

`boxSizing`, `flexDirection`, `flexWrap`, [`opacity`](opacity.md),
[`children`](children.md), [`cover`](cover.md).
