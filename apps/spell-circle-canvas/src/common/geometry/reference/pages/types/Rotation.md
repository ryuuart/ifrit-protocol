---
kind: type
library: SigilGeometry
name: Rotation
qualified: sigil::geometry::path::Rotation
group: Frames
status: stable
---

# Rotation

A rotation of the sphere, as a value: an orthonormal basis you apply to
a direction, invert, and compose with another.

It is here rather than in a matrix header because it is the half of a
map that is not the map: a chart of one epoch's sky drawn from another
epoch's catalogue, a globe turned to bring a coast to the middle, and a
plate re-poled onto the ecliptic are all one projection under one
rotation. Composing them is the whole of what a caller does with it, so
`sigil::geometry::path::Rotation::then` is the verb and matrix
multiplication order is not something anybody has to remember:
`a.then(b)(v) == b(a(v))`.

## The three-angle form

`sigil::geometry::path::Rotation::zyz` is `Rz(a) · Ry(b) · Rz(c)` — the
three-angle form every rotation of the sphere can be written in, and
the one an epoch-to-epoch precession and a re-poling are both published
as. The caller owns which angles and which signs; this owns the
composition, which is where a hand-multiplied nine-term matrix goes
wrong.

`aboutX`, `aboutY` and `aboutZ` are a turn about one axis,
counterclockwise seen from the positive end of that axis.

## The directions it turns

`sigil::geometry::path::Spherical` is a direction on the unit sphere, in
degrees: `lonDeg` round the equator and `latDeg` up from it. Right
ascension and declination are these two under the sky's names;
longitude and latitude under the ground's. Its `direction()` is the unit
vector — `+x` at (0, 0), `+y` at (90, 0), `+z` at the north pole — and
`Spherical::of` reads a vector of any length back for its direction
alone, with `lonDeg` in [0, 360).

`sigil::geometry::path::angleBetween` is how far apart two directions
stand, in degrees: the great-circle angle, computed from the half-chord
so that a pair nearly on top of one another keeps its precision where
an `acos` of the dot product would lose it.

## See also

- `path/Projection.h` — the header: `Rotation`, `Spherical`,
  `angleBetween`, `Projection`
- [Projection](value:sigil::geometry::path::Projection) — the map the
  turn is applied before
