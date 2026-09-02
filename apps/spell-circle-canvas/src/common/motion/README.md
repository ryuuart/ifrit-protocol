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
| `SigilMotionBind`   | `bind/Bound.h`, `bind/BoundFloat.h`, `bind/WiggleNoise.h`; `bind/Bind.h` includes all three | `bind()`, `wiggle()` and the `Bound` chain builder; `BoundFloat` and `Envelope`, the evaluator; the wiggle noise field; `easeEqual()` and `boundMapEqual()` |
| `SigilMotionValues` | `values/Transition.h`, `values/Keyframes.h`, `values/Animatable.h`, `values/Animated.h`, `values/Lanes.h`, `values/Time.h`; `values/Values.h` includes all six | `Transition`, `ease::`, `ramp()`, `clamp01()` and `transitionEqual()`; `Transitioned`, `animate()`/`from()`/`to()`/`through()`; `Animatable<T>` and `propEqual()`; `AnimatedFloat`, the operations on a held motion, `isLive()` and `progressRamp()`; `Lane`, `LaneSlot` and the retargets; `quantizeTime()`, `stepIndex()`, `phase()` and `decay()` |
| `SigilMotionClock`  | `clock/FrameClock.h`, `clock/Ticker.h` | the clock and the ticker |
| `SigilMotionSchedule` | `schedule/Spread.h`, `schedule/Order.h`, `schedule/Cascade.h`; `schedule/Schedule.h` includes all three | `Spread`, the spec; `cascadeOrder()`, the five orderings; `Cascade` and `Beat`, a spread resolved against a frame's counts |

`SigilMotion` is the umbrella target over all four, and
`<sigilmotion/Animation.h>` is the umbrella header over every values and
bind header, so a consumer that includes `Animation.h` and links
`SigilMotion` sees every value and binding. Bind is the leaf: Values
links it because `Animatable<T>` can hold a shaped binding, Clock links
it because `Ticker::derive` runs one, and Schedule links it because a
spread's distribution curve compares under the same rule every other
curve does. Values links Clock in turn, because an animatable that is
MOVING is moving on a ticker — the values feature ships both the slot and
the motion the slot runs as. Schedule links NEITHER the clock nor the
values: see below.

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

A moving `Animatable<float>` has a second half: the motion a ticker is
actually running for it. That is `AnimatedFloat` — a held `Output<float>`,
whether it has started, and the endpoint it is flying at — and a consumer
that retains state keeps one beside each animatable it lets move. Four
operations are stated over one held motion, so the consumer's storage (a
fixed array, a vector, one member) stays its own business:

```cpp
AnimatedFloat*                    held;    // what the consumer retains
resolveFloatAt(held, v);                   // the value for this frame
transitionFloatAt(ticker, held, prev, next, fallback);  // a moved target
mountEntrance(ticker, held, v, extraDelaySeconds);      // the first appearance
```

`resolveFloatAt` is the reading order and the reason there is one body:
a bound `Output` wins (shaped through its map when it has one), then a
running ramp, then the plain value. `transitionFloatAt` starts a ramp
from WHERE THE VALUE IS rather than from the previous description, so a
target that moves mid-flight bends the motion instead of restarting it;
a motion already headed at the new target keeps flying, and a next value
that is plain or bound snaps and disconnects. `mountEntrance` plays the
`from` an `animate(from(a).to(b))` declares, or a `through({…})`
waypoint list segment by segment, after whatever extra delay the caller
staggers by. `resolveProp<T>` is the flattening underneath: an animatable
read against a fallback transition, giving a target, a binding, or a
spec.

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

## Schedules: how N units share one progress

A `Spread` says how a run of units divides one master progress between
them — the delay between one and the next, the order they are dealt in,
how long one unit's own motion lasts, and whether the whole thing loops.
It says nothing about WHAT a unit is. `Cascade` resolves it against the
counts a frame actually has, and then answers per index:

```cpp
Spread spec{.eachMs = 60, .durationMs = 420};
spec.from = Spread::From::Center;

Cascade cascade;                     // reused in place across frames
cascade.build(spec, unitCount, 0);
for (uint32_t i = 0; i < unitCount; ++i)
  paint(i, cascade.localTime(master, i, 0));   // this unit's own 0→1
```

`master` is a float in [0, 1] the caller owns — a track's progress, a
lane, a bare `phase()`. That is the whole interface, and it is why the
schedule feature links no clock: nothing in it reads time, so a text
engine, a set mounting its children, a feed's rows and a study's loop
counter can all drive the same body from four different clocks.

`spanMs()` is the DECLARE-TIME half: what a progress transition's
duration has to be for the last beat to close exactly as the master
arrives at 1, before any of the units exist. `Cascade::totalMs` is the
same number off a resolved cascade, and the two agree because one body
computes both.

Four things a spread can be, in the order they override each other: an
even ladder (`eachMs`), a fixed total divided across whatever the count
turns out to be (`amountMs`), an irregular table of start times cut
against a recording (`cueMs`, which replaces the ladder, the order and
the distribution outright), and a second spread nested inside every beat
of the first (`then()`, exactly one level deep). `loopMs` turns any of
them into a wrapping beat: each unit re-opens on its own cycle, phase-
offset by its start, and one sweep of the master 0→1 is one cycle.

`Cascade::beat()` is the schedule read BACK rather than driven — start
time, local time and whether the beat is running — for anything that has
to travel with a cascade without being one of its units: a playhead, a
travelling underline, a per-unit meter. Without it each of those restates
`i · eachMs` and stops agreeing with the engine the moment the cascade
nests or takes a table.

## Lanes: where a host's motions live

A retained host holds one `AnimatedFloat` per animatable it lets move,
and a patch has to bend the running motions of the old description onto
the endpoints the new one asks for. A **lane** is that pairing: an
animatable the description carries, and the address of the held motion
that serves it.

```cpp
enum class Family : uint8_t { Slot, Span };   // the HOST's storages

std::vector<Lane<Family>> prev, next;         // filled by the host
retargetSlots<Family>(ticker, anims, familyLanes(prev, Family::Slot),
                      familyLanes(next, Family::Slot), nodeTransition);
retargetFamily<Family>(ticker, spanAnims, familyLanes(prev, Family::Span),
                       familyLanes(next, Family::Span), nodeTransition);
```

`Family` is the host's own enumeration and nothing here reads it beyond
grouping — which is why lanes are motion's rather than a reconciler's.
A **fixed** family is a slot array whose rows are a property of the host,
so a row one description lacks ramps from or to the lane's `standing`
value and a row neither carries is skipped entirely. A **positional**
family is sized by the description, so a change of SHAPE drops the
running motions rather than carrying them onto endpoints that now mean
something else.

## Stillness, in three words

"Is anything still moving" is three different questions, and answering one
with another is how a tree that has come to rest goes on repainting
forever. Each has its own word here:

| word | asks | grain |
|---|---|---|
| **declared** — a value holds a binding or a transition | *could* this move? | one value, from the description alone |
| **running** — `isLive(anim, v)` | is it moving *now*? | one value plus the motion held for it |
| **settled** — `core::Settle` in SigilCore | has it provably *held still*? | a node's values, observed across frames |

The trap is that the first two can never say "it stopped". A binding
stays attached for the whole life of the value it drives, so a
declaration is permanent; and `AnimatedFloat::started` is permanent in
the same way, which is why `isLive` asks `Output::isConnected()` instead
— Choreograph disconnects an output when its motion finishes, and that is
the one thing in a running motion that changes when it lands.

Even "running" is a declaration about the *machinery*, not about the
numbers: a wave held at a constant phase is connected and moves nothing.
Only the third question is a FACT, and answering it means comparing the
values across frames, which is a caching concern and lives with the cache.
`Ticker::active()` is the same question asked of a whole animation domain
rather than one value — is any motion registered at all — and it is the
signal a host sleeps on.

## Comparing two descriptions

An identity prune asks whether two descriptions are provably the same,
and the animation values are the part of that question this library
answers. Four comparators, each beside the value it compares and each
under a field pin that fails the build when that value gains a member:

| comparator | rule |
|---|---|
| `easeEqual` | two curves are equal when both are the same plain function pointer; a capturing lambda is unequal to everything |
| `transitionEqual` | same duration, same delay, same curve — the curve read through `easing()`, so `{360ms, {}, 220ms}` compares as the default it behaves as |
| `boundMapEqual` | every one of `BoundFloat`'s fields, by hand, under the pin |
| `propEqual` | same form, then that form's contents; a bare binding by the Output's IDENTITY, never by the number behind it |

The last rule is the load-bearing one: a live binding stays connected for
the whole life of the value it drives, so comparing the sampled number
would let a moving value prune into a still one.

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

Every feature links `choreograph::choreograph` publicly. The only other
edge out of this directory is `SigilCoreComparable`, the header-only leaf
over the standard library and Boost.PFR that supplies `kFieldCount` — the
pin each comparator above sits under, so that a `static_assert` about
`BoundFloat`'s field count lives in the same file as `BoundFloat`. It
carries no kernel, no device and nothing that draws.

That is the point: consumers that also draw — a compositor, a 3D
renderer — link this library without inheriting a drawing library, and
can re-export its types into their own namespaces.

The library ships the values, the clock, and the motion a value runs as
— everything answerable from the animatable and the ticker alone.
Resolving an animatable against a CONTEXT is the consumer's: which of a
node's properties are animated at all, where their held motions live, and
what a resolved number then means to a paint, a layout or a render pass.
Anything that would pull a graphics, layout, or scene-graph dependency in
here does not belong here.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug \
  --target motion_clock_test motion_values_test motion_bind_test \
           motion_schedule_test
ctest --test-dir build -C Debug -R motion_ --output-on-failure
```

Targets: `SigilMotionBind`, `SigilMotionValues`, `SigilMotionClock`,
`SigilMotionSchedule` (the libraries, one per feature directory —
`bind/`, `values/`, `clock/`, `schedule/` — each holding its sources, its
`test/` and its `bench/`), `SigilMotion` (the umbrella), and one test per
library: `motion_bind_test`, `motion_values_test`, `motion_clock_test`
and `motion_schedule_test`, plus three
Google Benchmark binaries built by the `benches` target and run from a
Release build through `scripts/bench_ledger.py`: `motion_bind_bench`
(`BoundFloat::apply` per call under each envelope and the full chain, and
the wiggle field by octave), `motion_values_bench` (the consumer's read
of an `Animatable` lane per slot for each kind it can hold, and copying
and constructing such a lane) and `motion_schedule_bench` (resolving a
cascade for a frame's counts, and the per-unit local-time read). No GPU,
no assets, no runtime requirements. Each test links only the library it
exercises (plus the clock where a value is driven by the ticker) and
GoogleTest, and fails the build if a compositing header becomes
reachable from it — that is how the dependency boundary above stays
honest.
