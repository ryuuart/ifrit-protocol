---
kind: type
library: SigilGeometry
name: Projection
qualified: sigil::geometry::path::Projection
group: Frames
status: stable
---

# Projection

The sphere laid onto a plane, as a value.

A map is two decisions and nothing else: WHERE the sphere is looked at
from, and WHAT LAW carries an angle out from there into a distance on
the paper. Written as free functions, those decisions turn into a
scatter of `tan(half the co-latitude)` and `log tan(45 + lat/2)`
expressions with the centre, the scale and the handedness spelled again
at every call site — and two spellings of one map disagree about where
a star lands. Written as a `sigil::geometry::path::Projection`, they are
five fields set once, and every reading respects them.

```cpp
Projection plate{.scheme = Scheme::Stereographic,
                 .centre = {.latDeg = 90}, .scale = 235};
const glm::vec2 star = plate.at({.lonDeg = 63.4f, .latDeg = 31.8f});
const Spherical back = plate.from(star);   // and the way home
```

An aggregate, meant for designated initialisation: a positional
constructor could not gain a field later without breaking every call
site, and these are exactly the fields a caller wants to name. Trivially
copyable and comparable, so a consumer that caches drawings can prove
two frames asked for the same map.

## The plane is y up

`+y` is the direction of increasing latitude at the map's centre, which
is what "north is up" means, and the unit is the caller's own — pixels,
millimetres of paper, radii of a tropic. `Grid{.yScale = -1}` is how the
answer reaches a y-down canvas, and
[Grid](value:sigil::geometry::path::Grid) is also where an ANISOTROPIC
map belongs: a projection here is isotropic, because a per-axis scale
would turn a stereographic's circles into ellipses and none of the laws
would hold. A chart measured at one number of degrees per centimetre
across and another down is an isotropic projection under an anisotropic
unit map.

## Why one value and not one function per map

The five schemes differ in one line of arithmetic each and agree about
everything else — the centring, the handedness, the turn, the way back.
A caller comparing two projections of the same sky (which is what asking
"was this chart drawn on a cylinder or from a pole?" IS) needs them to
be the same kind of thing, held in a variable and swapped.
`sigil::geometry::path::Scheme` is therefore a field, not a name.

## What the projection does not know

A projection is a coordinate map, and the astronomy, cartography or
cartouche that stands on it is the caller's.
[Rotation](value:sigil::geometry::path::Rotation) covers the part that
IS geometry — an epoch's worth of precession, a globe turned to bring a
place to the middle, a chart re-poled onto the ecliptic — as a rotation
of the sphere; which angles to turn by is what the caller knows. A
measured artefact's own departures from its law (a centre that is not
quite the pole, an azimuth that runs a few per cent fast) belong beside
the artefact for the same reason.

## The scale

`sigil::geometry::path::Projection::scale` is PLANE UNITS PER RADIAN OF
ARC AT THE CENTRE — the one derivative all five schemes share there, so
changing the scheme leaves the middle of the map the size it was and
moves only what is far from it.

Further out each law has its own say, and the two worth knowing by
heart are: an orthographic's limb stands at `scale`, and a polar
stereographic's equator at twice it.

## The circles

`sigil::geometry::path::Projection::circleOf` answers the image of the
circle standing some arc away from a pole — an almucantar about a
zenith, a parallel about a pole, a great circle (at 90) about the pole
that defines it.

Only a conformal azimuthal map answers: under a stereographic every
circle on the sphere is a circle on the plane, which is what lets a
whole family of them be struck with a compass instead of plotted. Two
cases have no circle and come back absent — a scheme that does not
map circles to circles, and a circle passing through the very point
the projection is taken FROM, whose image is a straight line. The
second is not a degeneracy to guard against but a feature of the
drawing: the meridian through the centre of an astrolabe's plate IS
straight.

`sigil::geometry::path::circleThrough` stands beside it as the other way
to the same answer: the closed form knows what the image of a sphere
circle IS, and this is how a maker without the closed form STRIKES one —
through three of its points. Where both answer, they answer the same
circle.

## Reading it back

`sigil::geometry::path::Projection::from` is the way home. On an
orthographic map the far hemisphere lies on top of the near one, and
what comes back is the NEAR reading; past the limb there is no direction
at all and the answer is the point on the limb.

`sigil::geometry::path::Projection::radiusAt` is the plane distance the
map lays down for an arc from the centre, measured up the map's own
north line: the RADIUS on an azimuthal projection, where the map is
round and the same law holds at every bearing, and the ORDINATE on a
cylindrical one, where it does not. Signed, and odd about the centre, so
a southward arc comes back as a distance below it.
`sigil::geometry::path::Projection::arcAtRadius` is its inverse, and
`sigil::geometry::path::Projection::centredOn` answers the same map
about another centre — the sibling strip, the next panel, the same law
re-poled — which saves the field-by-field restatement where a scale or a
handedness gets silently dropped.

## Coming from a chart's own units

`sigil::geometry::path::offsetFrom` is the step a spherical construction
is made of: the horizon point at an azimuth, the pole of the great
circle a chart's own line is, a star offset from the one beside it. At a
pole north is not a direction, and the bearing is then measured from the
starting direction's own meridian — which is what the arithmetic does
anyway, continuously with everywhere else.

`sigil::geometry::path::Vantage` is which side of the sphere the map is
drawn from. The same directions laid down from outside the sphere and
from inside it are MIRROR IMAGES across the map's own north line, and
neither is a mistake in the other's arithmetic: a chart of the sky drawn
as it is seen from under it and the same sky engraved as it stands on a
globe read the opposite ways round.

## See also

- `path/Projection.h` — the header: `Projection`, `Scheme`, `Vantage`,
  `Spherical`, `Rotation`, `PlaneCircle`, `circleThrough`,
  `angleBetween`, `offsetFrom`
- [Rotation](value:sigil::geometry::path::Rotation) — the sphere turned
  before it is laid
