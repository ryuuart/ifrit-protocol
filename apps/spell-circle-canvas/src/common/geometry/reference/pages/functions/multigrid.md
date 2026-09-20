---
kind: function
library: SigilGeometry
name: multigrid
qualified: sigil::geometry::path::multigrid
group: Lattices
status: stable
---

# multigrid

N FAMILIES OF PARALLEL LINES DUALISED INTO A TILING OF RHOMBS — de
Bruijn's construction, which is the one operation an aperiodic rhomb
tiling is: each crossing of two lines becomes a rhomb whose edges are
the two families' normals, placed by counting how many lines of every
family stand between the crossing and the origin.

Five evenly spread families give the Penrose rhombs, four the
Ammann-Beenker squares and 45-degree rhombs, three the rhombille;
families at angles of the caller's own choosing give the tiling those
angles admit, and there is no other operation behind any of them.

## Solved in double, and the answer is in double

The place of a rhomb is a count of lines, and a count is read off a
crossing by a ceiling: a crossing that lands a hair on the wrong side
of a line moves that rhomb a whole edge, so the arithmetic that finds
it has to hold more digits than the picture it ends up in. A grid
rounded to float before it is dualised does not merely blur — it tiles
differently.

## The offsets must be regular, and a singular set is refused

Placing a rhomb means counting, for every family the crossing does not
belong to, how many of that family's lines stand between the crossing
and the origin — and a count exists only where the crossing lies
strictly between two of them. A point that lines of THREE or more
families run through has no such count: which side of the third line it
is read on is settled by the last digit of the arithmetic rather than
by the geometry, and the rhomb moves a whole edge with the answer, so
the patch comes back with a rhomb missing, or two rhombs on top of each
other, or two corners welded that are not one corner. Offsets are
REGULAR when no point of the plane lies on the lines of three or more
families, and SINGULAR when one does; a singular set answers an empty
tiling, the way families that cannot span the plane do. The judgement
covers the crossings `radius` asks for — lines meeting beyond the reach
cannot move a rhomb inside it.

Almost every offset set is regular, and the singular ones are the
exact coincidences a caller reaches for on purpose. ALL-ZERO OFFSETS
are singular whatever the families, since line zero of every one of
them runs through the origin. A ring of THREE families is singular
exactly when its offsets sum to a whole number, because three normals
spread over a whole turn sum to zero and the coincidence then repeats
at every crossing in the plane. For a longer ring the sum decides
nothing: five families at a fifth each sum to one and are regular,
and the tiling they dualise into is exactly fivefold about the
origin — which is the tiling a caller reaching for zero offsets was
after. Nudging a singular set instead of refusing it would answer,
but with one of the several tilings the singular grid stands between,
picked by the direction of the nudge rather than by the caller, and
for a symmetric member it would answer with a tiling that no longer
carries the symmetry that was asked for.

Two families that face the same way never cross and bound no rhomb,
and are passed over. Fewer than two families dualise into nothing.

## The ring

`sigil::geometry::path::multigridRing` answers `count` families evenly
spread, all at one spacing and one phase. The spread is a WHOLE TURN
for an odd count and a HALF TURN for an even one, which is the smallest
turn giving `count` distinct line directions: an even count over a
whole turn would land two families on the same lines, and a pair of
parallel families bounds no rhomb. An odd ring over the whole turn
carries the `count`-fold rotation that permutes its families
cyclically, so the tiling it dualises into is exactly symmetric about
the origin.

Five is the Penrose rhombs, four the Ammann-Beenker octagonal tiling,
three the rhombille.

## The phase of one family

`sigil::geometry::path::MultigridFamily::offset` is the phase, in whole
spacings: line `k` of the family is where the normal coordinate reaches
`(k - offset) * spacing`. The offsets are what pick one tiling out of
the family a set of directions admits. Only the fractional part of one
means anything — moving an offset by a whole number renumbers that
family's lines and leaves the lines themselves where they were — and
which fractions may be used together is the regularity rule above.

## See also

- `path/Lattice.h` — the header: `multigrid`, `multigridRing`,
  `MultigridFamily`, `MultigridRhomb`, `MultigridTiling`,
  `MultigridOptions`
- [lattice](page:SigilGeometry/functions/lattice) — the scanline fill in
  the same header
