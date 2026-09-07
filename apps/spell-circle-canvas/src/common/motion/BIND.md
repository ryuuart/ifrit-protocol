# SigilMotion — the bindings

The chapter on the leaf: `bind()`, the `Bound` chain builder and the
`BoundFloat` it fills, the fixed order its stages run in, the envelopes
that are the library's waveform vocabulary, and the value-noise field
behind `wiggle`. `README.md` beside this file is the library; `VALUES.md`
is the slot a shaped binding is held in and `CLOCK.md` the ticker that
runs a derivation through one.

## The chain

`Bound` is the builder and `BoundFloat::apply` runs a fixed
pipeline: pre-normalise (`source`/`window`), optional input clamp, the
envelope (`pingPong`/`cosine`/`trapezoid`/`square`/`wave`), ease curve
(`map`), `quantize`, the affine chain (`scale`/`offset`/`target`/`invert`,
composed in call order), `wrap`, `wiggle` noise, output `clamp`. The
wiggle phase is read from the *normalised* value, before the envelope and
the curve, so easing or folding the signal does not ease or fold the
shake.

The envelope is the shape a one-way phase takes across its span, and it is
the answer to the loop signals every study otherwise hand-steps into its
ticker:

```cpp
bind(&secs).source(0, 7.2f).cosine()                  // the breath
bind(&secs).source(0, 4.0f).pingPong().target(0, 240) // there and back
bind(&cycle).source(0, 15.f).trapezoid(0, .03f, .84f, .95f)  // hold, cut
bind(&secs).source(0, 1.06f).square(0.58f).target(0.1f, 1.f) // the blink
bind(&secs).wave([](float u) { return u * u; })       // your own period
```

Together the stages are the library's **waveform vocabulary** — each verb
is one of the classic signal shapes, spoken where its output lands:

| verb        | classic waveform | what it says |
|-------------|------------------|--------------|
| `pingPong`  | triangle         | there and back across the span, repeating |
| `cosine`    | sine             | the swell — 0, up to 1 at mid-span, back, eased |
| `square`    | pulse            | ON for the first `duty` of each period, OFF after; phase 0 is ON |
| `trapezoid` | gate             | ramp, hold at 1, ramp, dark — positions inside one pass |
| `wrap`      | sawtooth         | folds the **output** into [0, period) |
| `wiggle`    | noise            | smooth value noise, added in output units |
| `wave`      | custom           | your function on the folded phase u ∈ [0,1) |

The first four are envelopes and share one slot; `wrap` and `wiggle` are
their own later stages and compose with any of them. The distinction that
keeps `wrap` out of the envelope row: an envelope shapes the *normalised
phase* before the curve and the affine chain, while `wrap` folds the
value *after* the affine chain — a sawtooth over the schedule is an
unfolded phase driving `wrap`, not an envelope.

`cosine`, `pingPong` and `square` are periodic in the normalised phase, so
a monotonic seconds Output keeps breathing, bouncing or pulsing;
`trapezoid` names positions inside one pass and stays dark past its last
corner, so a repeating sheet rides a phase that already wraps. `wave` is
periodic by the same fold the others use: the function is evaluated on
u ∈ [0,1) and repeats whatever it drew there. Because the envelope runs
*before* `map`, any curve through (0,0) and (1,1) rounds a trapezoid's
shoulders while leaving its hold at exactly 1 and its dark at exactly 0 —
where the corners are and what shape the shoulders take are separate
decisions. `square` has no shoulders to round, and its phase 0 is ON —
a caret born at the start of its cycle is born visible.

The field behind `wiggle` is a **testable seam**: `bind/WiggleNoise.h`
names its three pieces — one lattice cell, one quintic-smoothed octave,
and the normalised fractal sum — in `sigil::motion::detail`, and states a
range for each. They sit in `detail` because they are this library's own
field and must never be swapped for a GPU hash bit-matched to a compute
kernel: doing that would tie every wiggle already written to a shader
ABI, and a future parity fix there would move all of them. But the ranges
are promises rather than incidental facts about the body — `wiggle`'s
`amount` is a peak displacement in the property's own units only because
the sum is normalised — so a test may name the three pieces and hold each
to its own range, which is the one thing outside this library that may.

## Gotchas

A binding carries ONE envelope. `pingPong`, `cosine`, `trapezoid`,
`square` and `wave` write the same slot, so naming a second replaces the
first exactly as a second `map()` replaces the first curve — there is no
raised cosine of a trapezoid. `trapezoid`'s four corners are held
non-decreasing, so a zero-length shoulder is an instant cut rather than a
division by zero and corners given out of order collapse onto the one
before them.

`wave`'s function is part of the binding's identity, compared the way
`map()`'s curve is: a consumer that prunes on equality can compare a plain
function pointer, while a capturing lambda compares unequal to everything
and re-patches on every describe. Name the shape as a free function where
that cost matters.
