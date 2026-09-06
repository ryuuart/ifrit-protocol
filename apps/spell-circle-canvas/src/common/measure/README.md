# SigilMeasure

Timing, statistics and check reporting, on the standard library plus
Boost.Container for the compact ordered counter table. The library gives you
a stopwatch, a lap timer that names the phases of one
span, a frame timer whose four marks feed the three lanes a render loop
is judged by, a rolling ring of samples with the summaries a HUD prints,
named counters, the frame sample a headless timing sweep snapshots, the
running moments a run of any length is summarised by, the histogram that
says what shape its spread has, the straight-line map that puts two runs
of different units on a common footing, the least-squares line, and
`Check` — a claim whose printed verdict is computed from the values it
reports. It has no domain-library dependency, so every other library can
measure itself without pulling in another Sigil layer.

Namespace `sigil::measure`. One static target, `SigilMeasure`; every
public header lives under `include/sigilmeasure/<subject>/` and is spelled
`<sigilmeasure/<subject>/X.h>`, and `<sigilmeasure/Measure.h>` includes
them all:

| header | holds |
|--------|-------|
| `time/Stopwatch.h`    | `Stopwatch` (`elapsedMs()`, `elapsedUs()`, `reset()`), `toMicroseconds()` for a caller holding two clock readings, and `ScopedMs`, which writes a block's milliseconds into a double at scope exit |
| `time/Laps.h`         | `Laps` — `mark(name)` returns the milliseconds since the previous mark and records the lap; `each()` reads them back |
| `time/FrameTimer.h`   | `FrameTimer` — `begin()`, `composed()`, `finished()`, `presented()` feeding the `frame()`, `work()` and `present()` rings, with `headroomFps()` and `presentedFps()` read off them |
| `stats/Samples.h`     | `Samples`, a rolling ring (`add`, `mean`, `percentile`, `min`, `max`, `last`, `size`, `samples`), the free `quantile()` it and everything else shares, `quantiles()` for several fractions off one sort, and `median()` |
| `stats/Moments.h`     | `Moments` — a run of any length summarised in six words: `add`, `of`, `merge`, `count`, `mean`, `sum`, `variance`, `sampleVariance`, `sd`, `sampleSd`, `skewness`, `min`, `max`, `range` |
| `stats/Histogram.h`   | `Histogram` — equal bins across a range: `add` (weighted), `over`, `binOf`, `edge`, `centre`, `binWidth`, `counts`, `count`, `fraction`, `density`, `total`, `below`, `above`, `mode`, `peak` |
| `stats/Rescale.h`     | `Rescale` — an invertible straight-line map, with `zScore()` and `unitRange()` deriving one from a run |
| `stats/Fit.h`         | `lineFit(xs, ys)` and the `LineFit` it answers — slope, intercept, `r2`, `correlation()`, max and rms residual, with `at()` and `residual()` |
| `stats/Counters.h`    | `Counters` — named `int64_t` counters (`add`, `get`, `reset`, `each`) |
| `stats/FrameSample.h` | `FrameSample` — the plain numbers a frame-budget gate judges a scene by |
| `check/Check.h`       | `Check`, the `check()` overloads, `failures()` and `Table` |

## Using it

```cpp
#include <sigilmeasure/Measure.h>

using namespace sigil::measure;

// Laps tile the frame: each starts where the last ended.
Laps laps;
layout();
stats.layoutMs = laps.mark("layout");
paint();
stats.paintMs = laps.mark("paint");

// Four marks, three lanes.
FrameTimer timer;                 // 120 frames deep by default
timer.begin();
record(canvas);
timer.composed();                 // the frame's own work ends here
flush();
timer.finished();                 // the backend is done with it
timer.presented();                // a wall-clock interval per call after the first
hud("work %.2f ms  p99 %.2f  headroom ~%.0f fps",
    timer.work().mean(), timer.work().percentile(0.99), timer.headroomFps());

// A claim and its verdict, printed from the same values it judges.
Table table;
table.add(check("pieces", 12, tiling.size()));
table.add(check("outer radius", 257.972, measured, 0.01));
for (const std::string& line : table.lines()) std::puts(line.c_str());
return table.failures();          // an exit code a build can read
```

## Mental model

**Two cost lanes, never one derived from the other.** `FrameTimer` keeps
the frame's own work (begin to composed) and the frame end to end (begin
to finished, backend flush included) as separate rings. A synchronous
backend drain is not work the frame did, but it is time the machine
spent: charging it to the work lane understates headroom, and leaving
it out of the frame lane understates the cost. So both are kept and
both are reported. `headroomFps()` is 1000 over the mean work time —
the rate the frame's work alone would allow, a ceiling rather than a
frame rate, which stays high exactly when a stutter comes from outside
the measured work. The presented lane is the only one that can see a
stutter at all: a window that mostly hits the vsync and occasionally
misses several in a row averages to very near the display rate, so its
tail (`present().percentile(0.99)`, `present().max()`) is what separates
"smooth" from "lagging".

**One quantile.** `quantile(samples, p)` sorts a copy and interpolates
linearly between the two ranks `p` falls between, so the median of
{1, 2, 3, 4} is 2.5. Empty reads 0, one sample reads itself at every
`p`, and `p` is clamped to [0, 1]. `Samples::percentile` is that function
over the ring's contents; nothing else in the tree defines its own.

**One line fit, in the caller's own precision.** `lineFit(xs, ys)` is
ordinary least squares, and it answers the residuals with the slope
because a study that quotes a slope without one has stated a preference
rather than a measurement: `r2` says how much of the ordinate the line
explains, `maxResidual` says how far the worst point stands off it, and
`at()`/`residual()` are the line evaluated so a drawing and its caption
go through one arithmetic. It is a template on the scalar and the sums
accumulate in that scalar — a caller that has always fitted in `float`
gets the `float` answer it had rather than a `double` one rounded back,
which is the difference between a drawing that holds and one that moves
by a sub-pixel. Fewer than two points, or every point at one abscissa,
is not a line: the answer is a zero slope through the mean with `r2` at
0, never a divide by zero.

**A spread is accumulated, never subtracted.** The obvious variance —
the mean of the squares less the square of the mean — is two large
numbers differing in their last digits, and for values that are large
and close together (a run of timestamps, a run of coordinates on a wide
sheet) the difference is nearly all rounding and can come out NEGATIVE.
Clamping that at zero hides the loss and reports no spread where there
was a real one. `Moments` folds each value's deviation from the mean so
far in as it arrives, so the sum is of small numbers and cannot go
negative, and shifting a whole run leaves its variance where it was.
That is also what makes it MERGEABLE: two halves summarised separately
amount to the same numbers as one pass over the whole, so a sweep can be
divided across workers and put back together.

**Which spread is being claimed is the caller's to say.** `variance()`
divides by the count and describes the values in hand as the whole of
what there is — every frame of a recording, every point of a drawing.
`sampleVariance()` divides by one less and claims something about what
those values were DRAWN FROM. Neither is a default that suits both, so
both are spelled out and neither is named `variance` alone by accident.

**`Moments` keeps no values, `Samples` keeps the last few, a `Histogram`
keeps a shape.** Three summaries, and the choice between them is what
has to be answered later. `Moments` costs six words whatever the run's
length and can never give a quantile back. `Samples` keeps a fixed
window and can, at the cost of a sort. `Histogram` keeps the shape of a
run of any length — one hump or two, a tail on one side, a wall at a
limit — and is the only one a drawing can be made of directly. What
falls outside its range is COUNTED, in `below()` and `above()`, rather
than clamped into an end bin (which would draw a wall that is not in the
data) or dropped (which would leave a total that does not add up).

**A derived rescaling is a statistic, not a drawing's scale.** `zScore`
and `unitRange` answer a `Rescale` computed FROM a run of numbers —
where its centre is, how wide it is — so that runs in different units
can be compared: a frame time and a byte count both become "how unusual
is this, for its own run". A drawing's scale is AUTHORED — a domain
someone chose, a range in pixels, a transform, a tick ladder — and
deriving one from the data is what makes an axis move every time a point
arrives. The value is three numbers rather than a lambda, so it is
storable, comparable and invertible: a reading taken back off a drawing
goes through the same arithmetic the other way.

**`Samples` is a ring.** `Samples(capacity)` keeps the last `capacity`
samples, oldest dropping first, and computes every summary on read — no
running sums, so `clear()` is exact and a sample that fell out of the
ring is gone from every number. `samples()` returns them oldest first.
Sized for a HUD: a percentile costs a sort of the ring.

**A check's sentence cannot drift from its measurement.** `check(label,
expected, actual)` returns a `Check` carrying both values formatted and
the verdict computed from them, and `Check::line()` prints
`  <label> <actual>   PASS` or `… FAIL want <expected>`. Three rules keep
it honest: the integral overload is constrained to integral types so a
float cannot be compared by truncation, the double overload takes a
tolerance with no default because how closely two numbers must agree is
a property of the construction being checked, and long labels push the
value column right rather than being clipped, since a clipped label
silently loses the qualifier at the end of a claim. `failures(checks)`
counts the misses, and `Table::lines()` prints the rows at one width
followed by a summary line.

**A verification is not only claims, and each row says what it is.**
`Check::standing` is a `Standing`: a `Claim` about the construction,
whose FAIL fails the run; a `Finding`, a claim about the SUBJECT — a
published formula that does not hold, a plate whose engraving
contradicts its legend — whose verdict is computed and printed exactly
as a claim's is and never counted against the run (`finding(check(…))`
restates one, `findings()` counts the ones that did not hold, and the
summary line says `, 1 finding`); a `Reading`, a measurement reported
beside the claims and judged by nobody, printed with no verdict
(`reading(label, value)` for a number, a count or a string); and a
`Heading`, a title over the rows under it, printed as its label alone
(`heading(title)`). `Table::checks()` counts the rows that carry a
verdict, and `Check::judged()` says whether one does. The standing is a
value on the row rather than a word in its text, so the sentence a
reader sees and the count a build reads cannot disagree about which
failures are the run's.

**A sample is plain numbers.** `FrameSample` holds `frameMs`, `workMs`,
`p99Ms` and `headroomFps` as values, so a snapshot taken when a sample
window closes survives the ring being cleared or refilled behind it.
It carries no writer: how a sample is serialized belongs to whoever
writes it out.

## Boundaries

Nothing here knows what a frame is drawn with, what a scene is, or what
a check is checking. The typed per-frame statistics each library keeps
about its own work — which nodes were painted, how many pictures were
recorded — stay with that library; this one holds the instruments they
are read through. Nothing here serializes: writing a check into a text
feed is the feed's library's business, and writing a sample as JSON is
the writer's, not this one's.

## Testing and benchmarks

`measure_test` is built from two files: `MeasureTest.cpp` for the
instruments, and `StatsTest.cpp` for the statistics, every claim in
which is asserted against an arithmetic answer that can be written down
— the variance of the first n whole numbers, the skewness of three zeros
and a one, the density of a flat run, the mean and deviation a z-score
leaves behind — rather than against a number this code once produced.
One case there is of a different kind and is the reason the accumulation
is shaped as it is: a run of large, close values, where the naive
formula is shown missing the answer the shifted run gives exactly.
Between them they cover the quantile's edges (empty, one sample,
interpolation, clamping), several fractions off one sort agreeing with
the single-fraction call, the line fit (an exact line found exactly, a
lifted point read off the residual rather than off the slope, a vertical
run answering no slope, and the float sums matching a hand-written
accumulation term for term), the ring's wrap-around, the counters, the lap
timer's naming and totals, `ScopedMs` leaving its target alone until
scope exit, the frame timer's lanes, and `Check::line` formatting as one
parameterised case per kind of claim — integral, tolerance, text, bare
condition, and a label longer than its column. Exactly one case reads the
wall clock, and it owns every claim that needs one: that the stopwatch,
the lap timer and `ScopedMs` advance with real time, and that a reset
sends the reading back — asserted against the span already measured,
since a stopwatch that ignored reset could only ever read higher, and
not against a ceiling a busy scheduler could cross. Every other timing case is deterministic, because
sleeping and then asserting on a duration asserts the operating system's
scheduler rather than anything this library promises. `measure_bench` times `Samples::add`,
`Samples::percentile` and `Samples::mean` per sample count, and in
`StatsBench.cpp` the per-value cost of `Moments::add` and
`Histogram::add`, the per-run cost of `Moments::of`, `Histogram::over`
and `zScore`, and the two quantile arms side by side, where the whole
point is that one sorts once and the other sorts per fraction; it builds
through the `benches` target and runs through `scripts/bench_ledger.py`.
