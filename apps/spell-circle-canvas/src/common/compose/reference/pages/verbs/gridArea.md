---
kind: verb
library: SigilCompose
name: gridArea
qualified: sigil::compose::Element::gridArea
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
python: sigil.compose.Element.gridArea
status: stable
---

# gridArea

Which NAMED REGION of the `layout()` scheme above it this child claims —
the same statement [`gridCells`](gridCells.md) makes, with the numbers left to
the scheme's own picture of itself.

## Description

```cpp
layout(layouts::Grid{.areas = {"head head", "nav  main"}})
    .children({masthead().gridArea("head")})
    .children({sidebar().gridArea("nav")});
```

**A name survives what four integers do not.** Insert a row into the
picture and every child stays in the region it named, where every
numbered child after the insertion would have moved one cell up.

**A name the picture does not carry is silent.** The numbers stay at
their defaults and nothing marks the claim as declared, so the child
flows into the next free cell exactly as a child that claimed nothing
does — rather than landing on cell (0, 0).

## See also

[`gridCells`](gridCells.md), `gridCellAlign`, `layouts::Grid`.
