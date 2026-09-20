---
kind: type
library: SigilMeasure
name: Moments
qualified: sigil::measure::Moments
group: Statistics
status: stable
---

# Moments

## Description

WHAT A RUN OF NUMBERS AMOUNTS TO, accumulated one at a time.

`sigil::measure::Samples` keeps the numbers and computes on read; this
keeps none of them and computes as they arrive, so it summarises a run
of any length in a fixed six words — a sweep of a million values, a
per-frame count that never ends. What it cannot do is anything that
needs the values back: a quantile, a histogram, a re-read after a wider
window was wanted. Reach for `sigil::measure::Samples` or a
`sigil::measure::Histogram` for those.

### The spread is accumulated, not subtracted

The obvious variance — the mean of the squares less the square of the
mean — is two large numbers differing in their last digits, and for
values that are large and close together (a run of timestamps, a run of
pixel coordinates on a wide sheet) the difference is nearly all
rounding, and can come out NEGATIVE. Clamping that at zero hides the
loss and reports a spread of none where there was a real one. Here each
value's deviation from the mean so far is folded in as it arrives, so
the sum is of small numbers and never goes negative.

### Which spread to take

`Moments::variance` is THE SPREAD OF THE RUN AS THE WHOLE POPULATION:
the mean squared deviation. Take it when the values in hand are
everything there is — every frame of a recording, every point of a
drawing.

`Moments::sampleVariance` is THE SPREAD OF WHAT THE RUN WAS DRAWN FROM:
Bessel's correction, dividing by one less. Take it when the values are a
SAMPLE of something larger and the answer is a claim about that larger
thing. Fewer than two values make no such claim and answer 0.

`Moments::skewness` is HOW LOPSIDED THE RUN IS: zero for anything
symmetric about its mean, positive when the long tail runs high,
negative when it runs low. What separates a frame time that is steady
with occasional spikes from one that is simply slow. A run with no
spread at all has no shape to report and answers 0.

`Moments::min` and `Moments::max` answer 0 over nothing, so that an
empty summary reads as empty rather than as an infinite span.
`Moments::sum` is recovered from the mean rather than carried beside it.

### Folding two halves together

`Moments::merge` takes everything another summary saw. Two halves of a
run summarised separately amount to the same numbers as the whole run in
one pass, which is what lets a sweep be divided across workers and the
parts put back together.

## See also

`sigil::measure::Samples`, `sigil::measure::Histogram`,
`sigil::measure::Rescale`.
