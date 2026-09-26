---
kind: type
library: SigilMeasure
name: Window
qualified: sigil::measure::Window
group: Statistics
status: stable
---

# Window

## Description

THE LAST FEW VALUES OF A STREAM, oldest first, and what they amount to.
`Window last{180}` holds the last 180 values; `Window recent{{.span =
4s}}` holds what was stamped in the last four seconds. `Window::add`
takes a value, `Window::mean`, `Window::min`, `Window::max`,
`Window::quantile` and `Window::latest` read the values held, and
`Window::values` is the held run itself.

A window's element is a number or a `std::chrono` span. A number is
read as a double; a span is read in its own unit held as a double, so a
window of frame times answers a mean frame time rather than a count of
ticks.

### Bounded by count, by time, or both

`WindowOptions::count` keeps that many values, the oldest leaving first.
`WindowOptions::span` keeps what was stamped within that long of the
newest time the window has seen, stamped by `Window::add` with a time.
With both, a value leaves when either says it is too old; with neither,
the window keeps everything until `Window::clear`.

The time is the CALLER'S clock: the scene's seconds, a feed's arrival
stamps. A window over scene time is therefore deterministic — the same
stamps give the same numbers on every machine — which is what lets a
plate carry one. `Window::advance` moves the window's time on with
nothing added, so a stream that fell silent empties rather than
freezing on its last values, and a stamp earlier than one already seen
does not wind the window back.

### The run is contiguous

`Window::values` is one span, oldest first, that a chart's trace or any
loop reads as it is, with no copy. The front a window has dropped is
released in blocks once it outweighs what is held, so a window costs at
most about twice its contents and an add stays constant time over a
long run. Nothing is cached: every reading is a pass over the values
held when it is asked, so a value that left the window is gone from
every number at once, and a quantile costs a sort of the window.

### Beside it

`sigil::measure::Rate` is a window of event stamps over a span of time,
read as `Rate::perSecond`; marking every frame makes it a frame counter.
`sigil::measure::Smoothed` follows a stream without holding any of it:
an exponential mean over about the last `SmoothedOptions::over` values,
a stated `SmoothedOptions::weight`, or a `SmoothedOptions::timeConstant`
that follows at the same speed whatever the step, and with
`SmoothedOptions::peak` the decaying marker a meter holds over its bar.
The first value it is given is taken whole, so it does not climb up
from zero.

## See also

`sigil::measure::Summary` for a run in hand, `sigil::measure::quantile`,
`sigil::measure::FrameTimer`, whose three lanes are windows of spans.
