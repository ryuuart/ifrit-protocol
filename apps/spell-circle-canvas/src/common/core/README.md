# SigilCore

The kernels a retained runtime is built on, and the leaves under them.

**The reconciler** — descriptions built fresh every frame, reconciled onto
a tree the host retains, so that only what changed is touched. It owns the
shape of that tree: which retained node answers to which description
(matched by key, then by position), the memo that skips a describe whose
inputs did not change, the identity prune that leaves an unchanged node
alone, and the counts of what a pass did.

**The caching proof** — what a host may keep between frames. Given what a
host declares about one node and what its children answered, it decides
whether a subtree is provably static for the frame, whether a value memo
may hold it, and whether the artefact in hand should be baked, replayed or
thrown away.

Everything a node retains beyond its place in the tree — layout state,
paint caches, running motions — is the host's, reached through named
operations the host implements on itself. That is the line both kernels
draw: they own the DECISION, the host owns the THING.

Under the kernels are four leaves, which libraries far from any reconciler
link on their own.

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

**Schedule** — where independent work runs. One parallel for over the task
runtime, taking a count, a grain and a body, so the runtime is named in one
file of this repository and in no header of it; and beside it a fan-out of
its own threads for calls that BLOCK on a disk or a server, which must not
sit on the workers a compute range shares.

**Hardware** — the GPU device itself: one device and its one command queue,
created here or adopted from a host that owns them, with the textures and
fences living on it named by handles that go stale rather than dangle, and
destruction that waits out the frames still in flight. It knows nothing
about what draws — no Skia, no Diligent, no Qt — which is exactly why it
sits here: a 2D backend and a 3D engine can stand on one device, name the
same texture and read the same fence, and neither has to link the other. A
host that holds one never spells a graphics API.

Namespace `sigil::core`, with `sigil::core::hardware` for the device
catalog. One target per directory:

| target | directory | holds |
|--------|-----------|-------|
| `SigilCoreComparable` | `comparable/` | comparable type erasure, the field pin |
| `SigilCoreCompute` | `compute/` | the seeded mixers, the stream and its distributions, the noise field, the identifying folds, the interval normal form, the shaped curve |
| `SigilCoreSchedule` | `schedule/` | the parallel for and its grain, and the fan-out for calls that block |
| `SigilCoreReconcile` | `reconcile/` | the reconciler, its memo, the inherited-value channel, the phase runner, the order declared reads imply |
| `SigilCoreCache` | `cache/` | the cache policy, the settled-subtree proof, the stability release, the bake seam, the keyed rebuild guard |
| `SigilCoreHardware` | `hardware/` | the GPU device and its queue, owned or adopted; textures and fences by generation-checked handle; deferred destruction |

`SigilCoreComparable` and `SigilCoreCompute` are header-only, so they are
INTERFACE targets and produce no archive; everything else is a static
library. `SigilCoreComparable` takes the standard library and Boost.PFR,
`SigilCoreCompute` the standard library alone, `SigilCoreSchedule` oneTBB
privately, and `SigilCoreReconcile` Boost.Unordered with Boost.ContainerHash
for its keyed indices. `SigilCoreHardware` takes the graphics API and
nothing else. Consumers still link only the feature they use, without
pulling in a drawing or layout library.

Every public header lives under `include/sigilcore/<feature>/` and is
spelled `<sigilcore/comparable/X.h>`, `<sigilcore/compute/X.h>`,
`<sigilcore/schedule/X.h>`, `<sigilcore/reconcile/X.h>` or
`<sigilcore/cache/X.h>`; `<sigilcore/comparable/Comparable.h>`,
`<sigilcore/compute/Compute.h>`, `<sigilcore/schedule/Schedule.h>`,
`<sigilcore/reconcile/Reconcile.h>` and `<sigilcore/cache/Cache.h>` include
their own directory's headers. The hardware feature's are
`<sigilcore/hardware/GpuDevice.h>`, `<sigilcore/hardware/Handle.h>` and
`<sigilcore/hardware/Fence.h>`.

## The chapters

One chapter per feature, beside this page. Each is the canon for the
feature it names — its headers, what it promises and how it is used — and
everything below is the library as a whole.

| chapter | what it covers |
|---------|----------------|
| **[RECONCILE.md](RECONCILE.md)** | the reconciler over a host: the prune, keyed matching, the memo and its environment, declared reads, and the phase runner |
| **[CACHE.md](CACHE.md)** | the caching proof: the policy, the settled-subtree verdict, the stability release, the bake seam and the rebuild guard |
| **[COMPARABLE.md](COMPARABLE.md)** | `Erased<Ops>` and `kFieldCount<T>`: what a value needs before anything can decide it did not change |
| **[COMPUTE.md](COMPUTE.md)** | the mixers, the stream and its distributions, the noise field, the identifying folds, the interval normal form and the shaped curve |
| **[SCHEDULE.md](SCHEDULE.md)** | the parallel for and its grain, and the fan-out a call that blocks runs on instead |
| **[HARDWARE.md](HARDWARE.md)** | `GpuDevice`: owning or adopting one, handles that go stale, deferred destruction, fences as timelines, and the two backends |

## What belongs here

A function earns a place in one of the two header-only leaves when three
things hold:

- **Two libraries need it identically.** Not "could share it" — actually
  compute the same number today, or have to agree with each other tomorrow
  because one reproduces the other (a shader and its CPU preview, a device
  executor and its reference).
- **The contract can be stated.** What it answers, what is fixed about it,
  and what breaks if it changes — in the header, evaluable by someone who
  has opened nothing else.
- **It brings its own test.** For the kernels that means behaviour over a
  fake host; for the leaves it means the outputs themselves, pinned.

Worked both ways: `Erased` qualifies because a reconciler's descriptions, a
mesh runtime and a point-operator runtime all carry a comparable erased
value, and one shape of it is what lets a value cross between them.
`kFieldCount` qualifies because a pin is worth nothing if each library
writes its own and one gets it subtly wrong. The seeded mixers qualify
because a stored render and a GPU kernel have to agree to the bit. An
easing curve does NOT qualify: it is animation's, and SigilMotion owns it —
which is why the comparator over that curve lives there too, beside the
curve, rather than in the kernel that happens to prune with it. Neither
does a numeric constant a single library reaches for — those stay with the
library that spells them.

## Boundary

SigilCoreComparable and SigilCoreCompute link nothing of this project's at
all — the standard library, and Boost.PFR for the field pin. That is the
whole point of them: a library anywhere in the tree can link one without
acquiring a kernel, and SigilMotion is one of the libraries that does, for
the pin its own comparators sit under. SigilCoreReconcile links
SigilCoreComparable (the erased seam value and the field pin) and
SigilMeasure (the published counts), and nothing that draws, lays out,
shapes text or animates. SigilCoreSchedule links nothing of this project's
either: a task runtime, privately, and the standard library's threads. That
privacy is the feature — a consumer hands over a count, a grain and a body,
so no header in this repository spells a task runtime, and the one that
runs every divided range in the process is chosen in one file.
SigilCoreCache links SigilCoreComparable alone — the header-only pair the
bake seam's erased value needs, not the reconciler beside it.
SigilCoreHardware links nothing of this project's either: the platform's
graphics API — Metal where there is one, the Vulkan loader resolved at run
time everywhere — and no Skia, no Diligent, no Qt. That is what makes one
device serviceable by two drawing libraries at once: SigilSkia stands
Graphite on it, a Diligent renderer adopts it, and neither knows the other
is there. What stays with a host: what a node retains, what a patch does to
it, how children are ordered for drawing, which descriptions compare equal,
which of its own values are volatile, and every artefact.

The leaves are linked from well outside this library: SigilGeometry's path
leaf builds its value-noise field on the mixers, its mesh and
point-operator runtimes are erased values, SigilWorld's geometry signature
is an FNV fold and its element comparator sits under the pin, and
SigilWeave's intercept cache keys with the stir. SigilCompose is one host —
its `Composer` holds a `Reconciler` over its `Instance` and `ElementNode`,
folds its Skia lanes, materials, gates and text into the proof's
declarations, holds a `Settle` over its own content scalars, and implements
`BakeOps` over its picture recordings. Yoga, text, paint and the meaning of
every term stay on its side of the seam.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/sigil.py setup --config Release
cmake --build build --config Release --target core_test
ctest --test-dir build -C Release --output-on-failure
```

The library has one test binary, `core_test`, built from every feature's
`test/` directory; ctest discovers one entry per CASE out of it, so a suite
or a case is selected by name with no target behind it —
`-R '^Reconciler\.'` for the reconciler's cases, `-R '^Field\.'` for the
field's:

| directory | suites | what they prove |
|---|---|---|
| `comparable/test/` | `Erased`, `Fields` | the erased value — empty, copies of one value, two comparable models compared by type and by value, the escape hatch equal to nothing but its own copies — and the field pin over aggregates of the shapes a comparable value takes |
| `compute/test/` | `Fnv1a`, `Fnv1aFold`, `Combine`, `Intervals`, `Noise`, `Stream`, `ChanceStream`, `Draws`, `Shapes`, `Sequences`, `Shuffle`, `Reservoir`, `Chance`, `Field`, `NoiseField`, `Curve` | the mixers and folds, pinned to the exact words and floats they produce; the stream, pinned to the mixer it names word for word and to a sequence for a seed, with each distribution's moments held to a tolerance; the field, pinned per kind and against a transcription of the value noise it agrees with, with the claims a pin cannot make — a period that really repeats, a range that octaves do not widen, and near values at near points; the curve, held to what makes it a value — two of the same shape at the same numbers are equal, two shapes at the same numbers are not, and a caller's own body compares by the same rule — and to the character each house shape is chosen for |
| `schedule/test/` | `ScheduleParallel`, `ScheduleConcurrentIo` | what the work seam promises: chunks disjoint and covering the range exactly once, the grain alone deciding when a range stays on its caller, a body's exception reaching the caller, and the blocking fan-out running every item once and joining every thread even when one item fails |
| `reconcile/test/` | `Reconciler`, `Env`, `Phases`, `Reads` | the reconciler over a fake host, the inherited-value channel, the phase runner and the read ordering |
| `cache/test/` | `CacheProof`, `Volatility`, `CacheHost`, `CacheSettle`, `CacheBake`, `RebuildGuard`, `CachedValue`, `QuantizeKey` | the settled-subtree proof, the stability release and the bake seam over a fake host, and the keyed rebuild guard with the quantized key a continuous input is bucketed by |
| `hardware/test/` | `HardwareHandle`, `MipChain` | what the device feature decides without a device: generation-checked handles, and how deep a mip chain a size allows |
| `hardware/test/DeviceTest.mm` | `HardwareDevice` (`gpu`) | a real device — what it comes up with, what it refuses to adopt, when a destroyed resource is really gone, who releases an imported texture, fences as timelines, and the levels a texture is built with |

The `HardwareDevice` suite exists on Apple alone and every case in it skips
where there is no GPU, which is why it carries a label: a case that skips is
not coverage on the machine it skipped on, and the label is how a runner is
told. The same questions on the Vulkan backend are asked in SigilGeometry's
`Device` suite, since that is where a Vulkan device exists to ask them of.

One file per subject, named for what it asserts: `HashTest`,
`IntervalsTest`, `NoiseTest`, `ChanceTest`, `FieldTest` and `CurveTest` in
`compute/test/`; `ErasedTest` in `comparable/test/` (the erasure and the
field pin are one subject — what a value needs before anything can decide
it did not change — and a consumer takes both or neither); `ParallelTest`
and `ConcurrentIoTest` in `schedule/test/`; `ReconcilerTest`, `EnvTest`,
`PhasesTest` and `ReadsTest` in `reconcile/test/`; `VolatilityTest`,
`SettleTest`, `BakeTest` and `RebuildTest` in `cache/test/`; `HandleTest`,
`MipChainTest` and `DeviceTest` in `hardware/test/`.

A case asserts one thing a public header promises and is named that promise
as a sentence, so a failure line reads as the claim that broke. It pins only
what editing this library could falsify. The exact words and floats in
`compute/test/` are exactly that: these bodies exist so a second
implementation of one of them agrees to the bit — a GPU kernel reproduces
the PCG three word for word, and a jitter, a point cook and a shader's CPU
twin all have to draw the same number for the same index — and no property
catches a drifted mixer, because "in range", "the same twice" and
"different for different seeds" are all still true of the wrong stream. A
claim made N times with one thing varying is one `TEST_P` whose parameter
is that thing, with its rows named: the FNV folds are one over seven
inputs, and the ranges the headers state for their draws are one over six
draws.

Each kernel is exercised over a fake host: `reconcile/test/FakeHost.h`, a
host with nothing behind it that records every operation the reconciler asks
of it as a structured event — the operation, whose key, and that operation's
own arguments — so a claim about what was asked and in what order survives
any rewording of what a host would print; and `cache/test/FakeCacheHost.h`,
whose nodes declare volatility, hold a numbered artefact instead of pixels
and count every operation asked of them. A fake host is the subject of a
measurement as much as of a test, so each feature's benchmark compiles with
its own `test/` on its include path and drives the host defined there — one
definition, and the two binaries cannot disagree about what they are
exercising. Each fake lives in its own `sigil::core::test` namespace under
the feature it serves, which is what lets every one of them spell the
plainest name for what it is — `FakeHost`, `FakeNode` — with no feature's
test reaching into another's directory to find out.

Every API name these documents spell is compiled against the headers that
own it: a generated translation unit inside `core_test` turns every
qualified name in `README.md` and in the six chapters into a probe, and the
generator fails on a documented name no header declares. Prose nobody
compiles is prose that goes stale, and a chapter is the same canon as the
page that links it.

The benchmarks are executables, not tests, and all of them are arms of one
binary, `core_bench`. The comparable arms time each erased comparison
against the same question asked of the model directly, so what erasure costs
is the difference between two arms; the compute arms time each mixer one
call at a time, which is how they are spent, and the stream, its
distributions and the field the same way, so what a source or an octave
costs is one arm beside another; the schedule arms time a divided range over
a body that does nothing but touch its item, at three sizes, so what is
measured is the split rather than any consumer's arithmetic; the reconcile
and cache arms time the reconciler and the proof over the fake hosts at
several node counts; and the hardware arms time the device. They build
through the `benches` target and run through `scripts/sigil.py bench`, which
is where any number about them belongs.
