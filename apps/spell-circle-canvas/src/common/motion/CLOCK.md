# SigilMotion — the clock

The chapter on the frame clock and the ticker: wall-clock time turned
into well-behaved per-frame deltas, the motions and the steppables one
tick runs, the fixed-rate lane a simulation is driven from, and the
signal a host sleeps on. `README.md` beside this file is the library;
`VALUES.md` is what the ticker moves, `SCHEDULE.md` the spreads that read
no clock at all and `PHYSICS.md` the point set a fixed lane steps.

## Using it

```cpp
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

using namespace sigil::motion;

FrameClock clock;
Ticker ticker;

// A per-frame job, offered the frame's delta.
float angle = 0.0f;
ticker.add([&](double deltaSeconds) { angle += deltaSeconds * 90.0f; });

// A fixed-rate simulation, stepped 27 times a second whatever the draw rate.
float betweenSteps = 0.0f;
Ticker::FixedStatus status;
ticker.addFixed(27.0, [&] { stepFire(); return true; }, 8, &betweenSteps, &status);

while (running) {
  const bool animating = ticker.tick(clock.tick());
  draw(angle, betweenSteps);
  if (!animating)
    blockUntilNextEvent();   // nothing is moving; stop burning frames
}
```

## Mental model

`FrameClock` produces deltas; `Ticker` consumes them. `Ticker::active()`
is the event-driven-redraw signal — true while a motion runs or any
steppable remains registered, so a host can render when it is true and
sleep when it is not.

`Ticker::tick` steps the motions first, in the order they were started
(`Ticker::run`, which the held motions in `VALUES.md` start), then every
steppable in registration order, so a steppable reading a moving value
reads this frame's number. A value shaped through `bind()` needs no step
of its own: it reads its source whenever it is read.

`Ticker::add` offers a steppable the frame's delta and the ticker's total
elapsed time, and it names the ones it reads: `[] {…}`, `[](double deltaSeconds) {…}`
and `[](double deltaSeconds, double elapsed) {…}` are all steppables. It may answer
whether it still needs frames, and one that answers nothing always does
(see the gotcha below). `addFixed` reads the same rule with nothing
offered: `[] {…}` or `[] { … return alive; }`.

## Who moves the clock

A run is repeatable when the time of every frame is a function of what
the run was told, and `PolicyClock` is where that is decided. It holds a
`FrameClock` under a `ClockPolicy`:

- `ClockPolicy::Wall` — a frame the host draws on its own moves by the
  time that passed, paused and time-scaled: a window.
- `ClockPolicy::Advance` — only `PolicyClock::step` moves it, by exactly
  the step it is handed: a capture, a test, a sweep.
- `ClockPolicy::Pause` — nothing moves it, a step included.
- `ClockPolicy::PauseWhileLoading` — the wall, except that a frame drawn
  while something the run asked for is still arriving moves nothing.

```cpp
#include <sigilmotion/clock/ClockPolicy.h>

PolicyClock clock;
clock.setPolicy(ClockPolicy::Advance, /*budgetSeconds=*/2.0);
for (int frame = 0; frame < 120; ++frame) {
  ticker.tick(clock.step(1.0 / 60.0));
  if (clock.budgetExpired()) settled();   // true on the 120th alone
}
```

`PolicyClock::frame` is the frame a host draws on its own and
`PolicyClock::step` the one a caller states; `PolicyClock::wall` is the
one reading a body needs, whether the wall moves its clock. A budget is
clock seconds from the moment it was set, and `PolicyClock::budgetExpired`
answers true on the frame that reaches it and never again; a clock that
does not move never spends one. `PolicyClock::setHeld` is the pause a
person presses and keeps the policy, and `PolicyClock::restart` counts
from zero again as a new session opens under the same clock.

## Gotchas

A frame under `PolicyClock` that moves nothing still takes its wall
reading, so a return to `ClockPolicy::Wall` measures from there rather
than catching up on the stretch the clock stood still for. A stated step
is not time-scaled and not stall-clamped: the caller chose it.

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

`addFixed` *discards* simulated time when the backlog exceeds
`maxCatchUp` — running slow for one frame instead of spiralling. When
that happens the frame's `FixedStatus::clamped` is the only signal, and
anything measured on that frame (a residual, a convergence rate) is
meaningless.

`active()` stays true while any steppable is registered, and a steppable
is only dropped when it returns `false`. A steppable that always returns
`true` pins the host awake forever — and so does one that returns NOTHING:
saying nothing about being finished is taken as never finished, so a void
steppable holds `active()` true for as long as it is added.
