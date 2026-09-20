---
kind: verb
library: SigilCompose
name: block
qualified: sigil::compose::Element::block
header: sigilcompose/core/verbs/Cascade.h
group: The cascade
python: sigil.compose.Element.block
status: stable
---

# block

The block everything under this node is set in, as a PARTIAL — the
paragraph half of what [`font`](font.md) does for the type.

## Description

**This is the ONE spelling of every block field.** The leading, the
alignment, the justification, the hyphenation, the tab stops, the
first- and last-line indents, widows and orphans, balanced ragging, the
breaking strategy, the last line, the writing mode, the line-break
locale and the line tables are all fields of `weave::Block`, and each
is written here.

```cpp
element.block({.alignment = weave::TextAlignment::kCenter});
element.block({.writingMode = weave::WritingMode::kVerticalRL});
```

**The fields it names override the block inherited and the rest
inherit**, exactly as the font does, and it cascades from wherever it
is written.

**A leaf that wrote a whole `weave::ParagraphStyle` inherits nothing**
for that block; a block named through `paragraphs(names)` is that
name's partial laid over the block in force.

**A VERTICAL leaf measures on the other axis.** Its main extent is its
height, its intrinsic width is one column pitch per column, and for
`Align::Baseline` it reports its first character's baseline. Per
character the mode is UTR#50's — ideographs upright with their `vert`
forms, Latin on its side — and a run that wants otherwise says so in
its own style. A run on a path ignores the mode altogether: its
baseline is its own geometry and has no columns to advance, so setting
both warns once and the path wins.

## See also

[`font`](font.md), `weave::Block`, `paragraphs`, `onPath`.
