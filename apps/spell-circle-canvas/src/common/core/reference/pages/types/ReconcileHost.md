---
kind: type
library: SigilCore
name: ReconcileHost
qualified: sigil::core::ReconcileHost
group: Reconcile
status: stable
---

# ReconcileHost

What the reconciler asks of its host.

The reconciler owns the tree's shape — which node answers to which
description, matched by key and then by position, memo resolution, the
identity prune and the pass counts — and the host owns everything a
node retains beyond that: layout state, paint caches, running motions.
The split is a set of named operations the host implements on itself.

## Reading a description

- `keyOf(description)` — the description's key; empty means positional.
- `equal(a, b)` — are two descriptions provably identical? Equal
  descriptions PRUNE: the node is not patched, nothing is dirtied, and
  only its children keep reconciling. Anything the host cannot compare
  must answer false.
- `reconcilesChildren(description)` — does the reconciler walk this
  node's children, or does the host fill them by another path (a slot)?
- `children(description)` — the child descriptions, as a sized range
  whose elements `descriptionOf()` reads a handle off.
- `memoOf(description)` — the description's Memo, or null when it is not
  one.
- `produce(memo)` — run the memo and read the description it made.

## Acting on a node

- `create(description, parent, ordinal, count)` — a fresh node for the
  description under the parent, patched once through the reconciler.
  `ordinal` is the node's order among the children created in the same
  patch and `count` the parent's child count, for a host that staggers
  mounts.
- `onPatched(node, prev, next)` — the description changed: `prev` is
  null on the first patch. The node and whatever it retains SURVIVE an
  identity change; a kind that cannot carry the old state over rebuilds
  it here and keeps the handle.
- `reorder(parent, structureChanged)` — the children now stand in the
  parent's `children` order; `structureChanged` says a child mounted,
  unmounted or moved, which the prune must not swallow.
- `remountRequired(match, parent)` — must this surviving node be retired
  and created afresh rather than patched in place? For a property fixed
  at mount.
- `invalidate(node)` — the one upward signal: the node's content changed
  and every cache above it is stale.
- `destroy(node, frame)` — the node left the tree in that reconcile
  pass. The host retires it now or queues it; nothing in the reconciler
  holds it after this call.

## See also

- `reconcile/Host.h` — the header: `ReconcileHost`
- `RECONCILE.md` — the chapter the passes and the prune are described in
