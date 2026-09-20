---
kind: type
library: SigilGeometry
name: Profile
qualified: sigil::geometry::path::Profile
group: Width laws
status: stable
---

# Profile

THE WIDTH LAW: how far a mark sits ACROSS its spine, as a comparable
value.

`along` is a fraction of the spine's arc length (or px of it, for a
law that says so); `across` is px on the spine's normal, positive to
the LEFT of travel — which, with y pointing down, is OUTSIDE a
clockwise path, and clockwise is Skia's own direction for rects and
circles. Everything in this leaf that takes a signed distance from a
path means that same side.

## What a profile value must carry

`sigil::geometry::path::ProfileScheme` asks for
`float across(float along) const`, `float max() const`, and EQUALITY.
Both extra members are load-bearing.

`max()` is what every cull and bleed calculation is sized from. A
varying width whose reach cannot be asked for can only be clipped, and
clipping in a cached picture is silent.

Equality is required because a profile is read LIVE, every frame.
Anything an author hands the library must participate in reconciler
equality, or a node that prunes goes on reading the value it was
described with and never sees the new one. An incomparable callable is
therefore not a profile; write a struct with `operator==`.

A PROFILE THAT RETURNS A NON-FINITE WIDTH DELETES THE WHOLE BAND. One
NaN vertex makes the built path non-finite and Skia draws none of it,
with no error. The seam does not guard this — clamp inside your own
law. Trigonometric laws are the usual source: `sqrt(sin(pi*along))` is
NaN at `along == 1` because the float pi rounds up.

## The px key

`sigil::geometry::path::PxKeyedProfileScheme` is optional, and one
line: a scheme that declares `static constexpr bool alongIsPx = true`
is keyed in PX OF ARC LENGTH from the spine's start rather than in a
fraction of it. Consumers that have measured their spine —
`sigil::geometry::path::profileOffset`, the band's rails — hand it
`along * lengthPx` through `sigil::geometry::path::Profile::acrossAt`.
Nothing else about the seam changes, and a scheme that says nothing
stays fraction-keyed.

WHY IT EXISTS. A decoration under a reveal is handed the REVEALED
contour, so a fraction is a fraction of what has been drawn SO FAR: a
law keyed to it SLIDES along the mark as the reveal grows. That looks
identical in a still frame and wrong in motion. Absolute distance from
the start does not move, which is what a calligraphic pressure law or
a flow-width law actually means.

The conversion cannot live in the author's value, because it needs the
length of the contour ACTUALLY being painted and only the paint-time
consumer knows that. So the seam converts, once, for every consumer.

## The taper

`sigil::geometry::path::profile::Taper` runs across LINEARLY from its start to
its end along the spine — the brush that lifts, the ribbon that
closes, the leader that narrows to its point.

The two ends are signed, and the sign is the side (positive is LEFT of
travel), so a taper from +8 to −8 crosses the spine at the middle
rather than narrowing: that is a strand trading sides, not a taper. A
taper to 0 is the point. It is keyed in the FRACTION of arc length, so
it stretches to whatever spine it is handed; a taper that must keep its
px shape under a reveal wants its own px-keyed law.

## The stepped width

`sigil::geometry::path::profile::Spans` is a run of spans, each holding one
width for its share of the spine — the flow that thins at every
junction it passes, the rule that changes weight at a stated station,
the bar whose thickness is a measurement rather than a curve.

`widthsPx` is read against `upTo`, the span boundaries in the
profile's own key, ASCENDING. Width *i* holds from boundary *i−1* (or
the start) to boundary *i*, and the LAST width holds from the last
boundary to the end — so `widthsPx` carries one more entry than
`upTo`. Fewer and the tail reads the last width there is; more and the
extra widths are never read. Empty is a width of zero everywhere.

A STEP IS A STEP. The width does not interpolate across a boundary,
because the thing this describes is a measurement that changes at a
place, not a curve sampled at one — a law that eased between its
stations would draw a shape nobody measured. `Taper` is the
interpolating one, and two of them beside each other is the ramp
between two stated widths.

## See also

- `path/Profile.h` — the header: `Profile`, `ProfileScheme`,
  `PxKeyedProfileScheme`, `Taper`, `Spans`
- [Wave](value:sigil::geometry::shapers::Wave) — the zero-mean
  law that is a centreline rather than a width
