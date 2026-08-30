# SigilMotion

Animation timing and animation *values*, with no renderer in them. The
library gives you a monotonic frame clock that turns wall-clock time into
well-behaved per-frame deltas, a ticker that steps a
[Choreograph](https://github.com/sansumbrella/Choreograph) timeline plus
any callbacks you register and tells you whether anything is still
moving, and a small set of value types describing how a property changes
over time. It links Choreograph and nothing else, so anything can use it
without dragging in a graphics stack.

Namespace `sigil::motion`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilmotion/<feature>/` and is spelled `<sigilmotion/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilMotionBind`   | `bind/Bound.h`, `bind/BoundFloat.h`, `bind/WiggleNoise.h`; `bind/Bind.h` includes all three | `bind()`, `wiggle()` and the `Bound` chain builder; `BoundFloat` and `Envelope`, the evaluator; the wiggle noise field |
| `SigilMotionValues` | `values/Transition.h`, `values/Keyframes.h`, `values/Animatable.h`, `values/Time.h`; `values/Values.h` includes all four | `Transition`, `ease::` and `ramp()`; `Transitioned`, `animate()`/`from()`/`to()`/`through()`; `Animatable<T>`; `quantizeTime()` and `phase()` |
| `SigilMotionClock`  | `clock/FrameClock.h`, `clock/Ticker.h` | the clock and the ticker |

`SigilMotion` is the umbrella target over all three, and
`<sigilmotion/Animation.h>` is the umbrella header over every values and
bind header, so a consumer that includes `Animation.h` and links
`SigilMotion` sees every value and binding (the clock headers are
included on their own). Bind is the leaf: Values links it because
`Animatable<T>` can hold a shaped binding, and Clock links it because
`Ticker::derive` runs one.

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
pipeline: pre-normalise (`source`/`window`), optional input clamp, the
envelope (`pingPong`/`cosine`/`trapezoid`/`square`/`wave`), ease curve
(`map`), `quantize`, the affine chain (`scale`/`offset`/`target`/`invert`,
composed in call order), `wrap`, `wiggle` noise, output `clamp`. The
wiggle phase is read from the *normalised* value, before the envelope and
the curve, so easing or folding the signal does not ease or fold the
shake.

The envelope is the shape a one-way phase takes across its span, and it is
the answer to the loop signals every study otherwise hand-steps into its
ticker:

```cpp
bind(&secs).source(0, 7.2f).cosine()                  // the breath
bind(&secs).source(0, 4.0f).pingPong().target(0, 240) // there and back
bind(&cycle).source(0, 15.f).trapezoid(0, .03f, .84f, .95f)  // hold, cut
bind(&secs).source(0, 1.06f).square(0.58f).target(0.1f, 1.f) // the blink
bind(&secs).wave([](float u) { return u * u; })       // your own period
```

Together the stages are the library's **waveform vocabulary** — each verb
is one of the classic signal shapes, spoken where its output lands:

| verb        | classic waveform | what it says |
|-------------|------------------|--------------|
| `pingPong`  | triangle         | there and back across the span, repeating |
| `cosine`    | sine             | the swell — 0, up to 1 at mid-span, back, eased |
| `square`    | pulse            | ON for the first `duty` of each period, OFF after; phase 0 is ON |
| `trapezoid` | gate             | ramp, hold at 1, ramp, dark — positions inside one pass |
| `wrap`      | sawtooth         | folds the **output** into [0, period) |
| `wiggle`    | noise            | smooth value noise, added in output units |
| `wave`      | custom           | your function on the folded phase u ∈ [0,1) |

The first four are envelopes and share one slot; `wrap` and `wiggle` are
their own later stages and compose with any of them. The distinction that
keeps `wrap` out of the envelope row: an envelope shapes the *normalised
phase* before the curve and the affine chain, while `wrap` folds the
value *after* the affine chain — a sawtooth over the schedule is an
unfolded phase driving `wrap`, not an envelope.

`cosine`, `pingPong` and `square` are periodic in the normalised phase, so
a monotonic seconds Output keeps breathing, bouncing or pulsing;
`trapezoid` names positions inside one pass and stays dark past its last
corner, so a repeating sheet rides a phase that already wraps. `wave` is
periodic by the same fold the others use: the function is evaluated on
u ∈ [0,1) and repeats whatever it drew there. Because the envelope runs
*before* `map`, any curve through (0,0) and (1,1) rounds a trapezoid's
shoulders while leaving its hold at exactly 1 and its dark at exactly 0 —
where the corners are and what shape the shoulders take are separate
decisions. `square` has no shoulders to round, and its phase 0 is ON —
a caret born at the start of its cycle is born visible.

## Gotchas

`Ticker` is not thread-safe. Use one per animation domain and touch it
only from that domain's thread.

`FrameClock::tick` returns `0.0` on its first call and while paused, but
it still advances its internal timestamp. That is deliberate: unpausing
produces no catch-up spike, because the paused span was consumed as it
went. A single tick reports at most `FrameClockOptions::maxDelta` (0.25 s
by default), so a suspended app or a debugger break yields a clamped step
rather than a giant one.

A binding carries ONE envelope. `pingPong`, `cosine`, `trapezoid`,
`square` and `wave` write the same slot, so naming a second replaces the
first exactly as a second `map()` replaces the first curve — there is no
raised cosine of a trapezoid. `trapezoid`'s four corners are held
non-decreasing, so a zero-length shoulder is an instant cut rather than a
division by zero and corners given out of order collapse onto the one
before them.

`wave`'s function is part of the binding's identity, compared the way
`map()`'s curve is: a consumer that prunes on equality can compare a plain
function pointer, while a capturing lambda compares unequal to everything
and re-patches on every describe. Name the shape as a free function where
that cost matters.

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

Each of the three libraries links `choreograph::choreograph` publicly and
nothing else outside this directory.
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
cmake --build build --config Debug \
  --target motion_clock_test motion_values_test motion_bind_test
ctest --test-dir build -C Debug -R motion_ --output-on-failure
```

Targets: `SigilMotionBind`, `SigilMotionValues`, `SigilMotionClock`
(the libraries, one per feature directory — `bind/`, `values/`, `clock/`
— each holding its sources, its `test/` and its `bench/`), `SigilMotion`
(the umbrella), and one test per library:
`motion_bind_test`, `motion_values_test` and `motion_clock_test`, plus two
Google Benchmark binaries built by the `benches` target and run from a
Release build through `scripts/bench_ledger.py`: `motion_bind_bench`
(`BoundFloat::apply` per call under each envelope and the full chain, and
the wiggle field by octave) and `motion_values_bench` (the consumer's read
of an `Animatable` lane per slot for each kind it can hold, and copying
and constructing such a lane). No GPU,
no assets, no runtime requirements. Each test links only the library it
exercises (plus the clock where a value is driven by the ticker) and
GoogleTest, and fails the build if a compositing header becomes
reachable from it — that is how the dependency boundary above stays
honest.
