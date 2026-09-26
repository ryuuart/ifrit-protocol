---
kind: type
library: SigilGeometry
name: Radial
qualified: sigil::geometry::shapes::Radial
group: Silhouettes
status: stable
---

# Radial

N vertices dealt round the box's centre, then joined into a loop, joined
chord by chord, or marked. Every figure that is a count of things about
a centre is a setting of it: the polygon and the star, a star polygon's
chords, a tick ladder, the segments of a dial, a ring of studs, a spiral
of seeds.

```cpp
shapes::radial(6);                                        // a hexagon
shapes::radial(10, {.radii = {1, 0.42f}});                // a five-pointed star
shapes::radial(7, {.skip = 2, .connect = path::Connect::Each});   // chords
shapes::radial(60, {.connect = path::Connect::None,
                    .marks = {path::Mark::line(0.88f), path::Mark::line(),
                              path::Mark::line(), path::Mark::line(),
                              path::Mark::line()}});      // a minute ring
```

The short names stay as one-line stock values over it, because each is
shorter at the call site than the general form: `polygon(sides,
rotation)`, `star(points, innerRatio, waist)`, and the division ladders
`ticks()`, `arcs()` and `chords()`, whose option structs convert through
`radialOf()`.

## Where the vertices stand

`sigil::geometry::path::RadialOptions::radii` is CYCLED over the
vertices, so `{1}` is a polygon and `{1, 0.42f}` a star.
`fromDegrees` places vertex 0 in the frame's convention — zero north and
clockwise unless `frame` says otherwise — and the vertices are dealt
evenly over `sweepDegrees`, or `stepDegrees` apart when that is set: the
golden angle, 137.508, with `growth` at `Growth::SquareRoot`, is
phyllotaxis. `closed` deals one vertex more, so a partial sweep has a
mark at both ends.

`sigil::geometry::path::RadialOptions::uniform` measures the radius as
half the SHORTER side of the box, which keeps a ladder on an oblong box
a circle; the default inscribes the arrangement in the box's own
ellipse, which is what a polygon or a star filling its box wants.

## How they are joined

`sigil::geometry::path::Connect::Loop` is one closed ring through every
vertex, or with a `skip` not coprime with the count one ring per orbit:
`{6/2}` is two triangles. `Connect::Each` makes every chord its own open
contour — the addressable-per-side form a text baseline walks as one
arc-length coordinate — trimmed by `inset` at each end.
`Connect::None` joins nothing and stands a `path::Mark` at each vertex
instead: an open line, a closed bar, a segment of the ring whose curved
sides are the ring's own arcs, or any figure.

## Marks, per vertex

`sigil::geometry::path::RadialOptions::marks` is cycled like the radii,
so a long mark every fifth is five marks. `each` is the escape hatch for
a ladder with more classes than that, and a mark with `inner == outer`
skips its vertex.
@trap A callable has no equality, so options carrying `each` compare
unequal to everything and a node shaped by them never prunes.

## See also

- `path/Radial.h` — the options, shared with `path::points`' radial
  pattern: `RadialOptions`, `Connect`, `Growth`, `Mark`
- [Ellipse](value:sigil::geometry::shapes::Ellipse) — the other general
  shape over a box
