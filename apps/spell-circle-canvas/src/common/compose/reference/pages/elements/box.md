---
kind: element
library: SigilCompose
name: box
qualified: sigil::compose::box
header: sigilcompose/core/Factories.h
group: Containers
python: sigil.compose.box
status: stable
example: box_element
common_verbs: [row, column, gap, padding, fill, borderRadius, children, flexGrow]
---

# box

An empty node. It is the flex container — give it children and they lay
out in a row or a column with the gap and padding it states — and it is
also the plainest leaf there is: a box with no children, a size and a
fill is a rectangle, a rule, a swatch, a bar.

Nothing about a box is decided by what it holds until it holds it. That
is why most of a scene is boxes: the shape of a page is the flex lines
between them, not a component vocabulary above them.

<!-- example: box_element -->

## Syntax

```cpp
Element box();
```

```python
def box(*children: Element) -> Element: ...
def box(children: Iterable[Element], /) -> Element: ...
```

## Parameters

None. Everything a box is, it is told.

In Python the children go in the CALL as well as in the verb, so
`compose.box(a, b)` and `compose.box([a, b])` are the block that C++
writes as `box().children({a, b})`.

## Description

A box is a flex node. Its children lay out along the main axis its
`row` or `column` chose — `row` unless it says otherwise — sized by
their own dimensions, their `flexGrow` and their `flexShrink`, and spaced by the
container's `gap`. `padding` is the air inside the box, `margin` the air
outside it.

A box with no children has no intrinsic size. It is exactly as big as it
was told to be, and a box nobody sized measures zero on its main axis —
which is the commonest way a leaf draws nothing at all. State a size, or
let flex give it one with `flexGrow`, or take the whole parent with
`cover`.

The verbs are all on `Element`, so the same node that IS a container is
also the node that carries the fill, the corners, the strokes, the
transform and the cache. A box does not wrap another node to be
decorated; it is the decorated thing.

Children are declared as one block, after every verb that says what is
done to the node, because that is the order a node reads in: what this
is, then what is in it.

```cpp
box().column().gap(9).children({
    heading(),
    each(rows, row),
    footer(),
});
```

## Examples

- `reference/examples/box_element.cpp` — three cards in a row, each a
  column of boxes, one of which is a bare leaf standing in as a rule.
- `reference/examples/box_element.py` — the same picture in Python.

## See also

[`stack`](stack.md) for children that share one box, `positioned` for
children that carry their own rects, [`text`](text.md) for the leaf that
holds words, and the *Flow and placement* and *Size and spacing* groups
on [the verb index](../../VERBS.md).
