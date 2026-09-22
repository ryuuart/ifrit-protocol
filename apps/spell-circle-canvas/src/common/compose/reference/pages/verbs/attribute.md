---
kind: verb
library: SigilCompose
name: attribute
qualified: sigil::compose::Element::attribute
header: sigilcompose/core/verbs/Structure.h
group: Facts and operators
status: stable
---

# attribute

A typed fact this node states about itself, under a name, for whatever
reads it. The node says what it IS — its tier, its hour, who it calls —
and nothing about what that does; a fact nothing reads does nothing.

## Description

```cpp
box().key("ledger")
     .attribute("tier", 1)
     .attribute("calls", std::vector<std::string>{"cache", "store"})
     .attribute("callout", Callout{.text = u8"failing health check"});
```

**Any value that copies and compares** is a fact: a number, a string, a
point, a list, a small struct stating a request. A string literal is
stored as a `std::string`. The value is read back in the type it was
written in and in no other — an `int` is not read as a `float` — except
by `number`, which an arranging operator reads a lane through whichever
numeric type it was written in.

**Who reads it decides what it means.** The scheme or operator on the
parent reads the facts of the children it places — `layouts::Radial`
told a `lane` stands each child at the fraction its fact makes of the
ring. An adding operator reads the facts of every node in its scope —
`connect::ByLane` wires each node to the keys it states. A scheme of the
older shape reads them through `LayoutInput::childAttributes`.

**A later fact under the same name replaces the earlier one.** `key` and
`styleClass` stay what they are: facts the kernel already knows.

**It is part of the node's identity for the prune.** Two descriptions
that differ in a fact are placed or read differently, so the node is
patched; two that agree prune.

## See also

`attributes` for a whole table laid over the node's own, `operators` for
what reads the facts, and `point` for a node that exists to carry them.
