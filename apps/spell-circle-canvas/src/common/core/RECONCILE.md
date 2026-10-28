# SigilCore — the reconciler

The chapter on the first kernel: descriptions built fresh every frame,
reconciled onto a tree the host retains, so that only what changed is
touched. It owns the shape of that tree — which retained node answers to
which description (matched by key, then by position), the memo that skips a
describe whose inputs did not change, the identity prune that leaves an
unchanged node alone, the describe a memo follows, and the counts of what a
pass did. `README.md` beside this file is the library; `CACHE.md` is the other kernel, which decides
what a host may KEEP between the passes this one drives, and `COMPARABLE.md`
is the leaf the erased seam value on a description comes from.

| header | holds |
|--------|-------|
| `reconcile/Reconciler.h` | `Reconciler<Host, Node, Description>` — `render()`, `replaceContent()`, `patch()`, `redescribe()`, `redescribeMoved()`, `patchChildren()`, `resolveMemo()`, `keyOf()`, `matchKeyOf()`, `indexKeys()`, `stats()`, `frame()`, and its `KeyIndex` |
| `reconcile/Host.h` | the `ReconcileHost` concept — the operations a host implements — and `DescriptionValue` |
| `reconcile/Node.h` | `Node<Derived, Description>` — the tree skeleton a host's node derives from: `parent`, `description`, `memoShell`, `followed`, `children` |
| `follow/Follow.h` | `Followable`, `FollowedRead`, `FollowedReads`, `FollowScope`, `following()`, `noteRead()` — a shared value that counts its changes, and the record of which of them one describe read (the `SigilCoreFollow` leaf, which the reconciler links) |
| `reconcile/Memo.h` | `Memo<Produced>` — a deferred describe and its key: `properties`, `equal`, `invoke` |
| `reconcile/Phases.h` | `Phase<Impl>` and `runPhases` — a host's declared pass list with its converging group |
| `reconcile/Reads.h` | `Facet`, `Read`, `orderByReads` — what one node reads off another, and the order that puts every reader after what it read |
| `reconcile/Stats.h` | `ReconcileStats` — the pass counts, and `report()` into `sigil::measure::Counters` |

## Using it

A host is any class that implements the `ReconcileHost` operations on
itself and holds a `Reconciler` over its own node and description types:

```cpp
#include <sigilcore/reconcile/Memo.h>
#include <sigilcore/reconcile/Node.h>
#include <sigilcore/reconcile/Reconciler.h>

using namespace sigil::core;

struct Description {                       // what the author builds each frame
  std::string key;
  int value = 0;
  std::vector<std::shared_ptr<Description>> children;
  std::optional<Memo<std::shared_ptr<Description>>> memo;
};
using DescriptionPtr = std::shared_ptr<Description>;

struct Instance : Node<Instance, DescriptionPtr> {   // what the host retains
  int lane = 0;                     // survives every patch
};

struct Host {
  Reconciler<Host, Instance, DescriptionPtr> reconciler{*this};
  std::unique_ptr<Instance> root;

  // reading a description
  static const std::string& keyOf(const DescriptionPtr& d) { return d->key; }
  static bool equal(const DescriptionPtr& a, const DescriptionPtr& b) {
    return a->key == b->key && a->value == b->value;
  }
  static bool reconcilesChildren(const DescriptionPtr&) { return true; }
  static const std::vector<DescriptionPtr>& children(const DescriptionPtr& d) {
    return d->children;
  }
  static const DescriptionPtr& descriptionOf(const DescriptionPtr& child) { return child; }
  static const Memo<DescriptionPtr>* memoOf(const DescriptionPtr& d) {
    return d->memo ? &*d->memo : nullptr;
  }
  static DescriptionPtr produce(const Memo<DescriptionPtr>& m) { return m.invoke(m.properties); }

  // acting on a node
  std::unique_ptr<Instance> create(const DescriptionPtr& d, Instance* parent,
                                   size_t ordinal, size_t count) {
    auto node = std::make_unique<Instance>();
    node->parent = parent;
    reconciler.patch(*node, d);     // the first patch is the mount
    return node;
  }
  void onPatched(Instance&, const Description* prev, const Description& next) {}
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

**A description is a value; a node is what it became.** The author builds a
fresh description every frame and throws it away. The reconciler walks it
beside the retained tree and asks, for every node, whether the new
description is provably identical to the one the node was last described
from (`equal`). If it is, the node PRUNES: nothing is patched, nothing is
dirtied, and only its children keep reconciling. If it is not, the
description is swapped in and the host's `onPatched` runs with the previous
one beside it. A field the host's `equal` leaves out does not produce a
wrong answer at the point of the mistake — it produces a node that never
patches again on that field. That is why `equal` must answer false for
anything it cannot compare.

**Children match by key, then by position.** A parent's children are
matched to the new list by key when they carry one, and among the unkeyed
by position. A matched node keeps its handle and everything it retains; the
host's `reorder` is told whether anything mounted, unmounted or moved,
because a reordered list changes what the parent paints even when every
child is identical. A child the new list does not name is retired through
`destroy`, after the reorder, stamped with the pass it left in (`frame()`).

**An identity change keeps the handle.** A node whose description changed
kind is still that node: `onPatched` sees the previous description and
rebuilds what the new kind cannot carry over, while the handle and the
lanes on it survive. The one exception is the host's own: a property fixed
at mount, which `remountRequired` names, retires the match and mounts
afresh.

**A reader declares what it reads, and the order follows.** Most of what a
settling pass does depends on the node it is looking at; some of it does
not. A label placed at a word, a rule cut to a block, a connector between
two boxes, a light aimed at a mesh — each is a node whose answer is a
function of ANOTHER node's finished answer, and until it says which node,
the only order a host can run them in is the order they were written in. A
reader written before what it reads is then one pass behind, every frame.

A `Read` is that declaration — a key, and which facet of that key's node is
read (`Bounds`, `Outline`, `Coverage`, `Units`) — and `orderByReads` turns
a set of them into the order the readers must run in. It knows nothing of
what a facet MEANS or how a key resolves; both are the host's.

It is STABLE, which is the property that makes adopting it free: readers
that read none of each other come out exactly as they went in, so a host
whose readers are independent runs them in the order it always did, and
only a real edge moves anything. A cycle is broken where it closes — the
readers caught in one keep their declaration order — so a cyclic
declaration is a slightly-off pass rather than a hang, which is the same
bargain the convergence cap makes.

**A memo is a pure function of its properties and of what it follows.** A
description can be a memo shell: properties, a comparison over them, and a
deferred describe. The reconciler compares the shell's properties against
the shell the node was last described from — a different type is a miss
before the comparison runs; on a hit the node's payload stands and the
describe is skipped, on a miss the describe runs and the result becomes the
payload. The shell rides on the node as `memoShell`; the payload is
`description`. Which of this library's own values are shaped to be a
memo's properties is `COMPUTE.md`'s last section.

**A memo's describe declares the shared values it reads.** The properties
are one half of a memo's key; the other half is every shared value its
describe reads that can change after it — a live number a sketch or an
engine writes. Such a value is a `Followable`: it counts its changing
writes in `Followable::revision`. The reconciler runs every memo's
describe inside a `FollowScope`, and every followable value read there
lands in the node's `followed` as a `FollowedRead` — one entry per value
however often it was read, at the revision it stood at. That record is a
DECLARED READ of the memo, beside its properties: a memo whose properties
compare equal still misses when `FollowedReads::moved` says one of its
reads has changed since, and a describe that reads nothing followable
records nothing and costs nothing.

Nothing is pushed to the reconciler when such a value changes. A host
keeps the nodes whose `followed` is not empty — the walk `indexKeys` takes
is the place to collect them — and asks at its settle:
`redescribeMoved()` describes again every one whose reads moved, once
each however many of its reads moved, by `redescribe()`: the same shell, a
fresh describe, patched in as `patch()` would and reconciling only that
node's subtree. A memo under another that moved is described by that
one's patch, never twice. Several changes before one settle are one
describe, and a change that lands before the settle is described in it.

A describe that WRITES a followable value it has already read would be
stale the moment it finished, and describing it again would write again.
`Followable::admitsWrite` refuses that write, and the first refusal of each
value is reported through the reporting door; the describe stands as it
read, and nothing loops. A read outside any memo's describe — a paint
program, a timer, a host describing its root — records nothing: it is a
snapshot, and a host that wants the subtree described again calls
`render()` or `replaceContent()` itself.

**The one upward signal is `invalidate`.** The reconciler never marks a
host's caches by itself; a node whose content changed under it — a slot's
content replaced through `replaceContent` — is reported through
`invalidate`, and the host stales what it keeps above the node.

**Animation is not the reconciler's.** A patch bends the running motions of
one description onto the endpoints of the next, and every part of that —
the lane that addresses a held motion, the retargets over a fixed or a
positional family, and the comparators that decide two animatable slots are
the same — is SigilMotion's, in `<sigilmotion/values/Lanes.h>` and beside
the values themselves. The reconciler calls a host's `onPatched` and the
host does the retarget; the kernel names no motion type at all, and links
no motion target.

**Phases converge.** A host declares its settling passes as a list of
`Phase<Impl>` — a name, a member function answering whether it moved
anything, and whether it converges. `runPhases` runs each non-converging
phase once and the contiguous converging group until a round changes
nothing or `maximumRounds` is reached, calling `settle` after every round that
changed something. The cap is what guarantees termination if two writers
ever disagree permanently.
