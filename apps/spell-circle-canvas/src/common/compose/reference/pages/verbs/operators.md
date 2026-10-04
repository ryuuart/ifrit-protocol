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
placed. Each settling round begins from the flex placement and no operator
turn, so a modifier-only list can nudge that placement once. Arranging
operators are written before
adding ones, because they run first whatever the list says; a list that says
otherwise is reported once.

**An operator is a comparable value** with `arrange(Arrangement&)` or
`add(Scope&)` and an equality, so an unchanged list over unchanged facts
prunes; a value with no equality is the escape hatch that never does. A
stock layout uses that same arranging call. `layout(scheme)` is this verb
under the shorter spelling.

**An arrangement borrows its child records.** `Arrangement::children` is a
span whose count and order belong to the composer. Each record carries the
child's measured size, baseline, cells, area and facts, together with its
current `rect` and `turnDegrees`. An arranger writes `place`, `centreAt`
and `turn` directly onto those records; keep no reference after the call.
`Arrangement::Child::sizeIn` resolves stated percentages against the box
that the arranger gives that child.

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
