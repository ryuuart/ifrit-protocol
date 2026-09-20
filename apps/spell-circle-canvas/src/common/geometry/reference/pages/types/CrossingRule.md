---
kind: type
library: SigilGeometry
name: CrossingRule
qualified: sigil::geometry::path::CrossingRule
group: Crossings
status: stable
---

# CrossingRule

The rule ladder, as ONE comparable value. Climb only as far as the
composition needs:

```cpp
crossing::alternate()                    // == sequence({Over, Under})
crossing::sequence({Over, Over, Under})  // any repeating pattern
crossing::pairs({{0,1},{1,2},{2,0}})     // dominance, cyclic allowed
MyRule{}                                 // your own decide() value
```

and pin exceptions onto whatever you chose with `.except(i, order)`.

The default is LIST ORDER: later strands pass over earlier ones. That
is what makes `layers` and `weave` formally one machine.

## What the whole set says

`sigil::geometry::path::CrossingRule::prepare` is handed the whole set
once per discovery and before the first `decide`. A rule about one
meeting ignores it; a rule about the WALK — every crossing you meet
along a strand alternating with the one before it — cannot be answered
without it, because nothing in a single
`sigil::geometry::path::Crossing` says how many crossings on its strand
come before it.

A holder that discovers crossings calls this each time it
rediscovers them and may call it as often as it likes: what it
computes is a function of the set alone. It does not enter equality
— it is derived from geometry, not authored — so a rule that has
been prepared still prunes against the same rule that has not.

## Pins are positional

`sigil::geometry::path::CrossingRule::except` pins ONE crossing,
layered over whatever rule this already is. The index is a position in
the discovered order, so a stable RULE survives a geometry change and a
pin does not — move a strand and pin 3 lands on a different meeting.
Use rules while a composition is still moving, and pins only once it is
settled and you are correcting one knot by eye.

Pins compose onto the base rule and never stack as separate
entries: there is one `.crossing` field, and this is how it takes
exceptions.

## The alternating weave of knot theory

`sigil::geometry::path::crossing::alternateAlong` is the rule where you
walk any strand from its start and the crossings you meet run over,
under, over, under.

IT IS NOT `alternate()`, and the difference is the whole reason both
exist. `alternate()` alternates by DISCOVERED ORDINAL — the order the
crossings were numbered in, which is arc length along the
lowest-indexed strand each one involves. That is one strand's walk,
and every other strand's crossings are numbered in whatever order the
first strand met them, so on anything more braided than two strands
the parity you meet walking strand 3 is arbitrary and the weave reads
as a mistake. This one alternates along EVERY strand, by sorting the
passes — two per crossing, one on each strand it joins — by strand and
then by arc length, and reading the parity of each pass.

A {7/2} heptagram is the smallest figure that tells them apart: seven
chords, two crossings on each and seven in all, and the plaited star
everyone draws by hand is this rule and not the other.

Where a diagram is NOT alternable — a crossing whose two passes both
come up even — the two strands cannot both go over and the pass on the
lower-indexed strand decides. Such a figure has no alternating weave
at all, and this is where it shows.

## A crossing is discovered, never authored

`sigil::geometry::path::discoverCrossings` flattens the paths and finds
every PROPER crossing among them, numbered along the boundary.
"Proper" is load-bearing — coincident paths and endpoint touches, such
as a shared polygon vertex, are meetings rather than crossings, and
reporting them would put a knot at every corner of every rectangle.

## See also

- `path/Crossings.h` — the header: `CrossingRule`, `Crossing`,
  `discoverCrossings`, `crossingPatch`, `Order`
- [crossingPatch](page:SigilGeometry/functions/crossingPatch) — the
  region a knot is repainted over
