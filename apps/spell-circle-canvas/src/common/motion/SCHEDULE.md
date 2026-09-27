# SigilMotion — the schedules

The chapter on values that differ per child and on how a run of units
shares one progress: `stagger()`, the value a tween field takes to step or
spread from one sibling to the next; the orderings it deals in; and the
`Schedule` a collective's tween resolves into against the counts a frame
actually has. `README.md` beside this file is the library. Nothing here reads a
clock, which is the point.

## A value per child

A stagger is a VALUE, not a verb on a parent: it sits in the field of
the child's own tween, and a host resolves it from the child's `Place`
among its siblings when it lays the child out.

```cpp
#include <sigilmotion/schedule/Stagger.h>

using namespace sigil::motion;

.opacity(animate({.from = 0.0f, .to = 1.0f, .delay = stagger(40ms)}))    // 0, 40, 80… ms
.rotate(animate({.to = stagger({0.0f, 360.0f})}))                         // first child 0, last 360
.opacity(animate({.from = 0.0f, .to = 1.0f,
                  .delay = stagger(60ms, {.from = StaggerFrom::Center})}))  // outward from the middle
```

`stagger(each)` steps from one child to the next; `stagger({first, last})`
spreads a value from the first child's to the last's, whatever the count;
`cues({…})` states every child's value outright, past the end the last.
The options are anime.js's — `.from` (`StaggerFrom::First`, `Center`,
`Last`, `Edges`, `Random`, or a child's index), `.grid = {columns, rows}`
and `.axis` for a distance across a grid, `.ease` for how the values
crowd, `.start` added to every value, `.reverse` — plus the house `.seed`
for the scatter and `.rankBy`, an order stated as one number per child,
dealt smallest first with ties together. `Staggered<V>::at()` is the
resolution a host runs; `Staggered<V>::value()` is the value for a child
alone.

`StaggerFrom::Center` and `StaggerFrom::Edges` span the same number of
steps as `StaggerFrom::First`, so a run's total is the same whichever end
it opens from.

## How N units share one progress

A text track's glyphs are units, not children, and they share ONE master
progress — but they are a collective on a tween like any other: the
tween's delay (a stagger, a table, or a plain duration they all share)
says when each unit's beat opens, its duration how long one unit's own
motion lasts, and its `loop`, `loopDelay` and `alternate` whether the
whole thing loops. `timingOf()` (`values/Tween.h`) reads that off a
tween, with a second stagger inside every beat when one is nested, into
the `Timing` a `Schedule` is built from; nobody authors a `Timing`.
`Schedule` resolves it against the counts a frame actually has, and then
answers per index:

```cpp
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Tween.h>

const Tween<float> each{.duration = 420ms,
                        .delay = stagger(60ms, {.from = StaggerFrom::Center})};
Schedule schedule;                   // reused in place across frames
schedule.build(timingOf(each), unitCount, 0);
for (uint32_t i = 0; i < unitCount; ++i)
  paint(i, schedule.localProgress(master, i));   // this unit's own 0→1
```

`master` is a float in [0, 1] the caller owns — a track's progress, a
lane, a bare `phase()`. That is the whole interface, and it is why the
schedule feature links no clock: nothing in it reads time.

`Timing::span()` is the DECLARE-TIME half: what a progress transition's
duration has to be for the last beat to close exactly as the master
arrives at 1, before any of the units exist. `Schedule::total()` is the
same number off a resolved schedule, and the two agree because one body
computes both. `Timing::loop` — any tween `loop` other than zero — turns
the timing into a wrapping beat: each unit re-opens on its own cycle of
one beat plus `Timing::loopDelay`, offset by its start, every other cycle
backwards under `Timing::alternate`, and one sweep of the master 0→1 is
one cycle.

`Schedule::beat()` is the schedule read BACK rather than driven — start,
local progress and whether the beat is running — for anything that has
to travel with a schedule without being one of its units: a playhead, a
travelling underline, a per-unit meter.

## Gotchas

A cue table of the wrong length warns once per shape: a unit past the end
starts at the table's last time, and entries past the last unit are
never read. A `rankBy` of the wrong length does the same.

A scatter (`StaggerFrom::Random`) is keyed on the unit count and the
seed, so the same count and seed deal the same order on every frame and
after every rebuild; two same-count staggers with seed 0 scatter
identically, and a nonzero seed deals an independent one.
