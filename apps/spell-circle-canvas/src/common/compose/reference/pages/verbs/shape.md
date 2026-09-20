---
kind: verb
library: SigilCompose
name: shape
qualified: sigil::compose::Element::shape
header: sigilcompose/core/verbs/Shape.h
group: Shape, corners and clipping
python: sigil.compose.Element.shape
status: stable
---

# shape

The node's silhouette: a path generator over its laid-out size, in
local coordinates. Spiky dialogs, scalloped frames, any
non-rectangular chrome.

## Syntax

```cpp
Element& shape(Shape path);

template <typename K, typename F>
Element& shape(K key, F fn);   // shape(keyedShape(key, fn))
```

```python
def shape(self, value: ShapeLike) -> Element: ...
```

In Python a shape is a `compose.Shape`, a `skia.Path` already cooked, or
a function of the box's width and height that answers a path. The keyed
spelling is C++ only.

## Description

**It overrides [`corners`](corners.md) outright.** The fill surface,
the clip, every stroke pass and every outline-following decoration —
`PathFormat`, `ContourWalk` — trace this instead.

**A shape is a REGION; a stroke is a mark on its boundary.** Filling
this is [`fill`](fill.md); drawing its edge is [`stroke`](stroke.md).

**A comparable generator prunes; a raw callable does not.** Every
`shapes::` generator is a comparable value, so a shaped node prunes
exactly like an unshaped one. A raw callable is accepted as the escape
hatch, but it never compares equal, so the node re-patches and
re-records on every describe — hold a `Shape` value stable, or wrap the
node in a memo, to get pruning back.

**The keyed spelling is the generator plus what it closes over.**
`shape(key, fn)` is `shape(keyedShape(key, fn))`, and `KeyedShape`
carries the one-key-one-drawing contract the author takes on. A path
already cooked wants `shape(heldPath(p))`.

## See also

[`corners`](corners.md), [`clip`](clip.md), [`stroke`](stroke.md),
`shapes::`, `KeyedShape`.
