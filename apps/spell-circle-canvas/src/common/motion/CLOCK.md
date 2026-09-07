# SigilMotion — the clock

The chapter on the frame clock and the ticker: wall-clock time turned
into well-behaved per-frame deltas, the timeline and the steppables one
tick runs, the fixed-rate lane a simulation is driven from, and the
signal a host sleeps on. `README.md` beside this file is the library;
`VALUES.md` is what the ticker moves, `BIND.md` the shaping a derivation
runs, `SCHEDULE.md` the spreads that read no clock at all and
`PHYSICS.md` the point set a fixed lane steps.

## Using it

```cpp
#include <sigilmotion/Animation.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

using namespace sigil::motion;
using namespace std::chrono_literals;

FrameClock clock;
Ticker ticker;

// A live value cell, ramped to 1 over 0.4 s by the master timeline.
ch::Output<float> opacity = 0.0f;
ticker.timeline().apply(&opacity).then<ch::RampTo>(1.0f, 0.4f);

// A second cell computed from the first every tick, shaped on the way.
ch::Output<float> trail;
ticker.derive(&trail, bind(&opacity).offset(-0.1f).clamp(0.0f, 1.0f));

// A fixed-rate simulation, stepped 27 times a second whatever the draw rate.
ch::Output<float> alpha;
Ticker::FixedStatus status;
ticker.addFixed(27.0, [&] { stepFire(); return true; }, 8, &alpha, &status);

while (running) {
  const bool animating = ticker.tick(clock.tick());
  draw(opacity.value(), trail.value(), alpha.value());
  if (!animating)
    blockUntilNextEvent();   // nothing is moving; stop burning frames
}
```

## Mental model

Choreograph supplies the vocabulary — `Timeline`, `Motion`, `Phrase`,
`Output<T>`, `EaseFn`. This library only drives it. `Output<T>` is the
live value cell: your code owns it, the ticker writes it, and everything
downstream reads it through a pointer.

`FrameClock` produces deltas; `Ticker` consumes them. `Ticker::active()`
is the event-driven-redraw signal — true while the timeline holds motions
or any steppable remains registered, so a host can render when it is true
and sleep when it is not.

`Ticker::tick` runs two phases. First the timeline and every steppable, in
registration order; then the derivations. Because derivations run second,
a derived cell never reads a source that has not been stepped this frame,
whatever order things were registered in.

## Gotchas

`Ticker` is not thread-safe. Use one per animation domain and touch it
only from that domain's thread.

`FrameClock::tick` returns `0.0` on its first call and while paused, but
it still advances its internal timestamp. That is deliberate: unpausing
produces no catch-up spike, because the paused span was consumed as it
went. A single tick reports at most `FrameClockOptions::maxDelta` (0.25 s
by default), so a suspended app or a debugger break yields a clamped step
rather than a giant one.

`Ticker::elapsed()` accumulates the deltas handed to `tick()`. It is not
wall time — a paused or time-scaled clock changes it accordingly.

`derive()` allows exactly one level. Self-derivation, deriving from
another derivation's destination, and two derivations writing the same
cell are all refused: the call returns `false` and writes a message to
stderr. The chain is also applied once at registration, so the destination
holds a correct value before the first tick.

`addFixed` *discards* simulated time when the backlog exceeds
`maxCatchUp` — running slow for one frame instead of spiralling. When
that happens the frame's `FixedStatus::clamped` is the only signal, and
anything measured on that frame (a residual, a convergence rate) is
meaningless.

`active()` stays true while any steppable is registered, and a steppable
is only dropped when it returns `false`. A steppable that always returns
`true` pins the host awake forever. Derivations never contribute to
`active()` — they are pure in their source, so if nothing else moves,
neither can they.
