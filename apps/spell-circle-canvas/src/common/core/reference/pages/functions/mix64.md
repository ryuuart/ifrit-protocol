---
kind: function
library: SigilCore
name: mix64
qualified: sigil::core::noise::mix64
group: Compute
status: stable
---

# mix64

Seeded, deterministic noise — the one place a mixer lives.

Three integer mixers — a 64-bit avalanche, the PCG word and the
xorshift step — and the unit floats squeezed out of them. Every
function in the header is a bit-exact function of its inputs on every
platform, so anything seeded by them re-rolls identically: a scattered
brush stamp, a roughened outline, a drifted point cloud, a jittered
layout. A shader that has to agree with a CPU preview to the bit
reproduces `pcgHash` and `pcgUnit` rather than inventing its own; the
point-operator compute kernel does exactly that.

## The constants and the shift schedules are not tuning knobs

Renders stored as bytes are seeded through here, and a GPU kernel
reproduces `pcgAdvance`, `pcgMix` and `pcgHash` word for word. Changing
a constant does not fail a build — it re-rolls every stored render and
desynchronizes the two ends of every operator chain that runs on both.

`hash`, `lattice`, `pcgHash`, `xorshiftNext` and `xorshift64Next` are
different mixers with different outputs, kept side by side because each
seeds work that is compared byte-for-byte against stored renders. Pick
by what the caller already uses; new code takes `pcgHash`.

## The avalanche

`mix64` is two xor-shift-multiply rounds and a final xor-shift, so a
one-bit change anywhere in the input changes about half the result. It
is a bijection — every input maps to its own output — so a counter
walked through it never repeats a value before the counter does. The
stateless form is `mix64(x + kMix64Gamma)`; a stream is a counter
advanced by the gamma and read through it.

`sigil::core::noise::Mix64Stream` is that stream: a 64-bit
counter stepped by the gamma and read through `mix64`, with the unit
floats squeezed out of it. It is a different function from `pcgNext`
and `xorshiftNext`, so seeded work compared byte-for-byte against a
stored render cannot swap one for another. What it buys over those two
is the 64-bit counter — a caller with two integers to fold into a seed
(a glyph's index within its run, and the run's within its text) packs
them into one word with no mixing of its own. Every draw takes the HIGH
half of the avalanche, which is the half a splitmix64 mixes best. Not a
cryptographic generator and not a substitute for one.

## The xorshift step

`sigil::core::noise::xorshiftNext` is one xorshift32 step, in
the shift schedule 13 left, 17 right, 5 left. It is a second stream
beside the PCG one, for the same reason the constants are fixed: a
scatter already keyed to these three shifts draws a different sequence
from `pcgNext`, so the two are not interchangeable in anything stored
as bytes. New code takes `pcgUnitNext`. Zero is the one state to keep
out: all three shifts fix it, so a stream that reaches zero stays
there.

## The lattice mixer

`sigil::core::noise::lattice` takes three integer coordinates
and a seed to one well-mixed word — what value noise asks at each
corner of a cell, and what anything indexed by a grid position asks for
a stable draw. The three coordinate weights are large odd words, so a
step of one along any axis moves the sum far; the xor-shift-multiply
and the final fold are what turn that sum into an avalanche. The
multiplies are meant to WRAP: unsigned operands make that wrap the
defined kind, where signed ones would overflow, and the bits are the
same either way.

## See also

- `compute/Noise.h` — the header: `mix64`, `Mix64Stream`, `lattice`,
  `xorshiftNext`, `pcgHash`, `pcgUnit`
- `COMPUTE.md` — the chapter these mixers are catalogued in
