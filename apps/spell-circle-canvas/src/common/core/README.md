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

Under the kernels are two leaves, which libraries far from any
reconciler link on their own.

**Comparable** — what a value needs before anything can decide it did not
change: type erasure that keeps its equality, so a set of operations can
ride on a value and two holders can still ask whether they carry the
same one; and the field pin, which fails the build when a struct grows a
member a hand-written comparator does not mention.

**Compute** — the arithmetic several libraries have to agree on to the
bit: the seeded mixers a jitter draws from, and the folds a cache key is
accumulated with. The standard library is the whole of its dependencies,
so a shader's CPU twin, a point cook, a text cache and a resource store
all reach the same bodies.

Namespace `sigil::core`. One target per directory:

| target | directory | holds |
|--------|-----------|-------|
| `SigilCoreComparable` | `comparable/` | comparable type erasure, the field pin |
| `SigilCoreCompute` | `compute/` | the seeded mixers, the identifying folds |
| `SigilCoreReconcile` | `reconcile/` | the reconciler, its memo, the inherited-value channel, the animation lanes, the phase runner |
| `SigilCoreCache` | `cache/` | the cache policy, the settled-subtree proof, the stability release, the bake seam |

The two leaves are header-only, so they are INTERFACE targets and produce
no archive; the two kernels are static libraries. `SigilCoreComparable`
takes the standard library and Boost.PFR, `SigilCoreCompute` the standard
library alone — which is what lets any library link either without
pulling a kernel, a device or a drawing library behind it.

Every public header lives under `include/sigilcore/<feature>/` and is
spelled `<sigilcore/comparable/X.h>`, `<sigilcore/compute/X.h>`,
`<sigilcore/reconcile/X.h>` or `<sigilcore/cache/X.h>`;
`<sigilcore/comparable/Comparable.h>`, `<sigilcore/compute/Compute.h>`,
`<sigilcore/reconcile/Reconcile.h>` and `<sigilcore/cache/Cache.h>`
include their own directory's headers.

| header | holds |
|--------|-------|
| `comparable/Erased.h` | `Erased<Ops>` — comparable type erasure: a set of operations carried on the value that implements them |
| `comparable/Fields.h` | `kFieldCount<T>` — how many direct non-static data members an aggregate has, and the pin a hand-written comparator sits under |
| `compute/Noise.h` | `noise::hash` (a per-index float in [-1, 1]), and the PCG family `noise::pcgAdvance`, `noise::pcgMix`, `noise::pcgHash`, `noise::pcgNext`, `noise::pcgUnit`, `noise::pcgUnitNext` |
| `compute/Hash.h` | `hash::kFnvOffset`, `hash::kFnvPrime`, `hash::fnv1a` over a word or over text, and `hash::combine` — the stir that folds one more word into a hash in hand |
| `reconcile/Reconciler.h` | `Reconciler<Host, Node, Desc>` — `render()`, `replaceContent()`, `patch()`, `patchChildren()`, `resolveMemo()`, `keyOf()`, `matchKeyOf()`, `indexKeys()`, `stats()`, `frame()`, and its `KeyIndex` |
| `reconcile/Host.h` | the `ReconcileHost` concept — the operations a host implements — and `DescValue` |
| `reconcile/Node.h` | `Node<Derived, Desc>` — the tree skeleton a host's node derives from: `parent`, `desc`, `memoShell`, `children` |
| `reconcile/Memo.h` | `Memo<Produced>` — a deferred describe and its key: `props`, `equal`, `invoke`, `env` |
| `reconcile/Env.h` | `env::Provide`, `env::inherited`, `env::inheritedOr`, `env::bound`, and the `detail::EnvSnapshot`, `detail::envStack`, `detail::envEqual`, `detail::EnvRestore` a memo is built on |
| `reconcile/Erased.h` | `Erased<Ops>` under the name a description spells it by; the type is `comparable/Erased.h`'s |
| `reconcile/Compare.h` | `easeEqual`, `transitionEqual`, `boundMapEqual`, `propEqual`, their field pins, and `detail::fields` — the animation values decomposed member by member |
| `reconcile/Lanes.h` | `AnimatedFloat`, `AnimatedFloats`, `LaneSlot<Family>`, `Lane<Family>`, `familyLanes`, `ResolvedProp`, `resolveProp`, `resolveFloatAt`, `transitionFloatAt`, `retargetSlots`, `retargetFamily`, `mountEntrance` |
| `reconcile/Phases.h` | `Phase<Impl>` and `runPhases` — a host's declared pass list with its converging group |
| `reconcile/Stats.h` | `ReconcileStats` — the pass counts, and `report()` into `sigil::measure::Counters` |
| `cache/Policy.h` | `Cache` — the three-valued cache policy: `Auto`, `Always`, `Never` |
| `cache/Volatility.h` | `NodeVolatility`, `SubtreeVerdict`, `ChildVolatility` and `foldSubtree` — the settled-subtree proof |
| `cache/Settle.h` | `Settle<Values>` — the stability release: `observe`, `release`, `moved`, `frames`, `held`, `restart` |
| `cache/Bake.h` | `BakeOps<Target>`, `Bake<Target>`, `BakeState`, `BakeAction`, `decideBake`, `runBake` — the bake seam |

## Using it

A host is any class that implements the `ReconcileHost` operations on
itself and holds a `Reconciler` over its own node and description types:

```cpp
#include <sigilcore/reconcile/Reconcile.h>

using namespace sigil::core;

struct Desc {                       // what the author builds each frame
  std::string key;
  int value = 0;
  std::vector<std::shared_ptr<Desc>> children;
  std::optional<Memo<std::shared_ptr<Desc>>> memo;
};
using DescPtr = std::shared_ptr<Desc>;

struct Instance : Node<Instance, DescPtr> {   // what the host retains
  int lane = 0;                     // survives every patch
};

struct Host {
  Reconciler<Host, Instance, DescPtr> reconciler{*this};
  std::unique_ptr<Instance> root;

  // reading a description
  static const std::string& keyOf(const DescPtr& d) { return d->key; }
  static bool equal(const DescPtr& a, const DescPtr& b) {
    return a->key == b->key && a->value == b->value;
  }
  static bool reconcilesChildren(const DescPtr&) { return true; }
  static const std::vector<DescPtr>& children(const DescPtr& d) {
    return d->children;
  }
  static const DescPtr& descOf(const DescPtr& child) { return child; }
  static const Memo<DescPtr>* memoOf(const DescPtr& d) {
    return d->memo ? &*d->memo : nullptr;
  }
  static DescPtr produce(const Memo<DescPtr>& m) { return m.invoke(m.props); }

  // acting on a node
  std::unique_ptr<Instance> create(const DescPtr& d, Instance* parent,
                                   size_t ordinal, size_t count) {
    auto node = std::make_unique<Instance>();
    node->parent = parent;
    reconciler.patch(*node, d);     // the first patch is the mount
    return node;
  }
  void onPatched(Instance&, const Desc* prev, const Desc& next) {}
  void reorder(Instance& parent, bool structureChanged) {}
  bool remountRequired(const Instance&, const Instance&) { return false; }
  void invalidate(Instance&) {}
  void destroy(std::unique_ptr<Instance> node, uint64_t frame) {}
};

Host host;
host.reconciler.render(host.root, describe());   // every frame
host.reconciler.stats().patchedNodes;            // what that cost
```

## Mental model

**A description is a value; a node is what it became.** The author
builds a fresh description every frame and throws it away. The
reconciler walks it beside the retained tree and asks, for every node,
whether the new description is provably identical to the one the node
was last described from (`equal`). If it is, the node PRUNES: nothing is
patched, nothing is dirtied, and only its children keep reconciling. If
it is not, the description is swapped in and the host's `onPatched` runs
with the previous one beside it. A field the host's `equal` leaves out
does not produce a wrong answer at the point of the mistake — it produces
a node that never patches again on that field. That is why `equal` must
answer false for anything it cannot compare.

**Children match by key, then by position.** A parent's children are
matched to the new list by key when they carry one, and among the
unkeyed by position. A matched node keeps its handle and everything it
retains; the host's `reorder` is told whether anything mounted,
unmounted or moved, because a reordered list changes what the parent
paints even when every child is identical. A child the new list does not
name is retired through `destroy`, after the reorder, stamped with the
pass it left in (`frame()`).

**An identity change keeps the handle.** A node whose description changed
kind is still that node: `onPatched` sees the previous description and
rebuilds what the new kind cannot carry over, while the handle and the
lanes on it survive. The one exception is the host's own: a property
fixed at mount, which `remountRequired` names, retires the match and
mounts afresh.

**A memo is a pure function of (props, environment).** A description can
be a memo shell: props, a comparison over them, and a deferred describe.
The reconciler compares the shell's captured environment first and its
props second against the shell the node was last described from; on a
hit the node's payload stands and the describe is skipped, on a miss the
describe runs under the environment its author had (`EnvRestore`) and
the result becomes the payload. The shell rides on the node as
`memoShell`; the payload is `desc`.

**An inherited value lands in the description.** `env::Provide<T>` binds
a value for a describe scope and `env::inherited<T>()` reads it four
levels down; the value is read DURING describe and lands in the reading
node's own description, so the prune is already an exact dependency
tracker and no phase learns a new concept. The environment reaches the
kernel only through the memo, where it is part of the key.

**The one upward signal is `invalidate`.** The reconciler never marks a
host's caches by itself; a node whose content changed under it — a
slot's content replaced through `replaceContent` — is reported through
`invalidate`, and the host stales what it keeps above the node.

**Lanes are addressed by where the motion lives.** A lane is one
`Animatable<float>` a description carries, with the slot the host holds
its motion in: a fixed row of the host's slot array, or a position in a
family whose length the description decides. `retargetSlots` ramps every
row from wherever its motion is now, using the lane's standing value
where one side of the diff lacks the field; `retargetFamily` does the
same for a positional family and DROPS the motions when the family's
shape changed, because a motion carried onto an endpoint that now means
something else is worse than none. `mountEntrance` plays what a
description declared as its entrance, after the host's extra delay.

**Phases converge.** A host declares its settling passes as a list of
`Phase<Impl>` — a name, a member function answering whether it moved
anything, and whether it converges. `runPhases` runs each non-converging
phase once and the contiguous converging group until a round changes
nothing or `maxRounds` is reached, calling `settle` after every round
that changed something. The cap is what guarantees termination if two
writers ever disagree permanently.

## The caching proof

**What a host reports is a DECLARATION, never a difference.** `foldSubtree`
takes one `NodeVolatility` — the `Cache` policy the author stated
(`Auto`, `Always`, `Never`), whether they asked for the subtree to be
held by a value memo, and five facts about what this node does off the
describe clock — and the `ChildVolatility` its children folded into. It
answers a `SubtreeVerdict`.
A host that reports a term one frame late has already replayed an artefact
of a frame that has changed; a host that reports one early pays a re-bake
and nothing else. When in doubt, declare.

**The five facts are five different questions.** `ownPaint` is
motion that changes how the node COMPOSITES without changing what it
draws — the node's own artefact still replays, because it is drawn through
the motion, while an ancestor's would contain it. `ownContent` rebuilds
what the node draws, and blocks the node's artefact and every ancestor's.
`memoOpaque` is volatility no value comparison can SEE, which is the one
term a value memo turns on. `readsBackdrop` is not volatility at all: the
node composites against what is already on the canvas, so it cannot be
inside a bake, whose ground is transparent black. `samplesDestination` is
the half of that which refuses to be the bake's ROOT too — a root's blend
and opacity are applied outside its bake, a destination-sampling filter
inside it.

**The promise is one-sided.** A subtree the proof calls settled cannot
change its pixels without the host being told first. A subtree it calls
volatile may well be standing perfectly still — proving THAT is the value
memo's job, and `Settle` is what makes it sound.

**A binding cannot say that it stopped.** It stays connected for the life
of the node it drives, so an entrance that played once declares exactly
what a loop declares. `Settle<Values>` separates them by observation:
`observe` counts consecutive frames on which the values a node draws from
resolved identically, `release` converts a warmed-up count into the
verdict, and `moved` — run once per draw over what was released —
re-declares the frame a value assigned from OUTSIDE moves again. All three
sides are required. Skipping one does not fail loudly; it replays a frame
that has already changed.

**The decision is the kernel's, the artefact is the host's.** `BakeOps`
names four operations over a `Target` the kernel never looks inside: take
the bake, replay it, drop it, and say whether one is held. `decideBake`
answers `Live`, `Take` or `Replay` from three facts. A host with several
tiers — a recorded command list, a rasterized image, a whole subtree
composited into one layer — writes one model per tier and asks the same
question of all of them, which is what stops each tier from growing its
own copy of the rule.

## The leaves

**A comparable value carries its own equality.** `Erased<Ops>` holds a
model behind an abstract interface, and copies of one value are equal by
their shared state; two separately built values are equal when they hold
the same model type and that type's `==` says so. A model with no `==`
is the escape hatch and compares equal to nothing but its own copies —
conservative on purpose, because a value that cannot answer must never
answer "unchanged". That is what lets a seam — which executor draws,
which resolver runs, which painter paints — ride on a description that
something else compares.

**A hand-written comparator forgets silently.** Leave a field out and two
different values compare equal, the holder concludes nothing changed, and
it keeps producing what the old value produced for as long as it lives.
Nothing detects that from outside, because the wrong answer looks exactly
like a value that really did not change. `kFieldCount<T>` reads the member
count off the type, so `static_assert(kFieldCount<T> == N)` beside the
comparator turns adding a field into a build failure that names the
comparator to go and fix.

**A mixer is a contract, not a choice.** Everything in `compute/` is a
bit-exact function of its inputs, and the constants are fixed by the
agreement between the places that compute them: renders stored as bytes
are seeded through `noise::`, a GPU kernel reproduces `pcgAdvance`,
`pcgMix` and `pcgHash` word for word, and cache keys are accumulated with
`hash::`. Changing a constant fails no build. It re-rolls every stored
render, desynchronizes the two ends of every operator chain that runs on
both, and re-buckets everything already keyed. The tests pin exact
outputs — words as words, floats as bits — because determinism, range and
"different for different inputs" all survive a body that drifted.

**Two mixers side by side are two different functions.** `noise::hash`
and `noise::pcgHash` mix differently and answer differently; each seeds
work compared byte-for-byte against stored renders, so neither can become
the other. New code takes `pcgHash`. The same rule decides every
candidate for this directory: a body that differs is a second function
under its own name, never a merge.

## What belongs here

A function earns a place in one of the leaves when three things hold:

- **Two libraries need it identically.** Not "could share it" — actually
  compute the same number today, or have to agree with each other
  tomorrow because one reproduces the other (a shader and its CPU
  preview, a device executor and its reference).
- **The contract can be stated.** What it answers, what is fixed about
  it, and what breaks if it changes — in the header, evaluable by
  someone who has opened nothing else.
- **It brings its own test.** For the kernels that means behaviour over
  a fake host; for the leaves it means the outputs themselves, pinned.

Worked both ways: `Erased` qualifies because a reconciler's descriptions,
a mesh runtime and a point-operator runtime all carry a comparable erased
value, and one shape of it is what lets a value cross between them.
`kFieldCount` qualifies because a pin is worth nothing if each library
writes its own and one gets it subtly wrong. The seeded mixers qualify
because a stored render and a GPU kernel have to agree to the bit. An
easing curve does NOT qualify: it is animation's, and SigilMotion owns
it. Neither does a numeric constant a single library reaches for — those
stay with the library that spells them.

## Boundary

SigilCoreComparable and SigilCoreCompute link nothing of this project's
at all — the standard library, and Boost.PFR for the field pin. That is
the whole point of them: a library anywhere in the tree can link one
without acquiring a kernel. SigilCoreReconcile links SigilCoreComparable
(the erased seam value and the field pin), SigilMotion (the animatable
values and the ticker the lanes ramp on) and SigilMeasure (the published
counts), and nothing that draws, lays out or shapes text. SigilCoreCache
links SigilCoreReconcile alone. What stays with a host: what a node
retains, what a patch does to it, how children are ordered for drawing,
which descriptions compare equal, which of its own values are volatile,
and every artefact.
The leaves are linked from well outside this library: SigilGeometry's
path leaf names the mixers as its own `noise::`, its mesh and
point-operator runtimes are erased values, SigilWorld's geometry
signature is an FNV fold and its element comparator sits under the pin,
and SigilWeave's intercept cache keys with the stir. SigilCompose is one
host — its `Composer` holds a `Reconciler` over its
`Instance` and `ElementNode`, folds its Skia lanes, materials, gates and
text into the proof's declarations, holds a `Settle` over its own content
scalars, and implements `BakeOps` over its picture recordings. Yoga,
text, paint and the meaning of every term stay on its side of the seam.

## Build and test

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug -R sigilcore --output-on-failure
```

`sigilcore_comparable_test` (`comparable/test/`) covers the erased value
— empty, copies of one value, two comparable models compared by type and
by value, and the escape hatch that is equal to nothing but its own
copies — and the field pin over aggregates of the shapes a comparable
value takes. `sigilcore_comparable_bench` (`comparable/bench/`) times
each of those comparisons against the same question asked of the model
directly, so what erasure costs is the difference between two arms.

`sigilcore_compute_test` (`compute/test/`) pins the mixers to the exact
words and floats they produce, floats compared as bits: a body that
drifted by one operation fails there rather than in a stored render
weeks later. `sigilcore_compute_bench` (`compute/bench/`) times each
mixer one call at a time, which is how they are spent.

`sigilcore_reconcile_test` (`reconcile/test/`) exercises the reconciler
over a fake host — `FakeHost.h`, a host with nothing behind it that logs
every operation — alongside the environment channel, the lanes and the
phase runner; it links `SigilCoreReconcile` alone, so an edge that pulled
a drawing library in would fail there. The
benchmark, `sigilcore_reconcile_bench` (`reconcile/bench/`), times the
reconciler over the same fake host at several node counts; it builds
through the `benches` target and runs through `scripts/bench_ledger.py`,
which is where any number about it belongs.

`sigilcore_cache_test` (`cache/test/`) does the same for the caching
kernel over `FakeCacheHost.h` — nodes that declare volatility, hold a
numbered artefact instead of pixels and count every operation asked of
them: a still tree, one driven lane deep in a subtree unsettling every
ancestor, a declared opt-out, the three sides of the release, and the
counts that say a settled tree bakes once and replays after. It links
`SigilCoreCache` alone. `sigilcore_cache_bench` (`cache/bench/`) times
the proof over the same fake host at several node counts.
