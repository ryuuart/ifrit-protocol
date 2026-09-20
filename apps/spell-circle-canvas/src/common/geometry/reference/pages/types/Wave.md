---
kind: type
library: SigilGeometry
name: Wave
qualified: sigil::geometry::shapers::Wave
group: Shapers
status: stable
---

# Wave

A smooth oscillation across the mark — the wave every wavy rule,
scalloped frame and ribbon edge is made of. Also the BRAID primitive:
strands that oscillate trade sides, and where they trade sides they
cross, which is what a braid is made of.

## As a profile it is zero-mean

That makes it a CENTRELINE — a strand path that swings either side of
the boundary. It is NOT a band width: a band asks its profile for a
width and this one goes negative half the time, which inverts the
rails wherever it does. An undulating band is the composition, a
positive offset PLUS an oscillation:

```cpp
struct Undulating {                     // width 20, wobbling by 6
  shapers::Wave wobble{6, 40};
  bool operator==(const Undulating &) const = default;
  float max() const { return 20.0f + wobble.max(); }
  float across(float a) const { return 20.0f + wobble.across(a); }
};
path::bandRegion(spine, path::Profile(Undulating{}));
```

## The wavelength reads differently at each seam

`wavelength` is PX, and the profile seam is asked in FRACTIONS of arc
length. There is no contour length available at that seam to convert
with, so the PROFILE reading treats `wavelength` as px-per-cycle on a
nominal 1000 px contour: on a spine much shorter or much longer than
that, the wavelength you get is not the one you asked for. That is also
why the braid takes its own phase count instead of deriving one. The
SHAPER reading has a real path and is exact.

The third member is `phase`, not a zigzag flag — the cornered
oscillation is `sigil::geometry::shapers::Zigzag`, a separate
value.

## The shelf it stands on

These are the stock values over two of this library's seams: what bends
one continuous mark, and the oscillating width law a strand that trades
sides is written as. Both live in `shapers::`, because a kit composes
over a seam and does not grow it — a seam's namespace whose contents
changed with which kit header a consumer happened to include would be a
namespace nobody could read off the seam's own directory. `Wave`
answers both seams at once, so one value is both.

Each is a comparable struct with its seam's required member, so a
caller's own value is indistinguishable from these at the call site,
which is the point of a seam. Nothing here decides anything a caller
did not ask for.

## See also

- `kit/Shapers.h` — the header: `Wave`, `Zigzag`, `Square`, `Jitter`,
  `Offset`, `Rounded`, `Chamfer`
