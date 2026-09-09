# SigilMotion — the point set

The chapter on the one feature here that is stepped rather than read:
the attributes a simulation is, the forces that push on them, the grid
that answers which of them are near which, the constraints that hold
them together, and the Verlet stepper over the three.
`README.md` beside this file is the library; `VALUES.md` holds the spring
that is the closed form of ONE value flying at a target, which is what
this is many of.

## The point set

A spring is one value flying at a target. A **point set** is many of
them, pushing on each other and tied together, and there is no closed
form for that — so this is the one feature here that is stepped rather
than read.

```cpp
#include <sigilmotion/physics/Physics.h>
using namespace sigil::motion::physics;

Points cloth;                                  // the attributes
std::vector<Constraint> weave;
for (int i = 0; i < 40; ++i) cloth.add({i * 10.0f, 0}, {}, 1.0f, i % 8 == 0);
for (int i = 1; i < 40; ++i) weave.push_back(distance(i - 1, i, 10.0f));

const std::vector<Force> forces{gravity({0, 980}), wind(breeze, 40.0f),
                                repel(cursor, 6000.0f, 120.0f)};
const Verlet stepper{.dt = 1.0f / 120.0f, .damping = 0.4f, .iterations = 8};

ticker.addFixed(120.0, [&] { stepper.step(cloth, forces, weave); return true; });
```

**`Points` is attributes, not particles.** `position`, `previous`,
`velocity`, `force`, `mass` and `pinned` are six parallel vectors,
because everything that reads a simulation reads one property of all of
it. They are public: a simulation is a value the caller reads and
writes, and `add`/`remove`/`clear` exist only so the attributes cannot be
left at different lengths. `remove` moves the LAST point into the hole,
which renumbers — so a set with constraints over it is grown and
cleared, not thinned. The `force` attribute is the caller's to pre-load:
a step ACCUMULATES its forces onto whatever the attribute already holds
and clears it once it has integrated, so a push written between two
steps is spent exactly once and one written before a step that is never
taken is still there for the next one.

**Reading it into a drawing is one loop.** A stamping leaf's pool holds
its own attributes of two-float positions, so a frame copies `position[i]`
into the pool's position attribute and takes whatever else it draws from —
a rotation off the velocity's angle, a scale off the mass, an alpha off
an age the caller keeps beside these attributes. Nothing here holds a colour,
an age or a size: what a point IS on screen is the drawing's business,
and a simulation that carried it would have to name a renderer. `Vec2`
converts from any two-float point by SHAPE (`fX`, `fY`), so a renderer's
point crosses in without this library including a renderer's header.

**`dt` is a prop of the stepper, not an argument.** That is the whole
determinism claim: a simulation stepped by a frame's delta is a
different simulation on every machine and on every frame that stutters,
while one stepped by a fixed number is the same run everywhere and can
be replayed and compared. A host with a varying clock drives it from
`Ticker::addFixed`, which is exactly this shape.

**One force value with a kind, one constraint value with a kind.** A
force is `Uniform` (an acceleration, so weight does not enter it),
`Drag`, `Attract` (negative strength is `repel`), `Wind` (a
`core::noise::Field` read at the point and taken as an angle), `Flock`
(the three steerings over the neighbours within a radius, sharing one
grid between them) or `Body`, the caller's own captureless function. A
constraint is `Distance` — a BAND, `rest` to `rest + slack`, which is
the stick, the spring and the rope in one value read three ways — or
`Pin`, a point held where the caller puts it this frame. Both compare
exactly, so a scene's forces and constraints are data a describe can
carry.

A distance band is solved by normalising the offset, which costs a
square root per constraint per pass. `approximate` (and `stick()`, which
sets it) replaces the normalisation with `rest² / (|offset|² + rest²) −
½` — the same number to first order around the rest length, drifting as
the pair moves away from it, so a chain converges to the same shape by a
different path and one pass of one is not one pass of the other. It is a
prop rather than a second kind because it changes only how a pass is
arrived at, never what the constraint says.

**Constraints move positions; the velocity is recovered afterwards**
from the movement the step actually achieved. That is what makes a chain
solvable by walking the list a few times, and what makes a point stopped
by a stick lose the speed the stick took without any force having said
so. `iterations` is how many walks: more is stiffer, not more correct.

## What is near what

**One grid answers every question of the form "what is near here".**
`Neighbourhood` indexes a run of positions and answers
`Neighbourhood::within` — the indices inside a radius of a place — so
the cost of asking follows how many points are NEAR one rather than how
many there are at all. `Flock` builds one over the positions at its own
reach and asks it once per point; a packing, a poisson scatter, a
collision pass and a density gather are the same question and read the
same index.

```cpp
Neighbourhood near(cloth.position, 40.0f);   // the reach about to be asked
std::vector<uint32_t> found;
for (size_t i = 0; i < cloth.size(); ++i) {
  near.within(cloth.position[i], 40.0f, found);
  crowding[i] = (float)found.size() - 1.0f;    // itself is in the answer
}
```

**The answer comes back in index order**, and that is a promise rather
than an accident of how the buckets are laid down. A force sums over the
neighbours it finds, a sum of floats is not associative, and so an
answer arriving in bucket order would give a different number from the
walk over every pair that finds exactly the same neighbours — and a
different number again on any day the cell size moved. Index order is
the one order every way of finding neighbours agrees on, which is what
makes the index a REPLACEMENT for the walk rather than a second answer.
A point of the set is its own neighbour and comes back with the rest,
because a coincident pair means one thing to a flock and another to a
collision; dropping it is the caller's.

**The cell size is the one dial.** Left at zero it is chosen so the
average cell holds a couple of points; given explicitly it should be
about the radius that will be asked for, since cells much smaller than
the radius are many to walk and cells much larger sweep in points that
were never close. The grid is bounded in cells, so a size given is a
request the index may coarsen and `Neighbourhood::cell` answers what it
used. It is a SNAPSHOT: the positions are copied in, so a caller may
move its points while the index still answers about where they were, and
a moved set is a `Neighbourhood::build` rather than an update — which is
what keeps every point of one pass seeing the same arrangement.

The same grid stands in three dimensions in the geometry library's path
leaf, where outlines and scatters ask it. It is not what stands here,
and the reason is a link edge rather than a disagreement: that leaf
publishes Skia, glm and a Boost map, while this feature's boundary is
that a consumer which also draws links it without inheriting a drawing
library. What crossed is the shape — two counting passes into one bucket
array, a cell size that holds a couple of points, and a cell count
bounded by the point count.

## What is born, ages and dies

A **particle** is not a different kind of point. It is a point with a
beginning and an end: something put it there with a velocity drawn from
a range, it carries numbers of its own while it lives, and it leaves the
set when its time is up. `Particles` is those three over a `Points`, and
`Emitter` is what puts them in.

```cpp
Particles fire;
fire.attribute("size").rate = 1.2f;                 // it grows while it lives
Attribute& red = fire.attribute("red");
red.rate = -0.4f;                              // and cools
red.least = 0.0f;

const Emitter mouth{
    .from = EmitFrom::Segment,
    .at = {0, 400},
    .size = {180.0f, 0.0f},                    // the fissure it comes off
    .aim = {0, -1},
    .cone = 0.6f,                              // the ejection cone
    .speed = {.mean = 180.0f, .variation = 60.0f},
    .mass = {.mean = 1.0f, .variation = 0.4f},  // and what it weighs
    .attributes = {{std::string(kLife), {.mean = 1.4f, .variation = 0.5f}},
              {"size", {.mean = 4.0f, .variation = 1.6f}},
              {"red", {.mean = 1.0f, .variation = 0.35f}}},
    .fixed = {{"mouth", 3.0f}},                // which one threw it
    .rate = 6000.0f};

core::chance::Stream stream = core::chance::Stream::pcg(1982);
ticker.addFixed(60.0, [&] {
  mouth.emit(fire, stream, 1.0f / 60.0f);
  stepper.step(fire.points, forces);
  fire.live(1.0f / 60.0f);
  fire.reap();
  return true;
});
```

**Every attribute is a `Roughly`**: a middle, how far either side of it a
draw may fall, and a scale on the whole draw. It is stated that way
rather than as a pair of bounds because that is how a recipe is read and
adjusted — the middle is the thing being made, the variation is how
ragged the set of them is — and the scale is kept out of the two numbers
so one recipe can be worn at many sizes. It is a `chance` shape, so a
stream samples it the way it samples `core::chance::Gaussian`.

**A birth costs the same words whatever the numbers are.** A variation of
zero still draws, and the draws come off the stream in one order: the
place, the angle off the aim, which side of the aim, the speed, the
weight where it varies, then `Emitter::attributes` in the order they are
written. That order is part of what an emitter IS — it is why a seed
replays a cloud — so an attribute is added at the END of the list when an
existing cloud must not move.

**`Emitter::mass` is a range like the rest, with one exception written
into it**: a weight that does not vary is STAMPED rather than drawn and
costs no word. `Roughly::constant` is what the draw would have answered
— nothing times whatever a stream said is nothing — so an emitter
stating a weight as a single number costs exactly what its attributes
cost, and a cloud a seed replays does not move because a weight was
given a range it does not use. It is the bargain `Emitter::fixed`
strikes, and it is the one place `Roughly` is read without spending a
word; a birth attribute always draws. What a drawn weight then means is
the stepper's: a force is divided by the mass it is pushing, so a heavy
particle in the same wind moves less, while `gravity` is stated as an
acceleration and carries the whole cloud down together.
Nothing here reads a wall clock: `Emitter::emit` is handed a span of
time and `Emitter::carry` holds the fraction of a particle it left over,
so a rate finer than one birth per step still arrives at that rate.
`Emitter::burst` is the same births at once, for the puff at the head of
a plume and for a consumer whose own law says how many arrive this step.
`Emitter::fixed` is beside `Emitter::attributes` for the number a birth
is STAMPED with rather than drawn — which of many mouths threw it, a
material index, a tag to sort by — and it costs no draw, so adding one
does not move a cloud a seed replays.

**`EmitFrom` is the mouth**: a `Point`, a `Segment`, a `Box`, a `Disc`
(evenly over its AREA, so the middle is not crowded) or a `Ring`. The
first three are stated in the emitter's own frame, whose first axis is
`Emitter::along`; the two round ones do not read it, because they are the
same shape whichever way they are turned.

**The attributes are why this is not a parallel array.** A death moves the
LAST particle into the hole it leaves — in `points`, in `age`, in `life`
and in every named attribute at once — so a size, a colour or an index saying
which emitter threw this one has to travel with the particle or be
silently renumbered. `Particles::attribute` names one; `Attribute::rate`, bounded
by `Attribute::least` and `Attribute::most`, is the whole of the model an attribute
changes under while a particle lives, and `Particles::live` applies it.

**Ageing and death are two calls, not one.** `Particles::live` makes
everything older and moves every attribute by its own rate; `Particles::reap`
removes what is dead and compacts. A cloud usually dies of more than age
— of running out of light, of falling below the ground, of leaving the
frame — so the rules between them are the consumer's:
`Particles::expired` is the age rule written out, and
`Particles::reap(dead)` takes a predicate that spells the others beside
it.

**The stepper does not know a particle from a point.** `Particles::points`
is the set, and the forces, the constraints and `Verlet` above run over
it unchanged.
