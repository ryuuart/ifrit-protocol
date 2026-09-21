---
kind: verb
library: SigilCompose
name: textThreadTo
qualified: sigil::compose::Text::textThreadTo
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# textThreadTo

The frame this one fills into — the next link of a chain over one
`weave::Story`.

## Description

```cpp
root.children({frame(article).key("a").textThreadTo("b").width(Dimension(280))})
    .children({frame(article).key("b").textThreadTo("c").width(Dimension(280))})
    .children({frame(article).key("c").width(Dimension(280))});
```

**Each frame fills from where the one before it stopped**, so the cut
moves as any frame's measure moves.

**A frame that threads somewhere has a remainder BY DESIGN.** Overflow
is the normal case there and draws no marker, whatever ellipsis the leaf
asked for. The last frame of a chain is the one that threads nowhere,
and it keeps its.

**A frame nothing threads into is a chain's head** and starts at the
story's first word. A chain that closes on itself stops where it closes,
as a cyclic borrow does.

**Last-wins.** A frame threads into exactly one frame, so a chain that
named another one first no longer waits for it: the read is replaced
rather than added to.

## See also

[`textThreadBalance`](textThreadBalance.md), `frame`, `weave::Story`,
`Composer::settling`.
