# SigilMotion — the engine

The chapter on the one clock a sketch's motion runs on: the `Engine`
every animation, timeline and timer is started from, the `Playback`
each one hands back, and — for the host that owns the engine and the
reconciler that runs a description's values — the frame that moves it
and the held motions it steps. `README.md` beside this file is the
library; `VALUES.md` is what an engine runs, `SCHEDULE.md` the staggers
that read no clock at all and `PHYSICS.md` the point set a timer steps.

## Using it

```cpp
#include <sigilmotion/clock/Engine.h>

using namespace sigil::motion;
using namespace std::chrono_literals;

Engine engine;                                    // ctx.engine, in a sketch

Animatable<float> glow = animatable(0.0f);
engine.animate(glow, {.to = 1.0f, .duration = 400ms});      // anime.js animate

Animatable<float> title = animatable(0.0f), rule = animatable(0.0f);
engine.timeline()                                           // createTimeline
    .add(title, {.to = 1.0f, .duration = 750ms}, at(500ms))
    .add(rule, {.to = 1.0f}, withPrevious())
    .call([&] { armed = true; });

Animatable<glm::vec2> at = animatable(glm::vec2(0));     // any value with a line
engine.animate(at, {.to = glm::vec2(120, 40)});

std::vector<Animatable<float>> letters(5, animatable(0.0f));
engine.animate(letters, {.to = 1.0f, .delay = stagger(40ms)});  // a collective

Animatable<float> wave = animatable(0.0f);
engine.timer([&] { wave = std::sin(engine.elapsed().count() * 1.6); });
engine.timer([&] { stepFire(); }, {.stepRate = 27.0});      // a fixed-rate simulation
```

`Engine::animate` runs a `Tween` on a live value — made live from the
value it holds if it is not one — and a second animation on the same
value takes it over, unless the tween says `Composition::Blend`, when it
rides on top of the first so the value's velocity carries. It runs ONE
body for every value type an `Animatable<T>` holds: a float, a
`glm::vec2` or `glm::vec3`, a `Duration`, a `material::Color`, anything
`interpolate()` draws a line between (`VALUES.md`). A value that does not
add — a colour — has no difference to ride on top of a running motion,
so under `Composition::Blend` it starts again from where it stands.
Handed a list of values, `Engine::animate` runs the tween on every one
of them as siblings: a field written as `stagger()` or `cues()` resolves
to each value's own from its place in the list, and the `Timeline` it
hands back controls the run as one. `Timeline`
places tweens, collectives and calls in time: `at(time)`, `afterEnd(offset)` — the
position an item takes when none is named — `afterPrevious(offset)`,
`withPrevious(offset)` and `atLabel(name, offset)`, with `Timeline::label`
naming a moment. `Engine::timer` runs a callback every frame, at an exact
`TimerOptions::stepRate` counted from total time whatever the host draws
at, or throttled to at most `TimerOptions::frameRate`; the callback names
what it reads — `[] {…}`, `[](Duration delta) {…}` or
`[](Duration delta, Duration elapsed) {…}` — and may answer false to stop.
A step-rate timer's `Timer::betweenSteps()` is the render interpolant and
`Timer::droppedTime()` says a frame dropped time rather than spiral.

Every one of them is a `Playback` and answers to the same control:
`Playback::play`, `Playback::pause`, `Playback::resume`,
`Playback::restart`, `Playback::reverse`, `Playback::alternate`,
`Playback::seek`, `Playback::complete`, `Playback::cancel`,
`Playback::revert` (back to where the targets stood before it started)
and `Playback::onComplete`; `Playback::isRunning`, `Playback::isPaused`,
`Playback::isCompleted`, `Playback::currentTime` and `Playback::progress`
read it back. A handle is a handle: the engine runs a playback whether or
not one is kept.

`Engine::isRunning()` is the event-driven-redraw signal — true while a
playback runs or a held motion moves — so a host renders while it is true
and sleeps when it is not. It is a declaration, never a proof that a
number changes: a timer that writes the same value every frame is
running. `Engine::elapsed()` is engine time, the sum of every frame's
movement.

## The host: who moves the clock

A sketch never moves its engine; the host that owns it does, one frame at
a time. `Engine::advance()` is a frame the engine takes on its own — the
`EngineOptions::fixedStep` a headless sweep states once, or the wall
clock's movement since the last frame, held, scaled by the speed and
clamped to `EngineOptions::maxWallStep` so a stalled frame does not jump
every motion forward. `Engine::advance(Duration to)` is a frame a caller
states: it moves to that engine time by exactly the difference, and the
next wall frame counts from its own reading rather than catching up on
the stretch the caller stepped.

`ClockPolicy` (`advanced/ClockPolicy.h`) says who may move it:

- `ClockPolicy::Wall` — the wall moves it: a window.
- `ClockPolicy::Advance` — only a stated frame does: a capture, a test.
- `ClockPolicy::Pause` — nothing does, a stated frame included.
- `ClockPolicy::PauseWhileLoading` — the wall, except while something the
  run asked for is still arriving (`Engine::setArriving`).

`Engine::setPolicy` sets it with a budget of engine time, and
`Engine::isBudgetExpired` answers true on the one frame that reaches it.
`Engine::setHeld` is the pause a person presses and `Engine::setSpeed`
the speed they pick; `Engine::isPaused` says a frame now would move
nothing; `Engine::frames` counts frames taken; `Engine::restart` counts
from zero again as a new session opens under the same engine.

## The reconciler's seam

`advanced/Held.h` is below a sketch's vocabulary: what a retained host —
Compose's and World's reconcilers — runs a description's values through.
A moving `Animatable<float>` has a second half there: the motion the
engine is actually running for it, a `HeldMotion` — a live value, whether
it has started, the endpoint it is flying at and the motion writing it.
The operations are stated over one held motion, so the consumer's storage
stays its own business:

```cpp
HeldMotion* held;                                   // what the consumer retains
valueOf(held, value);                               // the value for this frame
retarget(engine, held, previous, next, fallback, place);  // a moved target
enter(engine, held, value, place);                  // the first appearance
progress(engine, held, transition);                 // a synthesized 0→1
```

`valueOf` is the reading order: a live value wins (shaped through its
stages when it has any), then a running ramp, then the constant.
`retarget` starts a ramp from WHERE THE VALUE IS rather than from the
previous description, so a target that moves mid-flight bends the motion
instead of restarting it; a motion already headed at the new target
keeps flying; a next value that is constant or live snaps; and a change
whose transition says `Composition::Blend` rides on top of the running
ramp. `enter` plays the `from` a tween names, through its keyframes or to
its `to`, as many times as its `loop` says. Every staggered field
resolves against the `Place` the node mounted at.

A **lane** pairs an animatable a description carries with the address of
the held motion that serves it:

```cpp
enum class Family : uint8_t { Slot, Span };   // the HOST's storages
using enum Family;                            // named unqualified below
std::vector<Lane<Family>> previous, next;     // filled by the host
retargetFixed<Family>(engine, slots, familyLanes(previous, Slot),
                      familyLanes(next, Slot), nodeTransition, place);
retargetPositional<Family>(engine, spans, familyLanes(previous, Span),
                           familyLanes(next, Span), nodeTransition, place);
```

A **fixed** family is a slot array whose rows are a property of the host,
so a row one description lacks ramps from or to the lane's `standing`
value. A **positional** family is sized by the description, so a change
of SHAPE drops the running motions rather than carrying them onto
endpoints that now mean something else.

## Gotchas

An engine is not thread-safe: one per animation domain, touched only from
that domain's thread.

A frame that moves nothing still takes its wall reading, so a return to
`ClockPolicy::Wall` measures from there rather than catching up on the
stretch the clock stood still for. A stated frame is not scaled and not
clamped: the caller chose it.

A step-rate timer DROPS simulated time when the backlog passes
`TimerOptions::catchUp` — running slow for one frame instead of
spiralling — and `Timer::droppedTime` is the only signal that anything
measured on that frame is meaningless.

A timer that never answers false runs until it is cancelled, and holds
`Engine::isRunning()` true for as long as it does: cancel it, or give it
a `TimerOptions::duration`, when it has nothing left to do.
