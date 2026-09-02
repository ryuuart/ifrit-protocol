# SigilMeasure

Timing, statistics and check reporting, on the standard library alone. The
library gives you a stopwatch, a lap timer that names the phases of one
span, a frame timer whose four marks feed the three lanes a render loop
is judged by, a rolling ring of samples with the summaries a HUD prints,
named counters, the frame sample a headless timing sweep snapshots, and
`Check` — a claim whose printed verdict is computed from the values it
reports. It links nothing, so every
other library can measure itself with it without acquiring a dependency.

Namespace `sigil::measure`. One static target, `SigilMeasure`; every
public header lives under `include/sigilmeasure/<subject>/` and is spelled
`<sigilmeasure/<subject>/X.h>`, and `<sigilmeasure/Measure.h>` includes
them all:

| header | holds |
|--------|-------|
| `time/Stopwatch.h`    | `Stopwatch` (`elapsedMs()`, `reset()`) and `ScopedMs`, which writes a block's milliseconds into a double at scope exit |
| `time/Laps.h`         | `Laps` — `mark(name)` returns the milliseconds since the previous mark and records the lap; `each()` reads them back |
| `time/FrameTimer.h`   | `FrameTimer` — `begin()`, `composed()`, `finished()`, `presented()` feeding the `frame()`, `work()` and `present()` rings, with `headroomFps()` and `presentedFps()` read off them |
| `stats/Samples.h`     | `Samples`, a rolling ring (`add`, `mean`, `percentile`, `min`, `max`, `last`, `size`, `samples`), and the free `quantile()` it and everything else shares |
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

`measure_test` covers the quantile's edges (empty, one sample,
interpolation, clamping), the ring's wrap-around, the counters, the lap
timer's naming and totals, `ScopedMs` leaving its target alone until
scope exit, the frame timer's lanes, and `Check::line` formatting as one
parameterised case per kind of claim — integral, tolerance, text, bare
condition, and a label longer than its column. Exactly one case reads the
wall clock, and it owns every claim that needs one: that the stopwatch,
the lap timer and `ScopedMs` advance with real time and that a reset
sends the reading back. Every other timing case is deterministic, because
sleeping and then asserting on a duration asserts the operating system's
scheduler rather than anything this library promises. `measure_bench` times `Samples::add`,
`Samples::percentile` and `Samples::mean` per sample count; it builds
through the `benches` target and runs through `scripts/bench_ledger.py`.
