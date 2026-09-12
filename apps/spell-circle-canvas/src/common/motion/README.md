# SigilMotion

Animation timing and animation *values*, with no renderer in them. The
library gives you a monotonic frame clock that turns wall-clock time into
well-behaved per-frame deltas, a ticker that steps a
[Choreograph](https://github.com/sansumbrella/Choreograph) timeline plus
any callbacks you register and tells you whether anything is still
moving, a small set of value types describing how a property changes
over time, a schedule saying how a run of units shares one progress, and
a point set that is stepped rather than read. It links Choreograph and
two header-only SigilCore leaves, so anything can use it without
dragging in a graphics stack.

Namespace `sigil::motion`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilmotion/<feature>/` and is spelled `<sigilmotion/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilMotionBind`   | `bind/Bound.h`, `bind/BoundFloat.h`, `bind/Curve.h`, `bind/WiggleNoise.h`; `bind/Bind.h` includes all four | `bind()`, `wiggle()` and the `Bound` chain builder; `BoundFloat` and `Envelope`, the evaluator; `ease::`, the animation's name for SigilCore's shaped curve value and its house shapes; the wiggle noise field; `easeEqual()` and `boundMapEqual()` |
| `SigilMotionValues` | `values/Transition.h`, `values/Keyframes.h`, `values/Animatable.h`, `values/Animated.h`, `values/Lanes.h`, `values/Oscillator.h`, `values/Sequence.h`, `values/Spring.h`, `values/Time.h`; `values/Values.h` includes all nine | `Transition`, `ramp()`, `clamp01()` and `transitionEqual()`; `Transitioned`, `animate()`/`from()`/`to()`/`through()`; `Animatable<T>` and `propertyEqual()`; `AnimatedFloat`, the operations on a held motion, `isLive()` and `progressRamp()`; `Lane`, `LaneSlot` and the retargets; `quantizeTime()`, `stepIndex()`, `phase()`, `decay()` and `flash()`; `Spring`, `spring()` and `springMoving()`; `Oscillator` and `Wave`, the repeating signal; `Sequence`, `Step` and `Interpolation`, the keyed track |
| `SigilMotionClock`  | `clock/FrameClock.h`, `clock/Ticker.h` | the clock and the ticker |
| `SigilMotionSchedule` | `schedule/Spread.h`, `schedule/Order.h`, `schedule/Cascade.h`; `schedule/Schedule.h` includes all three | `Spread`, the spec; `cascadeOrder()`, the five orderings; `Cascade` and `Beat`, a spread resolved against a frame's counts |
| `SigilMotionPhysics` | `physics/Points.h`, `physics/Forces.h`, `physics/Neighbourhood.h`, `physics/Constraints.h`, `physics/Verlet.h`, `physics/Particles.h`; `physics/Physics.h` includes all six | `Vec2` and `Points`, the lanes a simulation is; `Force` with `gravity()`, `drag()`, `attract()`/`repel()`, `wind()` and `boids()`; `Neighbourhood`, the grid a flock and everything else that reads more than one point at a time asks what is near what; `Constraint` with `distance()`, `stick()`, `spring()`, `range()` and `pin()`; `Verlet`, the stepper; `Particles` and `Attribute`, a point set that is born, ages and dies, with `Emitter`, `EmitFrom`, `Roughly`, `BirthAttribute` and `FixedAttribute`, what puts particles into one |

`SigilMotion` is the umbrella target over all five, and
`<sigilmotion/Animation.h>` is the umbrella header over every values and
bind header, so a consumer that includes `Animation.h` and links
`SigilMotion` sees every value and binding. Bind is the leaf: Values
links it because `Animatable<T>` can hold a shaped binding, Clock links
it because `Ticker::derive` runs one, and Schedule links it because a
spread's distribution curve compares under the same rule every other
curve does. Values links Clock in turn, because an animatable that is
MOVING is moving on a ticker — the values feature ships both the slot and
the motion the slot runs as. Schedule links NEITHER the clock nor the
values: see below. Physics links neither, and does not link Choreograph
either — a step is a number of seconds the caller states.

## The chapters

One chapter per feature, beside this page. Each is the canon for the
feature it names, and everything below is the library as a whole.

| chapter | what it covers |
|---------|----------------|
| **[CLOCK.md](CLOCK.md)** | `FrameClock` and `Ticker`: deltas, the two phases of a tick, derivations, the fixed-rate lane, and the signal a host sleeps on |
| **[VALUES.md](VALUES.md)** | `Transition`, `Animatable<T>` and its four forms, the held `AnimatedFloat` a ticker runs, `Oscillator` and `Sequence`, `Spring`, the lanes a host retargets through, and the three words for stillness |
| **[BIND.md](BIND.md)** | `bind()`, the `Bound` chain and the fixed order `BoundFloat::apply` runs its stages in, the envelopes that are the waveform vocabulary, and the wiggle field |
| **[PHYSICS.md](PHYSICS.md)** | `Points`, `Force`, `Neighbourhood`, `Constraint` and `Verlet`: the one feature here that is stepped rather than read, and `Particles` with the `Emitter` that fills it |
| **[SCHEDULE.md](SCHEDULE.md)** | `Spread`, `cascadeOrder()` and `Cascade`: how N units share one progress, from a master float and nothing else |

## Comparing two descriptions

An identity prune asks whether two descriptions are provably the same,
and the animation values are the part of that question this library
answers. Four comparators, each beside the value it compares and each
under a field pin that fails the build when that value gains a member:

| comparator | rule |
|---|---|
| `easeEqual` | two curves are equal when both are the same plain function pointer, or both are the same `core::curve::Curve` shape at the same settings; a capturing lambda is unequal to everything |
| `transitionEqual` | same duration, same delay, same curve — the curve read through `easing()`, so `{360ms, {}, 220ms}` compares as the default it behaves as |
| `boundMapEqual` | every one of `BoundFloat`'s fields, by hand, under the pin |
| `propertyEqual` | same form, then that form's contents; a bare binding by the Output's IDENTITY, never by the number behind it |

The last rule is the load-bearing one: a live binding stays connected for
the whole life of the value it drives, so comparing the sampled number
would let a moving value prune into a still one.

## Boundary

Every feature but physics links `choreograph::choreograph` publicly. The
only other edges out of this directory reach the two header-only leaves
under SigilCore that depend on the standard library and nothing else:
`SigilCoreComparable` for `kFieldCount`, the pin each comparator above
sits under — so that a `static_assert` about `BoundFloat`'s field count
lives in the same file as `BoundFloat` — and `SigilCoreCompute` for the
seeded mixer the scattered ordering ranks with, so that a
`Spread::From::Random` permutation is the same permutation wherever in
the tree it is dealt,
for the noise field a wind reads, so that a flow a simulation drifts
along and the same flow drawn as a picture are the same field, and for
the SHAPED CURVE itself: `core::curve::Curve` and every house shape are
that leaf's, because a colour ramp and a keyed track reshape a unit
position exactly as an easing does and the colour leaf links no
animation runtime. `ease::` here is the animation's word for those, and
nothing more — a curve gets its arithmetic and its equality from the
leaf, and reads the same wherever it is held.
Both carry no kernel, no device and nothing that draws. Boost is
private, in one place: the scheduler's own table — a Boost unordered map
on `SigilMotionSchedule` — which no public header names.

`SigilMotionSchedule` links neither the clock nor the values. A cascade
is a pure function of a master float in [0, 1] and two integer counts,
and keeping it that way is what lets a text engine drive it from a
track's progress, a set from a lane and a study from a bare `phase()`.

That is the point: consumers that also draw — a compositor, a 3D
renderer — link this library without inheriting a drawing library, and
spell `sigil::motion` at the call site rather than re-exporting it.

The library ships the values, the clock, and the motion a value runs as
— everything answerable from the animatable and the ticker alone.
Resolving an animatable against a CONTEXT is the consumer's: which of a
node's properties are animated at all, where their held motions live, and
what a resolved number then means to a paint, a layout or a render pass.
Anything that would pull a graphics, layout, or scene-graph dependency in
here does not belong here — the physics feature included: it holds
positions and velocities and no colour, no size and no age, because what
a point looks like is the drawing's business.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target motion_test
ctest --test-dir build -C Release --output-on-failure
```

Targets: `SigilMotionBind`, `SigilMotionValues`, `SigilMotionClock`,
`SigilMotionPhysics` and `SigilMotionSchedule` — the libraries, one per
feature directory (`bind/`, `values/`, `clock/`, `physics/`,
`schedule/`), each holding its sources, its
`test/` and its `bench/` — plus `SigilMotion`, the umbrella.

One test binary, `motion_test`, built from every feature's `test/`
directory; ctest discovers one entry per CASE out of it, so a suite or a
case is selected by name with no target behind it — `-R '^Physics\.'`
for the stepper's cases, `-R '^Cascade\.'` for the schedule's:

| directory | suites | what they prove | what the feature must not be able to link |
|---|---|---|---|
| `bind/test/` | `Bind`, `Stages`, `StagePairs`, `Envelopes`, `PeriodicEnvelopes`, `BindNoise` | the `bind()` chain: every stage against the arithmetic it stands in for, the place each stage owns, the envelopes, `wrap`, the wiggle field, and the two comparators field by field | anything above the leaf — the record that carries a curve is the lowest thing here |
| `clock/test/` | `FrameClock`, `Ticker` | one reading after another, pause, time scale and the stall ceiling; the Ticker stepping motions, steppables and derivations, and the fixed step that keeps its own rate whatever the host draws at | a renderer |
| `values/test/` | `Values`, `Forms`, `Animated`, `Lanes`, `Oscillator`, `Sequence`, `Spring` | `Transition`, the `animate()` builders, `quantizeTime`, the four forms an `Animatable<T>` holds, the two signals read from a time alone, springs, the held motion of an animatable, and the lanes a host retargets through | a renderer |
| `physics/test/` | `Physics`, `Particles` | the lanes a point set is and what `remove` does to their numbering, each force against the arithmetic it stands in for, a distance band read as a stick, a spring and a rope, the velocity a constraint pass gives back, the same run reproduced from the same `dt`, and the degenerate settings a caller can hand in; then a rate that produces the count it promises, a mouth that puts its births where its shape says, lifetimes that expire and compact the set, a named attribute that rides through a death, and the same seed twice as the same cloud | **the clock** — a step is a number of seconds the caller states, and a link edge to a timeline would be the first step to something in here reading time for itself |
| `schedule/test/` | `Spread`, `Order`, `Cascade`, `CascadeOrdering` | the orderings, the ladder, cue tables, the nested and looping cascade, and the field walk over a spread's equality | **the clock** — a cascade is a pure function of a master float and two counts, and a link edge to the clock would be the first step to something in here reading time for itself |

No binary needs a GPU, a font, an asset or a network, so none of them
carries a ctest label and none of them skips. No test in any of them reads
a wall clock either: every frame length, every phase and every progress is
a number the case hands in, so a slow machine changes nothing about what
they assert.

A case asserts one thing a public header promises and is named that
promise as a sentence, so a failure line reads as the claim that broke. It
pins only what editing this library could falsify — the numbers a stage
computes, the order the stages take, the discriminant of a slot, the
fields a comparator reads — never elapsed time, which is the bench
ledger's. A claim made N times with one thing varying is one `TEST_P`
whose parameter is that thing and whose rows are named: the chain's stages
against the arithmetic they stand in for, the pairs of stages written
either way round, the envelopes that stay inside [0,1] and the ones that
repeat every period, the four forms an `Animatable<float>` holds, and the
orderings a cascade deals its ranks in.

One file per subject, named for what it asserts. In `bind/test/`,
`BindTest` (the chain builder), `BoundFloatTest` (the evaluation, the
envelopes and the wrap), `WiggleNoiseTest` (the noise stage and the field
under it) and `CurveComparatorTest` (the two comparators); then
`clock/test/ClockTest.cpp`; then `physics/test/PhysicsTest.cpp` (the
stepper) and `ParticlesTest.cpp` (what is born, ages and dies);
`schedule/test/ScheduleTest.cpp`; and, in `values/test/`, `ValuesTest`
(the values themselves), `AnimatedTest` (the held motion), `LanesTest`
(the lane list), `OscillatorTest` and `SequenceTest` (the two signals
read from a time alone) and `SpringTest` (the one value that carries its
own velocity).

Fixtures more than one test binary needs live in `test/support/` at the
library root, and every motion test includes `"support/<Name>.h"`. Two
headers sit there. `StandsAlone.h` fails the build if a drawing library's
headers become reachable from a motion test — the positive control under
every "SigilMotion alone" claim, without which those tests would pass for
the wrong reason on a machine where a compositing header happened to be on
the include path; every motion test opens with it. `Ramps.h` is the
transitioned value a test that drives a motion starts from: a linear ramp
to a target over a stated number of milliseconds, linear so the
assertions can read the value halfway through and name the number without
evaluating a curve. Otherwise each test links only the library it
exercises, plus the clock where a value is driven by the ticker, and
GoogleTest.

One Google Benchmark binary, `motion_bench`, is built by the `benches`
target and run from a Release build through `scripts/sigil.py bench`,
which is where any number about this library belongs. Its arms:
`bind/bench/` (`BoundFloat::apply`
per call under each envelope and the full chain, and the wiggle field by
octave), `values/bench/` (the consumer's read of an `Animatable`
lane per slot for each kind it can hold, copying and constructing such
a lane, and the two time-only signals read one call at a time),
`clock/bench/` (the frame clock's own step, the timeline
stepped with N motions on it, and the derivation pass at N derived
cells), `physics/bench/` (a field of free particles, the same field
flocking — which is where comparing every pair shows — a chain of
sticks under its constraint passes, and ten thousand particles born,
stepped, aged and reaped, each measured on its own) and `schedule/bench/` (resolving a
cascade for a frame's counts, and the per-unit local-time read).
