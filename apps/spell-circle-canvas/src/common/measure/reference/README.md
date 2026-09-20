# SigilMeasure reference

The prose the headers' briefs stand over. A header says what a thing is,
what it accepts, what holds when it is not called, and the one trap a
caller cannot read off the signature; the reasoning behind a numeric
choice, and the rule that takes a paragraph to derive, are on a page
here.

| Page | What it holds |
| --- | --- |
| `pages/types/Check.md` | a claim whose printed line is computed from what it reports: the four standings, the line's shape, the overloads and what each refuses, and the table a run prints as |
| `pages/types/Moments.md` | the one-pass summary, the spread accumulated rather than subtracted, and which of the two spreads to take |
| `pages/types/Histogram.md` | equal bins, what falls outside and is counted, and weights rather than integers |
| `pages/types/LineFit.md` | the least-squares line and the residuals that make it evidence |
| `pages/types/Rescale.md` | a statistic and not a drawing's scale, and the two derivations over it |
| `pages/types/FrameTimer.md` | four marks, three rings, why the two cost lanes are separate, and the sample a gate reads |
| `pages/types/Laps.md` | the tiling marks, and the name a lap borrows rather than copies |

## What this library is for

Timing, statistics and check reporting. Every other library measures
itself with this one, so it has no domain-library dependency at all.

## The quantile every reading shares

`sigil::measure::quantile` answers the value at a fraction of the sorted
samples, interpolated linearly between the two ranks it falls between:
the median of {1, 2, 3, 4} is 2.5, not 2 or 3. An empty list reads 0, a
single sample reads itself at every fraction, and the fraction is
clamped. It sorts a copy, so the caller's order is untouched.

`sigil::measure::quantiles` reads several fractions of one run for ONE
sort: a summary that reports a median and two tails would otherwise sort
the same numbers three times. Each answer is the one the single-fraction
call would have given, and the fractions are read in the order they were
asked for rather than in sorted order. `sigil::measure::median` is the
middle of the run under the name a reader of the caption will use.

Every quantile in the library reads through one interpolation body, so
the single-fraction call and the several-fractions call cannot drift
apart in what "the median" means.

## The rings and the watches

`sigil::measure::Samples` holds the last few samples, oldest dropping
first. The summaries read every sample the ring holds; none is cached,
so a ring that is read every frame costs a pass over its contents each
time, and a percentile costs a sort. Sized for a HUD, not for a
histogram.

`sigil::measure::Stopwatch` reads milliseconds on the steady clock
rather than on system time, so a clock adjustment mid-measure cannot
produce a negative or absurd reading. `Stopwatch::elapsedUs` is the same
span in microseconds — the unit a per-frame reading wants, where a
millisecond is already the whole budget.

`sigil::measure::ScopedMs` writes the milliseconds a scope took into the
double it was given, at scope exit:

```cpp
double layoutMs;
{ ScopedMs timed(layoutMs); layout(); }
```

The target is ASSIGNED, not accumulated, so a block entered twice
reports its last run.

`sigil::measure::Counters` is a set of counters addressed by name,
created on first use and iterated in name order so a printed set reads
the same every run. Reading a name that was never counted is 0, not an
error, and `Counters::reset` keeps the names so a set printed after a
reset still lists what it counts.
