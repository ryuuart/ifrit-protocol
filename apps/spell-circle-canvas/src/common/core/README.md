# SigilCore

The vocabulary every other Sigil library shares: the leaves a library far
from any renderer, any device and any retained tree links on its own. A
leaf here earns its place by being needed identically by two libraries,
and depends on the standard library and on nothing of this project's but
another leaf here. The kernels a retained runtime is built on — the
reconciler and the caching proof — are SigilTree's, and the GPU device is
SigilGpu's; both stand on this library and neither is part of it.

**Comparable** — what a value needs before anything can decide it did not
change: type erasure that keeps its equality, so a set of operations can
ride on a value and two holders can still ask whether they carry the same
one; and the field pin, which fails the build when a struct grows a member
a hand-written comparator does not mention.

**Compute** — the arithmetic several libraries have to agree on to the bit:
the seeded mixers a jitter draws from, the stream a caller holds one of
them as and the distributions drawn out of it, the noise field read at a
point, the folds a cache key is accumulated with, the normal form a set of
runs over one axis is put in, and the shaped curve a unit position is
reshaped by. The standard library is the whole of its dependencies, so a
shader's CPU twin, a point cook, a text cache and a resource store all
reach the same bodies.

**Callable** — the one prefix search in the tree: a verb that offers a
caller more than most callers read takes its callable through here, so that
`[](SkCanvas& c) {…}`, `[](SkCanvas& c, const PaintContext& ctx) {…}` and
`[] {…}` are all the same verb's argument and nobody spells a parameter to
ignore it. The rule — the parameters a callable names are the FIRST of what
the verb offers, dropped from the end and never from the middle — is stated
and tested once, and the holder over it erases the call the way a
`std::function` does.

**Schedule** — where independent work runs. One parallel for over the task
runtime, taking a count, a grain and a body, so the runtime is named in one
file of this repository and in no header of it; and beside it a fan-out of
its own threads for calls that BLOCK on a disk or a server, which must not
sit on the workers a compute range shares.

**Follow** — what a describe read that can change after it. A shared
value a describe can read — a live number a sketch writes — derives from
`sigil::core::Followable` and advances its revision on every write that
changes it; a host opens a `sigil::core::FollowScope` around a describe,
and every such value read inside lands in a `sigil::core::FollowedReads`
with the revision it stood at, once per value. The describe is stale
exactly when one of those revisions has moved. SigilTree's reconciler
follows every memo's describe through it, and SigilMotion makes a live
value's cell followable through it, so both link this leaf and neither
links the other.

**Memo** — rebuild on change. `sigil::core::RebuildGuard` takes the
"compare the inputs, rebuild when they differ" cache an animated consumer
otherwise hand-rolls, with the key one declared tuple rather than a row of
`m_last*` members and an if-condition that has to stay in step with them;
`sigil::core::CachedValue` holds the value the last build answered beside
its guard; and `sigil::core::quantizeKey` is the one arithmetic such a key
wants: an input that drifts by a fraction per frame is snapped, so most
frames pose the same problem and hit. Nothing here knows what is being
rebuilt, which is why it is a leaf here and not part of a tree's cache: a
text kit's layout guard is this guard with a paragraph's own inputs folded
in.

**Report** — the one door a library's warning goes out through. A library
that has something to say and no caller to say it to calls
`sigil::core::report` with its own name and the message; the line is
written once, as `[library] message`, to the standard error, unless a host
has installed a `sigil::core::Reporter` with `sigil::core::installReporter`,
which then hears every library's warnings in its place. Beside it stands
the one error a door that can refuse answers: `sigil::core::Error`, a
message written for a person, the line of a text it is about and a code
the refusing library numbers its refusals by, each of the last two unset
until stated. Every library's refusing door answers
`std::expected<T, core::Error>`; a library whose own error carries more
converts to this one.

Namespace `sigil::core`. One target per directory:

| target | directory | holds |
|--------|-----------|-------|
| `SigilCoreComparable` | `comparable/` | comparable type erasure, the field pin |
| `SigilCoreCompute` | `compute/` | the seeded mixers, the stream and its distributions, the noise field, the identifying folds, the interval normal form, the shaped curve, the degrees-to-radians rounding, the time unit and its CSS reader |
| `SigilCoreCallable` | `callable/` | the prefix search, the concept over it, and the type-erased call a verb holds one in |
| `SigilCoreSchedule` | `schedule/` | the parallel for and its grain, and the fan-out for calls that block |
| `SigilCoreFollow` | `follow/` | the followable value and its revision, the record of what one describe read, and the scope that records it |
| `SigilCoreMemo` | `memo/` | the keyed rebuild guard, the value held with its guard, and the snap a drifting key is taken through |
| `SigilCoreReport` | `report/` | the reporting door: `report`, the installed `Reporter`, and the one line format; the `Error` a refusing door answers |

`SigilCoreComparable`, `SigilCoreCompute`, `SigilCoreCallable` and
`SigilCoreMemo` are header-only, so they are INTERFACE targets and produce
no archive; everything else is a static library. `SigilCoreComparable`
takes the standard library and Boost.PFR, `SigilCoreCompute`,
`SigilCoreCallable` and `SigilCoreMemo` the standard library alone,
`SigilCoreSchedule` oneTBB privately, `SigilCoreFollow` the reporting door
alone, and `SigilCoreReport` the standard library alone. Consumers still link only the feature they use, without
pulling in a drawing or layout library.

Every public header lives under `include/sigilcore/<feature>/` and is
spelled `<sigilcore/comparable/X.h>`, `<sigilcore/compute/X.h>`,
`<sigilcore/callable/Callable.h>`,
`<sigilcore/schedule/X.h>`, `<sigilcore/follow/Follow.h>` or
`<sigilcore/memo/Rebuild.h>`. The time unit is
`<sigilcore/compute/Duration.h>`, and the reporting door's headers are
`<sigilcore/report/Report.h>` and `<sigilcore/report/Error.h>`.

## The chapters

One chapter per feature, beside this page. Each is the canon for the
feature it names — its headers, what it promises and how it is used — and
everything below is the library as a whole.

| chapter | what it covers |
|---------|----------------|
| **[COMPARABLE.md](COMPARABLE.md)** | `Erased<Ops>` and `kFieldCount<T>`: what a value needs before anything can decide it did not change |
| **[COMPUTE.md](COMPUTE.md)** | the mixers, the stream and its distributions, the noise field, the identifying folds, the interval normal form, the shaped curve, the angle and the time unit |
| **[CALLABLE.md](CALLABLE.md)** | `Callable<Signature>`, `callPrefix` and `PrefixCallable`: a callable called with the parameters it named |
| **[SCHEDULE.md](SCHEDULE.md)** | the parallel for and its grain, and the fan-out a call that blocks runs on instead |

## What belongs here

A function earns a place in one of the header-only leaves when three
things hold:

- **Two libraries need it identically.** Not "could share it" — actually
  compute the same number today, or have to agree with each other tomorrow
  because one reproduces the other (a shader and its CPU preview, a device
  executor and its reference).
- **The contract can be stated.** What it answers, what is fixed about it,
  and what breaks if it changes — in the header, evaluable by someone who
  has opened nothing else.
- **It brings its own test.** For a computing leaf that means the outputs
  themselves, pinned; for a holding one, behaviour over a value with
  nothing behind it.

Worked both ways: `Erased` qualifies because a reconciler's descriptions, a
mesh runtime and a point-operator runtime all carry a comparable erased
value, and one shape of it is what lets a value cross between them.
`kFieldCount` qualifies because a pin is worth nothing if each library
writes its own and one gets it subtly wrong. The seeded mixers qualify
because a stored render and a GPU kernel have to agree to the bit. An
easing curve does NOT qualify: it is animation's, and SigilMotion owns it —
which is why the comparator over that curve lives there too, beside the
curve, rather than in the reconciler that happens to prune with it. Neither
does a numeric constant a single library reaches for — those stay with the
library that spells them.

## Boundary

SigilCoreComparable, SigilCoreCompute, SigilCoreCallable and
SigilCoreMemo link nothing of this project's at all — the standard
library, and Boost.PFR for the field pin. That is the whole point of them:
a library anywhere in the tree can link one without acquiring a kernel, a
device or a drawing library, and SigilMotion is one of the libraries that
does, for the pin its own comparators sit under and the time unit every
length it animates over is stated in. SigilCoreFollow links the reporting
door alone, for the one report a refused write makes. SigilCoreSchedule
links nothing of this project's either: a task runtime, privately, and the
standard library's threads. That privacy is the feature — a consumer hands
over a count, a grain and a body, so no header in this repository spells a
task runtime, and the one that runs every divided range in the process is
chosen in one file. SigilCoreReport is the standard library alone.

The leaves are linked from well outside this library: SigilTree's
reconciler carries the erased value on every description and follows a
memo's describe through the follow leaf, SigilGpu answers the reporting
door's error when it refuses a device, SigilMeasure states every span in
the time unit, SigilGeometry's path leaf builds its value-noise field on
the mixers, its mesh and point-operator runtimes are erased values,
SigilWorld's geometry signature is an FNV fold and its element comparator
sits under the pin, and SigilWeave's intercept cache keys with the stir
and its layout guard is the memo with a paragraph's inputs folded in.

## Build and test

[docs/overview/testing.md](../../../docs/overview/testing.md) is the
contract every library here is built, tested and measured under: one
`core_test` over every feature's `test/` and one `core_bench` over every
feature's `bench/`, ctest one entry per SUITE, what a case may pin, and
what a label promises. What is only true of SigilCore:

| directory | suites | what they prove |
|---|---|---|
| `callable/test/` | `CoreCallable` | which callables a signature accepts and which it refuses, answered at compile time; that the parameters a callable did not name are still evaluated and dropped; that every accepted spelling of one drawing answers the same; and that an empty holder is false |
| `comparable/test/` | `Erased`, `Fields` | the erased value — empty, copies of one value, two comparable models compared by type and by value, the escape hatch equal to nothing but its own copies — and the field pin over aggregates of the shapes a comparable value takes |
| `compute/test/` | `Angle`, `Duration`, `Fnv1a`, `Fnv1aFold`, `Combine`, `Intervals`, `Noise`, `Stream`, `ChanceStream`, `Draws`, `Shapes`, `Sequences`, `Shuffle`, `Reservoir`, `Chance`, `Field`, `NoiseField`, `Curve` | the mixers and folds, pinned to the exact words and floats they produce; the stream, pinned to the mixer it names word for word and to a sequence for a seed, with each distribution's moments held to a tolerance; the field, pinned per kind and against a transcription of the value noise it agrees with, with the claims a pin cannot make — a period that really repeats, a range that octaves do not widen, and near values at near points; the curve, held to what makes it a value and to the character each house shape is chosen for; the angle conversion, pinned to the nearest float of each exact ratio and held against the two spellings that round twice; the angle that names its unit, read from CSS text in each of the four units and refused without one, reaching its own measure unchanged; the time unit read from CSS text in seconds and milliseconds and refused without a unit |
| `schedule/test/` | `ScheduleParallel`, `ScheduleConcurrentIo` | what the work seam promises: chunks disjoint and covering the range exactly once, the grain alone deciding when a range stays on its caller, a body's exception reaching the caller, and the blocking fan-out running every item once and joining every thread even when one item fails |
| `follow/test/` | `CoreFollow` | a read inside a scope recorded once at its revision, a changing write making the record stale and an equal one not, nothing recorded outside a scope, nested scopes, and a write of a value the followed describe read refused and reported once |
| `memo/test/` | `RebuildGuard`, `CachedValue`, `QuantizeKey` | the keyed rebuild guard — when a build fires, what a throw leaves behind — the value held with it, and the quantized key a continuous input is bucketed by |
| `report/test/` | `CoreReport`, `CoreError` | the one line format, the standard error when no reporter is installed, and an installed reporter hearing a warning in its place; the error's line and code unset until stated, compared as a value and carried as the refusing side of an expected answer |

Nothing here needs a device, a font or a network.

**The exact words and floats in `compute/test/` are the one place this
library pins a number rather than a property.** These bodies exist so a
second implementation of one of them agrees to the bit — a GPU kernel
reproduces the PCG three word for word, and a jitter, a point cook and a
shader's CPU twin all have to draw the same number for the same index —
and no property catches a drifted mixer, because "in range", "the same
twice" and "different for different seeds" are all still true of the
wrong stream.

The benchmark arms are shaped so each one's cost is read as a difference
against its neighbour: the comparable arms time each erased comparison
against the same question asked of the model directly, so what erasure
costs is one arm beside another; the compute arms time each mixer, the
stream, its distributions and the field one call at a time, which is how
they are spent; the schedule arms time a divided range over a body that
does nothing but touch its item, at three sizes, so what is measured is
the split rather than any consumer's arithmetic.
