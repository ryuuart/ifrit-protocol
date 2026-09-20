---
kind: verb
library: SigilCompose
name: preserve3d
qualified: sigil::compose::Element::preserve3d
header: sigilcompose/core/verbs/Depth.h
group: Depth
python: sigil.compose.Element.preserve3d
status: stable
---

# preserve3d

The shared space: this node's children keep the depth their own lanes
give them — their planes compose with this node's rather than
flattening into it — and are painted back to front by the depth of each
child's centre, whatever order they were declared in. A cube is six
children of one such node.

## Syntax

```cpp
Element& preserve3d(bool on = true);
```

```python
def preserve3d(self, on: bool = True) -> Element: ...
```

## Description

**Nested spaces compound.** A child that does not declare one ends the
space at its own plane, and its children are flat inside it.

**PLANES DO NOT INTERSECT.** A child crossing another is drawn whole,
in the order their centres sort. Stated so it is not discovered.

**A node that composites as a group cannot host a space.** A clip, an
opacity below 1, a blend that is not source-over, an effect, a
backdrop, a mask, a coverage boundary or an explicit `Cache::Texture` /
`Cache::Group` flattens the node exactly as CSS's grouping properties
do: its children are then projected one by one onto its plane, in tree
order, with no depth between them.

The node's own paint stands at the front of its own plane and is drawn
before its children.

## See also

`perspective`, `backface`, `rotateX`, `rotateY`, `translateZ`.
