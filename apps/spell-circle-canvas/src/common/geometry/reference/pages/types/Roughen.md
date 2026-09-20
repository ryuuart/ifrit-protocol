---
kind: type
library: SigilGeometry
name: Roughen
qualified: sigil::geometry::path::operations::Roughen
group: Path operators
status: stable
---

# Roughen

Seeded jitter along the contour normal. `smooth` rebuilds with
Catmull-Rom, which is Illustrator's Smooth points against Corner.

## One seeded stream

The displacement is drawn from ONE SEEDED STREAM, the same value
every other seeded thing in this tree draws from, so a roughened
outline re-rolls identically on every platform and a caller that
wants an evenly spread jitter rather than an independent one says so
with `source` — a low-discrepancy sequence roughens without the
clumps independent draws leave. Each contour draws from its own
stream, so adding one contour does not re-roll the others.

`seed` and `parameter` are the pair every seeded value in this tree
is described by, spelled the same way here as in `Distribution`: the
parameter is the sequence's own dial — a Halton base, a stratum
count — and a source that has none ignores it.

## The distorts beside it

`sigil::geometry::path::operations::Roughen`, `sigil::geometry::path::operations::Zigzag`,
`sigil::geometry::path::operations::PuckerBloat` and
`sigil::geometry::path::operations::Twirl` are the DISTORTS as parameter structs.
Each is a small value carrying its dials and applying on demand, so a
recipe stays editable — restack, retune, re-apply; the source path is
never consumed. `sigil::geometry::path::operations::chain` composes any of them
with ad-hoc lambdas.

Distorts run over the resampled-polyline currency, so they respect
contours and closure and compose with blend keys, extrude sources,
and pathfinder results alike.

## See also

- `path/Operations.h` — the header: `Roughen`, `Zigzag`,
  `PuckerBloat`, `Twirl`, `chain`
- [offset](page:SigilGeometry/functions/offset) — the boolean and
  offset family in the same header
