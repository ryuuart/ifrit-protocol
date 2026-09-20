---
kind: type
library: SigilMaterial
name: Ramp
qualified: sigil::material::Ramp
group: Colour
status: stable
---

# Ramp

ONE COLOUR RAMP AS A VALUE: the stops, the space they are walked in, the
shape of the walk, which way round it runs, and what the caller's own
numbers mean at either end. It answers a colour for a number, and it is
callable, so anything that takes an interpolator over a position takes a
ramp with no adapter.

It exists because those five decisions were otherwise re-spelled at every
site a stop list was read. A look chosen once for a whole sheet is one of
these carried as a token, rather than a stop list plus four conventions
repeated and drifting.

## Anatomy

`Ramp::stops` is the colours and where they sit, in order, on the unit
interval. Two stops at one position are a HARD EDGE — the band boundary a
ramp says with no blend across it.

`Ramp::space` is which space the colours BETWEEN two stops are walked in,
and none of the four is right for every ramp. `RampSpace::Srgb` walks the
numbers a file stores, which is what a renderer's gradient does, so it is
the space to pick when the ramp must match one drawn as a gradient.
`RampSpace::Linear` walks the light the numbers stand for, which is the
answer when the ramp means a quantity of light. `RampSpace::Oklab` walks
what an eye reports: even steps, and no dark band where two saturated
stops cross. `RampSpace::Oklch` walks the same space around the hue
circle rather than across it — the difference between a red-to-green ramp
passing through grey and one passing through orange and yellow.

`Ramp::arc` is which way round that circle, read only under `Oklch`:
`HueArc::Shorter` is the arc under half a turn, `HueArc::Longer` the
other one, and `HueArc::Increasing` and `HueArc::Decreasing` force the
sign for a sweep that must keep turning the same way across the wrap.

`Ramp::easing` is a `core::curve::Curve` — SigilCore's shaped curve, the
same value an animation eases with — passed over the position before the
stops are read, so a ramp dwells at one end without moving its stops. It
carries its own parameters and compares by them.

`Ramp::reverse` reads the stops from the far end. It is a flag rather
than a second stop list because a reversed colormap is the same value
seen the other way, and the pair a diverging scale needs cannot then
drift apart.

`Ramp::domainLow` and `Ramp::domainHigh` are what the caller's numbers
mean: the value landing on the first stop and the value landing on the
last. So a ramp reads a temperature, a depth or a count directly, and the
normalisation is in the value rather than at every call site.

`Ramp::at` is the colour at a value in the caller's own units, and
`Ramp::position` is where that value lands on the unit interval after the
domain, the reversal and the easing — exposed because a caller drawing
the ramp itself has to walk the same positions.

Outside the domain it CLAMPS, exactly as the stop ladder clamps outside
the stop list: a ramp carries no answer for what lies beyond its ends,
and a flat band keeps an out-of-range input visible instead of inventing
a colour for it.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Ramp{.stops = {...}}` | C++ | the stops, everything else defaulted — OKLab, shorter arc, straight walk, unit domain |
| `Ramp{.stops = {...}, .space = RampSpace::Srgb}` | C++ | designated initialisers for the five decisions |
| `ramp(palette)` | C++ | a fixed table read continuously: one stop per entry, evenly spaced |
| `ramp(palette, space)` | C++ | the same, in a stated space |
| `kit::viridis()` and the other stock ramps | C++ | a named colormap — a stock VALUE over the type, not a type of its own; take one, move its domain, reverse it, and it is still a ramp |
| `material.Ramp(stops=[...], space=...)` | Python | the same five decisions, by keyword |
| `material.ramp(palette)` | Python | a fixed table read continuously |

A stop is a `RampStop`: `RampStop::pos` and `RampStop::color`.

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `palette` | function | SigilMaterial — the ramp read at band centres into a fixed table |
| a data scale's interpolator | argument | SigilData — a ramp IS an interpolator, so the unit position goes straight to one |

A consumer that wants the ramp DRAWN rather than read asks its stops for
a gradient paint; a consumer that wants one colour out of it calls it.

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `ramp` | function | SigilMaterial — the way back from a fixed table |
| `kit::viridis`, `kit::magma`, `kit::inferno`, `kit::plasma`, `kit::turbo`, `kit::redBlue`, `kit::brownTeal`, `kit::cubehelix` | function | SigilMaterial |

## Description

Every member is a plain number, a small enumeration, a curve that
compares by its parameters, or a stop list, so two ramps compare exactly
and a memo keyed on one can be skipped.

`sampleRamp` is the ladder underneath, for a caller holding bare stops:
straight sRGB between neighbours, which is what a renderer's gradient
draws. `rampBracket` is the one search both readings go through, so the
CPU sample and the ramp's own read cannot disagree about which stops a
position lies between.

`palette(ramp, entries)` reads a ramp at BAND CENTRES — centres rather
than ends, because a table of N entries stands for N bands and not for N
points on a line. `ramp(palette)` is the way back, and not an inverse: a
palette says there is nothing between its entries, and asking for a ramp
of one is the caller deciding otherwise.

## See also

- `color/Ramp.h` — the header: `Ramp`, `RampSpace`, `HueArc`, `palette`,
  `ramp`
- `color/Color.h` — the ladder underneath: `RampStop`, `RampBracket`,
  `rampBracket`, `sampleRamp`
- [Palette](value:sigil::material::Palette) — the fixed table, which is
  not a ramp with more stops
- [Color](value:sigil::material::Color) — what a ramp answers
- [Paint](value:sigil::material::skia::Paint) — where a ramp's stops
  become a gradient
