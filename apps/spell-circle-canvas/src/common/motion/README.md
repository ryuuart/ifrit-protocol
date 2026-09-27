# SigilMotion

Animation timing and animation *values*, with no renderer in them. The
library gives you `Animatable<T>`, the one noun for a value that changes
over time — a constant, a described motion, a live value you write, or a
live value followed through a binding — `Tween`, the one description of
a motion, the `Engine` animations, timelines and timers run on and that
tells a host whether anything is still moving, `stagger()` and the
schedule saying how a run of units shares one progress, and a point set
that is stepped rather than read. It links
two header-only SigilCore leaves and nothing else outside the tree but a
private Boost table, so anything can use it without dragging in a
graphics stack.

## The first screen

What a sketch writes, every other parameter defaulted — anime.js's first
page, in C++. Each line names the header that owns it.

```cpp
using Duration = std::chrono::duration<double>;          // 320ms, 1.2s            time/Duration.h

// a value that moves                                       values/Animatable.h, values/Tween.h
Animatable<float> fade = animate({.from = 0.0f, .to = 1.0f});   // an entrance: plays on mount
Animatable<float> lift = animate({.to = lifted ? -8.0f : 0.0f}); // eases whenever `.to` changes
box.transition(320ms);                                    // every plain value on a node eases
Animatable<float> wave = animatable(0.0f);                // a live value you write
wave = std::sin(seconds);
Animatable<float> turn = bind(wave, {.to = {-8, 8}});     // follow a live number   bind/Binding.h

// curves                                                   ease/Ease.h
ease::linear · ease::{in,out,inOut}{Quad,Cubic,Quart,Quint,Sine,Expo,Circ} · ease::outBack()

// one sibling after another                                schedule/Stagger.h
.delay = stagger(40ms) · .to = stagger({0.0f, 360.0f}) · stagger(60ms, {.from = StaggerFrom::Center})

// the engine a sketch's motion runs on (ctx.engine)        clock/Engine.h
engine.animate(glow, {.to = 1.0f, .duration = 400ms});   // a float, a glm::vec2, a colour…
engine.animate(letters, {.to = 1.0f, .delay = stagger(40ms)}); // a list: siblings
engine.timeline().add(title, {.to = 1.0f}, at(500ms)).add(rule, {.to = 1.0f}, withPrevious());
engine.timer([&] { wave = std::sin(engine.elapsed().count()); });
play() pause() resume() restart() reverse() seek(t) complete() cancel() revert() onComplete(f)
float phase(Duration time, Duration period);             // wrapping [0, 1)        values/Time.h
```

The defaults: a tween takes 250ms on `ease::outQuad` with no delay, from
where the value rests; a timeline item starts after everything already on
it has ended; a timer runs every frame until it is cancelled. When a
default is wrong, each verb takes one options struct as its last argument
— `Tween`, `Transition`, `Binding`, `StaggerOptions`, `TimerOptions` —
and the chapters below spell every field. The control a host, a
reconciler or a test needs (`Engine::advance`, `ClockPolicy`, the held
motions and lanes) is in `advanced/` and `CLOCK.md`, never on this
screen.

Namespace `sigil::motion`. One feature library per directory, linked by
what a consumer uses; every public header lives under
`include/sigilmotion/<feature>/` and is spelled `<sigilmotion/<feature>/X.h>`:

| target | headers | holds |
|--------|---------|-------|
| `SigilMotionEase`   | `ease/Ease.h` | `Easing`, the one type every curve slot holds; the named curves `ease::linear`, then `ease::inQuad`, `ease::outQuad`, `ease::inOutQuad` and the same three for Cubic, Quart, Quint, Sine, Expo and Circ as plain functions, and for Back, Elastic and Bounce as factories over their shape parameter (`ease::outBack(overshoot)`, `ease::outElastic(amplitude, period)`, `ease::inOutBounce(overshoot)`); the families `ease::in(power)`, `ease::out(power)`, `ease::inOut(power)`, `ease::steps(count, jumpAtStart)`, `ease::cubicBezier(x1, y1, x2, y2)` and `ease::smoothstep`; `ease::Curve`, SigilCore's comparable shaped curve; `easeEqual()` |
| `SigilMotionTime`   | `time/Duration.h` | `Duration`, the one time unit |
| `SigilMotionBind`   | `bind/Binding.h`, `bind/WiggleNoise.h` | `Binding`, the stages a followed number is shaped through as fields, with `Range`, `Envelope` and its `envelope::` factories, and `Wiggle`; the wiggle noise field |
| `SigilMotionValues` | `values/Tween.h`, `values/Interpolate.h`, `values/Transition.h`, `values/Animatable.h`, `values/Oscillator.h`, `values/Spring.h`, `values/Time.h` | `Tween`, `Keyframe`, `Composition`, `animate()` and `tweenEqual()`; `interpolate()`, the one line a value of any type moves along, with `Additive` and `Interpolable`; `Transition`, `clamp01()` and `transitionEqual()`; `Animatable<T>`, `animatable()`, `bind()` and `propertyEqual()`; `quantizeTime()`, `stepIndex()`, `phase()`, `decay()` and `flash()`; `Spring`, `SpringParameters`, `Spring::step` and `Spring::isSettled`; `Oscillator` and `Wave`, the repeating signal |
| `SigilMotionClock`  | `clock/Engine.h`, `clock/Playback.h`, `clock/Animation.h`, `advanced/ClockPolicy.h`, `advanced/Held.h` | `Engine`, with `Playback`, `Animation`, `Timeline`, `Timer`, `Position` and its `at()`, `afterEnd()`, `afterPrevious()`, `withPrevious()` and `atLabel()`, `TimerOptions` and `EngineOptions`; `ClockPolicy`, who moves it; `HeldMotion`, `valueOf()`, `retarget()`, `enter()`, `progress()`, `Lane`, `LaneSlot`, `familyLanes()`, `retargetFixed()` and `retargetPositional()`, the reconciler's seam |
| `SigilMotionSchedule` | `schedule/Stagger.h`, `schedule/Schedule.h` | `stagger()`, `cues()` and `Staggered<V>`, a value that differs per child, with `StaggerOptions`, `StaggerFrom`, `StaggerAxis` and `Place`; `staggerSteps()`, the orderings; `Timing`, and `Schedule` and `Beat`, a timing resolved against a frame's counts |
| `SigilMotionPhysics` | `physics/Points.h`, `physics/Forces.h`, `physics/Neighbourhood.h`, `physics/Constraints.h`, `physics/Verlet.h`, `physics/Particles.h` | `Vec2` and `Points`, the lanes a simulation is; `Force` with `gravity()`, `drag()`, `attract()`/`repel()`, `wind()` and `boids()`; `Neighbourhood`, the grid a flock and everything else that reads more than one point at a time asks what is near what; `Constraint` with `distance()`, `stick()`, `spring()`, `range()` and `pin()`; `Verlet`, the stepper; `Particles` and `Attribute`, a point set that is born, ages and dies, with `Emitter`, `EmitFrom`, `Roughly`, `BirthAttribute` and `FixedAttribute`, what puts particles into one |

`SigilMotion` is the umbrella target over all seven, so a consumer of
every value and binding names one link. Time is the floor, a header over
the standard library, so every feature — physics included — states a
length of time without linking anything that reads one. Ease is the
next, because every other feature holds a curve; Bind links it, and
Values links Bind because `Animatable<T>` can hold a shaped value, and
Schedule links Ease because a stagger's curve compares under the same
rule every other curve does; Values links Schedule because a tween's
fields can be staggered. Clock links Values, because what an engine RUNS
is an animatable and a tween — the values describe, the clock moves.
Schedule links NEITHER the clock nor the values: see below. Physics links
neither — a step is a length of time the caller states.

## The chapters

One chapter per feature, beside this page. Each is the canon for the
feature it names, and everything below is the library as a whole.

| chapter | what it covers |
|---------|----------------|
| **[CLOCK.md](CLOCK.md)** | `Engine`: animations, timelines and timers and the playback they answer to; the host's frame, who moves the clock and its budget; and the held motions and lanes a reconciler runs |
| **[VALUES.md](VALUES.md)** | `Animatable<T>` and its four forms, `Tween` and `Transition`, `Oscillator`, `Spring`, and the three words for stillness |
| **[BIND.md](BIND.md)** | `bind()`, the `Binding` fields and the fixed order `Binding::apply` runs them in, the envelopes that are the waveform vocabulary, and the wiggle field |
| **[PHYSICS.md](PHYSICS.md)** | `Points`, `Force`, `Neighbourhood`, `Constraint` and `Verlet`: the one feature here that is stepped rather than read, and `Particles` with the `Emitter` that fills it |
| **[SCHEDULE.md](SCHEDULE.md)** | `stagger()`, a value per child resolved from its place among its siblings; `Timing` and `Schedule`: how N units share one progress, from a master float and nothing else |

## Comparing two descriptions

An identity prune asks whether two descriptions are provably the same,
and the animation values are the part of that question this library
answers. Four comparators, each beside the value it compares and each
under a field pin that fails the build when that value gains a member:

| comparator | rule |
|---|---|
| `easeEqual` | two curves are equal when both are the same plain function pointer, or both are the same `core::curve::Curve` shape at the same settings; a capturing lambda is unequal to everything |
| `tweenEqual` | same endpoints, keyframes, timing, curves, repeat and composition |
| `transitionEqual` | same duration, same delay, same curve, same composition — the curve read through `easing()`, so an empty curve compares as the default it behaves as |
| `Binding::operator==` | every one of `Binding`'s fields, by hand, under the pin; the curves under `easeEqual` |
| `propertyEqual` | same form, then that form's contents; a live value by the IDENTITY of the cell it reads, never by the number behind it |

The last rule is the load-bearing one: a live binding stays connected for
the whole life of the value it drives, so comparing the sampled number
would let a moving value prune into a still one.

## Boundary

No feature links an animation runtime: the curves, the live value and
the motion that writes it are this library's own. `ease::` spells the
standard curves itself, to the same numbers as Choreograph's, and only
the ease feature's test links Choreograph, to hold them to those
numbers. The
only other edges out of this directory reach the two header-only leaves
under SigilCore that depend on the standard library and nothing else:
`SigilCoreComparable` for `kFieldCount`, the pin each comparator above
sits under — so that a `static_assert` about `Binding`'s field count
lives in the same file as `Binding` — and `SigilCoreCompute` for the
seeded mixer the scattered ordering ranks with, so that a
`StaggerFrom::Random` permutation is the same permutation wherever in
the tree it is dealt,
for the noise field a wind reads, so that a flow a simulation drifts
along and the same flow drawn as a picture are the same field, and for
the SHAPED CURVE itself: `core::curve::Curve` and every house shape are
that leaf's, because a colour ramp and a keyed track reshape a unit
position exactly as an easing does and the colour leaf links no
animation runtime. The parameterised `ease::` curves are values of that
leaf's type, so a shaped curve gets its equality from the leaf and reads
the same wherever it is held.
Both carry no kernel, no device and nothing that draws. Boost is
private, in one place: the scheduler's own table — a Boost unordered map
on `SigilMotionSchedule` — which no public header names.

`SigilMotionSchedule` links neither the clock nor the values. A schedule
is a pure function of a master float in [0, 1] and two integer counts,
and keeping it that way is what lets a text engine drive it from a
track's progress, a set from a lane and a study from a bare `phase()`.

That is the point: consumers that also draw — a compositor, a 3D
renderer — link this library without inheriting a drawing library, and
spell `sigil::motion` at the call site rather than re-exporting it.

The library ships the values, the clock, and the motion a value runs as
— everything answerable from the animatable and the engine alone.
Resolving an animatable against a CONTEXT is the consumer's: which of a
node's properties are animated at all, where their held motions live, and
what a resolved number then means to a paint, a layout or a render pass.
Anything that would pull a graphics, layout, or scene-graph dependency in
here does not belong here — the physics feature included: it holds
positions and velocities and no colour, no size and no age, because what
a point looks like is the drawing's business.

## Build and test

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`motion_test` over every feature's `test/` and one `motion_bench` over
every feature's `bench/`, ctest one entry per CASE, what a case may pin,
and what a label promises. What is only true of SigilMotion:

Targets: `SigilMotionEase`, `SigilMotionBind`, `SigilMotionValues`,
`SigilMotionClock`, `SigilMotionPhysics` and `SigilMotionSchedule` — one
per feature directory (`ease/`, `bind/`, `values/`, `clock/`, `physics/`,
`schedule/`), each
holding its sources, its `test/` and its `bench/` — plus `SigilMotion`,
the umbrella.

| directory | suites | what they prove | what the feature must not be able to link |
|---|---|---|---|
| `ease/test/` | `Ease` | every named curve against the animation runtime's own numbers at 65 positions, exactly; the power families at a whole power against the named curve; the steps; and `easeEqual` over plain, shaped and captured curves | the rest of the library — the curves are its bottom floor, and the animation runtime is linked only to read its numbers against |
| `bind/test/` | `Binding`, `Stages`, `Envelopes`, `PeriodicEnvelopes`, `BindingEquality`, `Wiggle`, `WiggleNoise` | a `Binding`: every stage against the arithmetic it stands in for, the order the stages run in, the envelopes, `wrap`, the wiggle field, and its equality field by field | anything above the leaf — the record that carries a curve is the lowest thing here |
| `clock/test/` | `Engine`, `Playback`, `Timeline`, `Timer`, `Held`, `Lanes` | the wall and stated frames, the stall ceiling, the hold and the speed; which frames each policy lets move the clock and the budget that runs out once; animations, blends, timelines and their positions, the playback verbs; the step-rate timer that keeps its own rate whatever the host draws at; the held motion of an animatable and the lanes a host retargets through | a renderer |
| `values/test/` | `Values`, `Forms`, `Oscillator`, `Spring` | `Tween` and `Transition`, a tween read at a time, `quantizeTime`, the four forms an `Animatable<T>` holds, the signals read from a time alone, and springs | the clock and a renderer |
| `physics/test/` | `Physics`, `Particles` | the lanes a point set is and what `remove` does to their numbering, each force against the arithmetic it stands in for, a distance band read as a stick, a spring and a rope, the velocity a constraint pass gives back, the same run reproduced from the same `timeStep`, and the degenerate settings a caller can hand in; then a rate that produces the count it promises, a mouth that puts its births where its shape says, lifetimes that expire and compact the set, a named attribute that rides through a death, and the same seed twice as the same cloud | **the clock** — a step is a number of seconds the caller states, and a link edge to a timeline would be the first step to something in here reading time for itself |
| `schedule/test/` | `Stagger`, `Order`, `Schedule`, `ScheduleOrdering` | a stagger resolved per child, the orderings, the ladder, cue tables, the nested and looping schedule, and a stagger's equality | **the clock** — a schedule is a pure function of a master float and two counts, and a link edge to the clock would be the first step to something in here reading time for itself |

No binary needs a GPU, a font, an asset or a network, so none carries a
label and none skips. **No test in any of them reads a wall clock
either**: every frame length, every phase and every progress is a number
the case hands in, so a slow machine changes nothing about what they
assert. The parameterised rows are a binding's stages against the
arithmetic they stand in for, the envelopes that stay inside [0,1] and the ones that repeat
every period, the four forms an `Animatable<float>` holds, and the
orderings a stagger deals its steps in.

One file per subject: in `bind/test/`,
`BindingTest` (the stages, the envelopes and the wrap),
`WiggleNoiseTest` (the noise stage and the field under it) and
`CurveComparatorTest` (the two comparators); `ease/test/EaseTest.cpp`;
then
`clock/test/ClockTest.cpp`; then `physics/test/PhysicsTest.cpp` (the
stepper) and `ParticlesTest.cpp` (what is born, ages and dies);
`schedule/test/ScheduleTest.cpp`; and, in `values/test/`, `ValuesTest`
(the values themselves), `AnimatedTest` (the held motion), `LanesTest`
(the lane list), `OscillatorTest` (the signal
read from a time alone) and `SpringTest` (the one value that carries its
own velocity).

Two headers sit in `test/support/` at the library root, and every motion
test includes `"support/<Name>.h"`. **`StandsAlone.h` fails the build if
a drawing library's headers become reachable from a motion test** — the
positive control under every "SigilMotion alone" claim, without which
those tests would pass for the wrong reason on a machine where a
compositing header happened to be on the include path; every motion test
opens with it. `Ramps.h` is the transitioned value a test that drives a
motion starts from: a linear ramp to a target over a stated number of
milliseconds, linear so the assertions can read the value halfway
through and name the number without evaluating a curve. Otherwise each
test links only the library it exercises, and GoogleTest.

`motion_bench`'s arms: `ease/bench/` (one curve per call through the
`Easing` a slot holds — a plain function, a shaped curve, and the CSS
Bézier), `bind/bench/` (`Binding::apply` per call under
each envelope and every stage at once, and the wiggle field by octave),
`values/bench/` (the consumer's read of an `Animatable` lane per slot
for each kind it can hold, copying and constructing such a lane, and the
two time-only signals read one call at a time), `clock/bench/` (the
engine stepped with N held motions on it, and the step-rate timer), `physics/bench/` (a field of
free particles, the same field flocking — which is where comparing every
pair shows — a chain of sticks under its constraint passes, and ten
thousand particles born, stepped, aged and reaped, each measured on its
own) and `schedule/bench/` (resolving a schedule for a frame's counts,
and the per-unit local-time read).
