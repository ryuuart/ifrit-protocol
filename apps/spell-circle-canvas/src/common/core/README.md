# SigilCore

The kernels a retained runtime is built on. Two of them.

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

Namespace `sigil::core`. One static target per directory:

| target | directory | holds |
|--------|-----------|-------|
| `SigilCoreReconcile` | `reconcile/` | the reconciler, its memo, the inherited-value channel, the animation lanes, the phase runner |
| `SigilCoreCache` | `cache/` | the cache policy, the settled-subtree proof, the stability release, the bake seam |

Every public header lives under `include/sigilcore/<feature>/` and is
spelled `<sigilcore/reconcile/X.h>` or `<sigilcore/cache/X.h>`;
`<sigilcore/reconcile/Reconcile.h>` and `<sigilcore/cache/Cache.h>`
include their own directory's headers.

| header | holds |
|--------|-------|
| `reconcile/Reconciler.h` | `Reconciler<Host, Node, Desc>` — `render()`, `replaceContent()`, `patch()`, `patchChildren()`, `resolveMemo()`, `keyOf()`, `matchKeyOf()`, `indexKeys()`, `stats()`, `frame()`, and its `KeyIndex` |
| `reconcile/Host.h` | the `ReconcileHost` concept — the operations a host implements — and `DescValue` |
| `reconcile/Node.h` | `Node<Derived, Desc>` — the tree skeleton a host's node derives from: `parent`, `desc`, `memoShell`, `children` |
| `reconcile/Memo.h` | `Memo<Produced>` — a deferred describe and its key: `props`, `equal`, `invoke`, `env` |
| `reconcile/Env.h` | `env::Provide`, `env::inherited`, `env::inheritedOr`, `env::bound`, and the `detail::EnvSnapshot`, `detail::envStack`, `detail::envEqual`, `detail::EnvRestore` a memo is built on |
| `reconcile/Erased.h` | `Erased<Ops>` — comparable type erasure for a seam value on a description |
| `reconcile/Compare.h` | `easeEqual`, `transitionEqual`, `boundMapEqual`, `propEqual`, and the `detail::fields` pins with `detail::kFieldCount` |
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

## Boundary

SigilCoreReconcile links SigilMotion (the animatable values and the
ticker the lanes ramp on) and SigilMeasure (the published counts), and
nothing that draws, lays out or shapes text. SigilCoreCache links
SigilCoreReconcile alone, for the comparable type erasure a seam value
takes. What stays with a host: what a node retains, what a patch does to
it, how children are ordered for drawing, which descriptions compare
equal, which of its own values are volatile, and every artefact.
SigilCompose is one host — its `Composer` holds a `Reconciler` over its
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

`sigilcore_reconcile_test` (`reconcile/test/`) exercises the reconciler
over a fake host — `FakeHost.h`, a host with nothing behind it that logs
every operation — alongside the environment channel, the type erasure,
the lanes and the phase runner; it links `SigilCoreReconcile` alone, so
an edge that pulled a drawing library in would fail there. The
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
