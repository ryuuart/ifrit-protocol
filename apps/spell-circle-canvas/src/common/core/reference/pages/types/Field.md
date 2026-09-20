---
kind: type
library: SigilCore
name: Field
qualified: sigil::core::noise::Field
group: Compute
status: stable
---

# Field

A NOISE LOOK AS ONE VALUE, read at a point.

`sigil::core::noise` answers a number for an INDEX:
neighbouring indices are unrelated, which is what a per-stamp jitter
wants. A field answers a number for a POSITION, and points near each
other read near values, which is what a displacement, a drift, a grain
and a flow want. It is built on the same `noise::lattice` mixer, so the
two agree about what a seed means.

## One value with props, not a header per kind

Perlin, simplex and cellular noise are the `kind`; fBm is `octaves`
with `gain` and `lacunarity`; ridged and billowed noise is the `fold`;
a tileable field is a `period`; a warped one is `warp`. Every member is
a plain number or a small enumeration, so the value compares exactly
and a memo keyed on one can be skipped, and a look chosen once for a
whole sheet is one of these carried as a token rather than ten
arguments repeated at every call site.

The ten numbers are what a grain, a drift, a flow or an erosion is set
by, and they are chosen once for a drawing far more often than they are
chosen per call — which is what makes this a value to carry rather than
a call to repeat. It compares exactly, member for member, so a memo
keyed on one may be skipped and a theme may bind one.

## The range

It is [-1, 1] for every kind at `Fold::None` and `Fold::Ridged`, and
[0, 1] at `Fold::Turbulence`; the octave sum is divided by the
amplitudes that went into it, so adding octaves changes the detail and
never the range. `Value` and `Worley` reach the ends of it exactly;
`Gradient` and `Simplex` approach them without a hard bound, which is
the amplitude every implementation of those two has, and neither is
clamped. A fold is not symmetric — once octaves are summed, `Ridged`
sits well above the floor and `Turbulence` well below the ceiling,
which is what a crease at every scale does to a sum.

## What agrees with what

`FieldKind::Value` at one octave IS the trilinear value noise
the path tier's own value noise answers, to the bit: the same lattice word,
squeezed the same way, eased with the same smoothstep, in the same
[-1, 1]. Two other value noises in this tree deliberately do NOT agree
with it and must not be re-spelled as it — SigilDraw's noise field has p5's
shape (a cosine blend of the low 24 bits, in [0, 1)), and the kit's
SkSL grain has a sine-fract hash, which is not a good hash and is the
right one there because it is the same arithmetic on every device that
can run the shader. Each of the three seeds pictures stored as bytes;
re-spelling any of them as another re-rolls those pictures.

## The two gradient kinds

`sigil::core::noise::gradientNoise` is PERLIN GRADIENT NOISE in two
dimensions, in [-1, 1]: zero at every lattice point and shaped by the
direction each corner pulls, which is what removes the blobbiness of
value noise. The interpolated dot product of unit gradients is bounded
by half the square root of two, so the result is scaled by its
reciprocal to reach the ends of the range.

`sigil::core::noise::simplexNoise` reads the same gradients on a
triangular lattice instead of a square one: three corners contribute
instead of four, and there is no direction along which the cell
boundaries line up, which is the axis-aligned ridging a square lattice
leaves at high frequency. It does NOT tile — the skew that turns
squares into triangles does not carry a Cartesian period through it, so
a `period` is ignored for this kind rather than silently producing a
seam.

## See also

- `compute/Field.h` — the header: `Field`, `FieldKind`, `Fold`,
  `gradientNoise`, `simplexNoise`
- `COMPUTE.md` — the chapter the mixers and the folds are described in
