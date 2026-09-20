---
kind: type
library: SigilMeasure
name: Laps
qualified: sigil::measure::Laps
group: Timing
status: stable
---

# Laps

## Description

Marks laid through one span of work, each named for the phase that just
ended. `Laps::mark` returns the milliseconds since the previous mark (or
since construction) and records them under that name, so consecutive
laps tile the span exactly: the end of one is the start of the next,
with no gap between. The recorded laps read back through `Laps::each` in
the order they were laid, and `Laps::totalMs` is their sum — the span
from construction, or the last reset, to the last mark.

### A name is borrowed, not copied

So the caller owns it and must outlive the timer: they are string
literals in practice, and a lap timer that allocated a string per phase
would be a cost inside the span it times. A name that is a temporary
would be dangling by the time `Laps::each` read it, so a `std::string`
rvalue is REFUSED at compile time rather than stored: build the name
into something that outlives the timer, or name the phase with a
literal.

## See also

`sigil::measure::Stopwatch` for one span, `sigil::measure::FrameTimer`
for the frame.
