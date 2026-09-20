---
kind: type
library: SigilMeasure
name: FrameTimer
qualified: sigil::measure::FrameTimer
group: Timing
status: stable
---

# FrameTimer

## Description

Four marks a render loop lays, three rings they feed.

- `FrameTimer::begin` at the top of the frame;
- `FrameTimer::composed` when the frame's OWN work is done — every
  command recorded, nothing yet flushed to the backend;
- `FrameTimer::finished` when the backend is done with the frame, flush
  included;
- `FrameTimer::presented` when the frame reaches the screen.

`FrameTimer::work` holds begin→composed, `FrameTimer::frame` holds
begin→finished, and `FrameTimer::present` holds the wall-clock delta
between consecutive presented marks.

### Why the two cost lanes are separate

Rather than one derived from the other: they answer different
questions. A synchronous backend drain is not work the frame does, but
it is time the machine spent, so charging it to the work lane
understates headroom and leaving it out of the frame lane understates
the cost. On a backend with no flush the two lanes hold the same
numbers.

A loop that times its own spans may add samples directly through
`FrameTimer::addFrame`, `FrameTimer::addWork` and
`FrameTimer::addPresent` instead of laying marks; the rings are the same
either way.

### The two rates

`FrameTimer::headroomFps` is NOT a frame rate: it is the rate the
frame's work alone would allow, with nothing said about presenting it or
flushing it. A ceiling, which stays high exactly when a stutter comes
from outside the measured work — so read it beside the end-to-end time,
never instead of it. It is 0 when no work sample has landed.

`FrameTimer::presentedFps` is frames per second as actually shown, from
the mean present interval; 0 before two frames have been presented.

The first presented mark after construction or a reset seeds the cadence
and adds no sample; there is no previous frame to measure from.
`FrameTimer::reset` empties every ring and forgets the last presented
mark, and `FrameTimer::resetPresentation` forgets only the presentation
cadence — for a pause, after which the interval across the gap is not a
frame time.

### The sample a gate reads

`sigil::measure::FrameSample` is the timing a headless sweep snapshots
when its sample window closes: both numbers a frame-budget gate judges
and the one it derives from. Plain numbers, so a snapshot survives the
ring it was read from being cleared or refilled; how a sample is written
out is the writer's business, not that struct's.

`FrameSample::frameMs` is the mean end-to-end frame time in
milliseconds, backend flush included: what the machine actually spent
per frame. `FrameSample::workMs` is the mean of the frame's own work,
the backend flush taken out. `FrameSample::p99Ms` is the tail of the
end-to-end lane. `FrameSample::headroomFps` is 1000 over the work time —
a ceiling, on the same terms as the timer's own.

## See also

`sigil::measure::Samples`, `sigil::measure::Laps`,
`sigil::measure::Stopwatch`.
