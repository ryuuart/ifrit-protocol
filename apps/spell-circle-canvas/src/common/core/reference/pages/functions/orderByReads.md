---
kind: function
library: SigilCore
name: orderByReads
qualified: sigil::core::orderByReads
group: Reconcile
status: stable
---

# orderByReads

ORDERS READERS AFTER WHAT THEY READ.

The keys give each reader's OWN key — empty for one nothing can read —
and the reads give, in the same order, what each one declares it reads.
The answer is a permutation of the indices in which every reader comes
after every reader whose key it reads.

## Declared reads

A reconciled tree is settled by passes, and most of what a pass does
depends only on the node it is looking at. Some of it does not: a label
placed at a word, a rule cut to a block, a connector between two boxes,
a light aimed at a mesh — each is a node whose answer is a function of
ANOTHER node's finished answer. Until a reader says which node it reads,
the only order a host can run them in is the order they were written in,
and a reader written before what it reads is then one pass behind.

A `sigil::core::Read` is that declaration: a key, and which
facet of that key's node is being read. Nothing here knows what a facet
MEANS — a bounds is a rect to one host and a world-space box to another
— and nothing here resolves a key. Both belong to the host; the ordering
does not.

## It is stable

That is the property that makes it safe to adopt: readers that read none
of each other keep the order they were given, so a host whose readers
are independent — which is nearly every host, on nearly every tree —
runs them in exactly the order it ran them in before. Only a real edge
moves anything.

## A cycle is broken where it closes

The readers caught in one keep their declaration order among themselves
and are emitted after everything they could be put after; a cyclic
declaration is therefore a slightly-off pass rather than a hang, which
is the same bargain a bounded convergence loop makes. A read of a key no
reader answers to is not an edge and orders nothing — the key may name
an ordinary node, which is settled before any reader runs.

## See also

- `reconcile/Reads.h` — the header: `orderByReads`, `Read`
- `RECONCILE.md` — the chapter the passes are described in
