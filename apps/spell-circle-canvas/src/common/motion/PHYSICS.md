# SigilMotion — the point set

The chapter on the one feature here that is stepped rather than read:
the lanes a simulation is, the forces that push on them, the constraints
that hold them together, and the Verlet stepper over the three.
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

Points cloth;                                  // the lanes
std::vector<Constraint> weave;
for (int i = 0; i < 40; ++i) cloth.add({i * 10.0f, 0}, {}, 1.0f, i % 8 == 0);
for (int i = 1; i < 40; ++i) weave.push_back(distance(i - 1, i, 10.0f));

const std::vector<Force> forces{gravity({0, 980}), wind(breeze, 40.0f),
                                repel(cursor, 6000.0f, 120.0f)};
const Verlet stepper{.dt = 1.0f / 120.0f, .damping = 0.4f, .iterations = 8};

ticker.addFixed(120.0, [&] { stepper.step(cloth, forces, weave); return true; });
```

**`Points` is lanes, not particles.** `position`, `previous`,
`velocity`, `force`, `mass` and `pinned` are six parallel vectors,
because everything that reads a simulation reads one property of all of
it. They are public: a simulation is a value the caller reads and
writes, and `add`/`remove`/`clear` exist only so the lanes cannot be
left at different lengths. `remove` moves the LAST point into the hole,
which renumbers — so a set with constraints over it is grown and
cleared, not thinned.

**Reading it into a drawing is one loop.** A stamping leaf's pool holds
its own lanes of two-float positions, so a frame copies `position[i]`
into the pool's position lane and takes whatever else it draws from —
a rotation off the velocity's angle, a scale off the mass, an alpha off
an age the caller keeps beside these lanes. Nothing here holds a colour,
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
neighbour search) or `Body`, the caller's own captureless function. A
constraint is `Distance` — a BAND, `rest` to `rest + slack`, which is
the stick, the spring and the rope in one value read three ways — or
`Pin`, a point held where the caller puts it this frame. Both compare
exactly, so a scene's forces and constraints are data a describe can
carry.

**Constraints move positions; the velocity is recovered afterwards**
from the movement the step actually achieved. That is what makes a chain
solvable by walking the list a few times, and what makes a point stopped
by a stick lose the speed the stick took without any force having said
so. `iterations` is how many walks: more is stiffer, not more correct.

**The flock compares every pair.** A neighbour index over the point set
answers the same question in the time one query takes rather than the
time the whole set does, and it is the same index a packing, a poisson
scatter and a collision pass all want; the bench's `FlockStep` arm
beside `ParticleStep` is what it would move, and `applyFlock` is the one
body that changes when the tree grows one.
