---
kind: verb
library: SigilCompose
name: textThreadBalance
qualified: sigil::compose::Element::textThreadBalance
header: sigilcompose/core/Text.h
group: Content
python: sigil.compose.Element.textThreadBalance
status: stable
---

# textThreadBalance

This frame opens a BALANCED RUN of its chain — itself and every frame
after it up to the next frame that opens one, or the chain's end.

## Description

```cpp
frame(article).key("a").thread("b").textThreadBalance();
```

**The run is filled to the SHALLOWEST DEPTH that still holds what it was
asked to hold**, found by halving the depth the frames declare. Every
frame of the run resolves to that one depth, which is what makes three
columns of one story three columns of the same length instead of two
full ones and a stub.

**`throughLine` is what the run must hold**, as a story-relative line
number. The default holds ALL of the story; a number holds it down to
that line and leaves the rest to the frames after the run. That is how a
run of columns stops at a spanning element — the content above it is
balanced and shortened to fit, and what is left resumes below.

**The frames must declare a depth in pixels.** That depth is the ceiling
the halving starts from, and a run whose frames are sized by anything
else is left alone.

## See also

[`thread`](thread.md), `frame`, `weave::Story`.
