# SigilMotion — the bindings

The chapter on following a live number: `bind()`, the `Binding` whose
fields are the stages a followed number is shaped through, the fixed
order those stages run in, the envelopes that are the library's waveform
vocabulary, and the value-noise field behind `Wiggle`. `README.md` beside
this file is the library; `VALUES.md` is the `Animatable` a binding reads
and hands back.

## The stages, as fields

`bind(source, stages)` answers an `Animatable<float>` that reads `source`
through `stages` whenever it is read — as live as its source and costing
nothing until somebody reads it:

```cpp
#include <sigilmotion/values/Animatable.h>

using namespace sigil::motion;

Animatable<float> seconds = animatable(0.0f);
Animatable<float> flip = animatable(0.0f);

box().rotateY(bind(flip, {.to = {0, 360}}))                      // a card flip
     .opacity(bind(seconds, {.from = {0, 7.2f},
                             .envelope = envelope::cosine()}))   // the breath
     .scaleX(bind(seconds, {.from = {0, 4.0f}, .alternate = true,
                            .to = {0, 240}}))                    // there and back
     .translateX(bind(seconds, {.to = {0, 0},
                                .wiggle = {.amount = 12, .frequency = 7,
                                           .seed = 1}}));        // a shake at rest
```

`Binding::apply` runs the stages in the order the fields are declared,
and designated initialisers make a call site name them in that order, so
a binding reads the way it runs:

| field | stage |
|---|---|
| `from`, `clampFrom` | normalise the source's range onto [0, 1]; clamp there when set — a beat inside a longer timeline and nothing else |
| `alternate` | there and back: a triangle of period 1 |
| `envelope` | the shape the phase takes across its span |
| `ease` | a curve — any `ease::` curve or the caller's own |
| `quantize` | that many evenly spaced levels; 0 and 1 are continuous |
| `reverse` | 1 − v |
| `to` | [0, 1] onto the output range |
| `wrap` | the output folded into [0, wrap); 0 folds nothing |
| `wiggle` | smooth noise, in output units |
| `clamp` | the output bounded; the default range is unbounded |

The wiggle's phase is read from the *normalised* value, before
`alternate`, the envelope and the curve, so folding or easing the signal
does not fold or ease the shake.

A shaped value can itself be followed: `bind(bind(x, first), second)`
reads `second` applied to `first` applied to `x`.

## The envelopes

The envelope is the shape a one-way phase takes across its span, and it
is the answer to the loop signals every study otherwise hand-steps:

| factory | classic waveform | what it says |
|---|---|---|
| `envelope::cosine()` | sine | the swell — 0, up to 1 at mid-span, back, eased |
| `envelope::square(duty)` | pulse | ON for the first `duty` of each period, OFF after; phase 0 is ON |
| `envelope::trapezoid(riseStart, holdStart, holdEnd, fallEnd)` | gate | ramp, hold at 1, ramp, dark — positions inside one pass |
| `envelope::shaped(curve)` | custom | the caller's curve on the folded phase u ∈ [0, 1) |

`alternate` (the triangle), `wrap` (the sawtooth) and `wiggle` (noise)
are their own stages and compose with any envelope. An envelope shapes
the normalised phase before the curve and the output range; `wrap`
folds the value after the output range — a sawtooth over the schedule is
an unfolded phase driving `wrap`, not an envelope.

`cosine`, `square` and `shaped` are periodic in the normalised phase, so
a monotonic seconds value keeps breathing or pulsing; `trapezoid` names
positions inside one pass and stays dark past its last corner, so a
repeating gate rides a phase that already wraps. Because the envelope
runs before `ease`, any curve through (0,0) and (1,1) rounds a
trapezoid's shoulders while leaving its hold at exactly 1 and its dark
at exactly 0.

The field behind `Wiggle` is a **testable seam**: `bind/WiggleNoise.h`
names its three pieces — one lattice cell, one quintic-smoothed octave,
and the normalised fractal sum — in `sigil::motion::detail`, and states a
range for each. They sit in `detail` because they are this library's own
field and must never be swapped for a GPU hash bit-matched to a compute
kernel: doing that would tie every wiggle already written to a shader
ABI. The ranges are promises — `Wiggle::amount` is a peak displacement
in the property's own units only because the sum is normalised — so a
test may name the three pieces and hold each to its own range.

## Gotchas

A binding carries ONE envelope. `trapezoid`'s corners are held
non-decreasing, so a zero-length shoulder is an instant cut rather than
a division by zero and corners given out of order collapse onto the one
before them.

A curve in a binding — `ease` or `envelope::shaped` — is part of the
binding's identity, compared under `easeEqual`'s rule: a plain function
compares by its address, while a capturing lambda compares unequal to
everything and re-patches on every describe. Name the shape as a free
function where that cost matters.

A `Wiggle` with an `amount` of zero is no wiggle, and that is the
default; a wiggled property that should hold at rest names
`.to = {rest, rest}` so the source's own motion contributes nothing.
