---
kind: verb
library: SigilCompose
name: area
qualified: sigil::compose::Element::area
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
python: sigil.compose.Element.area
status: stable
---

# area

Which NAMED REGION of the `layout()` scheme above it this child claims —
the same statement [`cells`](cells.md) makes, with the numbers left to
the scheme's own picture of itself.

## Syntax

```cpp
Element& area(std::string_view name);
```

```python
def area(self, name: str) -> Element: ...
```

## Description

```cpp
layout(layouts::Grid{.areas = {"head head", "nav  main"}})
    .children({masthead().area("head")})
    .children({sidebar().area("nav")});
```

**A name survives what four integers do not.** Insert a row into the
picture and every child stays in the region it named, where every
numbered child after the insertion would have moved one cell up.

**A name the picture does not carry is silent.** The numbers stay at
their defaults and nothing marks the claim as declared, so the child
flows into the next free cell exactly as a child that claimed nothing
does — rather than landing on cell (0, 0).

## See also

[`cells`](cells.md), `cellAlign`, `layouts::Grid`.
