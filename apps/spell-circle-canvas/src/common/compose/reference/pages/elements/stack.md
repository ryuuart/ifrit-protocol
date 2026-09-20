---
kind: element
library: SigilCompose
name: stack
qualified: sigil::compose::stack
header: sigilcompose/core/Factories.h
group: Containers
python: sigil.compose.stack
status: stable
example: stack_element
common_verbs: [cover, left, top, right, bottom, zIndex, clip, corners, fill]
---

# stack

An overlap container: every child shares the box, painted in zIndex then
declaration order. It is the node for a plate with things ON it — a
scrim over a photograph, a badge in a corner, a caption along the
bottom, a hit surface over the lot.

<!-- example: stack_element -->

## Syntax

```cpp
Element stack();
```

```python
def stack(*children: Element) -> Element: ...
def stack(children: Iterable[Element], /) -> Element: ...
```

## Parameters

None.

## Description

A stack makes EVERY child absolute, and it does so AFTER the child's own
layout properties, so a child cannot rejoin the flex flow from inside
one. What it keeps is its insets: `.top(12).right(12)` pins a badge to
a corner without stretching it, `.left(18).right(18).bottom(16)` pins a
caption across the foot, and `cover` fills the whole box. A child that
pins nothing sits at the top-left at its own size.

Mixed flow — some children stacked, the rest in a line — is a box with a
stack inside it, not a stack with flex children.

Paint order is `zIndex` first and declaration order second, so the
ground is written first and the things on it after, in the order they
are read. That is the whole of the rule: there is no z-shuffling pass
and nothing is reordered by what it paints.

A stack is an ordinary node in its parent's flow: size it, grow it, pin
it, decorate it, cache it like any box.

## Examples

- `reference/examples/stack_element.cpp` — a plate, a scrim over it, a
  badge pinned to one corner and a caption pinned across the foot.
- `reference/examples/stack_element.py` — the same picture in Python.

## See also

[`box`](box.md) for children in a line, `positioned` for children that
carry their own rects and skip flexbox entirely, and the *Flow and
placement* group on [the verb index](../../VERBS.md).
