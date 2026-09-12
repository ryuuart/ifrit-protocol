# SigilMotion — the values

The chapter on the values a property is described by: the transition
and the animatable slot, the held motion a ticker is running for one, the
two signals that are functions of a time alone, the spring that carries
its own velocity, the lanes a retained host retargets through, and the
three words for stillness. `README.md` beside this file is the library;
`BIND.md` is the shaped binding an animatable can hold and `CLOCK.md` the
ticker a moving one moves on.

## The four forms

`Animatable<T>` is the property slot a consumer stores. It accepts a
constant, a transition spec, a bare `Output<T>*`, or a shaped `bind()`:

```cpp
Animatable<float> a = 1.0f;                                  // constant
Animatable<float> b = animate(from(0.0f).to(1.0f), {400ms}); // entrance
Animatable<float> c = &opacity;                              // live cell
Animatable<float> d = bind(&phase).window(0.2f, 0.6f)
                          .map(ease::outBack()).target(-70, 170);
```

Those are the four forms it holds, discriminated by `index()`: `0` plain
constant, `1` `Transitioned<T>`, `2` bare `Output<T>*`, `3` shaped
binding. `BIND.md` is the chapter on the fourth.

## The held motion

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
staggers by. `resolveProperty<T>` is the flattening underneath: an animatable
read against a fallback transition, giving a target, a binding, or a
spec.

## Two signals that are functions of a time and nothing else

`bind()` shapes a phase somebody else is stepping, and `Transitioned`
plays once when a node mounts. Both need a ticker and an `Output`. These
two need neither: a number in, a number out, the same answer every time
it is asked. That is what makes them readable from a bake, a scrub, a
force's strength and a test as well as from a frame.

```cpp
const Oscillator breath{.wave = Wave::Sine, .hertz = 0.4f,
                        .amplitude = 0.08f, .centre = 1.0f};
const Sequence flare{.steps = {{0.0f, 0.f}, {0.06f, 1.f}, {0.4f, 0.15f}},
                     .interpolation = Interpolation::CatmullRom};

scale(breath.at(clock.elapsed()));      // read wherever the number is wanted
glow(flare.at(ageOfTheHit));
```

**`Oscillator` is one repeating signal with properties**: the `wave` — sine,
triangle, sawtooth or square with a `duty` — `hertz`, a starting
`phase` in cycles, an `amplitude` and the `centre` it swings about.
Every wave is stated on the same folded phase and answers on [-1, 1]
before the amplitude, so swapping one for another keeps the timing and
the range and changes only the feel; the triangle is on the sine's
phase for exactly that reason. `fold(seconds)` is where in the cycle a
time falls, for anything travelling with the signal, and `shape(u)` is
the waveform on a phase that has already been folded — which is what
`bind(&value).source(0, period).wave(...)` is handed. What it removes at
a call site is the FOLD: `sin(t*k)` is one expression, but a wave that
starts somewhere, swings by something and sits about something is four,
and a hand-written modulus is what gets a negative time wrong.

**`Sequence` is a number given at several times.** `Step{at, value,
curve}` are the keys, in the caller's own units, and `interpolation`
says what happens between them: `Hold` for states that cut, `Linear`
shaped by each key's own `core::curve::Curve`, `CatmullRom` for the spline
through them — a prop rather than a second type, since the keys are the
same keys. An envelope is one of these, and so is a step sequencer, a
cue list and a curve authored elsewhere. Outside the keys it is flat
unless it `loop`s, in which case the last key's time is the wrap point
and a loop is authored with its last key repeating its first; across
that seam the spline reaches for the keys either side of the join
rather than for the key that closes it. Two keys at one time are a cut.

Both are comparable, and both are CALLABLE — so either one plugs into
the bind chain's `wave()` or `map()` as the shape, and into anything
else that hands a number to an interpolator. A capturing lambda would
compare unequal to everything and re-patch every describe, which is what
carrying the shape as a value rather than as a closure avoids.

## The spring: the one value that carries its own velocity

Every other value here is a function of a progress or a clock reading.
A **spring** is not: it is a position and a velocity, stepped towards a
target that is allowed to move.

```cpp
Spring cursor;                                   // value 0, at rest
SpringParameters p{.periodSeconds = 0.39f, .damping = 0.22f};
cursor = spring(cursor, selectedX, dt, p);       // every frame
if (!springMoving(cursor, selectedX)) sleep();   // done, to within a pixel
```

`periodSeconds` is the period the spring would ring at with no damping —
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
restart when one of them moves; `transitionFloatAt` bends by starting a
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

`springMoving` is the *running* question asked of a spring. An
exponential approach never exactly arrives, so rest is a tolerance rather
than a fact, and it is stated once as a distance and a rate together: a
value sitting on its target at speed is passing through it, not resting
on it.

## Lanes: where a host's motions live

A retained host holds one `AnimatedFloat` per animatable it lets move,
and a patch has to bend the running motions of the old description onto
the endpoints the new one asks for. A **lane** is that pairing: an
animatable the description carries, and the address of the held motion
that serves it.

```cpp
enum class Family : uint8_t { Slot, Span };   // the HOST's storages
using enum Family;                            // named unqualified below

std::vector<Lane<Family>> prev, next;         // filled by the host
retargetSlots<Family>(ticker, anims, familyLanes(prev, Slot),
                      familyLanes(next, Slot), nodeTransition);
retargetFamily<Family>(ticker, spanAnims, familyLanes(prev, Span),
                       familyLanes(next, Span), nodeTransition);
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
the same way, which is why `isLive` asks the output's `isConnected()`
instead
— Choreograph disconnects an output when its motion finishes, and that is
the one thing in a running motion that changes when it lands.

Even "running" is a declaration about the *machinery*, not about the
numbers: a wave held at a constant phase is connected and moves nothing.
Only the third question is a FACT, and answering it means comparing the
values across frames, which is a caching concern and lives with the cache.
`Ticker::active()` is the same question asked of a whole animation domain
rather than one value — is any motion registered at all — and it is the
signal a host sleeps on.

## Gotchas

`Transition` is an aggregate, so `{360ms, {}, 220ms}` value-initialises
`ease` to an *empty* `std::function`, which compiles and then throws
`bad_function_call` when called. Read the curve through
`Transition::easing()`, which substitutes the default; never read `ease`
directly.
