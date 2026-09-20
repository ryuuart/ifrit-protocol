---
kind: type
library: SigilGeometry
name: PolarFrame
qualified: sigil::geometry::path::PolarFrame
group: Frames
status: stable
---

# PolarFrame

A figure's own polar coordinate system: a centre, a radius that `r = 1`
lands on, and the convention flags. It converts `(angle, radius)` —
numbers measured off a reference drawing — into a point, an SkRect, or
an arc-length fraction, in the angle convention that drawing uses.

An aggregate, meant for designated initialisation: a positional
constructor could not gain a field later without breaking every call
site, and the convention flags are exactly the fields a caller wants to
name. Trivially copyable; it holds no Element and no node state.

```cpp
const PolarFrame fig{.centre = {kRR, kRR}, .radius = kR};  // North/CW
g.children({disc(fig.at(126.0f, 0.72f), 6.0f).fill(ink)});
```

## The convention is the reason this is a value and not a function

Engraved and statistical plates commonly measure clockwise from twelve
o'clock; Skia measures from due east. Written as a bare `polar()`
helper, that difference is a sign flip and a -90 that every call site
repeats and every reader has to reverse-engineer. Written as a
`sigil::geometry::path::PolarFrame`, it is one field set once, and every
conversion respects it.

It decides nothing — the caller supplies the angle and the radius — so
it is the peer of `sigil::geometry::path::centred`, not of a placement
policy like `arrange`, and the two compose.

### The conversions are the point

The arithmetic this exists to hold is of this shape:

```cpp
float frac(float thDeg) { return fmod((thDeg - 90) / 360 + 4, 1); }
// theta -> the arc-length fraction of shapes::circle(), whose
//   contour starts at due EAST and runs clockwise.
```

That is a library convention — where `shapes::circle()`'s contour begins
— leaking into a caller's arithmetic, at every site that places a label
on a ring. `sigil::geometry::path::PolarFrame::fraction` and
`sigil::geometry::path::PolarFrame::skiaDeg` are that arithmetic written
once, with the convention carried in the value rather than in a comment
beside each copy.

`sigil::geometry::path::PolarFrame::skiaDeg` answers this frame's
degrees as a SCREEN angle: degrees from +x, increasing in the direction
that looks clockwise. That is exactly what Skia's `addArc`,
`shapes::arc()` and `shapes::sector()` take, so

```cpp
shapes::sector(fig.skiaDeg(hourStart), fig.skiaSweep(30.0f))
```

reads in the plate's units and draws in Skia's.
`sigil::geometry::path::PolarFrame::skiaSweep` is kept apart from it
because adding the origin twice is the classic bug when an arc is
spelled as two absolute angles.

## The arc-length fraction

`fraction()` answers the value a text path's `at` wants.
`shapes::circle()` is `addOval` on an `SkPathBuilder` with direction kCW
and `startIndex` 1,
so its contour starts at the oval's **due-east** extreme and advances
the way screen-clockwise runs. The fraction is therefore the screen
angle over 360, wrapped into [0, 1).

**The baseline argument is the direction of the PATH, not of this frame,
and the two are independent.** `shapes::circle(kCCW)` also starts due
east — `startIndex` is 1 either way — and then runs the other way round,
so at f = 0.25 it sits at 12 o'clock where the kCW contour sits at 6. A
frame whose `sense` is CCW on a baseline that is still kCW is an
ordinary thing to want (numbers running anticlockwise around a clockwise
ring), so that argument must not default to the frame's own sense:
conflating them puts every label half a turn out.

**Only exact on a circle.** A circle's arc length is proportional to its
angle; an ellipse's is not, so on a non-square box the result drifts
from the true arc-length fraction. Keep ring inscriptions on a square
box (`sigil::geometry::path::PolarFrame::box`).

`sigil::geometry::path::PolarFrame::degOf` is its inverse — an
arc-length fraction back into this frame's degrees, for labelling a ring
that was already placed by fraction, or for reading a hit test back out
in plate units.

## The rest of the readings

`at()` takes an angle and a NORMALISED radius; `px()` an angle and a
radius already in pixels, for a figure whose radii were measured that
way. `dir()` is the unit vector pointing out along an angle — the
direction a tick, a leader or a radial label runs. `box()` is the square
box of a normalised radius about the centre, the frame every silhouette
generator inscribes itself in.

`scaled()`, `about()` and `turned()` derive another frame that inherits
this one's conventions, so the four fields are never restated and a
convention is never silently dropped. `turned()` composes:
`f.turned(4.5f).turned(-4.5f) == f`.

`originDeg` is an extra origin offset in SCREEN degrees, applied after
`zero` and normally 0. Two things want it: a scanned source that is not
square to its own axes, and an index ring offset by half a division,
which otherwise becomes a stray constant added at every call site.

## Make one

`sigil::geometry::path::centred` is the rect of a size centred on a
point — the `x - w * 0.5f` arithmetic as a VALUE you can then inset,
union, or hand to a node's own rect verb. It is not a replacement for
`centerAt()`, which centres a node on its MEASURED size after layout:
use `centred` when you know the box and want the rect for something else
too, the panel geometry a caption, a rule and a shadow all read from.
There is deliberately no `xywh()` or `ltrb()` wrapper beside it, because
`SkRect::MakeXYWH` and `SkRect::MakeLTRB` already name those.

## See also

- `path/Frame.h` — the header: `PolarFrame`, `Grid`, `centred`, `Zero`,
  `Sense`
- [Grid](value:sigil::geometry::path::Grid) — the other frame in the
  same header, the unit map
