---
kind: verb
library: SigilCompose
name: initialLetter
qualified: sigil::compose::Element::initialLetter
header: sigilcompose/core/verbs/TextStyle.h
group: The text leaf
python: sigil.compose.Element.initialLetter
status: stable
---

# initialLetter

This passage's opening set large — a versal sized so its cap height
spans the lines it is given, seated on the baseline it sinks to, with
the lines under it wrapping the notch it cuts.

## Description

```cpp
text(body, bodyStyle).initialLetter({.lines = 3});
text(body, bodyStyle).initialLetter({.lines = 3, .sink = 1});
```

**No key, no second element and no split string.** The letter is part of
the passage, and the two numbers it is made of — the size that makes a
cap span three lines, and the baseline it lands on — are answered where
the block's pitch and the face's own metrics are, which is inside the
layout.

`weave::InitialLetter` carries how many letters, which metric the
alignment is made on, whether the following lines wrap the box or the
glyph, the standoff, and the style it is set in.

**It applies to the FIRST block of this passage**, whichever way that
block is styled — a whole style, a name, or the block in force.

## See also

[`paragraphs`](paragraphs.md), `block`, `weave::InitialLetter`.
