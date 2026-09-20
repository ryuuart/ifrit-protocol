---
kind: verb
library: SigilCompose
name: gridCells
qualified: sigil::compose::Element::gridCells
header: sigilcompose/core/verbs/Placement.h
group: Flow and placement
python: sigil.compose.Element.gridCells
status: stable
---

# gridCells

Which cells this child claims of the `layout()` scheme above it, and how
many it covers. Read by grid-shaped schemes — `layouts::Table`,
`layouts::Grid` — and by nothing else.

## Description

**Said HERE, on the child**, rather than in a list the scheme carries
beside it: a parallel list has nothing to check itself against, and an
inserted or reordered child silently shifts every entry after it onto
the wrong cell.

**A span of zero is raised to one.** Zero cells would place the child
nowhere and size it to nothing, which reads as "it vanished" rather than
as a mistake.

The `CellSpan` overload is the same claim as one value — the shape a
scheme reads it back as — so a caller computing a span passes what it
computed.

## See also

[`gridArea`](gridArea.md) for the named form, `gridCellAlign` for where the child
sits inside the cell box, and `CellSpan`.
