---
kind: verb
library: SigilCompose
name: operators
qualified: sigil::compose::Element::operators
header: sigilcompose/core/verbs/Structure.h
group: Facts and operators
status: stable
---

# operators

The operators this node runs over what is under it, in list order. An
ARRANGING operator places and turns the node's direct children during
layout, reading each one's measured size and facts; an ADDING operator
runs once layout has settled, reads every node in its scope, and
attaches what it builds beside the authored children.

## Description

```cpp
box().children({each(services, serviceCard)})
     .operators({
         Tiers{"tier"},                                        // arranges
         Operator(connect::ByLane{.lane = "calls", .wire = rule})
             .zIndex(-1),                                      // adds, behind
     });
```

**The list is the order.** Each operator sees what the ones before it
left: a `layouts::Jitter` after a `layouts::Radial` nudges what the ring
placed. Every run of the arranging operators starts from the flex
layout's own answer, so a run after a text reflow answers the same
question the first did. Arranging operators are written before adding
ones, because they run first whatever the list says; a list that says
otherwise is reported once.

**An operator is a comparable value** with `arrange(Arrangement&)` or
`add(Scope&)` and an equality, so an unchanged list over unchanged facts
prunes; a value with no equality is the escape hatch that never does. A
placement scheme, `place(LayoutInput)`, is an operator too,
adapted when it is held; `layout(scheme)` is this verb under the
shorter spelling.

**The scope is closed.** A node under this one with operators of its
own is one node to them, with nothing under it; what it wants read from
outside it states as facts on its root. A node an operator added is read
by no operator, arranged by nothing and counted by no structural
pseudo-class.

**What an adding operator attaches** belongs to what it is about — to one
node, in that node's coordinates, gone when it goes, or to the scope —
and is an ordinary element beside the authored children, after them and
out of their flow. `zIndex` and `styleClass` stated on the `Operator`
are what its additions paint at and are dressed by where they state
none of their own. An unchanged tree mounts its additions once.

**A later call appends.**

## See also

`attribute` for the facts an operator reads, `layout` for the shorter
spelling of one arranging operator, and `Operator`, `Arrangement` and
`Scope` for the seam itself.
