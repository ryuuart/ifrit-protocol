---
kind: type
library: SigilGeometry
name: Grid
qualified: sigil::geometry::path::Grid
group: Frames
status: stable
---

# Grid

Author in the artefact's own units; multiply once.

A value rather than a `float g(float)` for two reasons. It needs three
things — scale, origin and snap — because an artefact's box is rarely
at the canvas origin and a pixel-art plate wants its positions on a
pitch. And more than one grid has to be alive at once: a plate at a 4 px
geometry pitch carrying a readout on a 2.5 px text pitch is ordinary,
and a free function cannot do that without a second name.

```cpp
const Grid geo{.scale = 4.0f}, type{.scale = 2.5f};
box().rect(geo.rect(12, 8, 40, 16))
     .children({text(u8"HIT", ts)
                    .at({type.positionX(13), type.positionY(9)})});
```

A LENGTH takes no origin and a POSITION does, which is why the four
readings are named apart: a width is not a position, and adding the
origin to one is the bug the split prevents.
`sigil::geometry::path::Grid::lengthX` and
`sigil::geometry::path::Grid::lengthY` are the lengths;
`sigil::geometry::path::Grid::positionX` and
`sigil::geometry::path::Grid::positionY` the positions.

## The y axis

`sigil::geometry::path::Grid::yScale` is the y axis as a multiple of
`scale` — its direction and its relative size in one number.

−1 is the MATH FRAME: y counts UP from the origin, which is what a
plotted function, a projected sky and a surveyed elevation are drawn
in, and it is the difference between reading an artefact's own numbers
off the page and negating every one of them at the call site. Anything
else is an anisotropic map: 0.5 draws a unit half as tall as it is
wide, which a chart whose two axes are different quantities wants.

1, the default, is the canvas's own frame — y down, square units.

`lengthY` comes back SIGNED under a flip, because a length up the page
IS negative in canvas px, and `positionY` is measured down the canvas
even where the axis runs up.

## Snapping

`sigil::geometry::path::Grid::snap` snaps the RESULT to a multiple of
that many canvas px, zero being off. It is a canvas-px pitch, not a unit
count: snapping to the grid's own pitch and snapping to the device pixel
are different values.

`sigil::geometry::path::Grid::snapped` rounds half away from zero, like
`std::round`, but CONSTEXPR — which `std::round` is not before C++23.
That is why it is hand-rolled: a unit map typically feeds `constexpr`
canvas constants (a canvas width declared as so many artefact units),
and a helper that cannot run at compile time cannot be used for those.

`sigil::geometry::path::Grid::matrix` is the affine matrix, for handing
a whole SkPath through in one go. It is NOT snapped — a matrix cannot
round per-point, and pretending otherwise is how a "snapped" plate ends
up half on the grid.

## Rects and runs

Both `sigil::geometry::path::Grid::rect` overloads answer a SORTED rect,
so a flipped frame answers a rect and not an inside-out one: under a
negative `yScale` the unit-space top is the canvas-space bottom, and
every consumer of an SkRect reads left ≤ right and top ≤ bottom. The
overload taking an SkRect maps it corner by corner, so a snapped grid
keeps both edges on the grid rather than only the near one.

`sigil::geometry::path::Grid::map` carries a polyline from artefact
units into canvas px, and `sigil::geometry::path::Grid::scaled` answers
a grid at a fraction of this one's scale, same origin, y axis and snap —
the nested unit system.

## See also

- `path/Frame.h` — the header: `Grid`, `PolarFrame`, `centred`
- [PolarFrame](value:sigil::geometry::path::PolarFrame) — the polar
  frame beside this cartesian one
