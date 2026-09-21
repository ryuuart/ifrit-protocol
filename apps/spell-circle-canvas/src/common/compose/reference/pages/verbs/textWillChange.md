---
kind: verb
library: SigilCompose
name: textWillChange
qualified: sigil::compose::Text::textWillChange
header: sigilcompose/core/verbs/TextStyle.h
group: The text leaf
python: sigil.compose.Element.textWillChange
status: stable
---

# textWillChange

An input of this passage is moving — a measure that animates, a frame
that grows, content that changes from one frame to the next — so this
layout is one of a run of them rather than an answer somebody asked for
once.

## Description

**It buys two things.** The break decisions of a block set in a uniform
measure are kept and reused, keyed on the words and on the measure taken
to the whole pixel below it, so a measure already crossed costs no break
decision at all. And the block is broken against the MEASURE alone
rather than against the frame's supply of lines, so a frame that only
changes in DEPTH changes which lines it holds and never where they
break. `Composer::settling` reports what a frame actually got for it.

**`candidates` is the floor under a frame the optimizing breaker cannot
finish**: how many break candidates it may weigh for one block — one
candidate being one line it scores — before that block is filled
greedily for that frame and counted as a degrade. Everything is back the
next frame the floor is met, and 0 is no floor. It is a COUNT and not a
stretch of clock, so a passage draws the same on a loaded machine as on
an idle one.

**NOTHING INFERS THIS.** A live layout answers the overflow tail
differently from a settled one — it is broken against the measure rather
than against the lines the frame has left — so a guess would change the
setting of a page that never moves. A passage that moves says so.

```cpp
text(caption, body).width(Dimension(slider)).textWillChange(true, 6000);
```

## See also

`Composer::settling`, [`textThreadTo`](textThreadTo.md), `maxTextLines`.
