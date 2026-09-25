# SigilMotion — the values

The chapter on the values a property is described by: the animatable,
the tween and the transition, the signals that are functions of a time
alone, the spring that carries its own velocity, and the words for
stillness. `README.md` beside this file is the library; `BIND.md` is the
shaped binding an animatable can hold and `CLOCK.md` the engine a moving
one moves on, with the held motions a reconciler runs.

## The four forms

`Animatable<T>` is the one noun for a value that can change over time:
what every property verb takes, and what a sketch holds for a number it
drives. It holds a constant, a described motion, a live value somebody
writes, or a live value followed through a `Binding`:

```cpp
Animatable<float> a = 1.0f;                                  // constant
Animatable<float> b = animate({.from = 0.0f, .to = 1.0f});   // entrance
Animatable<float> c = animatable(0.0f);                      // live value
c = std::sin(engine.elapsed().count());                      // …written
Animatable<float> d = bind(c, {.from = {0.2f, 0.6f}, .clampFrom = true,
                               .ease = ease::outBack(), .to = {-70, 170}});
```

Those are the four forms, read back by `Animatable::form()` — constant,
described, live and bound.
`BIND.md` is the chapter on the fourth. `Animatable::value()` is the
number now whatever the form — a described motion answers where it
rests — and `Animatable::isRunning()` says whether the value is declared
to move: a live value always is, a shaped one when its source is.

A LIVE value is shared, not copied: `animatable()` makes one cell, and
every copy reads and writes it, so handing `c` to a property connects the
property to the number rather than taking a snapshot of it. The cell
lives as long as any copy does. Assigning a number to a live value writes
the cell; assigning one to any other form makes it that constant. Two
live values compare by the cell they read (`Animatable::identity()`),
never by the number behind it.

## The tween

`Tween<T>` is the one description of a motion, and `animate()` makes a
property value of one:

```cpp
.opacity(animate({.from = 0.0f, .to = 1.0f, .duration = 320ms}))   // an entrance
.translateY(animate({.to = lifted ? -8.0f : 0.0f}))                 // eases on change
.scale(animate({.from = 0.8f, .keyframes = {{.to = 1.07f, .duration = 80ms},
                                            {.to = 1.0f}}}))        // a path
```

Its fields are declared in the order a designated initialiser names
them: `from`, `to`, `keyframes`, `duration` (250ms), `delay` (0ms), `ease`
(`ease::outQuad`), `loop`, `alternate`, `composition`. A tween that NAMES
`.from` is an entrance: it plays once, when its owner first appears, and
afterwards the property behaves as though only `.to` had been written. A
tween with `.to` alone eases on change: the property starts out holding
it, and a later description carrying a different one eases there from
wherever the value is. A keyframe with no duration takes the tween's
duration divided by the number of keyframes; one with no curve takes the
tween's. `Tween::rest()` is where it comes to rest, and `Composition`
says what a change mid-flight does to the motion already running —
`Replace` starts again from the value on screen, `Blend` adds the change
on top so its velocity carries. `Transition` is the same timing for every
plain value on a node, and a consumer's `.transition(320ms)` takes a
duration alone.

## Two signals that are functions of a time and nothing else

`bind()` shapes a phase somebody else is stepping, and a tween plays when
a host runs it. `Tween::at` reads a tween with no host at all, and so do
these two: a number in, a number out, the same answer every time it is
asked. That is what makes them readable from a bake, a scrub, a force's
strength and a test as well as from a frame.

```cpp
const Oscillator breath{.wave = Wave::Sine, .hertz = 0.4f,
                        .amplitude = 0.08f, .centre = 1.0f};
const Tween<float> flare{.from = 0.0f,
                         .keyframes = {{.to = 1.0f, .duration = 60ms},
                                       {.to = 0.15f, .duration = 340ms}}};

scale(breath.at(clock.elapsed()));      // read wherever the number is wanted
glow(flare.at(ageOfTheHit));
```

**`Oscillator` is one repeating signal with properties**: the `wave` — sine,
triangle, sawtooth or square with a `duty` — `hertz`, a starting
`phase` in cycles, an `amplitude` and the `centre` it swings about.
Every wave is stated on the same folded phase and answers on [-1, 1]
before the amplitude, so swapping one for another keeps the timing and
the range and changes only the feel; the triangle is on the sine's
phase for exactly that reason. `fold(time)` is where in the cycle a
time falls, for anything travelling with the signal, and `shape(u)` is
the waveform on a phase that has already been folded — which is what
`envelope::shaped(...)` is handed. What it removes at
a call site is the FOLD: `sin(t*k)` is one expression, but a wave that
starts somewhere, swings by something and sits about something is four,
and a hand-written modulus is what gets a negative time wrong.

**A tween read at a time is a number given at several times**: its
keyframes are the keys, the delay holds `from`, the passes repeat as
`loop` and `alternate` say, and past the last pass it rests. An envelope
is one of these, and so is a cue list and a curve authored elsewhere.

The oscillator is comparable and CALLABLE — so it plugs into a binding's
`ease` or `envelope::shaped` as the shape, and into anything else that
hands a number to an interpolator. A capturing lambda would
compare unequal to everything and re-patch every describe, which is what
carrying the shape as a value rather than as a closure avoids.

## The spring: the one value that carries its own velocity

Every other value here is a function of a progress or a clock reading.
A **spring** is not: it is a position and a velocity, stepped towards a
target that is allowed to move.

```cpp
Spring cursor;                                   // value 0, at rest
SpringParameters parameters{.period = 390ms, .damping = 0.22f};
cursor = cursor.step(selectedX, delta, parameters);  // every frame
if (cursor.isSettled(selectedX)) sleep();         // done, to within a pixel
```

`period` is how long the spring would take to ring once with no damping —
how fast — and `damping` is the ratio: under 1 it overshoots and rings,
at 1 it arrives as fast as it can without ever crossing, over 1 it crawls
in from one side. The two are independent, which is the reason they are
the pair named: re-timing a bounce leaves its shape, reshaping it leaves
its timing. Successive extremes shrink by `exp(-ζπ/√(1-ζ²))`, so a
damping is picked from the overshoot a designer can see rather than from
a stiffness nobody can.

The target is an argument to the step and not a member of the spring,
because a target that moves mid-flight is the whole reason to reach for
one. An `ease::` curve runs between two fixed endpoints and can only
restart when one of them moves; a retarget bends by starting a
new ramp from where the value is, which loses the speed it had. A spring
keeps that speed and turns.

It is solved in closed form rather than integrated, so **one step of any
size is exact**: fifty steps of a frame and one step of fifty frames land
on the same value. That is what makes it safe on the delta a frame clock
actually hands over — the clock's clamped quarter-second is a big step
and not an explosion — and it takes no substepping to keep it there. It
is also why a caller with no state to keep can have the closed form for
free, stepping a spring at rest by the age of the thing it animates,
exactly as `decay` is read.

`Spring::isSettled` is the *settled* question asked of a spring. An
exponential approach never exactly arrives, so rest is a tolerance rather
than a fact, and it is stated once as a distance and a rate together: a
value sitting on its target at speed is passing through it, not resting
on it.

The state and the settings both compare field for field. A held spring
is part of the description of the thing it moves, and an owner asking
whether that description changed compares it rather than stepping it
again to see — which also tells a value on its target at speed from one
at rest there, the distinction the whole pair of numbers exists for.

## Stillness, in three words

"Is anything still moving" is three different questions, and answering one
with another is how a tree that has come to rest goes on repainting
forever. Each has its own word here:

| word | asks | grain |
|---|---|---|
| **declared** — `Animatable::isRunning()`: a value is live | *could* this move? | one value, from the description alone |
| **running** — `isRunning(held, value)` | is it moving *now*? | one value plus the motion held for it |
| **settled** — `core::Settle` in SigilCore | has it provably *held still*? | a node's values, observed across frames |

The trap is that the first two can never say "it stopped". A live value
stays live for the whole life of the value it drives, so a declaration is
permanent; and `HeldMotion::started` is permanent in the same way, which
is why a held motion's running asks whether a motion is still WRITING it
— the one thing in a running motion that changes when it lands.

Even "running" is a declaration about the *machinery*, not about the
numbers: a wave held at a constant phase is live and moves nothing. Only
the third question is a FACT, and answering it means comparing the values
across frames, which is a caching concern and lives with the cache.
`Engine::isRunning()` is the same question asked of a whole animation
domain rather than one value — is anything declared to move at all — and
it is the signal a host sleeps on.

## Gotchas

`Transition` and `Tween` are aggregates, so `.ease = {}` value-initialises
the curve to an *empty* `Easing`, which compiles and then throws
`bad_function_call` when called. Read the curve through
`Transition::easing()` or `Tween::easing()`, which substitute the
default; never read `ease` directly.
