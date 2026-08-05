# SigilMotion

Animation timing and animation *values*, with no renderer in them. The
library gives you a monotonic frame clock that turns wall-clock time into
well-behaved per-frame deltas, a ticker that steps a
[Choreograph](https://github.com/sansumbrella/Choreograph) timeline plus
any callbacks you register and tells you whether anything is still
moving, and a small set of value types describing how a property changes
over time. It links Choreograph and nothing else, so anything can use it
without dragging in a graphics stack.

Namespace `sigil::motion`. Headers `FrameClock.h`, `Ticker.h`,
`Animation.h`.

## Using it

```cpp
#include <sigilmotion/Animation.h>
#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>

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

`Animatable<T>` is the property slot a consumer stores. It accepts a
constant, a transition spec, a bare `Output<T>*`, or a shaped `bind()`:

```cpp
Animatable<float> a = 1.0f;                                  // constant
Animatable<float> b = animate(from(0.0f).to(1.0f), {400ms}); // entrance
Animatable<float> c = &opacity;                              // live cell
Animatable<float> d = bind(&phase).window(0.2f, 0.6f)
                          .map(ease::outBack()).target(-70, 170);
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

`Animatable<T>` holds one of four forms, discriminated by `index()`: `0`
plain constant, `1` `Transitioned<T>`, `2` bare `Output<T>*`, `3` shaped
binding. `Bound` is the builder and `BoundFloat::apply` runs a fixed
pipeline: pre-normalise (`source`/`window`), optional input clamp, ease
curve (`map`), `quantize`, the affine chain (`scale`/`offset`/`target`/
`invert`, composed in call order), `wrap`, `wiggle` noise, output `clamp`.
The wiggle phase is read from the *normalised* value, before the curve, so
easing the signal does not ease the shake.

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

`Transition` is an aggregate, so `{360ms, {}, 220ms}` value-initialises
`ease` to an *empty* `std::function`, which compiles and then throws
`bad_function_call` when called. Read the curve through
`Transition::easing()`, which substitutes the default; never read `ease`
directly.

## Boundary

`SigilMotion` links `choreograph::choreograph` publicly and nothing else.
That is the point: consumers that also draw — a compositor, a 3D
renderer — link it without inheriting a drawing library, and can
re-export its types into their own namespaces.

The library ships the values and the clock. It does not resolve them.
Turning an `Animatable<T>` into a concrete value for a given frame, in a
given paint or render context, belongs to whoever owns that context.
Anything that would pull a graphics, layout, or scene-graph dependency in
here does not belong here.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug --target motion_test
ctest --test-dir build -C Debug -R motion_test --output-on-failure
```

Targets: `SigilMotion` (static library) and `motion_test`. No GPU, no
assets, no runtime requirements. `motion_test` links only `SigilMotion`
and GoogleTest, and fails the build if a compositing header becomes
reachable from it — that is how the dependency boundary above stays
honest.
