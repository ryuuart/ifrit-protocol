# SigilMeasure

How long something took, what a run of numbers amounts to, and whether a
claim about it held. A stopwatch and a timed block; the summary, quantile
and histogram of a run in hand; the window, smoothed reading and rate of
a live stream; and `Check` — a claim whose printed verdict is computed
from the values it reports. It has no domain-library dependency, so every
other library can measure itself without pulling in another Sigil layer,
and a sketch can read its own numbers with the same instruments.

Namespace `sigil::measure`, target `SigilMeasure`. Every duration is a
`std::chrono` span: `measure::Duration` is seconds held as a double, a
literal such as `380ms` converts into it, and `measure::Milliseconds`
turns it back into a number for a printout. The one outside link,
Boost.Container, stays behind the implementation of the counter table;
no public header includes it.

## Tier 1 — the whole front door

One include, `<sigilmeasure/Measure.h>`, and every call on one screen:

```cpp
#include <sigilmeasure/Measure.h>

using namespace sigil::measure;
using namespace std::chrono_literals;

// ── HOW LONG ─────────────────────────────────────────────────────────────
Stopwatch watch;                                      // starts now
Duration spent = watch.elapsed();                     // watch.restart() starts it again
Duration baked = timed([&] { bake(); });              // one block, one call
double shown = Milliseconds(baked).count();           // a number for a printout

// ── A RUN IN HAND ────────────────────────────────────────────────────────
Summary heights = summary(points, heightOf);          // count, sum, mean, min, max, deviation
double tail = quantile(frameTimes, 0.99);             // the one quantile
Histogram shape = Histogram::over(values, {.bins = 20});   // bins, counts, below/above, peak

// ── A LIVE STREAM ────────────────────────────────────────────────────────
Window last{180};                                     // add, mean, min, max, quantile, latest, values
Smoothed level{12};                                   // about the last 12: add, value
Rate arrivals{4s};                                    // mark(at), advance(now), perSecond, times

// ── A PROOF ──────────────────────────────────────────────────────────────
CheckTable table;
table.add(heading("THE RETE"))
     .add(check("pieces", 12, tiling.size()))         // exact, for counts
     .add(check("outer radius", 257.972, measured, 0.01))   // reals take a tolerance
     .add(finding(check("legend holds", 1.0, ratio, 0.01)))
     .add(reading("residual", 5.6e-16));
for (const std::string& line : table.lines()) std::puts(line.c_str());
return table.failures();                              // an exit code a build can read
```

- `time/Stopwatch.h` — `Duration`, `Milliseconds`, `Microseconds`, `Stopwatch`, `timed`
- `stats/Summary.h` — `Summary`, `summary`
- `stats/Quantile.h` — `quantile`, `Measurable`, `ReadingOf`
- `stats/Histogram.h` — `Histogram`, `HistogramOptions`
- `stats/Window.h` — `Window`, `WindowOptions`
- `stats/Smoothed.h` — `Smoothed`, `SmoothedOptions`
- `stats/Rate.h` — `Rate`, `RateOptions`
- `check/Check.h` — `Check`, `Standing`, `CheckColumns`, `CheckTable`, `check`, `finding`, `reading`, `heading`

## Tier 2 — one options struct per instrument

Every instrument that has anything to set takes one struct, last:

```cpp
Window recent{{.span = 4s}};                // what arrived in the last four seconds: add(value, at)
Window both{{.count = 600, .span = 10s}};   // whichever says a value is too old first
Smoothed build{{.weight = 0.15}};           // the new value's weight, stated
Smoothed follow{{.timeConstant = 250ms}};   // add(value, elapsed): the same speed at any frame rate
Smoothed meter{{.over = 30, .peak = true}}; // rises at once, falls over about 30 values
Rate frames{{.span = 1s}};
Histogram fixed{{.low = 0, .high = 40, .bins = 20}};   // then add(value), add(value, weight)
table.lines({.labelWidth = 22, .valueWidth = 9});
```

`WindowOptions::count` and `WindowOptions::span` bound a window; a value
leaves when either says it is too old. `SmoothedOptions::over` is the
exponential mean over about that many values, each new one weighing
2 / (over + 1); `SmoothedOptions::weight` states that weight directly,
which is what keeps a reading written as `old · 0.85 + new · 0.15`
exactly the number it was; `SmoothedOptions::timeConstant` is read by
`Smoothed::add` when it is handed the time that passed, so the reading
follows at the same speed whatever the step; `SmoothedOptions::peak`
makes it a decaying peak. `HistogramOptions::low` and
`HistogramOptions::high` fix a range; left equal, `Histogram::over` takes
the range from the run. `check()`'s tolerance keeps no default: how
closely two numbers must agree is the construction's to say.

## Tier 3 — control, under `advanced/`

What a host, a bench or a study reaches for and a sketch rarely does:

- `advanced/FrameTimer.h` — `FrameTimer`, `FrameTimerOptions`, `FrameSample`: four marks a render loop lays and the three lanes they feed, the one implementation the product renderer, the live host and the plate sweep all read
- `advanced/Moments.h` — `Moments`: the running moments under `Summary`, with the sample spread, the skew and a merge
- `advanced/Quantiles.h` — `quantiles`, `median`: several fractions off one sort
- `advanced/Rescale.h` — `Rescale`, `zScore`, `unitRange`: a straight-line map derived from a run
- `advanced/LineFit.h` — `LineFit`, `lineFit`: the least-squares line and its residuals
- `advanced/Laps.h` — `Laps`, `ScopedDuration`: named phases of one span, and a block timed into a value
- `advanced/Counters.h` — `Counters`: named integer counters in name order
- `advanced/CheckFormat.h` — `line`, `failures`, `findings`: one check as a printed line, and the counts over a loose run of them

```cpp
#include <sigilmeasure/advanced/FrameTimer.h>

FrameTimer timer{{.pause = 1s}};  // 120 frames deep; a present gap of a second is a pause
timer.begin();
record(canvas);
timer.composed();                 // the frame's own work ends here
flush();
timer.finished();                 // the backend is done with it
timer.presented();                // an interval per call after the first
hud("work %.2f ms  p99 %.2f  headroom ~%.0f fps",
    Milliseconds(timer.work().mean()).count(),
    Milliseconds(timer.work().quantile(0.99)).count(), timer.headroomFps());
const FrameSample sample = timer.sample();   // plain values, for a table
```

## Mental model

**One unit of time.** Every reading that is a span of time is a
`std::chrono` duration, so a stopwatch, a window of frame times and a
rate over a feed's arrival stamps add and compare without a conversion
in between, and no name carries its unit. A printout converts once, at
the edge: `Milliseconds(spent).count()`.

**A run in hand, or a stream.** `summary`, `quantile` and
`Histogram::over` read a run that is already in a container, once; a
projection — a member pointer or a callable — picks the number out of
each element, so a run of points is summarised without copying out its
heights. `Window`, `Smoothed` and `Rate` are fed a stream one value at a
time and are read whenever the reader likes.

**Two cost lanes, never one derived from the other.** `FrameTimer` keeps
the frame's own work (begin to composed) and the frame end to end (begin
to finished, backend flush included) as separate lanes. A synchronous
backend drain is not work the frame did, but it is time the machine
spent: charging it to the work lane understates headroom, and leaving
it out of the frame lane understates the cost. So both are kept and
both are reported. `FrameTimer::headroomFps` is the rate the mean work
time would allow — a ceiling rather than a frame rate, which stays high
exactly when a stutter comes from outside the measured work. The
presented lane is the only one that can see a stutter at all: a window
that mostly hits the vsync and occasionally misses several in a row
averages to very near the display rate, so its tail
(`present().quantile(0.99)`, `present().max()`) is what separates
"smooth" from "lagging".

**One quantile.** `quantile(values, fraction)` sorts a copy and
interpolates linearly between the two ranks the fraction falls between,
so the 0.5 quantile of {1, 2, 3, 4} is 2.5. Empty reads 0, one value
reads itself at every fraction, and the fraction is clamped to [0, 1].
`Window::quantile` is that function over the values held, and
`quantiles` reads several fractions through the same body off one sort;
nothing else in the tree defines its own.

**A window is contiguous.** `Window::values` is the held run itself,
oldest first, as one span, so a chart's trace or any loop reads it with
no copy. What falls out of the window leaves every reading at once:
nothing is cached, so a mean or a quantile is a pass over what is held.
A window over TIME is stamped by the caller's clock — the scene's
seconds, a feed's arrivals — and `Window::advance` moves that time on
with nothing added, so a stream that fell silent empties rather than
freezing on its last values. A window over scene time is therefore
deterministic, and so is a `Rate`, which is the same window counting
events.

**A spread is accumulated, never subtracted.** The obvious variance —
the mean of the squares less the square of the mean — is two large
numbers differing in their last digits, and for values that are large
and close together it can come out NEGATIVE. `Summary` folds each
value's deviation from the mean so far in as it arrives, so the sum is
of small numbers and cannot go negative, and two summaries of two halves
merge into the summary of the whole. `Summary::deviation` takes the
values in hand as everything there is; the sample spread, which claims
something about what they were drawn from, is `Moments::sampleVariance`
under `Summary::moments`.

**What falls outside a histogram is counted.** A value past either edge
goes to `Histogram::below` or `Histogram::above`, rather than being
clamped into an end bin (which would draw a wall that is not in the
data) or dropped (which would leave a total that does not add up).

**A check's sentence cannot drift from its measurement.** `check(label,
expected, actual)` returns a `Check` carrying both values formatted and
the verdict computed from them, and a printed line reads
`  <label> <actual>   PASS` or `… FAIL want <expected>`. The integral
overload is constrained to integral types so a float cannot be compared
by truncation, the real overload takes a tolerance with no default, and
a long label pushes the value column right rather than being clipped.

**A verification is not only claims, and each row says what it is.**
`Check::standing` is a `Standing`: a `Claim` about the construction,
whose FAIL fails the run; a `Finding`, a claim about the SUBJECT, whose
verdict is printed as a claim's is and never counted against the run
(`finding(check(…))` restates one); a `Reading`, a measurement reported
beside the claims and judged by nobody (`reading(label, value)`); and a
`Heading`, a title over the rows under it (`heading(title)`).
`CheckTable::checks` counts the rows that carry a verdict,
`CheckTable::failures` the claims that did not hold and
`CheckTable::findings` the findings that did not, and the summary line
`CheckTable::lines` ends with says both. The standing is a value on the
row rather than a word in its text, so the sentence a reader sees and
the count a build reads cannot disagree.

## Boundaries

Nothing here knows what a frame is drawn with, what a scene is, or what
a check is checking. The typed per-frame statistics each library keeps
about its own work stay with that library; this one holds the
instruments they are read through. A drawing's AUTHORED scale — a
domain someone chose, a range in pixels, ticks — is SigilData's; what a
run of numbers SAYS about itself — its extent, mean, spread, quantiles,
bins — is this library's, and the two meet at the call site as two
numbers, so neither links the other. Nothing here serializes or draws:
writing a check into a text feed is the feed's business, and drawing a
window as a sparkline is the chart's.

## Testing and benchmarks

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`measure_test` and one `measure_bench`, ctest one entry per CASE, what
a case may pin, and what a label promises. No case here carries one.
What is only true of SigilMeasure:

The cases sit beside each subject: `stats/test/` holds
`QuantileTest.cpp`, `StatsTest.cpp` (the summary, the moments, the
histogram and the rescale), `StreamTest.cpp` (the window, the smoothed
reading and the rate), `FitTest.cpp` and `CountersTest.cpp`;
`time/test/TimeTest.cpp` the stopwatch, the laps and the frame timer;
and `check/test/CheckTest.cpp` the claims.

Every statistical claim is asserted against an arithmetic answer that
can be written down — the variance of the first n whole numbers, the
skewness of three zeros and a one, the density of a flat run, a
smoothed reading after two steps of a stated weight, the count a span
leaves in a rate — rather than against a number this code once
produced. Every stamp a stream case feeds is stated, never read off a
clock.

**Exactly one case reads the wall clock**, and it owns every claim that
needs one: that the stopwatch, a timed block, the lap timer and
`ScopedDuration` advance with real time, and that a restart sends the
reading back — asserted against the span already measured, not against
a ceiling a busy scheduler could cross.

`measure_bench`'s arms are in `stats/bench/`: `WindowBench.cpp` times a
window's add (by count and over a span), its quantile and its mean at
the sizes a HUD and a sweep use, a smoothed reading's add and a rate's
mark; `StatsBench.cpp` the per-value cost of `Moments::add` and
`Histogram::add`, the per-run cost of `Moments::of`, `summary`,
`Histogram::over` and `zScore`, and the two quantile arms side by side,
where the whole point is that one sorts once and the other sorts per
fraction.
