---
kind: function
library: SigilGeometry
name: lattice
qualified: sigil::geometry::path::lattice
group: Lattices
status: stable
---

# lattice

PARALLEL LINES CUT TO THE EVEN-ODD INTERIOR of a set of rings: each
line's crossings with every edge, sorted along the line and paired, so
a ring inside another is a hole and two rings side by side are two
islands — the rule `sigil::geometry::path::containsEvenOdd` answers a
point with, and the rule a path filled with `SkPathFillType::kEvenOdd`
is drawn by.

## Description

A hatch, a fill drawn by a pen plotter, a shaded region and a mass of
strokes are all the same construction — lines at an angle, a spacing
apart, kept where they are inside and dropped where they are not — and
the marks it answers with are CENTRELINES, which is what separates it
from clipping a line pattern to an outline: a centreline can be walked,
drawn along with a natural-media tool, split, or joined to the next one.

The marks come in scan order, each running in the direction the lines
run. Rings of fewer than three points bound no area and are skipped;
the scan covers the rings that are left. Both ends of a ring are
joined whether or not it says it is closed, because an area is what is
being filled.

## The options

`sigil::geometry::path::LatticeOptions::origin` is where the ladder is
measured from: a point every line's spacing is counted from, so the same
lattice over a moving shape keeps its lines in the same places and the
fill stops crawling as the shape animates. Unset lays the first line
half a gap inside the rings, which is what fills a shape whose place is
not fixed.

`taper` is what each gap is multiplied by after the one before it: one
is an even lattice, more spreads the lines as the scan advances and less
crowds them. A gap never falls below an eighth of a unit, so a taper
toward zero crowds rather than stalls.

`maxLines` is the most lines one lattice lays down — a bound rather than
a preference: a spacing far smaller than the rings it fills would
otherwise answer with a vector nobody asked the size of.

## See also

- `path/Lattice.h` — the header: `lattice`, `LatticeMark`,
  `LatticeOptions`
- [multigrid](page:SigilGeometry/functions/multigrid) — the other
  construction in the same header
