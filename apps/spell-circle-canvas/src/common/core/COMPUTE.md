# SigilCore — the compute leaf

The chapter on the arithmetic several libraries have to agree on to the
bit: the seeded mixers a jitter draws from, the stream a caller holds one
of them as and the distributions drawn out of it, the noise field read at
a point, the folds a cache key is accumulated with, the normal form a set
of runs over one axis is put in, and the shaped curve a unit position is
reshaped by. `README.md` beside this file is the library; `COMPARABLE.md`
is the other leaf that links nothing, and `RECONCILE.md` holds the
inherited-value channel the last section here reads a bound value through.

The standard library is the whole of its dependencies, so a shader's CPU
twin, a point cook, a text cache and a resource store all reach the same
bodies.

| header | holds |
|--------|-------|
| `compute/Noise.h` | `noise::hash` (a per-index float in [-1, 1)), the 64-bit avalanche `noise::mix64` with its `noise::kMix64Gamma` and the `noise::Mix64Stream` that walks it (`bits`, `unit`, `signedUnit`, `range`), the PCG family `noise::pcgAdvance`, `noise::pcgMix`, `noise::pcgHash`, `noise::pcgNext`, `noise::pcgUnit`, `noise::pcgUnitNext`, the xorshift stream `noise::xorshiftNext`/`noise::xorshiftUnitNext`, and the grid mixer `noise::lattice` |
| `compute/Field.h` | `noise::Field` — one noise look read at a point (`kind`, `seed`, `dimension`, `frequency`, `octaves`, `gain`, `lacunarity`, `fold`, `warp`, `period`, `at`), its `noise::FieldKind` (`Value`, `Gradient`, `Simplex`, `Worley`) and `noise::Fold` (`None`, `Turbulence`, `Ridged`), with the bare kinds `noise::valueNoise`, `noise::gradientNoise`, `noise::simplexNoise`, `noise::worleyNoise` and the corner `noise::latticeUnit` under them |
| `compute/Chance.h` | `chance::Stream` — the seeded stream a caller holds (`bits`, `unit`, `signedUnit`, `range`, `below`, `normal`, `sample`, `reseed`) over a `chance::Source` (`Pcg`, `Mix64`, `Xorshift`, `Halton`, `Sobol`, `Golden`, `Stratified`); the shapes `chance::Uniform`, `chance::Gaussian`, `chance::Exponential`, `chance::Weighted`; `chance::shuffle`, `chance::Reservoir`; and `chance::Chance`, the token one sheet re-rolls from |
| `compute/Hash.h` | `hash::kFnvOffset`, `hash::kFnvPrime`, `hash::fnv1a` over a word or over text, and `hash::combine` — the stir that folds one more word into a hash in hand |
| `compute/Curve.h` | `curve::Curve` — a shape as a captureless `float(float, const float*)` beside the four `parameters` it reads, with `at`, `operator()` and equality by both (a default-built one is the identity ramp, and a caller's own captureless body is the escape hatch); the plain `curve::smoothstep`; and the house shapes `curve::cubicBezier`, `curve::outBack`, `curve::inBack`, `curve::inOutBack`, `curve::outElastic`, `curve::inElastic`, `curve::outBounce` |
| `compute/Intervals.h` | `IntervalEnds`, `Inverted`, `normalizeIntervals`, `complementIntervals`, `intersectIntervals` and `firstOverlap` — the sorted, disjoint normal form a set of runs is put in, and the three combinators over it, with the endpoint type and the epsilon the caller's |

`<sigilcore/compute/Compute.h>` includes all six.

## Why the curve is here

The curve is here for the same reason the mixers are. Three libraries
reshape a unit position — an animation eases one, a colour ramp walks one,
and a keyed track shapes the segment leaving a key — and all three also
have to answer whether two of their curves are the same, because the value
holding the curve is memoised on that answer. A curve written as a lambda
cannot answer: two built a frame apart from identical text are different
objects, so the record holding one is unequal to itself and never prunes.
`curve::Curve` keeps the shape and its numbers apart — a captureless
function pointer beside four floats — so equality is by parameters, and a
caller's own shape written the same way is comparable for free. The house
shapes are the Penner equations, spelled here rather than reached for in
an animation dependency, because the colour leaf links no animation
runtime and must still walk the same curve.

## A mixer is a contract, not a choice

Everything here is a bit-exact function of its inputs, and the constants
are fixed by the agreement between the places that compute them: renders
stored as bytes are seeded through `noise::`, a GPU kernel reproduces
`pcgAdvance`, `pcgMix` and `pcgHash` word for word, and cache keys are
accumulated with `hash::`. Changing a constant fails no build. It re-rolls
every stored render, desynchronizes the two ends of every operator chain
that runs on both, and re-buckets everything already keyed. The tests pin
exact outputs — words as words, floats as bits — because determinism,
range and "different for different inputs" all survive a body that
drifted.

**Which fold a key is accumulated with depends on how long it has to mean
the same thing.** `hash::fnv1a` and `hash::combine` are pinned bodies with
pinned constants: a key that is STORED, or that two libraries or two runs
must agree on bit for bit, is folded with them, because their answer is a
fact the tests hold to. A key that lives no longer than the table it
buckets is folded with Boost's own `hash_combine`, whose answer the
standard library and Boost are both free to change between versions and
which nothing outside that process ever sees. Reaching for the pinned fold
on a throwaway table key costs nothing but says something untrue about the
value; reaching for Boost's on a stored key is a defect that appears as a
cache that misses everything after an upgrade.

**Two mixers side by side are two different functions.** `noise::hash` and
`noise::pcgHash` mix differently and answer differently; each seeds work
compared byte-for-byte against stored renders, so neither can become the
other. New code takes `pcgHash`. The same rule decides every candidate for
this directory: a body that differs is a second function under its own
name, never a merge.

The same rule holds for the STREAMS. `noise::Mix64Stream`, `noise::pcgNext`
and `noise::xorshiftNext` walk three different mixers, and a caller already
keyed to one draws a different sequence from the others. What the splitmix
stream buys over the other two is its 64-bit counter: a caller with two
integers to fold into one seed packs them into a word and does no mixing of
its own.

## The stream and what is drawn from it

**A stream is a mixer a caller can HOLD.** `noise::` answers words from a
state the caller carries; `chance::Stream` is that state and that choice as
one copyable value, so a component keeps one in a member, a describe takes
one by reference, and a function that scatters points takes the stream
rather than a seed and a mixer name. It is not a fourth mixer:
`Stream::pcg(s).bits()` is the word `noise::pcgNext` answers for a state of
`s`, and the same holds for the other two, so replacing a hand-carried
state with a stream does not move a picture. Every unit draw in both files
is the same squeeze — the 24 mantissa bits a float holds exactly — so
`[0, 1)` is the range drawn and not only the range written down.

**A distribution is a value, not a function per name.** A shape is anything
with an `Answer` type and a `draw(Stream&) const`, and `stream.sample(shape)`
is the only call, so `Gaussian{0, 2}`, `Weighted{weights}` and a caller's
own shape are reached the same way and a new distribution costs a struct
with two members rather than a method here. `Weighted` answers an INDEX, so
one shape serves every element type; `Reservoir` holds indices for the same
reason.

**Low discrepancy is a source, not a second vocabulary.** Halton, Sobol,
the golden-ratio recurrence and a stratified ladder are not random — they
are the numbers that fill an interval most evenly for the count drawn so
far, which is what a scatter that must not clump wants. They sit under
`bits()` beside the three mixers, so every shape above draws
low-discrepancy for free when the stream is one of them, and the four
golden-angle constants spelled by hand around this tree are
`Stream::golden`. `below()` scales the unit draw rather than rejecting
words for exactness: the bias is one part in 2^24, and rejection would
throw terms out of a sequence, which is the one thing these sources exist
to keep.

## The field

**A field is a number for a POSITION.** The mixers answer a number for an
INDEX — neighbouring indices are unrelated, which is what a per-stamp
jitter wants and what a displacement, a drift or a flow cannot use.
`noise::Field` reads the same `noise::lattice` mixer at a point, so points
near each other read near values and the two agree about what a seed means.
It is one value with props and not a header per kind: Perlin, simplex and
cellular noise are the `kind`, fBm is `octaves` with `gain` and
`lacunarity`, ridged and billowed noise is the `fold`, a tileable field is
a `period`, a warped one is `warp`. Eight of those are plain numbers and
two are small enumerations, so a look chosen once for a sheet is one of
these carried rather than ten arguments repeated, and a memo keyed on one
may be skipped.

**Three value noises, and only one pair of them agrees.**
`FieldKind::Value` at one octave IS the `valueNoise` in SigilGeometry's
path leaf, to the bit — the same lattice word over the whole 32 bits, the
same smoothstep, the same [-1, 1] — and the test holds it there against a
transcription of that body. SigilDraw's `NoiseField` deliberately does not
agree with either: it has p5's shape, a cosine blend of the LOW 24 bits in
[0, 1). Nor does the
kit's SkSL grain, whose sine-fract hash is not a good hash and is the right
one there because it is the same arithmetic on every device that can run
the shader. All three seed pictures stored as bytes, so this is the same
rule the mixers live under: a body that differs is a second function under
its own name, never a merge.

**What a period promises, and where it does not.** A `period` folds the
LATTICE coordinate, so the field repeats every `period / frequency` of the
caller's units on every axis, and an octave's period is the base one
multiplied by `lacunarity` — which makes the tiling exact when the
lacunarity is a whole number and approximate when it is not.
`FieldKind::Simplex` never tiles: the skew that turns squares into
triangles carries no Cartesian period through it, so a period on a simplex
field is ignored rather than producing a seam, and the header and the test
both say so.

## Which of these a theme may carry

A drawing's look is chosen once and read in many places, and
`reconcile/Env.h` is how a value chosen once reaches every reader without
being passed through them: `env::Provide<T>` binds it for a describe scope
and `env::inherited<T>()` reads it any number of levels down, at no cost to
the prune, because an inherited value is read DURING describe and lands in
the reading node's own description. Two of the values above are shaped for
exactly that, and they are shaped for it on purpose:

- **`chance::Chance` is a sheet's seed.** The seed, the source drawn from,
  and the `density` and `jitter` a scatter is set by; every element asks
  for its own stream by a NAME, which is folded into the seed, so the
  streams are independent, a new element leaves the others' draws alone,
  and changing the one seed re-rolls the whole sheet at once — which is
  otherwise an edit to every seed literal in a drawing.
- **`noise::Field` is a sheet's grain.** Ten numbers that say what a grain,
  a drift or an erosion looks like, so one bound value makes a whole
  sheet's paper agree instead of ten arguments being re-spelled at each
  call.

`Env.h` binds two conditions on anything carried this way, and both are why
these are structs of plain members with a defaulted `==`. A binding is
keyed by its C++ TYPE, so a token group is its own small struct and there
is no name-keyed lookup to invent. And a derived value must be MATERIALISED
into the type: a token carrying a `std::function`, a sampler or any other
closure compares equal to nothing and makes every memo below it a permanent
miss. `Field` is the ten numbers, never a bound sampler; `Chance` is the
seed and the source, never a stream that has already been drawn from.

The line between a token and an argument runs through the same place in
both: WHAT the look is, is chosen for the sheet; WHERE it is read, is the
call's own. A field's props are a token and the point it is evaluated at is
an argument; a chance's seed and source are a token and the shape drawn
from it is an argument. Nothing here binds anything — this library holds no
theme and knows of no drawing — it only says which of its values are shaped
to be bound by one that does.
