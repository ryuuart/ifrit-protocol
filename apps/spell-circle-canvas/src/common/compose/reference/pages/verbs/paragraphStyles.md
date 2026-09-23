---
kind: verb
library: SigilCompose
name: paragraphStyles
qualified: sigil::compose::Text::paragraphStyles
header: sigilcompose/core/verbs/TextStyle.h
group: The text leaf
status: stable
---

# paragraphStyles

How each BLOCK of this passage is set — one entry per block, in block
order, a block being the text between two hard breaks.

## Description

```cpp
text(weave::rich(body).add(u8"A heading\nand its body text\nand more"))
    .paragraphStyles({headingStyle, bodyStyle});
```

**A block past the end of the list is set by this leaf's own fields
alone** — its alignment, justification, hyphenation and tab stops — so
one entry styles the first block and leaves the rest plain, which is
what a heading over a body wants.

`weave::ParagraphStyle` carries the leading, the air before and after,
the four indents, the keeps, and whichever of the four layout-wide
settings the block overrides; each of those falls back to this leaf's
own where the block leaves it unset.

**The name form resolves through the paragraph half of the sheet in force
where the leaf LANDS**, when it lays out, and lies over the block in
force there — so a named block keeps the leading it inherits and changes
only what its rule says. A name no sheet in force carries warns once and
changes nothing about its block.

**The two spellings are alternatives**, and the last one written stands.

## See also

`paragraph`, `initialLetter`, `weave::ParagraphStyle`, `applyStyleSheet`.
